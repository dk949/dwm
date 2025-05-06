#ifndef XBACKLIGHT_XBACKLIGHT_HPP
#define XBACKLIGHT_XBACKLIGHT_HPP

typedef enum {
    BACKLIGHT_INTERNAL_ERROR = -1,
    BACKLIGHT_OK = 0,
    BACKLIGHT_OPEN_ERROR = 1,
    BACKLIGHT_READ_ERROR = 2,
    BACKLIGHT_WRITE_ERROR = 3,
    BACKLIGHT_XRANDR_ERROR = 4,
    BACKLIGHT_ATOM_ERROR = 5,
    BACKLIGHT_PROPERTY_ERROR = 6,
} backlight_error_t;

// dpy_name is the display to be used, If set to NULL, will use DISPLAY env variable
// If using alt_backlight dpy_name is the filename of the brightness file
backlight_error_t bright_setup(char const *dpy_name, int step_conf, int time_conf);

// Backlight values go from 0 to 100

// Increment brightness of the backlight
backlight_error_t bright_inc_(double value);

// Decrement brightness of the backlight
backlight_error_t bright_dec_(double value);

// Set brightness of the backlight to a certain number
backlight_error_t bright_set_(double value);

backlight_error_t bright_get_(double *value);

#endif  // XBACKLIGHT_XBACKLIGHT_HPP
