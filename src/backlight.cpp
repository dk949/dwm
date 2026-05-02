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

static BacklightError nanCheck() {
    if (std::isnan(scale)) {
        lg::error("backlight: Brightness scale was not setup correctly");
        return BacklightError::Internal;
    }
    return BacklightError::Ok;
}

static BacklightError brightSetImpl(FilePtr fp, int value) {
    if (auto res = nanCheck(); res != BacklightError::Ok) return res;
    if (fprintf(fp.get(), "%d\n", std::clamp(value, 0, static_cast<int>(scale))) < 0) {
        lg::error("backlight: Could not write to brightness file: {}", strError(errno));
        return BacklightError::Write;
    } else
        return BacklightError::Ok;
}

static BacklightError brightGetImpl(FilePtr fp, int *oldValue) {
    std::array<char, std::numeric_limits<int>::digits10 + 1> buf {};
    if (!oldValue) {
        lg::error("backlight: Internal error: `oldValue` is null");
        return BacklightError::Internal;
    }

    unsigned long bytes_read = fread(buf.data(), 1, sizeof(buf), fp.get());
    if (bytes_read <= 0) {
        lg::error("backlight: Could not read from brightness file: {}", strError(errno));
        return BacklightError::Read;
    }
    if (bytes_read >= buf.size()) {
        lg::error("backlight: Too many bytes read from brightness file: {}", bytes_read);
        return BacklightError::Format;
    }
    buf[bytes_read] = 0;
    auto *end = buf.begin() + bytes_read;
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

static BacklightError readScaleFile(char const *scale_file) {
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
    if (auto res = readScaleFile(scale_file); res != BacklightError::Ok) return res;
    return BacklightError::Ok;
}

enum { UP = 1, DOWN = -1 };

static BacklightError brightModify(double value, int dir) {
    if (auto res = nanCheck(); res != BacklightError::Ok) return res;
    int old_value = 0;
    {
        auto get = FilePtr {fopen(get_brightness, "r")};
        if (!get) {
            lg::error("backlight: Could not open brightness file {} for  modification: {}",
                get_brightness,
                strError(errno));
            return BacklightError::Open;
        }
        BacklightError ret = brightGetImpl(std::move(get), &old_value);
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
        double i_value = (value * dir / 100. * scale) + old_value;
        BacklightError ret = brightSetImpl(std::move(set), static_cast<int>(i_value));
        return ret;
    }
}

BacklightError brightInc(double value) {
    return brightModify(value, UP);
}

BacklightError brightDec(double value) {
    return brightModify(value, DOWN);
}

BacklightError brightSet(double value) {
    if (auto res = nanCheck(); res != BacklightError::Ok) return res;
    auto fp = FilePtr {fopen(set_brightness, "w")};
    if (!fp) {
        lg::error("backlight: Could not open brightness file for writing: {}", strError(errno));
        return BacklightError::Open;
    }
    BacklightError ret = brightSetImpl(std::move(fp), static_cast<int>((value / 100.) * scale));
    return ret;
}

BacklightError brightGet(double *value) {
    if (auto res = nanCheck(); res != BacklightError::Ok) return res;
    auto fp = FilePtr {fopen(get_brightness, "r")};
    if (!fp) {
        lg::error("backlight: Could not open brightness file {} for reading: {}", get_brightness, strError(errno));
        return BacklightError::Open;
    }
    int old_value = 0;
    BacklightError ret = brightGetImpl(std::move(fp), &old_value);
    *value = ((static_cast<double>(old_value)) / scale) * 100.;
    return ret;
}
