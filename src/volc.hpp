#ifndef DWM_VOLC_HPP
#define DWM_VOLC_HPP

#define VOLC_DEF_CARD     "default"
#define VOLC_DEF_SEL      "Master"
#define VOLC_DEF_SEL_IDX  0
#define VOLC_ALL_CHANNELS ~0u
#define VOLC_ALL_DEFULTS  VOLC_DEF_SEL, VOLC_DEF_SEL_IDX, VOLC_DEF_CARD
#define VOLC_INC(X)       (volc_volume_t {.volume = (X), .action = volc_volume_t::VOLC_VOL_INC})
#define VOLC_DEC(X)       (volc_volume_t {.volume = -(X), .action = volc_volume_t::VOLC_VOL_INC})
#define VOLC_SET(X)       (volc_volume_t {.volume = (X), .action = volc_volume_t::VOLC_VOL_SET})
#define VOLC_SAME         (volc_volume_t {.volume = 0, .action = volc_volume_t::VOLC_VOL_SAME})

#define VOLC_GET_VOLUME VOLC_ALL_CHANNELS, VOLC_SAME, VOLC_CHAN_SAME

/*#define VOLC_VERBOSE*/

enum channel_switch_t {
    VOLC_CHAN_OFF = 0,
    VOLC_CHAN_ON,
    VOLC_CHAN_TOGGLE,
    VOLC_CHAN_SAME,

};


using snd_mixer_t = struct _snd_mixer;
using snd_mixer_elem_t = struct _snd_mixer_elem;
using snd_mixer_selem_id_t = struct _snd_mixer_selem_id;

struct volc_t {
    snd_mixer_t *handle;
    snd_mixer_elem_t *elem;
    snd_mixer_selem_id_t *sid;
    char const *card;
};

struct volc_volume_t {
    float volume;

    enum {
        VOLC_VOL_INC,
        VOLC_VOL_SET,
        VOLC_VOL_SAME,
    } action;
};

union volc_volume_state_t {
    long err;

    struct {
        channel_switch_t switch_pos;
        float volume;
    } state;
};

extern volc_t *volc_init(char const *selector, unsigned int selector_index, char const *card);
extern void volc_deinit(volc_t *volc);
extern volc_volume_state_t volc_volume_ctl(
    volc_t *volc, unsigned int channels, volc_volume_t new_volume, channel_switch_t channel_switch);

#endif  // DWM_VOLC_HPP
