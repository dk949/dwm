dwm - dynamic window manager
============================
This is my dwm build. Original project at [suckless.org]( https://dwm.suckless.org/)

Requirements
------------
* Libraries
    * X11
    * XCB
    * freetype2
    * (optionally) Xinerama
    * (optionally) asound
* make
* [slstatus](https://tools.suckless.org/slstatus/) status bar information
    * [my build of slstatus](https://github.com/dk949/slstatus)

Following tools are used by default, but can be changed in `config.h`

* [dmenu](https://tools.suckless.org/dmenu/) run launcher
    * [my build of dmenu](https://github.com/dk949/dmenu)
    * set with the `dmenucmd` variable
* [slock](https://tools.suckless.org/slock/) screen lock
    * [my build of slock](https://github.com/dk949/slock)
    * set with the `lockcmd` variable
* [picom](https://github.com/yshui/picom) compositor
    * set with the `compcmd` and `comkill` variables
* [st](https://st.suckless.org) terminal
    * [my build of slstatus](https://github.com/dk949/st)
    * set with the `termcmd` variable
* [firefox](https://www.mozilla.org/en-US/firefox/new/) browser
    * set with the `brwscmd` variable
* [spotify](https://open.spotify.com/) music player
    * set with the `musccmd` variable
* [dwm-scripts](https://github.com/dk949/dwm-scripts) various helper scripts
    * required for `powrcmd`, `htopcmd`, `nvimcmd` and `chatcmd`
* [dmenu-scripts](https://github.com/dk949/dmenu-scripts) more helper scripts
    * required for `symdmnu`, `grkdmnu` and `scrdmnu`

Installation
------------
Edit config.mk to match your local setup (dwm is installed into
the /usr/local namespace by default).

Afterwards enter the following commands to build and install dwm:
``` sh
make
make install # possibly as root
```

Running dwm
-----------
Add the following line to your .xinitrc to start dwm using startx:

    exec dwm

In order to connect dwm to a specific display, make sure that
the DISPLAY environment variable is set correctly, e.g.:

    DISPLAY=foo.bar:1 exec dwm

(This will start dwm on display :1 of the host foo.bar.)


Configuration
-------------
The configuration is done editing config.h and (re)compiling the source code.
Note: Most [patches](https://dwm.suckless.org/patches/) won't work, since the
this version is heavily modified (all diff files which have been applied are in
the `diffs` directory)
