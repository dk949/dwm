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

* [dmenu](https://tools.suckless.org/dmenu/)
    * [my build of dmenu](https://github.com/dk949/dmenu)
* [slock](https://tools.suckless.org/slock/)
    * [my build of slock](https://github.com/dk949/slock)
* [slstatus](https://tools.suckless.org/slstatus/)
    * [my build of slstatus](https://github.com/dk949/slstatus)
* [dwm-scripts](https://github.com/dk949/dwm-scripts)
* [volume-brightness-control](https://github.com/dk949/volume-brightness-controll)
* [picom](https://github.com/yshui/picom) compositor
* cmake (for building and installation)


Installation
------------
Edit CMakeLists.txt to match your local setup (dwm is installed into
the /usr/local namespace by default).

Afterwards enter the following commands to build and install dwm:

    mkdir build
    cd build
    cmake ..
    sudo make install


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
The configuration of dwm is done editing  config.h
and (re)compiling the source code.  
Note: Most [patches](https://dwm.suckless.org/patches/) won't work, since the config.h has been patched several times already \(all diff files which have been applied are in the diffs directory\)
