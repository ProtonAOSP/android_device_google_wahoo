/*
 * Copyright (C) 2019 The Android Open Source Project
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
#ifndef ANDROID_HARDWARE_VIBRATOR_HARDWARE_H
#define ANDROID_HARDWARE_VIBRATOR_HARDWARE_H

#include <cutils/properties.h>

#include "Vibrator.h"

namespace android {
namespace hardware {
namespace vibrator {
namespace V1_2 {
namespace implementation {

class HwApi : public Vibrator::HwApi {
  public:
    static std::unique_ptr<HwApi> Create();
    bool setAutocal(std::string value) override { return set(value, &mAutocal); }
    bool setOlLraPeriod(uint32_t value) override { return set(value, &mOlLraPeriod); }
    bool setActivate(bool value) override { return set(value, &mActivate); }
    bool setDuration(uint32_t value) override { return set(value, &mDuration); }
    bool setState(bool value) override { return set(value, &mState); }
    bool hasRtpInput() override { return has(mRtpInput); }
    bool setRtpInput(int8_t value) override { return set(value, &mRtpInput); }
    bool setMode(std::string value) override { return set(value, &mMode); }
    bool setSequencer(std::string value) override { return set(value, &mSequencer); }
    bool setScale(uint8_t value) override { return set(value, &mScale); }
    bool setCtrlLoop(bool value) override { return set(value, &mCtrlLoop); }
    bool setLpTriggerEffect(uint32_t value) override { return set(value, &mLpTrigger); }
    void debug(int fd) override;

  private:
    HwApi();
    template <typename T>
    bool has(const T &stream);
    template <typename T, typename U>
    bool get(T *value, U *stream);
    template <typename T, typename U>
    bool set(const T &value, U *stream);

  private:
    std::map<void *, std::string> mNames;
    std::ofstream mAutocal;
    std::ofstream mOlLraPeriod;
    std::ofstream mActivate;
    std::ofstream mDuration;
    std::ofstream mState;
    std::ofstream mRtpInput;
    std::ofstream mMode;
    std::ofstream mSequencer;
    std::ofstream mScale;
    std::ofstream mCtrlLoop;
    std::ofstream mLpTrigger;
};

class HwCal : public Vibrator::HwCal {
  private:
    static constexpr char AUTOCAL_CONFIG[] = "autocal";
    static constexpr char LRA_PERIOD_CONFIG[] = "lra_period";

    static constexpr uint32_t WAVEFORM_CLICK_EFFECT_MS = 6;
    static constexpr uint32_t WAVEFORM_TICK_EFFECT_MS = 2;
    static constexpr uint32_t WAVEFORM_DOUBLE_CLICK_EFFECT_MS = 135;
    static constexpr uint32_t WAVEFORM_HEAVY_CLICK_EFFECT_MS = 8;

  public:
    HwCal();
    bool getAutocal(std::string *value) override { return get(AUTOCAL_CONFIG, value); }
    bool getLraPeriod(uint32_t *value) override { return get(LRA_PERIOD_CONFIG, value); }
    bool getClickDuration(uint32_t *value) override {
        *value = property_get_int32((mPropertyPrefix + "click.duration").c_str(),
                                    WAVEFORM_CLICK_EFFECT_MS);
        return true;
    }
    bool getTickDuration(uint32_t *value) override {
        *value = property_get_int32((mPropertyPrefix + "tick.duration").c_str(),
                                    WAVEFORM_TICK_EFFECT_MS);
        return true;
    }
    bool getDoubleClickDuration(uint32_t *value) override {
        *value = WAVEFORM_DOUBLE_CLICK_EFFECT_MS;
        return true;
    }
    bool getHeavyClickDuration(uint32_t *value) override {
        *value = property_get_int32((mPropertyPrefix + "heavyclick.duration").c_str(),
                                    WAVEFORM_HEAVY_CLICK_EFFECT_MS);
        return true;
    }
    void debug(int fd) override;

  private:
    template <typename T>
    bool get(const char *key, T *value);

  private:
    std::string mPropertyPrefix;
    std::map<std::string, std::string> mCalData;
};

}  // namespace implementation
}  // namespace V1_2
}  // namespace vibrator
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_VIBRATOR_HARDWARE_H
