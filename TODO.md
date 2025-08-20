 ```
 _____ ___  ____   ___
|_   _/ _ \|  _ \ / _ \
  | || | | | | | | | | |
  | || |_| | |_| | |_| |
  |_| \___/|____/ \___/
```

List of things I'd like to implement but don't have time for

* [ ] Get status color working
  * Have color appear in the status bar. Will also require a slight rewrite of
    slstatus
* [X] Look at what changed between 6.2 and 6.5
* [ ] Some cool new patches (more for inspiration than anything else):
  * http://dwm.suckless.org/patches/windowmap/
  * http://dwm.suckless.org/patches/statuscolors/
* [ ] In monocle mode, only draw the top window
* [ ] Use a UTF-8 library instead of hand-rolling
* [ ] Look at `configurenotify`
* [ ] Fix vertical screen cutting off part of status
  * This will likely need something like doing a double high bar 🤷
* [ ] Check what happens with too many clients on different layouts
* [ ] Make sure `centeredmaster` is `cfact` aware.
* [X] Check if `util` functions are still needed
* [X] Replace all `NULL` with `nullptr`
* [X] Remove `XBACKLIGHT`
* [X] Add notifications
* [ ] Fix exceptions from libnoticeboard
* [ ] Handle Fireforx Picture in picture (instance: "Toolkit", class: "firefox",
  title: "Picture-in-Picture")
* [ ] Make `Client`s traversable by standard algorithms
    * Not sure if it'd be sensible to use `std::vector<std::shared_ptr<Client>>`
    * Maybe just give it a `begin` and `end` and call it a day
* [ ] Fix conversion warnings
* [ ] Fix (or silence) the `fontconfig` memory leak
* [ ] Fix incorrect tiling (rotation) behaviour with windows created by the same
      application.
* [X] Better abstraction for doing `Xinerama`
    * Avoid `#ifdef`s everywhere
* [ ] Fix zoom:
  * Emoji picker window: title: "Zoom Workplace", instance: "zoom", class: "zoom"
  * Notification window: title: '', instance: "zoom", class: "zoom",
