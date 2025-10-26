# dwm - dynamic window manager

This is a fork. Original project at [suckless.org]( https://dwm.suckless.org/)

## Requirements
* Libraries
    * X11
    * XCB
    * freetype2
    * (optionally) Xinerama
    * (optionally) asound
* cmake
* (optionally) ninja
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
* [kitty](https://st.suckless.org) terminal
    * set with the `termcmd` variable
* [firefox](https://www.mozilla.org/en-US/firefox/new/) browser
    * set with the `brwscmd` variable
* [spotify](https://open.spotify.com/) music player
    * set with the `musccmd` variable
* [dwm-scripts](https://github.com/dk949/dwm-scripts) various helper scripts
    * required for `powrcmd`, `htopcmd`, `nvimcmd` and `chatcmd`
* [dmenu-scripts](https://github.com/dk949/dmenu-scripts) more helper scripts
    * required for `symdmnu`, `grkdmnu` and `scrdmnu`

## Installation

```sh
git clone "https://github.com/Microsoft/vcpkg.git"
./vcpkg/bootstrap-vcpkg.sh -disableMetrics
./vcpkg/vcpkg install
# see CMakePresets.json for other presets
cmake --preset make-release -DCMAKE_INSTALL_PREFIX=/path/to/install/to
cmake --build build-release
cmake --install build-release
```

> [!NOTE]
> If `CMAKE_INSTALL_PREFIX` is not specified, the default system install
> directory (most likely `/usr/local`) will be used.

> [!WARNING]
> *DO NOT* use `--prefix` when running `--install`, the executable needs to have
> the absolute install path at compile time.

<!-- TODO(dk949): ^^ Fix this  -->

## Running dwm

Add the following line to your .xinitrc to start dwm using startx:

    exec dwm

In order to connect dwm to a specific display, make sure that
the DISPLAY environment variable is set correctly, e.g.:

    DISPLAY=foo.bar:1 exec dwm

(This will start dwm on display :1 of the host foo.bar.)


## Configuration

The configuration is done by editing `src/config.hpp` and (re)compiling the source code.
> [!WARNING]
> [Patches](https://dwm.suckless.org/patches/) (most likely) won't work, since the
> this version has been rewritten in c++.


## Debugging

dwm can be built with additional debugging functionality. This build will
produce additional debug logs and will use address sanitiser to check for memory
errors.

```sh
cmake --preset make # see CMakePresets.json for other debug presets
cmake --build build
```
