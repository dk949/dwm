#ifdef ASOUND
#    include "volc.h"

#    include "util.h"

#    include <alsa/asoundlib.h>
#    include <assert.h>
#    include <float.h>
#    include <getopt.h>
#    include <stdarg.h>
#    include <stdio.h>
#    include <stdlib.h>
#    include <string.h>
#    include <sys/poll.h>

#    define CARD_SZ 64

#    define CHECK_RANGE(val, min, max) (((val) < (min)) ? (min) : ((val) > (max)) ? (max) : (val))

// Avoiding c math lib
long vceil(double d) {
    /*
      if ((n - 0.0000000000000008) == floor(n))
          this will break :(
      */

    static double eps = 0.999999999999999;
    return (d + eps);
}

long convert_prange(float val, float min, float max) {
    return ((long)vceil(val * (max - min) * 0.01f + min));
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

    if (volume.action == VOLC_VOL_SAME) {
        return convert_prange_back(orig, (float)pmin, (float)pmax);
    }


    long val = convert_prange(volume.volume, (float)pmin, (float)pmax);
    if (volume.action == VOLC_VOL_INC) {
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
            WARN(" Mixer %s open error: %s", card, snd_strerror(*err));
            return NULL;
        }
        if ((*err = snd_mixer_attach(handle, card)) < 0) {
            WARN(" Mixer attach %s error: %s", card, snd_strerror(*err));
            snd_mixer_close(handle);
            return NULL;
        }
        if ((*err = snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
            WARN(" Mixer register error: %s", snd_strerror(*err));
            snd_mixer_close(handle);
            return NULL;
        }
        *err = snd_mixer_load(handle);
        if (*err < 0) {
            WARN(" Mixer %s load error: %s", card, snd_strerror(*err));
            snd_mixer_close(handle);
            return NULL;
        }
    }

    return handle;
}

extern volc_volume_state_t volc_volume_ctl(
    volc_t *volc, unsigned int channels, volc_volume_t new_volume, channel_switch_t channel_switch) {

    snd_mixer_selem_channel_id_t chn;
    volc_volume_state_t state;

    if (channels != VOLC_ALL_CHANNELS) {
        channels = 1 << channels;
    }

    int firstchn = 1;
    int any_set = 0;
    for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++) {
        int init_value;
        int new_value;

        if (!(channels & (1 << chn))) {
            continue;
        }
        if (!snd_mixer_selem_has_playback_channel(volc->elem, chn)) {
            continue;
        }

        switch (channel_switch) {
            case VOLC_CHAN_OFF:
            case VOLC_CHAN_ON:
                snd_mixer_selem_get_playback_switch(volc->elem, chn, &init_value);
                if (snd_mixer_selem_set_playback_switch(volc->elem, chn, channel_switch) < 0) {
                    continue;
                }
                break;
            case VOLC_CHAN_TOGGLE:
                if (firstchn || !snd_mixer_selem_has_playback_switch_joined(volc->elem)) {
                    snd_mixer_selem_get_playback_switch(volc->elem, chn, &init_value);
                    if (snd_mixer_selem_set_playback_switch(volc->elem, chn, init_value ? 0 : 1) < 0) {
                        continue;
                    }
                }
                break;
            default:;
        }

        if ((state.state.volume = get_set_volume(volc->elem, chn, new_volume)) <= 0) {
            continue;
        }

        snd_mixer_selem_get_playback_switch(volc->elem, chn, &new_value);
        state.state.switch_pos = new_value;

        firstchn = 0;
        any_set = 1;
    }
    if (!any_set) {
        WARN(" failed to set any chanels");
        state.err = -1;
    }

    return state;
}

extern volc_t *volc_init(char const *selector, unsigned int selector_index, char const *card) {
    int err = 0;
    volc_t *volc = malloc(sizeof(volc_t));
    snd_mixer_selem_id_alloca(&volc->sid);
    volc->card = card;

    snd_mixer_selem_id_set_index(volc->sid, selector_index);
    snd_mixer_selem_id_set_name(volc->sid, selector);

    volc->handle = get_handle(&err, volc->card);
    if (err) {
        free(volc);
        return NULL;
    }

    volc->elem = snd_mixer_find_selem(volc->handle, volc->sid);
    if (!volc->elem) {
        WARN(" Unable to find simple control '%s',%i",
            snd_mixer_selem_id_get_name(volc->sid),
            snd_mixer_selem_id_get_index(volc->sid));
        snd_mixer_close(volc->handle);
        free(volc);
        return NULL;
    }
    return volc;
}

extern void volc_deinit(volc_t *volc) {
    if (volc != NULL) {
        if (volc->handle != NULL) {
            snd_mixer_close(volc->handle);
        }
        free(volc);
    }
}

#endif  // ASOUND
