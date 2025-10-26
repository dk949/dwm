#ifndef DWM_CONFIG_HPP
#define DWM_CONFIG_HPP

#include "colors.hpp"
#include "layout.hpp"
#include "mapping.hpp"

#include <stdlib.h>
#include <X11/keysym.h>

#include <array>


/* appearance */

// These will be initialised when screen size is known
static unsigned int borderpx; /* border pixel of windows */
static unsigned int gappx;    /* gaps between windows */
static unsigned int snap;     /* snap pixel */

#define FONT_SIZE      "10"
#define NERD_FONT_SIZE "12"

static bool const showbar = true; /* 0 means no bar */
static int const topbar = 1;      /* 0 means bottom bar */
static char const *fonts[] = {
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

#define ttype(type) (1 << Tag##type)

/* tagging */
static constexpr auto tags = [] {
    std::array<char const *, 9> out;
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



static Rule const rules[] = {
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
    /* class                     , inst      , title        , tags          , switch , isfloating , isterminal , noswallow , monitor */
    {"firefox"                   , nullptr      , nullptr         , ttype(Browse) , 3      , 0          , 0          , 0         , -1},
    {"Google-chrome"             , nullptr      , nullptr         , ttype(Browse) , 3      , 0          , 1          , 1         , -1},
    {"jetbrains-clion"           , nullptr      , nullptr         , ttype(Code)   , 1      , 0          , 0          , 0         , -1},
    {"jetbrains-webstorm"        , nullptr      , nullptr         , ttype(Code)   , 1      , 0          , 0          , 0         , -1},
    {"jetbrains-idea"            , nullptr      , nullptr         , ttype(Code)   , 1      , 0          , 0          , 0         , -1},
    {"jetbrains-pycharm"         , nullptr      , nullptr         , ttype(Code)   , 1      , 0          , 0          , 0         , -1},
    {"jetbrains-studio"          , nullptr      , nullptr         , ttype(Code)   , 1      , 0          , 0          , 0         , -1},
    {"Steam"                     , nullptr      , nullptr         , ttype(Ent)    , 3      , 1          , 0          , 0         , -1},
    {"Spotify"                   , nullptr      , nullptr         , ttype(Ent)    , 1      , 0          , 0          , 0         , -1},
    {"st-256color"               , nullptr      , "spotify"       , ttype(Ent)    , 3      , 0          , 0          , 1         , -1},
    {"st-256color"               , nullptr      , "sysmon"        , ttype(Sys)    , 3      , 0          , 0          , 1         , -1},
    {"VirtualBox Machine"        , nullptr      , nullptr         , ttype(Sys)    , 1      , 0          , 0          , 0         , -1},
    {"qemu-system-i386"          , nullptr      , nullptr         , ttype(Sys)    , 0      , 1          , 1          , 1         , -1},
    {"Gimp"                      , nullptr      , nullptr         , ttype(Creat)  , 3      , 0          , 0          , 0         , -1},
    {"Blender"                   , nullptr      , nullptr         , ttype(Creat)  , 3      , 0          , 0          , 0         , -1},
    {"Darktable"                 , nullptr      , nullptr         , ttype(Creat)  , 1      , 0          , 0          , 0         , -1},
    {"MuseScore3"                , nullptr      , nullptr         , ttype(Creat)  , 1      , 0          , 0          , 0         , -1},
    {"discord"                   , nullptr      , nullptr         , ttype(Chat)   , 1      , 0          , 0          , 0         , -1},
    {"Slack"                     , nullptr      , nullptr         , ttype(Chat)   , 1      , 0          , 0          , 0         , -1},
    {"Mattermost"                , nullptr      , nullptr         , ttype(Chat)   , 1      , 0          , 0          , 0         , -1},
    {"Microsoft Teams - Preview" , nullptr      , nullptr         , ttype(Chat)   , 1      , 0          , 0          , 0         , -1},
    {"thunderbird"               , nullptr      , "Msgcompose"    , ttype(Chat)   , 1      , 0          , 1          , 1         , -1},
    {"thunderbird"               , nullptr      , nullptr         , ttype(Chat)   , 1      , 0          , 0          , 0         , -1},
    {"Zulip"                     , nullptr      , nullptr         , ttype(Chat)   , 1      , 0          , 0          , 0         , -1},
    {"Signal"                    , nullptr      , nullptr         , ttype(Chat)   , 1      , 0          , 0          , 0         , -1},
    {"zoom"                      , nullptr      , nullptr         , ttype(Chat)   , 1      , 0          , 0          , 0         , -1},
    {"testing"                   , nullptr      , nullptr         , 0             , 0      , 1          , 1          , 1         , -1},
    {"Xephyr"                    , nullptr      , nullptr         , 0             , 0      , 1          , 1          , 1         , -1},
    {"st-256color"               , nullptr      , "neovim"        , 0             , 0      , 1          , 0          , 1         , -1},
    {"st-256color"               , nullptr      , nullptr         , 0             , 0      , 0          , 1          , 1         , -1},
    {"kitty"                     , nullptr      , nullptr         , 0             , 0      , 0          , 1          , 1         , -1},
    // clang-format on
};


/* layout(s) */
static float const mfact = 0.5;   /* factor of master area size [0.05..0.95] */
static int const nmaster = 1;     /* number of clients in master area */
static int const resizehints = 1; /* 1 means respect size hints in tiled resizals */

static Layout const layouts[] = {
    /* symbol      arrange function */
    {"[]=",                   tile}, /* first entry is default */
    {"><>",                nullptr}, /* no layout function means floating behavior */
    {"[M]",                monocle},
    {"|M|",         centeredmaster},
    {">M>", centeredfloatingmaster},
};

/* key definitions */
#define MODKEY Mod4Mask
// clang-format off
#define TAGKEYS(KEY, TAG)                                                                                 \
    {MODKEY, KEY, view, {.ui = 1 << (TAG)}},                                                              \
    {MODKEY | ControlMask, KEY, toggleview, {.ui = 1 << (TAG)}},                                          \
    {MODKEY | ShiftMask, KEY, tag, {.ui = 1 << (TAG)}},                                                   \
    {MODKEY | ControlMask | ShiftMask, KEY, toggletag, { .ui = 1 << (TAG) }}
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


static Key const keys[] = {

    // clang-format off
    /* modifier                         key         function       argument */
    // Utility spawners
    {MODKEY                           , XK_r      , spawn          , {.v = dmenucmd}}    ,
#ifdef ASOUND
    {MODKEY                           , XK_F1     , volumechange   , {.i = VOL_MT  }}    ,
    {MODKEY                           , XK_F2     , volumechange   , {.i = VOL_DN*5}}    ,
    {MODKEY                           , XK_F3     , volumechange   , {.i = VOL_UP*5}}    ,
#endif // ASOUND
    {MODKEY                           , XK_Return , spawn          , {.v = termcmd}}     ,
    {MODKEY                           , XK_Next   , spawn          , {.v = lockcmd}}     ,
    {MODKEY | ShiftMask | ControlMask , XK_Next   , spawn          , {.v = powrcmd}}     ,
    {MODKEY | Mod1Mask                , XK_s      , spawn          , {.v = symdmnu}}     ,
    {MODKEY | Mod1Mask                , XK_g      , spawn          , {.v = grkdmnu}}     ,
    {MODKEY | Mod1Mask                , XK_i      , spawn          , {.v = scrdmnu}}     ,

    // Application spawn
    {MODKEY | ControlMask             , XK_b      , spawn          , {.v = brwscmd}}     ,
    {MODKEY | ControlMask             , XK_m      , spawn          , {.v = musccmd}}     ,
    {MODKEY | ControlMask             , XK_d      , spawn          , {.v = chatcmd}}     ,
    {MODKEY | ControlMask             , XK_n      , spawn          , {.v = nvimcmd}}     ,
    {MODKEY | ControlMask             , XK_Escape , spawn          , {.v = htopcmd}}     ,


    // dwm control
    {MODKEY                           , XK_b      , togglebar      , {0}}                ,
    {MODKEY | ShiftMask               , XK_j      , rotatestack    , {.i = +1}}          ,
    {MODKEY | ShiftMask               , XK_k      , rotatestack    , {.i = -1}}          ,
    {MODKEY                           , XK_j      , focusstack     , {.i = +1}}          ,
    {MODKEY                           , XK_k      , focusstack     , {.i = -1}}          ,
    // {MODKEY                           , XK_z      , iconify        , {0}}                ,
    {MODKEY                           , XK_i      , incnmaster     , {.i = +1}}          ,
    {MODKEY                           , XK_d      , incnmaster     , {.i = -1}}          ,
    {MODKEY                           , XK_h      , setmfact       , {.f = -0.02f}}       ,
    {MODKEY                           , XK_l      , setmfact       , {.f = +0.02f}}       ,
    {MODKEY | ShiftMask               , XK_h      , setcfact       , {.f = +0.25f}}       ,
    {MODKEY | ShiftMask               , XK_l      , setcfact       , {.f = -0.25f}}       ,
    {MODKEY | ShiftMask               , XK_o      , resetmcfact    , {0}}                ,
    {MODKEY | ShiftMask               , XK_Return , zoom           , {0}}                ,
    {MODKEY                           , XK_Tab    , view           , {0}}                ,
    {MODKEY                           , XK_w      , killclient     , {0}}                ,
    {MODKEY                           , XK_F5     , bright_dec     , {.f = 5.0f}}         ,
    {MODKEY                           , XK_F6     , bright_inc     , {.f = 5.0f}}         ,
    {MODKEY                           , XK_F11    , togglefs       , {0}}                ,
    {MODKEY                           , XK_t      , setlayout      , {.v = &layouts[0]}} ,
    {MODKEY                           , XK_f      , setlayout      , {.v = &layouts[1]}} ,
    {MODKEY                           , XK_m      , setlayout      , {.v = &layouts[2]}} ,
    {MODKEY                           , XK_u      , setlayout      , {.v = &layouts[3]}} ,
    {MODKEY                           , XK_o      , setlayout      , {.v = &layouts[4]}} ,
    {MODKEY                           , XK_space  , setlayout      , {0}}                ,
    {MODKEY | ShiftMask               , XK_space  , togglefloating , {0}}                ,
    {MODKEY                           , XK_0      , view           , {.ui = ~0u}}         ,
    {MODKEY | ShiftMask               , XK_0      , tag            , {.ui = ~0u}}         ,
    {MODKEY                           , XK_comma  , focusmon       , {.i = -1}}          ,
    {MODKEY                           , XK_period , focusmon       , {.i = +1}}          ,
    {MODKEY | ShiftMask               , XK_comma  , tagmon         , {.i = -1}}          ,
    {MODKEY | ShiftMask               , XK_period , tagmon         , {.i = +1}}          ,
    {MODKEY | ShiftMask               , XK_q      , quit           , {0}}                ,
    {MODKEY | ShiftMask               , XK_r      , restart        , {0}}                ,
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
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button const buttons[] = {
    // clang-format off
    /* click       , event mask , button  , function       , argument            , */
    {ClkLtSymbol   , 0          , Button1 , setlayout      , {0}}                ,
    {ClkLtSymbol   , 0          , Button3 , setlayout      , {.v = &layouts[2]}} ,
    {ClkWinTitle   , 0          , Button2 , zoom           , {0}}                ,
    {ClkClientWin  , MODKEY     , Button1 , movemouse      , {0}}                ,
    {ClkClientWin  , MODKEY     , Button2 , togglefloating , {0}}                ,
    {ClkClientWin  , MODKEY     , Button3 , resizemouse    , {0}}                ,
    {ClkTagBar     , 0          , Button1 , view           , {0}}                ,
    {ClkTagBar     , 0          , Button3 , toggleview     , {0}}                ,
    {ClkTagBar     , MODKEY     , Button1 , tag            , {0}}                ,
    {ClkTagBar     , MODKEY     , Button3 , toggletag      , {0}}                ,
    // clang-format on
};

static char const *get_bright_set_file(void) {
    char const *filename = getenv("DWM_BACKLIGHT_SET_FILE");
    if (filename) return filename;
    return "/sys/class/backlight/amdgpu_bl1/brightness";
}

static char const *get_bright_get_file(void) {
    char const *filename = getenv("DWM_BACKLIGHT_GET_FILE");
    if (filename) return filename;
    return "/sys/class/backlight/amdgpu_bl1/actual_brightness";
}

static char const *get_bright_max_file(void) {
    char const *filename = getenv("DWM_BACKLIGHT_MAX_FILE");
    if (filename) return filename;
    return "/sys/class/backlight/amdgpu_bl1/max_brightness";
}

#undef ttype

#endif  // DWM_CONFIG_HPP
