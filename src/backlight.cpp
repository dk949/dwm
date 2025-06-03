#include "backlight.hpp"

#include <stdio.h>
#include <string.h>

#include <algorithm>

backlight_error_t bright_set_impl(FILE *fp, int value) {
    if (fprintf(fp, "%d\n", std::clamp(value, 0, 255)) < 0)
        return BACKLIGHT_WRITE_ERROR;
    else
        return BACKLIGHT_OK;
}

backlight_error_t bright_get_impl(FILE *fp, int *oldValue) {
    char buf[5];
    memset(buf, 0, sizeof(buf));
    if (!oldValue) return BACKLIGHT_INTERNAL_ERROR;

    unsigned long bytesRead = fread(buf, 1, sizeof(buf), fp);
    if (bytesRead <= 0) return BACKLIGHT_READ_ERROR;
    *oldValue = -1;
    switch (bytesRead) {
        case 2: *oldValue = buf[0] - '0'; break;
        case 3: *oldValue = (buf[0] - '0') * 10 + (buf[1] - '0'); break;
        case 4: *oldValue = (buf[0] - '0') * 100 + (buf[1] - '0') * 10 + (buf[2] - '0'); break;
        default: return BACKLIGHT_INTERNAL_ERROR;
    }
    if (*oldValue == -1) return BACKLIGHT_INTERNAL_ERROR;
    return BACKLIGHT_OK;
}

static char const *filename;

backlight_error_t bright_setup(char const *file, int _1, int _2) {
    (void)_1, (void)_2;
    filename = file;
    return BACKLIGHT_OK;
}

enum { UP = 1, DOWN = -1 };

backlight_error_t bright_modify(double value, int dir) {

    FILE *fp = fopen(filename, "r+");
    if (!fp) return BACKLIGHT_OPEN_ERROR;
    int oldValue;
    backlight_error_t ret = bright_get_impl(fp, &oldValue);
    if (ret) {
        fclose(fp);
        return ret;
    }

    double iValue = ((value * dir) / 100.) * 255. + oldValue;
    ret = bright_set_impl(fp, (int)iValue);
    fclose(fp);
    return ret;
}

backlight_error_t bright_inc_(double value) {
    return bright_modify(value, UP);
}

backlight_error_t bright_dec_(double value) {
    return bright_modify(value, DOWN);
}

backlight_error_t bright_set_(double value) {
    FILE *fp = fopen(filename, "w");
    backlight_error_t ret = bright_set_impl(fp, (int)((value / 100.) * 255));
    fclose(fp);
    return ret;
}

backlight_error_t bright_get_(double *value) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return BACKLIGHT_OPEN_ERROR;
    int oldValue;
    backlight_error_t ret = bright_get_impl(fp, &oldValue);
    *value = (((double)oldValue) / 255.) * 100;
    fclose(fp);
    return ret;
}
