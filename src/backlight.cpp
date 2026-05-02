#include "backlight.hpp"

#include "file.hpp"
#include "log.hpp"
#include "strerror.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <limits>

static char const *set_brightness;
static char const *get_brightness;
static double scale = nan("");

static BacklightError nan_check() {
    if (std::isnan(scale)) {
        lg::error("backlight: Brightness scale was not setup correctly");
        return BacklightError::Internal;
    }
    return BacklightError::Ok;
}

static BacklightError bright_set_impl(FilePtr fp, int value) {
    if (auto res = nan_check(); res != BacklightError::Ok) return res;
    if (fprintf(fp.get(), "%d\n", std::clamp(value, 0, static_cast<int>(scale))) < 0) {
        lg::error("backlight: Could not write to brightness file: {}", strError(errno));
        return BacklightError::Write;
    } else
        return BacklightError::Ok;
}

static BacklightError bright_get_impl(FilePtr fp, int *oldValue) {
    std::array<char, std::numeric_limits<int>::digits10 + 1> buf {};
    if (!oldValue) {
        lg::error("backlight: Internal error: `oldValue` is null");
        return BacklightError::Internal;
    }

    unsigned long bytesRead = fread(buf.data(), 1, sizeof(buf), fp.get());
    if (bytesRead <= 0) {
        lg::error("backlight: Could not read from brightness file: {}", strError(errno));
        return BacklightError::Read;
    }
    if (bytesRead >= buf.size()) {
        lg::error("backlight: Too many bytes read from brightness file: {}", bytesRead);
        return BacklightError::Format;
    }
    buf[bytesRead] = 0;
    auto *end = buf.begin() + bytesRead;
    auto [ptr, ec] = std::from_chars(buf.begin(), end, *oldValue);
    if (ec != std::errc {}) {
        lg::error("backlight: Could not parse '{}' as an integer: {}", buf, std::make_error_code(ec).message());
        return BacklightError::Format;
    }
    if (ptr != end && *ptr != '\n') {
        lg::error("backlight: Could not parse '{}' as an integer", buf);
        return BacklightError::Format;
    }

    return BacklightError::Ok;
}

static BacklightError read_scale_file(char const *scale_file) {
    auto fp = FilePtr {fopen(scale_file, "r")};
    if (!fp) {
        lg::error("backlight: Could not open scale file {}: {}", scale_file, strError(errno));
        return BacklightError::Open;
    }
    int max = -1;
    errno = 0;
    if (fscanf(fp.get(), "%d", &max) == EOF || max < 0) {
        if (errno)
            lg::error("backlight: Could not read from scale file: {}", strError(errno));
        else
            lg::error("backlight: Could not parse scale file");
        return BacklightError::Format;
    }
    scale = max;
    return BacklightError::Ok;
}

BacklightError brightSetup(char const *bright_file, char const *actual_brightness, char const *scale_file) {
    set_brightness = bright_file;
    get_brightness = actual_brightness;
    if (auto res = read_scale_file(scale_file); res != BacklightError::Ok) return res;
    return BacklightError::Ok;
}

enum { UP = 1, DOWN = -1 };

static BacklightError bright_modify(double value, int dir) {
    if (auto res = nan_check(); res != BacklightError::Ok) return res;
    int oldValue = 0;
    {
        auto get = FilePtr {fopen(get_brightness, "r")};
        if (!get) {
            lg::error("backlight: Could not open brightness file {} for  modification: {}",
                get_brightness,
                strError(errno));
            return BacklightError::Open;
        }
        BacklightError ret = bright_get_impl(std::move(get), &oldValue);
        if (ret != BacklightError::Ok) return ret;
    }
    {

        auto set = FilePtr {fopen(set_brightness, "w")};
        if (!set) {
            lg::error("backlight: Could not open brightness file {} for  modification: {}",
                set_brightness,
                strError(errno));
            return BacklightError::Open;
        }
        double iValue = (value * dir / 100. * scale) + oldValue;
        BacklightError ret = bright_set_impl(std::move(set), static_cast<int>(iValue));
        return ret;
    }
}

BacklightError brightInc(double value) {
    return bright_modify(value, UP);
}

BacklightError brightDec(double value) {
    return bright_modify(value, DOWN);
}

BacklightError brightSet(double value) {
    if (auto res = nan_check(); res != BacklightError::Ok) return res;
    auto fp = FilePtr {fopen(set_brightness, "w")};
    if (!fp) {
        lg::error("backlight: Could not open brightness file for writing: {}", strError(errno));
        return BacklightError::Open;
    }
    BacklightError ret = bright_set_impl(std::move(fp), static_cast<int>((value / 100.) * scale));
    return ret;
}

BacklightError brightGet(double *value) {
    if (auto res = nan_check(); res != BacklightError::Ok) return res;
    auto fp = FilePtr {fopen(get_brightness, "r")};
    if (!fp) {
        lg::error("backlight: Could not open brightness file {} for reading: {}", get_brightness, strError(errno));
        return BacklightError::Open;
    }
    int oldValue = 0;
    BacklightError ret = bright_get_impl(std::move(fp), &oldValue);
    *value = ((static_cast<double>(oldValue)) / scale) * 100.;
    return ret;
}
