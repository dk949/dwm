#ifdef ASOUND
#include "volc.h"

#include <alsa/asoundlib.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>

static char err_msg[1024];
#define REPORT_ERR(...) sprintf(err_msg, __VA_ARGS__)
#define CARD_SZ         64

#ifdef VOLC_VERBOSE
#define VERBOSE_PRINT(FMT, ...) fprintf(stdout, FMT "\n", __VA_ARGS__)
#define VERBOSE_RET(TYPE, FMT, X)            \
    TYPE ret = (X);                          \
    fprintf(stdout, "ret = " FMT "\n", ret); \
    return ret;
#else
#define VERBOSE_PRINT(...)
#define VERBOSE_RET(TYPE, FMT, X) return (X);
#endif


#define CHECK_RANGE(val, min, max) (((val) < (min)) ? (min) : ((val) > (max)) ? (max) : (val))


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
    VERBOSE_PRINT("convert_prange val = %f, min = %f, max = %f", val, min, max);
    VERBOSE_RET(long, "%li", (long)vceil(val * (max - min) * 0.01f + min));
}

float convert_prange_back(long val, float min, float max) {
    VERBOSE_PRINT("convert_prange_back val = %li, min = %f, max = %f", val, min, max);
    VERBOSE_RET(float, "%f", ((100.f * (float)val) - min) / (max - min));
}

// volume as percentage: 100% is 100.0
static float get_set_volume(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t chn, volc_volume_t volume) {
    VERBOSE_PRINT("set_volume_simple: elem = %llu, chn = %d, volume.action = %d, volume.volume = %f",
        (unsigned long long)elem,
        chn,
        volume.action,
        volume.volume);
    if (!snd_mixer_selem_has_playback_volume(elem)) {
        VERBOSE_RET(float, "%f", -1);
    }

    long orig;
    long pmin;
    long pmax;

    if (snd_mixer_selem_get_playback_volume(elem, chn, &orig) < 0) {
        VERBOSE_RET(float, "%f", -1);
    }

    if (snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax) < 0) {
        VERBOSE_RET(float, "%f", -1);
    }

    if (volume.action == VOLC_VOL_SAME) {
        VERBOSE_RET(float, "%f", convert_prange_back(orig, (float)pmin, (float)pmax));
    }


    long val = convert_prange(volume.volume, (float)pmin, (float)pmax);
    if (volume.action == VOLC_VOL_INC) {
        val += orig;
    }
    val = CHECK_RANGE(val, pmin, pmax);
    if (snd_mixer_selem_set_playback_volume(elem, chn, val)) {
        VERBOSE_RET(float, "%f", -1);
    }
    VERBOSE_RET(float, "%f", convert_prange_back(val, (float)pmin, (float)pmax));
}

static snd_mixer_t *get_handle(int *err, const char *card) {
    VERBOSE_PRINT("get_handle: err = %d, card = %s", *err, card);
    snd_mixer_t *handle;
    {
        if ((*err = snd_mixer_open(&handle, 0)) < 0) {
            REPORT_ERR(" Mixer %s open error: %s", card, snd_strerror(*err));
            VERBOSE_RET(snd_mixer_t *, "%llu", NULL);
        }
        if ((*err = snd_mixer_attach(handle, card)) < 0) {
            REPORT_ERR(" Mixer attach %s error: %s", card, snd_strerror(*err));
            snd_mixer_close(handle);
            VERBOSE_RET(snd_mixer_t *, "%llu", NULL);
        }
        if ((*err = snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
            REPORT_ERR(" Mixer register error: %s", snd_strerror(*err));
            snd_mixer_close(handle);
            VERBOSE_RET(snd_mixer_t *, "%llu", NULL);
        }
        *err = snd_mixer_load(handle);
        if (*err < 0) {
            REPORT_ERR(" Mixer %s load error: %s", card, snd_strerror(*err));
            snd_mixer_close(handle);
            VERBOSE_RET(snd_mixer_t *, "%llu", NULL);
        }
    }

    VERBOSE_RET(snd_mixer_t *, "%llu", handle);
}



extern volc_volume_state_t
    volc_volume_ctl(volc_t *volc, unsigned int channels, volc_volume_t new_volume, channel_switch_t channel_switch) {
    VERBOSE_PRINT("volc_volume_ctl: volc = %llu, channels = %d, new_volume.action = %d ,new_volume.volume = %f, "
                  "channel_switch = %d",
        (unsigned long long)volc,
        channels,
        new_volume.action,
        new_volume.volume,
        channel_switch);


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
            VERBOSE_PRINT("wrong channel. channels = %x, current channel = %x", channels, 1 << chn);
            continue;
        }
        if (!snd_mixer_selem_has_playback_channel(volc->elem, chn)) {
            VERBOSE_PRINT("No playback for channel %x", 1 << chn);
            continue;
        }

        switch (channel_switch) {
            case VOLC_CHAN_OFF:
            case VOLC_CHAN_ON:
                snd_mixer_selem_get_playback_switch(volc->elem, chn, &init_value);
                if (snd_mixer_selem_set_playback_switch(volc->elem, chn, channel_switch) < 0) {
                    VERBOSE_PRINT("No playback switch for channel %x", 1 << chn);
                    continue;
                }
                break;
            case VOLC_CHAN_TOGGLE:
                if (firstchn || !snd_mixer_selem_has_playback_switch_joined(volc->elem)) {
                    snd_mixer_selem_get_playback_switch(volc->elem, chn, &init_value);
                    if (snd_mixer_selem_set_playback_switch(volc->elem, chn, init_value ? 0 : 1) < 0) {
                        VERBOSE_PRINT("No playback switch for channel %x", 1 << chn);
                        continue;
                    }
                }
                break;
            default:;
        }

        if ((state.state.volume = get_set_volume(volc->elem, chn, new_volume)) <= 0) {
            VERBOSE_PRINT("bad set volume on channel %x", 1 << chn);
            continue;
        }

        snd_mixer_selem_get_playback_switch(volc->elem, chn, &new_value);
        state.state.switch_pos = new_value;

        firstchn = 0;
        any_set = 1;
        VERBOSE_PRINT("set channel %x", 1 << chn);
    }
    if (!any_set) {
        REPORT_ERR(" failed to set any chanels");
        VERBOSE_PRINT("no channel was set.%s", "");
        state.err = -1;
    }

    return state;
}



extern volc_t *volc_init(const char *selector, unsigned int selector_index, const char *card) {
    int err = 0;
    memset(err_msg, 0, sizeof(err_msg));
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
        REPORT_ERR(" Unable to find simple control '%s',%i",
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


extern const char *volc_err_str() {
    return err_msg;
}

#endif  // ASOUND
