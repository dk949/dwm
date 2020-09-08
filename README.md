dwm - dynamic window manager
============================
This is my dwm build. Original project at [suckless.org]( https://dwm.suckless.org/)


Requirements
------------
In order to build dwm you will need:
* the Xlib header files.
This build also requires:
* [dmenu](https://tools.suckless.org/dmenu/)
    * \([my build](https://github.com/dk949/dmenu) also exists but it's not that different from stock\)
* [slock](https://tools.suckless.org/slock/)
    * [my slock](https://github.com/dk949/slock)
* [dwm-scripts](https://github.com/dk949/dwm-scripts)
* [picom](https://github.com/yshui/picom) compositor


Installation
------------
Edit config.mk to match your local setup (dwm is installed into
the /usr/local namespace by default).

Afterwards enter the following command to build and install dwm (if
necessary as root):

    make clean install


Running dwm
-----------
Add the following line to your .xinitrc to start dwm using startx:

    exec dwm

In order to connect dwm to a specific display, make sure that
the DISPLAY environment variable is set correctly, e.g.:

    DISPLAY=foo.bar:1 exec dwm

(This will start dwm on display :1 of the host foo.bar.)

In order to display status info in the bar, call `statusbar &` from [dwm-scripts](https://github.com/dk949/dwm-scripts) in your .xinitrc or .xprofile, either will work.



Configuration
-------------
The configuration of dwm is done editing  config.h
and (re)compiling the source code.
Note: Most [patches](https://dwm.suckless.org/patches/) won't work, since the config.h has been patched several times already \(all diff files which have been applied are in this repository\)
