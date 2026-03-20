#ifndef DWM_XBACKLIGHT_HPP
#define DWM_XBACKLIGHT_HPP

enum struct BacklightError {
    Internal = -1,
    Ok = 0,
    Open = 1,
    Read = 2,
    Write = 3,
    Xrandr = 4,
    Property = 6,
    Format = 7,
};

BacklightError brightSetup(char const *bright_file, char const *actual_brightness, char const *scale_file);

// Backlight values go from 0 to 100

// Increment brightness of the backlight
BacklightError brightInc_(double value);

// Decrement brightness of the backlight
BacklightError brightDec_(double value);

// Set brightness of the backlight to a certain number
BacklightError brightSet_(double value);

BacklightError brightGet_(double *value);

#endif  // DWM_XBACKLIGHT_HPP
