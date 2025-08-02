#include "backlight.hpp"

#include "file.hpp"
#include "log.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static char const *set_brightness;
static char const *get_brightness;
static double scale = nan("");

static backlight_error_t nan_check() {
    if (std::isnan(scale)) {
        lg::error("backlight: Brightness scale was not setup correctly");
        return BACKLIGHT_INTERNAL_ERROR;
    }
    return BACKLIGHT_OK;
}

static backlight_error_t bright_set_impl(FilePtr fp, int value) {
    if (auto res = nan_check()) return res;
    if (fprintf(fp.get(), "%d\n", std::clamp(value, 0, static_cast<int>(scale))) < 0) {
        lg::error("backlight: Could not write to brightness file: {}", std::strerror(errno));
        return BACKLIGHT_WRITE_ERROR;
    } else
        return BACKLIGHT_OK;
}

static backlight_error_t bright_get_impl(FilePtr fp, int *oldValue) {
    char buf[std::numeric_limits<int>::digits10 + 1];
    if (!oldValue) {
        lg::error("backlight: Internal error: `oldValue` is null");
        return BACKLIGHT_INTERNAL_ERROR;
    }

    unsigned long bytesRead = fread(buf, 1, sizeof(buf), fp.get());
    if (bytesRead <= 0) {
        lg::error("backlight: Could not read from brightness file: {}", std::strerror(errno));
        return BACKLIGHT_READ_ERROR;
    }
    if (bytesRead >= sizeof(buf)) {
        lg::error("backlight: Too many bytes read from brightness file: {}", bytesRead);
        return BACKLIGHT_FORMAT_ERROR;
    }
    buf[bytesRead] = 0;
    auto end = buf + bytesRead;
    auto [ptr, ec] = std::from_chars(buf, end, *oldValue);
    if (ec != std::errc {}) {
        lg::error("backlight: Could not parse '{}' as an integer: {}", buf, std::make_error_code(ec).message());
        return BACKLIGHT_FORMAT_ERROR;
    }
    if (ptr != end && *ptr != '\n') {
        lg::error("backlight: Could not parse '{}' as an integer", buf);
        return BACKLIGHT_FORMAT_ERROR;
    }

    return BACKLIGHT_OK;
}

static backlight_error_t read_scale_file(char const *scale_file) {
    auto fp = FilePtr {fopen(scale_file, "r")};
    if (!fp) {
        lg::error("backlight: Could not open scale file {}: {}", scale_file, std::strerror(errno));
        return BACKLIGHT_OPEN_ERROR;
    }
    int max;
    errno = 0;
    if (fscanf(fp.get(), "%d", &max) == EOF) {
        if (errno)
            lg::error("backlight: Could not read from scale file: {}", std::strerror(errno));
        else
            lg::error("backlight: Could not parse scale file");
        return BACKLIGHT_FORMAT_ERROR;
    }
    scale = max;
    return BACKLIGHT_OK;
}

backlight_error_t bright_setup(char const *bright_file, char const *actual_brightness, char const *scale_file) {
    set_brightness = bright_file;
    get_brightness = actual_brightness;
    if (auto res = read_scale_file(scale_file)) return res;
    return BACKLIGHT_OK;
}

enum { UP = 1, DOWN = -1 };

static backlight_error_t bright_modify(double value, int dir) {
    if (auto res = nan_check()) return res;
    int oldValue;
    {
        auto get = FilePtr {fopen(get_brightness, "r")};
        if (!get) {
            lg::error("backlight: Could not open brightness file {} for  modification: {}",
                get_brightness,
                std::strerror(errno));
            return BACKLIGHT_OPEN_ERROR;
        }
        backlight_error_t ret = bright_get_impl(std::move(get), &oldValue);
        if (ret) return ret;
    }
    {

        auto set = FilePtr {fopen(set_brightness, "w")};
        if (!set) {
            lg::error("backlight: Could not open brightness file {} for  modification: {}",
                set_brightness,
                std::strerror(errno));
            return BACKLIGHT_OPEN_ERROR;
        }
        double iValue = ((value * dir) / 100.) * scale + oldValue;
        backlight_error_t ret = bright_set_impl(std::move(set), (int)iValue);
        return ret;
    }
}

backlight_error_t bright_inc_(double value) {
    return bright_modify(value, UP);
}

backlight_error_t bright_dec_(double value) {
    return bright_modify(value, DOWN);
}

backlight_error_t bright_set_(double value) {
    if (auto res = nan_check()) return res;
    auto fp = FilePtr {fopen(set_brightness, "w")};
    if (!fp) {
        lg::error("backlight: Could not open brightness file for writing: {}", std::strerror(errno));
        return BACKLIGHT_OPEN_ERROR;
    }
    backlight_error_t ret = bright_set_impl(std::move(fp), (int)((value / 100.) * scale));
    return ret;
}

backlight_error_t bright_get_(double *value) {
    if (auto res = nan_check()) return res;
    auto fp = FilePtr {fopen(get_brightness, "r")};
    if (!fp) {
        lg::error("backlight: Could not open brightness file {} for reading: {}", get_brightness, std::strerror(errno));
        return BACKLIGHT_OPEN_ERROR;
    }
    int oldValue;
    backlight_error_t ret = bright_get_impl(std::move(fp), &oldValue);
    *value = (((double)oldValue) / scale) * 100;
    return ret;
}
