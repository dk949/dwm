#ifndef DWM_MAPPING_HPP
#define DWM_MAPPING_HPP

#include "layout.hpp"

#include <X11/X.h>

/// Arg functions for key and mouse bindings
struct Rule {
    char const *class_;
    char const *instance;
    char const *title;
    unsigned int tags;
    unsigned int switchtotag;
    bool isfloating;
    bool isterminal;
    bool noswallow;
    int monitor;
};

/// Union to pass generic data to key/mouse binding functions
union Arg {
    int i;
    unsigned int ui;
    float f;
    double d;
    Layout const *l;
    char const *const *cpp;
    void const *v;
};

/// Key or mouse combination callback
using MappingCallback = void (*)(Arg const &);

/// Key combination
struct Key {
    unsigned int mod;
    KeySym keysym;
    MappingCallback func;
    Arg arg;
};

struct Button {
    unsigned int click;
    unsigned int mask;
    unsigned int button;
    MappingCallback func;
    Arg arg;
};

enum {
    ClkTagBar,
    ClkLtSymbol,
    ClkStatusText,
    ClkWinTitle,
    ClkClientWin,
    ClkRootWin,
    ClkLast,
}; /* clicks */

#ifdef ASOUND
enum { VOL_DN = -1, VOL_MT = 0, VOL_UP = 1 };
#endif  // ASOUND

// .d
void bright_dec(Arg const &arg);
// .d
void bright_inc(Arg const &arg);
// .d
[[maybe_unused]]
void bright_set(Arg const &arg);
// .i
void focusmon(Arg const &arg);
// .ui
void focusmonabs(Arg const &arg);
// .i
void focusstack(Arg const &arg);
// void
void iconify(Arg const &);
// .i
void incnmaster(Arg const &arg);
// void
void killclient(Arg const &arg);
// void
void movemouse(Arg const &arg);
// void
void quit(Arg const &arg);
// void
void resetmcfact(Arg const &unused);
// void
void resizemouse(Arg const &arg);
// void
void restart(Arg const &arg);
// .i
void rotatestack(Arg const &arg);
// .f
void setcfact(Arg const &arg);
// .l
void setlayout(Arg const &arg);
// .i
void setmaster(Arg const &arg);
// .f
void setmfact(Arg const &arg);
// .cpp
void spawn(Arg const &arg);
// .ui
void tag(Arg const &arg);
// .i
void tagmon(Arg const &arg);
// void
void togglebar(Arg const &arg);
// void
void togglefloating(Arg const &arg);
// void
void togglefs(Arg const &arg);
// .ui
void toggletag(Arg const &arg);
// .ui
void toggleview(Arg const &arg);
// .ui
void view(Arg const &arg);
// void
void winpicker(Arg const &arg);
// void
void zoom(Arg const &arg);

#ifdef ASOUND
// .i
void volumechange(Arg const &arg);

#endif  // ASOUND

#endif  // DWM_MAPPING_HPP
