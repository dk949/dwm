#ifndef VOLC_H
#define VOLC_H

#define VOLC_DEF_CARD     "default"
#define VOLC_DEF_SEL      "Master"
#define VOLC_DEF_SEL_IDX  0
#define VOLC_ALL_CHANNELS ~0u
#define VOLC_ALL_DEFULTS  VOLC_DEF_SEL, VOLC_DEF_SEL_IDX, VOLC_DEF_CARD
#define VOLC_INC(X)       ((volc_volume_t) {.volume = (X), .action = VOLC_VOL_INC})
#define VOLC_DEC(X)       ((volc_volume_t) {.volume = -(X), .action = VOLC_VOL_INC})
#define VOLC_SET(X)       ((volc_volume_t) {.volume = (X), .action = VOLC_VOL_SET})
#define VOLC_SAME         ((volc_volume_t) {.volume = 0, .action = VOLC_VOL_SAME})

#define VOLC_GET_VOLUME VOLC_ALL_CHANNELS, VOLC_SAME, VOLC_CHAN_SAME
/*#define VOLC_VERBOSE*/

typedef enum chanel_switch {
    VOLC_CHAN_OFF = 0,
    VOLC_CHAN_ON,
    VOLC_CHAN_TOGGLE,
    VOLC_CHAN_SAME,

} channel_switch_t;


typedef struct _snd_mixer snd_mixer_t;
typedef struct _snd_mixer_elem snd_mixer_elem_t;
typedef struct _snd_mixer_selem_id snd_mixer_selem_id_t;

typedef struct volc {
    snd_mixer_t *handle;
    snd_mixer_elem_t *elem;
    snd_mixer_selem_id_t *sid;
    const char *card;

} volc_t;


typedef struct volc_volume {
    float volume;
    enum {
        VOLC_VOL_INC,
        VOLC_VOL_SET,
        VOLC_VOL_SAME,
    } action;

} volc_volume_t;

typedef union volc_volume_state {
    long err;
    struct {
        channel_switch_t switch_pos;
        float volume;
    } state;
} volc_volume_state_t;


extern volc_t *volc_init(const char *selector, unsigned int selector_index, const char *card);
extern void volc_deinit(volc_t *volc);
extern const char *volc_err_str();
extern volc_volume_state_t
    volc_volume_ctl(volc_t *volc, unsigned int channels, volc_volume_t new_volume, channel_switch_t channel_switch);

#endif  // VOLC_H
