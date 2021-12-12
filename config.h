/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int borderpx  = 4;  /* border pixel of windows */
static const unsigned int gappx     = 12; /* gaps between windows */
static const unsigned int snap      = 32; /* snap pixel */
static const int showbar            = 1;  /* 0 means no bar */
static const int topbar             = 1;  /* 0 means bottom bar */
static const int bright_time        = 60;       /* time in useconds to go from one screen brightness value to the next*/
static const int bright_steps       = 20;       /* number of steps it takes to move between brightness values */
static const char *fonts[]          = { "Hack:size=10" };
static const char dmenufont[]       = "Hack:size=10";


/* colors */

static const char c_active[]    = "#F8F8F2";
static const char c_inactive[]  = "#101421";
static const char c_black[]     = "#000000";
static const char c_red[]       = "#FF5555";
static const char c_green[]     = "#50FA7B";
static const char c_yellow[]    = "#F1FA8C";
static const char c_blue[]      = "#BD93F9";
static const char c_magenta[]   = "#FF79C6";
static const char c_cyan[]      = "#8BE9FD";
static const char c_white[]     = "#BFBFBF";
static const char c_blank[]     = "#000000";

/* Color assignment */
static const char *colors[][3] = {
    /*                      fg          bg              border   */
    [SchemeNorm]        = { c_active,   c_inactive,     c_inactive  },
    [SchemeSel]         = { c_inactive, c_active,       c_active    },
    [SchemeStatus]      = { c_active,   c_inactive,     c_blank     },  // Statusbar right
    [SchemeTagsSel]     = { c_inactive, c_active,       c_blank     },  // Tagbar left selected
    [SchemeTagsNorm]    = { c_active,   c_inactive,     c_blank     },  // Tagbar left unselected
    [SchemeInfoSel]     = { c_inactive, c_blue,         c_blank     },  // infobar middle selected
    [SchemeInfoNorm]    = { c_blue,     c_inactive,     c_blank     },  // infobar middle unselected
};


/* tagging                  |1      |2     |3     |4     |5     |6     |7      |8       |*/
static const char *tags[] = { "TERM", "WWW", "DEV", "ENT", "SYS", "GFX", "CHAT", "TERM" };

static const Rule rules[] = {
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
    /* class                             |inst |title     |tags   |switchtotag|isfloating|isterminal|noswallow|monitor */
    { "Gimp"                             , NULL, NULL     , 1 << 5,          3,         0,         0,        0,    -1},
    { "Steam"                            , NULL, NULL     , 1 << 3,          3,         1,         0,        0,    -1},
    { "firefox"                          , NULL, NULL     , 1 << 1,          3,         0,         0,        0,    -1},
    { "discord"                          , NULL, NULL     , 1 << 6,          1,         0,         0,        0,    -1},
    { "Zulip"                            , NULL, NULL     , 1 << 6,          1,         0,         0,        0,    -1},
    { "zoom"                             , NULL, NULL     , 1 << 7,          1,         0,         0,        0,    -1},
    { "VirtualBox Machine"               , NULL, NULL     , 1 << 4,          1,         0,         0,        0,    -1},
    { "jetbrains-clion"                  , NULL, NULL     , 1 << 2,          1,         0,         0,        0,    -1},
    { "jetbrains-webstorm"               , NULL, NULL     , 1 << 2,          1,         0,         0,        0,    -1},
    { "jetbrains-idea"                   , NULL, NULL     , 1 << 2,          1,         0,         0,        0,    -1},
    { "jetbrains-pycharm"                , NULL, NULL     , 1 << 2,          1,         0,         0,        0,    -1},
    { "jetbrains-studio"                 , NULL, NULL     , 1 << 2,          1,         0,         0,        0,    -1},
    { "ptolemy-vergil-VergilApplication" , NULL, NULL     , 0     ,          0,         0,         1,        1,    -1},
    { "testing"                          , NULL, NULL     , 0     ,          0,         1,         1,        1,    -1},
    { "Alacritty"                        , NULL, "spotify", 1 << 3,          3,         0,         0,        1,    -1},
    { "Alacritty"                        , NULL, "sysmon" , 1 << 4,          3,         0,         0,        1,    -1},
    { "Alacritty"                        , NULL, NULL     , 0     ,          0,         0,         1,        1,    -1},
};

/* layout(s) */
static const float mfact        = 0.5;  /* factor of master area size [0.05..0.95] */
static const int   nmaster      = 1;    /* number of clients in master area */
static const int   resizehints  = 1;    /* 1 means respect size hints in tiled resizals */

static const Layout layouts[] = {
    /* symbol   arrange function */
    { "[]=",    tile                    }, /* first entry is default */
    { "><>",    NULL                    }, /* no layout function means floating behavior */
    { "[M]",    monocle                 },
    { "|M|",    centeredmaster          },
    { ">M>",    centeredfloatingmaster  },
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY, TAG)                                                         \
        { MODKEY,                       KEY,    view,       { .ui = 1 << TAG } }, \
        { MODKEY|ControlMask,           KEY,    toggleview, { .ui = 1 << TAG } }, \
        { MODKEY|ShiftMask,             KEY,    tag,        { .ui = 1 << TAG } }, \
        { MODKEY|ControlMask|ShiftMask, KEY,    toggletag,  { .ui = 1 << TAG } }

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd)                                           \
    {                                                        \
        .v = (const char *[]) { "/bin/sh", "-c", cmd, NULL } \
    }

/* commands */
static       char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-l", "20", "-c",
                                  "-bw", "3",   "-x", "-o", "0.8",   NULL };

static const char *termcmd[] = { "alacritty",           NULL };
static const char *lockcmd[] = { "slock",               NULL }; // Lock the screen with slock
static const char *powrcmd[] = { "turnoff",             NULL }; // Lock the screen with slock
static const char *brwscmd[] = { "firefox",             NULL }; // Firefox browser
static const char *musccmd[] = { "spotify",             NULL }; // spotify-tui
static const char *htopcmd[] = { "sysmon",              NULL }; // system monitor aka htop
static const char *nvimcmd[] = { "neovim",              NULL }; // opens neovim
static const char *chatcmd[] = { "disc",                NULL }; // does discord

static const char *compcmd[] = { "picom-start",         NULL }; // volume mute

static const char *symdmnu[] = { "sym",                 NULL }; // Mathematical symbol selection
static const char *grkdmnu[] = { "greek",                 NULL }; // Mathematical symbol selection
static const char *scrdmnu[] = { "screenshot",          NULL }; // Screenshot taker

static const char *comkill[] = { "picom-end",           NULL };

static Key keys[] = {
    /* modifier                     key        function        argument */

    // Utility spawners
    { MODKEY,                       XK_r,       spawn,          { .v = dmenucmd     } },
    { MODKEY,                       XK_F12,     bright_inc,     { .f = 5.0         } },
    { MODKEY,                       XK_F11,     bright_dec,     { .f = 5.0         } },
    { MODKEY,                       XK_F1,      volumechange,   { .i = VOL_MT      } },
    { MODKEY,                       XK_F2,      volumechange,   { .i = VOL_DN      } },
    { MODKEY,                       XK_F3,      volumechange,   { .i = VOL_UP      } },
    { MODKEY,                       XK_m,       spawn,          { .v = comkill      } },
    { MODKEY,                       XK_t,       spawn,          { .v = compcmd      } },
    { MODKEY,                       XK_Return,  spawn,          { .v = termcmd      } },
    { MODKEY,                       XK_Next,    spawn,          { .v = lockcmd      } },
    { MODKEY|ShiftMask|ControlMask, XK_Next,    spawn,          { .v = powrcmd      } },

    // Application spawn
    { MODKEY|ControlMask,           XK_b,       spawn,          { .v = brwscmd      } },
    { MODKEY|ControlMask,           XK_m,       spawn,          { .v = musccmd      } },
    { MODKEY|ControlMask,           XK_d,       spawn,          { .v = chatcmd      } },
    { MODKEY|ControlMask,           XK_n,       spawn,          { .v = nvimcmd      } },
    { MODKEY|ControlMask,           XK_Escape,  spawn,          { .v = htopcmd      } },


    { MODKEY,                       XK_b,       togglebar,      { 0                 } },
    { MODKEY|ShiftMask,             XK_j,       rotatestack,    { .i = +1           } },
    { MODKEY|ShiftMask,             XK_k,       rotatestack,    { .i = -1           } },
    { MODKEY,                       XK_j,       focusstack,     { .i = +1           } },
    { MODKEY,                       XK_k,       focusstack,     { .i = -1           } },
    { MODKEY,                       XK_i,       incnmaster,     { .i = +1           } },
    { MODKEY,                       XK_d,       incnmaster,     { .i = -1           } },

    { MODKEY,                       XK_h,       setmfact,       { .f = -0.05        } },
    { MODKEY,                       XK_l,       setmfact,       { .f = +0.05        } },

    // Dmenu stuff
    { MODKEY|Mod1Mask,              XK_s,       spawn,          { .v = symdmnu      } },
    { MODKEY|Mod1Mask,              XK_g,       spawn,          { .v = grkdmnu      } },
    { MODKEY|Mod1Mask,              XK_i,       spawn,          { .v = scrdmnu      } },

    // Still doesn't work
  //{ MODKEY,                       XK_o,       setmfact,       {.f =  0.00         } },

    { MODKEY|ShiftMask,             XK_h,       setcfact,       { .f = +0.25        } },
    { MODKEY|ShiftMask,             XK_l,       setcfact,       { .f = -0.25        } },
    { MODKEY|ShiftMask,             XK_o,       setcfact,       { .f = 0.00         } },

    { MODKEY|ShiftMask,             XK_Return,  zoom,           { 0                 } },
    { MODKEY,                       XK_Tab,     view,           { 0                 } },
    { MODKEY,                       XK_w,       killclient,     { 0                 } },

    { MODKEY,                       XK_t,       setlayout,      { .v = &layouts[0]  } },
    { MODKEY,                       XK_f,       setlayout,      { .v = &layouts[1]  } },
    { MODKEY,                       XK_m,       setlayout,      { .v = &layouts[2]  } },
    { MODKEY,                       XK_u,       setlayout,      { .v = &layouts[3]  } },
    { MODKEY,                       XK_o,       setlayout,      { .v = &layouts[4]  } },
    { MODKEY,                       XK_space,   setlayout,      { 0                 } },

    { MODKEY|ShiftMask,             XK_space,   togglefloating, { 0                 } },
    { MODKEY,                       XK_0,       view,           { .ui = ~0          } },
    { MODKEY|ShiftMask,             XK_0,       tag,            { .ui = ~0          } },
    { MODKEY,                       XK_comma,   focusmon,       { .i = -1           } },
    { MODKEY,                       XK_period,  focusmon,       { .i = +1           } },
    { MODKEY|ShiftMask,             XK_comma,   tagmon,         { .i = -1           } },
    { MODKEY|ShiftMask,             XK_period,  tagmon,         { .i = +1           } },
    { MODKEY|ShiftMask,             XK_q,       quit,           { 0                 } },

      TAGKEYS(                      XK_1,                         0                   ),
      TAGKEYS(                      XK_2,                         1                   ),
      TAGKEYS(                      XK_3,                         2                   ),
      TAGKEYS(                      XK_4,                         3                   ),
      TAGKEYS(                      XK_5,                         4                   ),
      TAGKEYS(                      XK_6,                         5                   ),
      TAGKEYS(                      XK_7,                         6                   ),
      TAGKEYS(                      XK_8,                         7                   ),
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
    /* click                event mask      button          function        argument */
    { ClkLtSymbol,          0,              Button1,        setlayout,      { 0                 } },
    { ClkLtSymbol,          0,              Button3,        setlayout,      { .v = &layouts[2]  } },
    { ClkWinTitle,          0,              Button2,        zoom,           { 0                 } },
    { ClkStatusText,        0,              Button2,        spawn,          { .v = termcmd      } },
    { ClkClientWin,         MODKEY,         Button1,        movemouse,      { 0                 } },
    { ClkClientWin,         MODKEY,         Button2,        togglefloating, { 0                 } },
    { ClkClientWin,         MODKEY,         Button3,        resizemouse,    { 0                 } },
    { ClkTagBar,            0,              Button1,        view,           { 0                 } },
    { ClkTagBar,            0,              Button3,        toggleview,     { 0                 } },
    { ClkTagBar,            MODKEY,         Button1,        tag,            { 0                 } },
    { ClkTagBar,            MODKEY,         Button3,        toggletag,      { 0                 } },
};
