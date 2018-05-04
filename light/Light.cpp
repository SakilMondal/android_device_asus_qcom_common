/*
 * Copyright (C) 2018 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author := dev_harsh1998
// msm8916 uses node of green led for its white led.

#define LOG_TAG "LightService.msm8916"

#include <log/log.h>
#include "Light.h"
#include <fstream>

#define LEDS            "/sys/class/leds/"
#define LCD_LED         LEDS "lcd-backlight/"
#define BRIGHTNESS      "brightness"
#define GREEN           LEDS "green/"
#define PWM             "pwm_us"

namespace {
/*
 * Write value to path and close file.
 */
static void set(std::string path, std::string value) {
    std::ofstream file(path);
    /* Only write brightness value if stream is open, alive & well */
    if (file.is_open()) {
        file << value;
    } else {
        /* Fire a warning a bail out */
        ALOGE("failed to write %s to %s", value.c_str(), path.c_str());
        return;
    }
}

static void set(std::string path, int value) {
    set(path, std::to_string(value));
}

static void handlemsm8916Backlight(const LightState& state) {
    uint32_t brightness = state.color & 0xFF;
    set(LCD_LED BRIGHTNESS, brightness);
}
static inline bool isLit(const LightState& state) {
    return state.color & 0x00ffffff;
}

static void handlemsm8916Notification(const LightState& state) {
    uint32_t base_brightness = 0;

    if(isLit(state))
        base_brightness = (state.color & 0xff000000) ? (state.color & 0xff000000) >> 24 : 255;

    /* Turn Led Off */
    set(GREEN BRIGHTNESS, 0);

    if ((state.flashMode == Flash::HARDWARE) || (state.flashMode == Flash::TIMED)) {
    uint32_t onMS  = state.flashOnMs;
    uint32_t offMS = state.flashOffMs;
    uint32_t blinkMS = onMS + offMS;

    /*
    * Kang math from sony
    */
    uint32_t holder = (onMS * 255) / blinkMS;

    /* the low 4 bits are ignored, so round up if necessary */
    if (holder > 0 && holder < 16)
        holder = 16;

    uint32_t holderMain = offMS * 1000;

    /* Pulsate the leds according to logic */
    set(GREEN BRIGHTNESS, holder);
    set(GREEN PWM, holderMain);
    }
    else {


    set(GREEN BRIGHTNESS, base_brightness);
    set(GREEN PWM, 100);
    }

}

/* Keep sorted in the order of importance. */
static std::vector<LightBackend> backends = {
    {Type::ATTENTION, handlemsm8916Notification},
    {Type::NOTIFICATIONS, handlemsm8916Notification},
    {Type::BATTERY, handlemsm8916Notification},
    {Type::BACKLIGHT, handlemsm8916Backlight},
};

}

namespace android {
namespace hardware {
namespace light {
namespace V2_0 {
namespace implementation {

Return<Status> Light::setLight(Type type, const LightState& state) {
    LightStateHandler handler;
    bool handled = false;

    /* Lock global mutex until light state is updated. */
    std::lock_guard<std::mutex> lock(globalLock);

    /* Update the cached state value for the current type. */
    for (LightBackend& backend : backends) {
        if (backend.type == type) {
            backend.state = state;
            handler = backend.handler;
        }
    }
     /* If no handler has been found, then the type is not supported. */
    if (!handler) {
        return Status::LIGHT_NOT_SUPPORTED;
    }

    /* Light up the type with the highest priority that matches the current handler. */
    for (LightBackend& backend : backends) {
        if (handler == backend.handler && isLit(backend.state)) {
            handler(backend.state);
            handled = true;
            break;
        }
    }
    /* If no type has been lit up, then turn off the hardware. */
    if (!handled) {
        handler(state);
    }
    return Status::SUCCESS;
}

Return<void> Light::getSupportedTypes(getSupportedTypes_cb _hidl_cb) {
    std::vector<Type> types;

    for (const LightBackend& backend : backends) {
        types.push_back(backend.type);
    }

    _hidl_cb(types);

    return Void();
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace light
}  // namespace hardware
}  // namespace android
