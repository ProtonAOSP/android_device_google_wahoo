/*
 * Copyright (C) 2017 The Android Open Source Project
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
#ifndef ANDROID_HARDWARE_VIBRATOR_V1_2_VIBRATOR_H
#define ANDROID_HARDWARE_VIBRATOR_V1_2_VIBRATOR_H

#include <android/hardware/vibrator/1.2/IVibrator.h>
#include <hidl/Status.h>

#include <fstream>

namespace android {
namespace hardware {
namespace vibrator {
namespace V1_2 {
namespace implementation {

class Vibrator : public IVibrator {
  public:
    // APIs for interfacing with the kernel driver.
    class HwApi {
      public:
        virtual ~HwApi() = default;
        // Stores the COMP, BEMF, and GAIN calibration values to use.
        //   <COMP> <BEMF> <GAIN>
        virtual bool setAutocal(std::string value) = 0;
        // Stores the open-loop LRA frequency to be used.
        virtual bool setOlLraPeriod(uint32_t value) = 0;
        // Activates/deactivates the vibrator for durations specified by
        // setDuration().
        virtual bool setActivate(bool value) = 0;
        // Specifies the vibration duration in milliseconds.
        virtual bool setDuration(uint32_t value) = 0;
        // Specifies the active state of the vibrator
        // (true = enabled, false = disabled).
        virtual bool setState(bool value) = 0;
        // Reports whether setRtpInput() is supported.
        virtual bool hasRtpInput() = 0;
        // Specifies the playback amplitude of the haptic waveforms in RTP mode.
        // Negative numbers indicates braking.
        virtual bool setRtpInput(int8_t value) = 0;
        // Specifies the mode of operation.
        //   rtp        - RTP Mode
        //   waveform   - Waveform Sequencer Mode
        //   diag       - Diagnostics Routine
        //   autocal    - Automatic Level Calibration Routine
        virtual bool setMode(std::string value) = 0;
        // Specifies a waveform sequence in index-count pairs.
        //   <index-1> <count-1> [<index-2> <cound-2> ...]
        virtual bool setSequencer(std::string value) = 0;
        // Specifies the scaling of effects in Waveform mode.
        //   0 - 100%
        //   1 - 75%
        //   2 - 50%
        //   3 - 25%
        virtual bool setScale(uint8_t value) = 0;
        // Selects either closed loop or open loop mode.
        // (true = open, false = closed).
        virtual bool setCtrlLoop(bool value) = 0;
        // Specifies waveform index to be played in low-power trigger mode.
        // 0    - Disabled
        // 1+   - Waveform Index
        virtual bool setLpTriggerEffect(uint32_t value) = 0;
        // Emit diagnostic information to the given file.
        virtual void debug(int fd) = 0;
    };

    // APIs for obtaining calibration/configuration data from persistent memory.
    class HwCal {
      public:
        virtual ~HwCal() = default;
        // Obtains the COMP, BEMF, and GAIN calibration values to use.
        virtual bool getAutocal(std::string *value) = 0;
        // Obtains the open-loop LRA frequency to be used.
        virtual bool getLraPeriod(uint32_t *value) = 0;
        // Obtains the duration for the click effect
        virtual bool getClickDuration(uint32_t *value) = 0;
        // Obtains the duration for the tick effect
        virtual bool getTickDuration(uint32_t *value) = 0;
        // Obtains the duration for the double-click effect
        virtual bool getDoubleClickDuration(uint32_t *value) = 0;
        // Obtains the duration for the heavy-click effect
        virtual bool getHeavyClickDuration(uint32_t *value) = 0;
        // Emit diagnostic information to the given file.
        virtual void debug(int fd) = 0;
    };

  public:
    Vibrator(std::unique_ptr<HwApi> hwapi, std::unique_ptr<HwCal> hwcal);

    // Methods from ::android::hardware::vibrator::V1_0::IVibrator follow.
    using Status = ::android::hardware::vibrator::V1_0::Status;
    Return<Status> on(uint32_t timeoutMs) override;
    Return<Status> off() override;
    Return<bool> supportsAmplitudeControl() override;
    Return<Status> setAmplitude(uint8_t amplitude) override;

    using EffectStrength = ::android::hardware::vibrator::V1_0::EffectStrength;
    Return<void> perform(V1_0::Effect effect, EffectStrength strength,
                         perform_cb _hidl_cb) override;
    Return<void> perform_1_1(V1_1::Effect_1_1 effect, EffectStrength strength,
                             perform_cb _hidl_cb) override;
    Return<void> perform_1_2(Effect effect, EffectStrength strength, perform_cb _hidl_cb) override;

    // Methods from ::android.hidl.base::V1_0::IBase follow.
    Return<void> debug(const hidl_handle &handle, const hidl_vec<hidl_string> &options) override;

  private:
    Return<Status> on(uint32_t timeoutMs, bool forceOpenLoop, bool isWaveform);
    template <typename T>
    Return<void> performWrapper(T effect, EffectStrength strength, perform_cb _hidl_cb);
    Return<void> performEffect(Effect effect, EffectStrength strength, perform_cb _hidl_cb);
    std::unique_ptr<HwApi> mHwApi;
    std::unique_ptr<HwCal> mHwCal;
    uint32_t mClickDuration;
    uint32_t mTickDuration;
    uint32_t mDoubleClickDuration;
    uint32_t mHeavyClickDuration;
};
}  // namespace implementation
}  // namespace V1_2
}  // namespace vibrator
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_VIBRATOR_V1_2_VIBRATOR_H
