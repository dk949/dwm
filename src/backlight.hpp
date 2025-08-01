#ifndef DWM_XBACKLIGHT_HPP
#define DWM_XBACKLIGHT_HPP

enum backlight_error_t {
    BACKLIGHT_INTERNAL_ERROR = -1,
    BACKLIGHT_OK = 0,
    BACKLIGHT_OPEN_ERROR = 1,
    BACKLIGHT_READ_ERROR = 2,
    BACKLIGHT_WRITE_ERROR = 3,
    BACKLIGHT_XRANDR_ERROR = 4,
    BACKLIGHT_ATOM_ERROR = 5,
    BACKLIGHT_PROPERTY_ERROR = 6,
    BACKLIGHT_FORMAT_ERROR = 7,
};

backlight_error_t bright_setup(char const *bright_file, char const *actual_brightness, char const *scale_file);

// Backlight values go from 0 to 100

// Increment brightness of the backlight
backlight_error_t bright_inc_(double value);

// Decrement brightness of the backlight
backlight_error_t bright_dec_(double value);

// Set brightness of the backlight to a certain number
backlight_error_t bright_set_(double value);

backlight_error_t bright_get_(double *value);

#endif  // DWM_XBACKLIGHT_HPP
