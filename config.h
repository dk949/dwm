/* See LICENSE file for copyright and license details. */
#include "layout.h"
#include "mapping.h"

#include <stdlib.h>
#include <X11/keysymdef.h>

/* appearance */

// These will be initialised when screen size is known
static unsigned int borderpx; /* border pixel of windows */
static unsigned int gappx;    /* gaps between windows */
static unsigned int snap;     /* snap pixel */

#define FONT_SIZE      "10"
#define NERD_FONT_SIZE "12"

static int const showbar = 1; /* 0 means no bar */
static int const topbar = 1;  /* 0 means bottom bar */
static char const *fonts[] = {
    "JetBrains Mono:size=" FONT_SIZE ":antialias=true:autohint=true",
    "JetBrainsMono Nerd Font:size=" NERD_FONT_SIZE ":antialias=true:autohint=true",
    "Noto Emoji:size=" FONT_SIZE ":antialias=true:autohint=true",
};
static char const dmenufont[] = "JetBrains Mono:size=" FONT_SIZE ":antialias=true:autohint=true";

static int const bright_time = 60;  /* time in useconds to go from one screen brightness value to the next*/
static int const bright_steps = 20; /* number of steps it takes to move between brightness values */

#ifndef XBACKLIGHT
/* Either provide the name of the file here, or set this to NULL and provide it
   in the get_bright_file function. If bright_file is not NULL get_bright_file
   may return NULL, but one of them has to be a valid pointer.
*/
static char const *bright_file = NULL;
#endif  // XBACKLIGHT


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
static char const *colors[][3] = {
  // clang-format off
    /*                      fg          bg              border   */
    [SchemeNorm]           = { c_active   , c_inactive , c_inactive },
    [SchemeSel]            = { c_inactive , c_active   , c_active   },
    [SchemeStatus]         = { c_active   , c_inactive , c_blank    }, // Statusbar right
    [SchemeTagsSel]        = { c_inactive , c_active   , c_blank    }, // Tagbar left selected
    [SchemeTagsNorm]       = { c_active   , c_inactive , c_blank    }, // Tagbar left unselected
    [SchemeInfoSel]        = { c_inactive , c_blue     , c_blank    }, // infobar selected
    [SchemeInfoNorm]       = { c_blue     , c_inactive , c_blank    }, // infobar unselected
    [SchemeInfoProgress]   = { c_green    , c_inactive , c_blank    }, // infobar middle progress
    [SchemeOffProgress]    = { c_red      , c_inactive , c_blank    }, // infobar middle progress
    [SchemeBrightProgress] = { c_yellow   , c_inactive , c_blank    }, // infobar middle progress
  // clang-format on
};

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
static char const *tags[] = {
    [TagTerm1] = "   ",
    [TagBrowse] = "  ",
    [TagCode] = " 󰅩 ",
    [TagEnt] = "  ",
    [TagSys] = "  ",
    [TagCreat] = "  ",
    [TagChat] = " 󰙯 ",
    [TagTerm2] = "   ",
    [TagTerm3] = "   ",
};



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
    {"firefox"                   , NULL      , NULL         , ttype(Browse) , 3      , 0          , 0          , 0         , -1},
    {"Google-chrome"             , NULL      , NULL         , ttype(Browse) , 3      , 0          , 1          , 1         , -1},
    {"jetbrains-clion"           , NULL      , NULL         , ttype(Code)   , 1      , 0          , 0          , 0         , -1},
    {"jetbrains-webstorm"        , NULL      , NULL         , ttype(Code)   , 1      , 0          , 0          , 0         , -1},
    {"jetbrains-idea"            , NULL      , NULL         , ttype(Code)   , 1      , 0          , 0          , 0         , -1},
    {"jetbrains-pycharm"         , NULL      , NULL         , ttype(Code)   , 1      , 0          , 0          , 0         , -1},
    {"jetbrains-studio"          , NULL      , NULL         , ttype(Code)   , 1      , 0          , 0          , 0         , -1},
    {"Steam"                     , NULL      , NULL         , ttype(Ent)    , 3      , 1          , 0          , 0         , -1},
    {"Spotify"                   , NULL      , NULL         , ttype(Ent)    , 1      , 0          , 0          , 0         , -1},
    {"st-256color"               , NULL      , "spotify"    , ttype(Ent)    , 3      , 0          , 0          , 1         , -1},
    {"st-256color"               , NULL      , "sysmon"     , ttype(Sys)    , 3      , 0          , 0          , 1         , -1},
    {"VirtualBox Machine"        , NULL      , NULL         , ttype(Sys)    , 1      , 0          , 0          , 0         , -1},
    {"qemu-system-i386"          , NULL      , NULL         , ttype(Sys)    , 0      , 1          , 1          , 1         , -1},
    {"Gimp"                      , NULL      , NULL         , ttype(Creat)  , 3      , 0          , 0          , 0         , -1},
    {"Darktable"                 , NULL      , NULL         , ttype(Creat)  , 3      , 0          , 0          , 0         , -1},
    {"MuseScore3"                , NULL      , NULL         , ttype(Creat)  , 1      , 0          , 0          , 0         , -1},
    {"discord"                   , NULL      , NULL         , ttype(Chat)   , 1      , 0          , 0          , 0         , -1},
    {"Slack"                     , NULL      , NULL         , ttype(Chat)   , 1      , 0          , 0          , 0         , -1},
    {"Mattermost"                , NULL      , NULL         , ttype(Chat)   , 1      , 0          , 0          , 0         , -1},
    {"Microsoft Teams - Preview" , NULL      , NULL         , ttype(Chat)   , 1      , 0          , 0          , 0         , -1},
    {"thunderbird"               , NULL      , "Msgcompose" , ttype(Chat)   , 1      , 0          , 1          , 1         , -1},
    {"thunderbird"               , NULL      , NULL         , ttype(Chat)   , 1      , 0          , 0          , 0         , -1},
    {"Zulip"                     , NULL      , NULL         , ttype(Chat)   , 1      , 0          , 0          , 0         , -1},
    {"zoom"                      , NULL      , NULL         , ttype(Chat)   , 1      , 0          , 0          , 0         , -1},
    {"testing"                   , NULL      , NULL         , 0             , 0      , 1          , 1          , 1         , -1},
    {"Xephyr"                    , NULL      , NULL         , 0             , 0      , 1          , 1          , 1         , -1},
    {"st-256color"               , NULL      , "neovim"     , 0             , 0      , 1          , 0          , 1         , -1},
    {"st-256color"               , NULL      , NULL         , 0             , 0      , 0          , 1          , 1         , -1},
  // clang-format on
};


/* layout(s) */
static float const mfact = 0.5;   /* factor of master area size [0.05..0.95] */
static int const nmaster = 1;     /* number of clients in master area */
static int const resizehints = 1; /* 1 means respect size hints in tiled resizals */

static Layout const layouts[] = {
  /* symbol      arrange function */
    {"[]=",                   tile}, /* first entry is default */
    {"><>",                   NULL}, /* no layout function means floating behavior */
    {"[M]",                monocle},
    {"|M|",         centeredmaster},
    {">M>", centeredfloatingmaster},
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY, TAG)                                                                                 \
    {MODKEY, KEY, view, {.ui = 1 << (TAG)}}, {MODKEY | ControlMask, KEY, toggleview, {.ui = 1 << (TAG)}}, \
        {MODKEY | ShiftMask, KEY, tag, {.ui = 1 << (TAG)}}, {                                             \
        MODKEY | ControlMask | ShiftMask, KEY, toggletag, {                                               \
            .ui = 1 << (TAG)                                                                              \
        }                                                                                                 \
    }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static char const *dmenucmd[] = {
    "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-l", "20", "-c", "-bw", "3", "-x", "-o", "0.8", NULL};

static char const *termcmd[] = {"st", NULL};
static char const *termclass = "st-256color";

static char const *lockcmd[] = {"slock", NULL};    // Lock the screen with slock
static char const *powrcmd[] = {"turnoff", NULL};  // Lock the screen with slock
static char const *brwscmd[] = {"firefox", NULL};  // Firefox browser
static char const *musccmd[] = {"spotify", NULL};  // spotify-tui
static char const *htopcmd[] = {"sysmon", NULL};   // system monitor aka htop
static char const *nvimcmd[] = {"neovim", NULL};   // opens neovim
static char const *chatcmd[] = {"disc", NULL};     // does discord
// static char const *mttrmst[] = {"mattermost-desktop", NULL};  // mattermost
// static char const *zulpcmd[] = {"zulip", NULL};               // zulip


static char const *symdmnu[] = {"sym", NULL};         // Mathematical symbol selection
static char const *grkdmnu[] = {"greek", NULL};       // Mathematical symbol selection
static char const *scrdmnu[] = {"screenshot", NULL};  // Screenshot taker


static Key keys[] = {

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
    {MODKEY                           , XK_h      , setmfact       , {.f = -0.02}}       ,
    {MODKEY                           , XK_l      , setmfact       , {.f = +0.02}}       ,
    {MODKEY | ShiftMask               , XK_h      , setcfact       , {.f = +0.25}}       ,
    {MODKEY | ShiftMask               , XK_l      , setcfact       , {.f = -0.25}}       ,
    {MODKEY | ShiftMask               , XK_o      , resetmcfact    , {0}}                ,
    {MODKEY | ShiftMask               , XK_Return , zoom           , {0}}                ,
    {MODKEY                           , XK_Tab    , view           , {0}}                ,
    {MODKEY                           , XK_w      , killclient     , {0}}                ,
    {MODKEY                           , XK_F5     , bright_dec     , {.f = 5.0}}         ,
    {MODKEY                           , XK_F6     , bright_inc     , {.f = 5.0}}         ,
    {MODKEY                           , XK_F11    , togglefs       , {0}}                ,
    {MODKEY                           , XK_t      , setlayout      , {.v = &layouts[0]}} ,
    {MODKEY                           , XK_f      , setlayout      , {.v = &layouts[1]}} ,
    {MODKEY                           , XK_m      , setlayout      , {.v = &layouts[2]}} ,
    {MODKEY                           , XK_u      , setlayout      , {.v = &layouts[3]}} ,
    {MODKEY                           , XK_o      , setlayout      , {.v = &layouts[4]}} ,
    {MODKEY                           , XK_space  , setlayout      , {0}}                ,
    {MODKEY | ShiftMask               , XK_space  , togglefloating , {0}}                ,
    {MODKEY                           , XK_0      , view           , {.ui = ~0}}         ,
    {MODKEY | ShiftMask               , XK_0      , tag            , {.ui = ~0}}         ,
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
static Button buttons[] = {
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

static char const *get_bright_file(void) {
    char const *filename = getenv("DWM_BACKLIGHT_FILE");
    if (filename) return filename;
    return "/sys/class/backlight/amdgpu_bl0/brightness";
}

#undef ttype
