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

enum struct Click {
    TagBar,
    LtSymbol,
    StatusText,
    WinTitle,
    ClientWin,
    RootWin,
    Last,
};

struct Button {
    Click click;
    unsigned int mask;
    unsigned int button;
    MappingCallback func;
    Arg arg;
};


#ifdef ASOUND
static constexpr float VOL_DN = -1;
static constexpr float VOL_MT = 0;
static constexpr float VOL_UP = 1;
#endif  // ASOUND

void bright_dec(double arg);
void bright_inc(double arg);
[[maybe_unused]]
void bright_set(double arg);
void dmenu_run();
void focusMon(int arg);
void focusMonAbs(unsigned arg);
void focusStack(int arg);
void iconify();
void incNmaster(int arg);
void killClient();
void movemouse();
void quit();
void resetMcfact();
void resizeMouse();
void restart();
void rotateStack(int arg);
void setCfact(float arg);
void setLayout(Layout const *arg);
void setMaster(int arg);
void setMfact(float arg);
void spawn(char const *const *arg);
void tag(unsigned arg);
void tagMon(int arg);
void toggleBar();
void toggleFloating();
void toggleFs();
void toggleTag(unsigned arg);
void toggleView(unsigned arg);
void view(unsigned arg);
void shiftView(int dir);
void winPicker();
void zoom();

#ifdef ASOUND
void volumeChange(float arg);
#endif  // ASOUND

#endif  // DWM_MAPPING_HPP
