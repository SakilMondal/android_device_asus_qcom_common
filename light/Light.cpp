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

namespace android {
namespace hardware {
namespace light {
namespace V2_0 {
namespace implementation {

#define LEDS            "/sys/class/leds/"
#define LCD_LED         LEDS "lcd-backlight/"
#define BRIGHTNESS      "brightness"
#define GREEN           LEDS "green/"
#define PWM             "pwm_us"

/*
 * Write value to path and close file.
 */
static void set(std::string path, std::string value) {
    std::ofstream file(path);
    file << value;
}

static void set(std::string path, int value) {
    set(path, std::to_string(value));
}

static void handlemsm8916Backlight(const LightState& state) {
    uint32_t brightness = state.color & 0xFF;
    set(LCD_LED BRIGHTNESS, brightness);
}

static void handlemsm8916Notification(const LightState& state) {
    uint32_t blink, Fakepwm, pwm, brightness_level;

    /*
     * default brightness & blinkMS to 0
     */
    blink = 0;

    /*
     * msm8916 specific brightness calculation
     * for a not case (RGB extraction way)
     */
    brightness_level = (state.color & 0xff000000) ? (state.color & 0xff000000) >> 24 : 255;

    /* Turn off msm8916's led initially */
    set(GREEN BRIGHTNESS, blink);

    /* Implementation of configuration */
    if ((state.flashMode == Flash::HARDWARE) || (state.flashMode == Flash::TIMED)) {
        uint32_t onMS  = state.flashOnMs;
        uint32_t offMS = state.flashOffMs;

        if (onMS > 0 && offMS > 0) {
        uint32_t blinkMS = onMS + offMS;

        /*
        * pwm specifies the ratio of ON versus OFF
        * pwm = 0 -> always off
        * pwm = 255 -> always on
        *
        * Some Mathamatics below
        *
        */
        Fakepwm = (onMS * 255) / blinkMS;

        /* the low 4 bits are ignored, so round up if necessary */
        if (Fakepwm > 0 && Fakepwm < 16)
            Fakepwm = 16;

        blink = 1;
        pwm = offMS * 1000;
        }
        else {
            blink = 0;
            pwm   = 0;
        }

        /*
         * Let's pulsate them :D
         */
        if (!blink) {
            set(GREEN BRIGHTNESS, brightness_level);
            set(GREEN PWM, 100);
        }
        else {
            set(GREEN BRIGHTNESS, Fakepwm);
            set(GREEN PWM, pwm);
        }
    }
}

static std::map<Type, std::function<void(const LightState&)>> lights = {
    {Type::BACKLIGHT, handlemsm8916Backlight},
    {Type::NOTIFICATIONS, handlemsm8916Notification},
    {Type::BATTERY, handlemsm8916Notification},
    {Type::ATTENTION, handlemsm8916Notification},
};

Light::Light() {}

Return<Status> Light::setLight(Type type, const LightState& state) {
    auto it = lights.find(type);

    if (it == lights.end()) {
        return Status::LIGHT_NOT_SUPPORTED;
    }

    /*
     * Lock global mutex until light state is updated.
    */
    std::lock_guard<std::mutex> lock(globalLock);
    it->second(state);
    return Status::SUCCESS;
}

Return<void> Light::getSupportedTypes(getSupportedTypes_cb _hidl_cb) {
    std::vector<Type> types;

    for (auto const& light : lights) types.push_back(light.first);

    _hidl_cb(types);

    return Void();
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace light
}  // namespace hardware
}  // namespace android
