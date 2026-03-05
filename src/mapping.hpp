#ifndef DWM_MAPPING_HPP
#define DWM_MAPPING_HPP

#include "layout.hpp"
#include "variant_utils.hpp"

#include <X11/X.h>

#include <variant>

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

/// Variant to pass generic data to key/mouse binding functions
using Arg = std::variant<  //
    std::monostate,
    int,
    unsigned int,
    float,
    double,
    Layout const *,
    char const *const *,
    void const *>;
static_assert(sizeof(Arg) == sizeof(void *) * 2);

/// Key or mouse combination callback
using MappingCallback = fn_ptr_variant_t<Arg>;

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

void bright_dec(double arg);
void bright_inc(double arg);
[[maybe_unused]]
void bright_set(double arg);
void dmenu_run();
void focusmon(int arg);
void focusmonabs(unsigned arg);
void focusstack(int arg);
void iconify();
void incnmaster(int arg);
void killclient();
void movemouse();
void quit();
void resetmcfact();
void resizemouse();
void restart();
void rotatestack(int arg);
void setcfact(float arg);
void setlayout(Layout const *arg);
void setmaster(int arg);
void setmfact(float arg);
void spawn(char const *const *arg);
void tag(unsigned arg);
void tagmon(int arg);
void togglebar();
void togglefloating();
void togglefs();
void toggletag(unsigned arg);
void toggleview(unsigned arg);
void view(unsigned arg);
void winpicker();
void zoom();

#ifdef ASOUND
// .i
void volumechange(int arg);

#endif  // ASOUND

#endif  // DWM_MAPPING_HPP
