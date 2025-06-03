#include "volc.hpp"

#include "log.hpp"

#include <alsa/asoundlib.h>
#include <assert.h>
#include <float.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/poll.h>

#define CHECK_RANGE(val, min, max) (((val) < (min)) ? (min) : ((val) > (max)) ? (max) : (val))

// Avoiding c math lib
long vceil(double d) {
    /*
      if ((n - 0.0000000000000008) == floor(n))
          this will break :(
      */

    static double eps = 0.999999999999999;
    return (long)(d + eps);
}

long convert_prange(float val, float min, float max) {
    return vceil((double)(val * (max - min) * 0.01f + min));
}

float convert_prange_back(long val, float min, float max) {
    return ((100.f * (float)val) - min) / (max - min);
}

// volume as percentage: 100% is 100.0
static float get_set_volume(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t chn, volc_volume_t volume) {
    if (!snd_mixer_selem_has_playback_volume(elem)) {
        return -1;
    }

    long orig;
    long pmin;
    long pmax;

    if (snd_mixer_selem_get_playback_volume(elem, chn, &orig) < 0) {
        return -1;
    }

    if (snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax) < 0) {
        return -1;
    }

    if (volume.action == volc_volume_t::VOLC_VOL_SAME) {
        return convert_prange_back(orig, (float)pmin, (float)pmax);
    }


    long val = convert_prange(volume.volume, (float)pmin, (float)pmax);
    if (volume.action == volc_volume_t::VOLC_VOL_INC) {
        val += orig;
    }
    val = CHECK_RANGE(val, pmin, pmax);
    if (snd_mixer_selem_set_playback_volume(elem, chn, val)) {
        return -1;
    }
    return convert_prange_back(val, (float)pmin, (float)pmax);
}

static snd_mixer_t *get_handle(int *err, char const *card) {
    snd_mixer_t *handle;
    {
        if ((*err = snd_mixer_open(&handle, 0)) < 0) {
            lg::error(" Mixer {} open error: {}", card, snd_strerror(*err));
            return nullptr;
        }
        if ((*err = snd_mixer_attach(handle, card)) < 0) {
            lg::error(" Mixer attach {} error: {}", card, snd_strerror(*err));
            snd_mixer_close(handle);
            return nullptr;
        }
        if ((*err = snd_mixer_selem_register(handle, nullptr, nullptr)) < 0) {
            lg::error(" Mixer register error: {}", snd_strerror(*err));
            snd_mixer_close(handle);
            return nullptr;
        }
        *err = snd_mixer_load(handle);
        if (*err < 0) {
            lg::error(" Mixer {} load error: {}", card, snd_strerror(*err));
            snd_mixer_close(handle);
            return nullptr;
        }
    }

    return handle;
}

extern volc_volume_state_t volc_volume_ctl(
    volc_t *volc, unsigned int channels, volc_volume_t new_volume, channel_switch_t channel_switch) {

    // snd_mixer_selem_channel_id_t chn;
    volc_volume_state_t state;

    if (channels != VOLC_ALL_CHANNELS) {
        channels = 1 << channels;
    }

    int firstchn = 1;
    int any_set = 0;
    for (int chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++) {
        int init_value;
        int new_value;

        if (!(channels & (1 << chn))) {
            continue;
        }
        if (!snd_mixer_selem_has_playback_channel(volc->elem, (snd_mixer_selem_channel_id_t)chn)) {
            continue;
        }

        switch (channel_switch) {
            case VOLC_CHAN_OFF:
            case VOLC_CHAN_ON:
                snd_mixer_selem_get_playback_switch(volc->elem, (snd_mixer_selem_channel_id_t)chn, &init_value);
                if (snd_mixer_selem_set_playback_switch(volc->elem, (snd_mixer_selem_channel_id_t)chn, (int)channel_switch)
                    < 0) {
                    continue;
                }
                break;
            case VOLC_CHAN_TOGGLE:
                if (firstchn || !snd_mixer_selem_has_playback_switch_joined(volc->elem)) {
                    snd_mixer_selem_get_playback_switch(volc->elem, (snd_mixer_selem_channel_id_t)chn, &init_value);
                    if (snd_mixer_selem_set_playback_switch(volc->elem,
                            (snd_mixer_selem_channel_id_t)chn,
                            init_value ? 0 : 1)
                        < 0) {
                        continue;
                    }
                }
                break;
            case VOLC_CHAN_SAME:
            default:;
        }

        if ((state.state.volume = get_set_volume(volc->elem, (snd_mixer_selem_channel_id_t)chn, new_volume)) <= 0) {
            continue;
        }

        snd_mixer_selem_get_playback_switch(volc->elem, (snd_mixer_selem_channel_id_t)chn, &new_value);
        state.state.switch_pos = (channel_switch_t)new_value;

        firstchn = 0;
        any_set = 1;
    }
    if (!any_set) {
        lg::warn(" failed to set any chanels");
        state.err = -1;
    }

    return state;
}

extern volc_t *volc_init(char const *selector, unsigned int selector_index, char const *card) {
    int err = 0;
    volc_t *volc = new volc_t {};
    snd_mixer_selem_id_alloca(&volc->sid);
    volc->card = card;

    snd_mixer_selem_id_set_index(volc->sid, selector_index);
    snd_mixer_selem_id_set_name(volc->sid, selector);

    volc->handle = get_handle(&err, volc->card);
    if (err) {
        delete volc;
        return nullptr;
    }

    volc->elem = snd_mixer_find_selem(volc->handle, volc->sid);
    if (!volc->elem) {
        lg::warn(" Unable to find simple control '{}',{}",
            snd_mixer_selem_id_get_name(volc->sid),
            snd_mixer_selem_id_get_index(volc->sid));
        snd_mixer_close(volc->handle);
        delete volc;
        return nullptr;
    }
    return volc;
}

extern void volc_deinit(volc_t *volc) {
    if (volc != nullptr) {
        if (volc->handle != nullptr) snd_mixer_close(volc->handle);
        delete volc;
    }
}
