#ifndef DWM_MAPPING_HPP
#define DWM_MAPPING_HPP

#include <X11/X.h>

/// Arg functions for key and mouse bindings
struct Rule {
    char const *class_;
    char const *instance;
    char const *title;
    unsigned int tags;
    unsigned int switchtotag;
    int isfloating;
    int isterminal;
    int noswallow;
    int monitor;
};

/// Union to pass generic data to key/mouse binding functions
union Arg {
    int i;
    unsigned int ui;
    float f;
    void const *v;
};

/// Key or mouse combination callback
using MappingCallback = void (*)(Arg const &);

/// Key combination
struct Key {
    unsigned int mod;
    KeySym keysym;
    MappingCallback func;
    Arg const arg;
};

struct Button {
    unsigned int click;
    unsigned int mask;
    unsigned int button;
    MappingCallback func;
    Arg const arg;
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

void bright_dec(Arg const &arg);
void bright_inc(Arg const &arg);
[[maybe_unused]] void bright_set(Arg const &arg);
void focusmon(Arg const &arg);
void focusstack(Arg const &arg);
void iconify(Arg const &);
void incnmaster(Arg const &arg);
void killclient(Arg const &arg);
void movemouse(Arg const &arg);
void quit(Arg const &arg);
void resetmcfact(Arg const &unused);
void resizemouse(Arg const &arg);
void restart(Arg const &arg);
void rotatestack(Arg const &arg);
void setcfact(Arg const &arg);
void setlayout(Arg const &arg);
void setmaster(Arg const &arg);
void setmfact(Arg const &arg);
void spawn(Arg const &arg);
void tag(Arg const &arg);
void tagmon(Arg const &arg);
void togglebar(Arg const &arg);
void togglefloating(Arg const &arg);
void togglefs(Arg const &arg);
void toggletag(Arg const &arg);
void toggleview(Arg const &arg);
void view(Arg const &arg);
void zoom(Arg const &arg);

#ifdef ASOUND
void volumechange(Arg const &arg);

#endif  // ASOUND

#endif  // DWM_MAPPING_HPP
