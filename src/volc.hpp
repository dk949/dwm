#ifndef DWM_VOLC_HPP
#define DWM_VOLC_HPP

#define VOLC_DEF_CARD     "default"
#define VOLC_DEF_SEL      "Master"
#define VOLC_DEF_SEL_IDX  0
#define VOLC_ALL_CHANNELS ~0u
#define VOLC_ALL_DEFULTS  VOLC_DEF_SEL, VOLC_DEF_SEL_IDX, VOLC_DEF_CARD
#define VOLC_INC(X)       (VolcVolume {.volume = (X), .action = VolcVolume::VOLC_VOL_INC})
#define VOLC_DEC(X)       (VolcVolume {.volume = -(X), .action = VolcVolume::VOLC_VOL_INC})
#define VOLC_SET(X)       (VolcVolume {.volume = (X), .action = VolcVolume::VOLC_VOL_SET})
#define VOLC_SAME         (VolcVolume {.volume = 0, .action = VolcVolume::VOLC_VOL_SAME})

#define VOLC_GET_VOLUME VOLC_ALL_CHANNELS, VOLC_SAME, VOLC_CHAN_SAME

/*#define VOLC_VERBOSE*/

enum ChannelSwitch {
    VOLC_CHAN_OFF = 0,
    VOLC_CHAN_ON,
    VOLC_CHAN_TOGGLE,
    VOLC_CHAN_SAME,

};


using SndMixer = struct _snd_mixer;
using SndMixerElem = struct _snd_mixer_elem;
using SndMixerSelemId = struct _snd_mixer_selem_id;

struct Volc {
    SndMixer *handle;
    SndMixerElem *elem;
    SndMixerSelemId *sid;
    char const *card;
};

struct VolcVolume {
    float volume;

    enum {
        VOLC_VOL_INC,
        VOLC_VOL_SET,
        VOLC_VOL_SAME,
    } action;
};

union VolcVolumeState {
    long err;

    struct {
        ChannelSwitch switch_pos;
        float volume;
    } state;
};

Volc *volcInit(char const *selector, unsigned int selector_index, char const *card);
void volcDeinit(Volc *volc);
VolcVolumeState volcVolumeCtl(Volc *volc, unsigned int channels, VolcVolume new_volume, ChannelSwitch channel_switch);

#endif  // DWM_VOLC_HPP
