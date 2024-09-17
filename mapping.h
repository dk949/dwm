#ifndef MAPPING_H
#define MAPPING_H

#include <X11/X.h>

/// Arg functions for key and mouse bindings
typedef struct Rule {
    char const *class;
    char const *instance;
    char const *title;
    unsigned int tags;
    unsigned int switchtotag;
    int isfloating;
    int isterminal;
    int noswallow;
    int monitor;
} Rule;

/// Union to pass generic data to key/mouse binding functions
typedef union Arg {
    int i;
    unsigned int ui;
    float f;
    void const *v;
} Arg;

/// Key or mouse combination callback
typedef void (*MappingCallback)(Arg const *);

/// Key combination
typedef struct Key {
    unsigned int mod;
    KeySym keysym;
    MappingCallback func;
    Arg const arg;
} Key;

typedef struct Button {
    unsigned int click;
    unsigned int mask;
    unsigned int button;
    MappingCallback func;
    Arg const arg;
} Button;

enum {
    ClkTagBar,
    ClkLtSymbol,
    ClkStatusText,
    ClkWinTitle,
    ClkClientWin,
    ClkRootWin,
    ClkLast,
}; /* clicks */

enum {
    SchemeNorm,
    SchemeSel,
    SchemeStatus,
    SchemeTagsSel,
    SchemeTagsNorm,
    SchemeInfoSel,
    SchemeInfoNorm,
    SchemeInfoProgress,
    SchemeOffProgress,
    SchemeBrightProgress,
}; /* color schemes */

#ifdef ASOUND
enum { VOL_DN = -1, VOL_MT = 0, VOL_UP = 1 };
#endif  // ASOUND

void bright_dec(Arg const *arg);
void bright_inc(Arg const *arg);
void bright_set(Arg const *arg) __attribute__((unused));
void focusmon(Arg const *arg);
void focusstack(Arg const *arg);
void iconify(Arg const *);
void incnmaster(Arg const *arg);
void killclient(Arg const *arg);
void movemouse(Arg const *arg);
void quit(Arg const *arg);
void resetmcfact(Arg const *unused);
void resizemouse(Arg const *arg);
void restart(Arg const *arg);
void rotatestack(Arg const *arg);
void setcfact(Arg const *arg);
void setlayout(Arg const *arg);
void setmaster(Arg const *arg);
void setmfact(Arg const *arg);
void spawn(Arg const *arg);
void tag(Arg const *arg);
void tagmon(Arg const *arg);
void togglebar(Arg const *arg);
void togglefloating(Arg const *arg);
void togglefs(Arg const *arg);
void toggletag(Arg const *arg);
void toggleview(Arg const *arg);
void view(Arg const *arg);
void zoom(Arg const *arg);

#ifdef ASOUND
void volumechange(Arg const *arg);

#endif  // ASOUND

#endif  // MAPPING_H
