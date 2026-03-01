#ifndef DWM_CONFIG_HPP
#define DWM_CONFIG_HPP

#include "colors.hpp"
#include "layout.hpp"
#include "mapping.hpp"

#include <stdlib.h>
#include <X11/keysym.h>

#include <array>


/* appearance */

#define FONT_SIZE      "10"
#define NERD_FONT_SIZE "12"

static bool const showbar = true; /* 0 means no bar */
static int const topbar = 1;      /* 0 means bottom bar */
// TODO(dk949): Make this a std::array
inline constexpr auto fonts = std::array {
    "JetBrains Mono:size=" FONT_SIZE ":antialias=true:autohint=true",
    "JetBrainsMono Nerd Font:size=" NERD_FONT_SIZE ":antialias=true:autohint=true",
    "Noto Emoji:size=" FONT_SIZE ":antialias=true:autohint=true",
};
static char const dmenufont[] = "JetBrains Mono:size=" FONT_SIZE ":antialias=true:autohint=true";

static int const bright_time = 60;             /* time in useconds to go from one screen brightness value to the next*/
static int const bright_steps = 20;            /* number of steps it takes to move between brightness values */

static double const progress_fade_time = 1.5;  // How long progress bar will not disapear for (in seconds)


/* colors */

// clang-format off
static const char c_active[]   = "#F8F8F2";
static const char c_inactive[] = "#101421";
static const char c_black[]    = "#000000";
static const char c_red[]      = "#FF5555";
static const char c_green[]    = "#50FA7B";
static const char c_yellow[]   = "#F1FA8C";
static const char c_blue[]     = "#BD93F9";
static const char c_magenta[]  = "#FF79C6";
static const char c_cyan[]     = "#8BE9FD";
static const char c_white[]    = "#BFBFBF";
static const char c_blank[]    = "#000000";
// clang-format on

/* Color assignment */
// clang-format off
constexpr ColorSchemeName colors {
    .norm            = { .fg = c_active,   .bg = c_inactive, .border = c_inactive },
    .sel             = { .fg = c_inactive, .bg = c_active,   .border = c_active   },
    .status          = { .fg = c_active,   .bg = c_inactive, .border = c_blank    },
    .tags_sel        = { .fg = c_inactive, .bg = c_active,   .border = c_blank    },
    .tags_norm       = { .fg = c_active,   .bg = c_inactive, .border = c_blank    },
    .info_sel        = { .fg = c_inactive, .bg = c_blue,     .border = c_blank    },
    .info_norm       = { .fg = c_blue,     .bg = c_inactive, .border = c_blank    },
    .info_progress   = { .fg = c_green,    .bg = c_inactive, .border = c_blank    },
    .off_progress    = { .fg = c_red,      .bg = c_inactive, .border = c_blank    },
    .bright_progress = { .fg = c_yellow,   .bg = c_inactive, .border = c_blank    },
};
// clang-format on

enum TagTypes {
    TagTerm1 = 0,
    TagBrowse = 1,
    TagCode = 2,
    TagEnt = 3,
    TagSys = 4,
    TagCreat = 5,
    TagChat = 6,
    TagTerm2 = 7,
    TagTerm3 = 8,
};

#define ttype(type) (1u << Tag##type)

/* tagging */
static constexpr auto tag_symbols = [] {
    std::array<char const *, 9> out;  // NOLINT cppcoreguidelines-pro-type-member-init
    out[TagTerm1] = "   ";
    out[TagBrowse] = "  ";
    out[TagCode] = " 󰅩 ";
    out[TagEnt] = "  ";
    out[TagSys] = "  ";
    out[TagCreat] = "  ";
    out[TagChat] = " 󰙯 ";
    out[TagTerm2] = "   ";
    out[TagTerm3] = "   ";
    return out;
}();

static auto rules = std::array {
    /* xprop(1):
     *    WM_CLASS(STRING) = instance, class
     *    WM_NAME(STRING) = title
     */
    /*
    Rules for swicthtotag:
    - 0 is default behaviour
    - 1 automatically moves you to the tag of the newly opened application
    - 2 enables the tag of the newly opened application in addition to your existing enabled tags
    - 3 as 1, but closing that window reverts the view back to what it was previously (*)
    - 4 as 2, but closing that window reverts the view back to what it was previously (*)
    */
    // clang-format off
    /*     class                     , inst         , title        , tags          , switch , isfloating , isterminal , noswallow , monitor */
    Rule{"firefox"                   , nullptr      , nullptr      , ttype(Browse) , 3      , false      , false      , false     , -1},
    Rule{"Google-chrome"             , nullptr      , nullptr      , ttype(Browse) , 3      , false      , true       , true      , -1},
    Rule{"jetbrains-clion"           , nullptr      , nullptr      , ttype(Code)   , 1      , false      , false      , false     , -1},
    Rule{"jetbrains-webstorm"        , nullptr      , nullptr      , ttype(Code)   , 1      , false      , false      , false     , -1},
    Rule{"jetbrains-idea"            , nullptr      , nullptr      , ttype(Code)   , 1      , false      , false      , false     , -1},
    Rule{"jetbrains-pycharm"         , nullptr      , nullptr      , ttype(Code)   , 1      , false      , false      , false     , -1},
    Rule{"jetbrains-studio"          , nullptr      , nullptr      , ttype(Code)   , 1      , false      , false      , false     , -1},
    Rule{"Steam"                     , nullptr      , nullptr      , ttype(Ent)    , 3      , true       , false      , false     , -1},
    Rule{"Spotify"                   , nullptr      , nullptr      , ttype(Ent)    , 1      , false      , false      , false     , -1},
    Rule{"st-256color"               , nullptr      , "spotify"    , ttype(Ent)    , 3      , false      , false      , true      , -1},
    Rule{"st-256color"               , nullptr      , "sysmon"     , ttype(Sys)    , 3      , false      , false      , true      , -1},
    Rule{"VirtualBox Machine"        , nullptr      , nullptr      , ttype(Sys)    , 1      , false      , false      , false     , -1},
    Rule{"qemu-system-i386"          , nullptr      , nullptr      , ttype(Sys)    , 0      , true       , true       , true      , -1},
    Rule{"Gimp"                      , nullptr      , nullptr      , ttype(Creat)  , 3      , false      , false      , false     , -1},
    Rule{"Blender"                   , nullptr      , nullptr      , ttype(Creat)  , 3      , false      , false      , false     , -1},
    Rule{"Darktable"                 , nullptr      , nullptr      , ttype(Creat)  , 1      , false      , false      , false     , -1},
    Rule{"MuseScore3"                , nullptr      , nullptr      , ttype(Creat)  , 1      , false      , false      , false     , -1},
    Rule{"discord"                   , nullptr      , nullptr      , ttype(Chat)   , 1      , false      , false      , false     , -1},
    Rule{"Slack"                     , nullptr      , nullptr      , ttype(Chat)   , 1      , false      , false      , false     , -1},
    Rule{"Mattermost"                , nullptr      , nullptr      , ttype(Chat)   , 1      , false      , false      , false     , -1},
    Rule{"Microsoft Teams - Preview" , nullptr      , nullptr      , ttype(Chat)   , 1      , false      , false      , false     , -1},
    Rule{"thunderbird"               , nullptr      , "Msgcompose" , ttype(Chat)   , 1      , false      , true       , true      , -1},
    Rule{"thunderbird"               , nullptr      , nullptr      , ttype(Chat)   , 1      , false      , false      , false     , -1},
    Rule{"Zulip"                     , nullptr      , nullptr      , ttype(Chat)   , 1      , false      , false      , false     , -1},
    Rule{"Signal"                    , nullptr      , nullptr      , ttype(Chat)   , 1      , false      , false      , false     , -1},
    Rule{"zoom"                      , nullptr      , nullptr      , ttype(Chat)   , 1      , false      , false      , false     , -1},
    Rule{"testing"                   , nullptr      , nullptr      , 0             , 0      , true       , true       , true      , -1},
    Rule{"Xephyr"                    , nullptr      , nullptr      , 0             , 0      , true       , true       , true      , -1},
    Rule{"st-256color"               , nullptr      , nullptr      , 0             , 0      , false      , true       , true      , -1},
    Rule{"kitty"                     , nullptr      , nullptr      , 0             , 0      , false      , true       , true      , -1},
    // clang-format on
};


/* layout(s) */
static constexpr float mfact = 0.5;       /* factor of master area size [0.05..0.95] */
static constexpr int nmaster = 1;         /* number of clients in master area */
static constexpr bool resizehints = true; /* 1 means respect size hints in tiled resizals */

static auto const layouts = std::array {
    /*       symbol      arrange function */
    Layout {"[]=",                   tile}, /* first entry is default */
    Layout {"><>",                nullptr}, /* no layout function means floating behavior */
    Layout {"[M]",                monocle},
    Layout {"|M|",         centeredmaster},
    Layout {">M>", centeredfloatingmaster},
};

/* key definitions */
#define MODKEY Mod4Mask
// clang-format off
#define TAGKEYS(KEY, TAG)                                                                                 \
    Key{MODKEY, KEY, view, {.ui = 1 << (TAG)}},                                                              \
    Key{MODKEY | ControlMask, KEY, toggleview, {.ui = 1 << (TAG)}},                                          \
    Key{MODKEY | ShiftMask, KEY, tag, {.ui = 1 << (TAG)}},                                                   \
    Key{MODKEY | ControlMask | ShiftMask, KEY, toggletag, { .ui = 1 << (TAG) }}
// clang-format on

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static char const *dmenucmd[] = {
    "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-l", "20", "-c", "-bw", "3", "-x", "-o", "0.8", nullptr};

static char const *termcmd[] = {"kitty", "-1", nullptr};

static char const *lockcmd[] = {"slock", nullptr};    // Lock the screen with slock
static char const *powrcmd[] = {"turnoff", nullptr};  // Lock the screen with slock
static char const *brwscmd[] = {"firefox", nullptr};  // Firefox browser
static char const *musccmd[] = {"spotify", nullptr};  // spotify-tui
static char const *htopcmd[] = {"sysmon", nullptr};   // system monitor aka htop
static char const *nvimcmd[] = {"neovim", nullptr};   // opens neovim
static char const *chatcmd[] = {"disc", nullptr};     // does discord
// static char const *mttrmst[] = {"mattermost-desktop", nullptr};  // mattermost
// static char const *zulpcmd[] = {"zulip", nullptr};               // zulip


static char const *symdmnu[] = {"sym", nullptr};         // Mathematical symbol selection
static char const *grkdmnu[] = {"greek", nullptr};       // Mathematical symbol selection
static char const *scrdmnu[] = {"screenshot", nullptr};  // Screenshot taker


static auto const keys = std::array {

    // clang-format off
    /*    modifier                         key         function       argument */
    // Utility spawners
    Key{MODKEY                           , XK_r      , spawn          , {.v = dmenucmd}}    ,
#ifdef ASOUND
    Key{MODKEY                           , XK_F1     , volumechange   , {.i = VOL_MT  }}    ,
    Key{MODKEY                           , XK_F2     , volumechange   , {.i = VOL_DN*5}}    ,
    Key{MODKEY                           , XK_F3     , volumechange   , {.i = VOL_UP*5}}    ,
#endif // ASOUND
    Key{MODKEY                           , XK_Return , spawn          , {.v = termcmd}}     ,
    Key{MODKEY                           , XK_Next   , spawn          , {.v = lockcmd}}     ,
    Key{MODKEY | ShiftMask | ControlMask , XK_Next   , spawn          , {.v = powrcmd}}     ,
    Key{MODKEY | Mod1Mask                , XK_s      , spawn          , {.v = symdmnu}}     ,
    Key{MODKEY | Mod1Mask                , XK_g      , spawn          , {.v = grkdmnu}}     ,
    Key{MODKEY | Mod1Mask                , XK_i      , spawn          , {.v = scrdmnu}}     ,
    Key{MODKEY | Mod1Mask                , XK_p      , winpicker      , {0}}     ,

    // Application spawn
    Key{MODKEY | ControlMask             , XK_b      , spawn          , {.v = brwscmd}}     ,
    Key{MODKEY | ControlMask             , XK_m      , spawn          , {.v = musccmd}}     ,
    Key{MODKEY | ControlMask             , XK_d      , spawn          , {.v = chatcmd}}     ,
    Key{MODKEY | ControlMask             , XK_n      , spawn          , {.v = nvimcmd}}     ,
    Key{MODKEY | ControlMask             , XK_Escape , spawn          , {.v = htopcmd}}     ,


    // dwm control
    Key{MODKEY                           , XK_b      , togglebar      , {0}}                ,
    Key{MODKEY | ShiftMask               , XK_j      , rotatestack    , {.i = +1}}          ,
    Key{MODKEY | ShiftMask               , XK_k      , rotatestack    , {.i = -1}}          ,
    Key{MODKEY                           , XK_j      , focusstack     , {.i = +1}}          ,
    Key{MODKEY                           , XK_k      , focusstack     , {.i = -1}}          ,
    // Key{MODKEY                           , XK_z      , iconify        , {0}}                ,
    Key{MODKEY                           , XK_i      , incnmaster     , {.i = +1}}          ,
    Key{MODKEY                           , XK_d      , incnmaster     , {.i = -1}}          ,
    Key{MODKEY                           , XK_h      , setmfact       , {.f = -0.02f}}       ,
    Key{MODKEY                           , XK_l      , setmfact       , {.f = +0.02f}}       ,
    Key{MODKEY | ShiftMask               , XK_h      , setcfact       , {.f = +0.25f}}       ,
    Key{MODKEY | ShiftMask               , XK_l      , setcfact       , {.f = -0.25f}}       ,
    Key{MODKEY | ShiftMask               , XK_o      , resetmcfact    , {0}}                ,
    Key{MODKEY | ShiftMask               , XK_Return , zoom           , {0}}                ,
    Key{MODKEY                           , XK_Tab    , view           , {0}}                ,
    Key{MODKEY                           , XK_w      , killclient     , {0}}                ,
    Key{MODKEY                           , XK_F5     , bright_dec     , {.f = 5.0f}}         ,
    Key{MODKEY                           , XK_F6     , bright_inc     , {.f = 5.0f}}         ,
    Key{MODKEY                           , XK_F11    , togglefs       , {0}}                ,
    Key{MODKEY                           , XK_t      , setlayout      , {.v = &layouts[0]}} ,
    Key{MODKEY                           , XK_f      , setlayout      , {.v = &layouts[1]}} ,
    Key{MODKEY                           , XK_m      , setlayout      , {.v = &layouts[2]}} ,
    Key{MODKEY                           , XK_u      , setlayout      , {.v = &layouts[3]}} ,
    Key{MODKEY                           , XK_o      , setlayout      , {.v = &layouts[4]}} ,
    Key{MODKEY                           , XK_space  , setlayout      , {0}}                ,
    Key{MODKEY | ShiftMask               , XK_space  , togglefloating , {0}}                ,
    Key{MODKEY                           , XK_0      , view           , {.ui = ~0u}}         ,
    Key{MODKEY | ShiftMask               , XK_0      , tag            , {.ui = ~0u}}         ,
    Key{MODKEY                           , XK_comma  , focusmon       , {.i = -1}}          ,
    Key{MODKEY                           , XK_period , focusmon       , {.i = +1}}          ,
    Key{MODKEY | ShiftMask               , XK_comma  , tagmon         , {.i = -1}}          ,
    Key{MODKEY | ShiftMask               , XK_period , tagmon         , {.i = +1}}          ,
    Key{MODKEY | ShiftMask               , XK_q      , quit           , {0}}                ,
    Key{MODKEY | ShiftMask               , XK_r      , restart        , {0}}                ,
    // clang-format on

    TAGKEYS(XK_1, 0),
    TAGKEYS(XK_2, 1),
    TAGKEYS(XK_3, 2),
    TAGKEYS(XK_4, 3),
    TAGKEYS(XK_5, 4),
    TAGKEYS(XK_6, 5),
    TAGKEYS(XK_7, 6),
    TAGKEYS(XK_8, 7),
    TAGKEYS(XK_9, 8),
};

/* button definitions */
static auto const buttons = std::array {
    /* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
    // clang-format off
    /*       click       , event mask , button  , function       , argument            , */
    Button{ClkLtSymbol   , 0          , Button1 , setlayout      , {0}}                ,
    Button{ClkLtSymbol   , 0          , Button3 , setlayout      , {.v = &layouts[2]}} ,
    Button{ClkWinTitle   , 0          , Button2 , zoom           , {0}}                ,
    Button{ClkClientWin  , MODKEY     , Button1 , movemouse      , {0}}                ,
    Button{ClkClientWin  , MODKEY     , Button2 , togglefloating , {0}}                ,
    Button{ClkClientWin  , MODKEY     , Button3 , resizemouse    , {0}}                ,
    Button{ClkTagBar     , 0          , Button1 , view           , {0}}                ,
    Button{ClkTagBar     , 0          , Button3 , toggleview     , {0}}                ,
    Button{ClkTagBar     , MODKEY     , Button1 , tag            , {0}}                ,
    Button{ClkTagBar     , MODKEY     , Button3 , toggletag      , {0}}                ,
    // clang-format on
};

inline char const *get_bright_set_file(void) {
    char const *filename = getenv("DWM_BACKLIGHT_SET_FILE");
    if (filename) return filename;
    return "/sys/class/backlight/amdgpu_bl1/brightness";
}

inline char const *get_bright_get_file(void) {
    char const *filename = getenv("DWM_BACKLIGHT_GET_FILE");
    if (filename) return filename;
    return "/sys/class/backlight/amdgpu_bl1/actual_brightness";
}

inline char const *get_bright_max_file(void) {
    char const *filename = getenv("DWM_BACKLIGHT_MAX_FILE");
    if (filename) return filename;
    return "/sys/class/backlight/amdgpu_bl1/max_brightness";
}

#undef ttype

#endif  // DWM_CONFIG_HPP
