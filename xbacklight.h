//
// Created by dk949 on 01/01/2021.
//

#ifndef XBACKLIGHT_XBACKLIGHT_H
#define XBACKLIGHT_XBACKLIGHT_H

// dpy_name is the display to be used, If set to NULL, will use DISPLAY env variable
int bright_setup(char *dpy_name, int step_conf, int time_conf);

// Backlight values go from 0 to 100

// Increment brightness of the backlight
int bright_inc_(double value);

// Decrement brightness of the backlight
int bright_dec_(double value);

// Set brightness of the backlight to a certain number
int bright_set_(double value);

int bright_get_(double *value);

#endif  // XBACKLIGHT_XBACKLIGHT_H
