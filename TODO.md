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
  * This happens when the whole thing doesn't fit on the screen
  * This will likely need something like doing a double high bar 🤷
* [ ] Check what happens with too many clients on different layouts
    * There's currently a separate check for it in each layout, but they are not
      consistent.
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
* [ ] Fix (or rewrite 🙁) volc to use correct device
    * Right now, if output is via headphones, volume buttons don't control it
* [ ] volc returns an error if trying to decrease volume below 0
* [ ] Why do we generate so many events when fading the progress bar???
* [ ] Try to fix mousemove and mouseresize
    * Currently they stop normal processing of events by the `loop` to filter
      out events they need
    * They should probably instead replace the event handlers for the events
      they want to filter
* [ ] Swallowing still buggy when all terminal windows share the same PID
    * When a terminal is swallowed, pasting into a different terminal ends up
      pasting into the swallowed terminal.
    * This might be a bug in kitty.
* [ ] Clean up process reaping
