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

#define LOG_TAG "VibratorService"

#include "Vibrator.h"

#include <cutils/properties.h>
#include <hardware/hardware.h>
#include <hardware/vibrator.h>
#include <log/log.h>

#include <cinttypes>
#include <cmath>
#include <fstream>
#include <iostream>

namespace android {
namespace hardware {
namespace vibrator {
namespace V1_2 {
namespace implementation {

static constexpr int8_t MAX_RTP_INPUT = 127;
static constexpr int8_t MIN_RTP_INPUT = 0;

static constexpr char RTP_MODE[] = "rtp";
static constexpr char WAVEFORM_MODE[] = "waveform";

// Use effect #1 in the waveform library for CLICK effect
static constexpr char WAVEFORM_CLICK_EFFECT_SEQ[] = "1 0";

// Use effect #2 in the waveform library for TICK effect
static constexpr char WAVEFORM_TICK_EFFECT_SEQ[] = "2 0";

// Use effect #3 in the waveform library for DOUBLE_CLICK effect
static constexpr char WAVEFORM_DOUBLE_CLICK_EFFECT_SEQ[] = "3 0";

// Use effect #4 in the waveform library for HEAVY_CLICK effect
static constexpr char WAVEFORM_HEAVY_CLICK_EFFECT_SEQ[] = "4 0";

// Timeout threshold for selecting open or closed loop mode
static constexpr int8_t LOOP_MODE_THRESHOLD_MS = 20;

using Status = ::android::hardware::vibrator::V1_0::Status;
using EffectStrength = ::android::hardware::vibrator::V1_0::EffectStrength;

Vibrator::Vibrator(std::unique_ptr<HwApi> hwapi, std::unique_ptr<HwCal> hwcal)
    : mHwApi(std::move(hwapi)), mHwCal(std::move(hwcal)) {
    std::string autocal;
    uint32_t lraPeriod;

    if (!mHwApi->setState(true)) {
        ALOGE("Failed to set state (%d): %s", errno, strerror(errno));
    }

    if (mHwCal->getAutocal(&autocal)) {
        mHwApi->setAutocal(autocal);
    }
    if (mHwCal->getLraPeriod(&lraPeriod)) {
        mHwApi->setOlLraPeriod(lraPeriod);
    }

    mHwCal->getClickDuration(&mClickDuration);
    mHwCal->getTickDuration(&mTickDuration);
    mHwCal->getDoubleClickDuration(&mDoubleClickDuration);
    mHwCal->getHeavyClickDuration(&mHeavyClickDuration);

    // This enables effect #1 from the waveform library to be triggered by SLPI
    // while the AP is in suspend mode
    if (!mHwApi->setLpTriggerEffect(1)) {
        ALOGW("Failed to set LP trigger mode (%d): %s", errno, strerror(errno));
    }
}

Return<Status> Vibrator::on(uint32_t timeoutMs, bool forceOpenLoop, bool isWaveform) {
    uint32_t loopMode = 1;

    // Open-loop mode is used for short click for over-drive
    // Close-loop mode is used for long notification for stability
    if (!forceOpenLoop && timeoutMs > LOOP_MODE_THRESHOLD_MS) {
        loopMode = 0;
    }

    mHwApi->setCtrlLoop(loopMode);
    if (!mHwApi->setDuration(timeoutMs)) {
        ALOGE("Failed to set duration (%d): %s", errno, strerror(errno));
        return Status::UNKNOWN_ERROR;
    }

    if (isWaveform) {
        mHwApi->setMode(WAVEFORM_MODE);
    } else {
        mHwApi->setMode(RTP_MODE);
    }

    if (!mHwApi->setActivate(1)) {
        ALOGE("Failed to activate (%d): %s", errno, strerror(errno));
        return Status::UNKNOWN_ERROR;
    }

    return Status::OK;
}

// Methods from ::android::hardware::vibrator::V1_2::IVibrator follow.
Return<Status> Vibrator::on(uint32_t timeoutMs) {
    return on(timeoutMs, false /* forceOpenLoop */, false /* isWaveform */);
}

Return<Status> Vibrator::off() {
    if (!mHwApi->setActivate(0)) {
        ALOGE("Failed to turn vibrator off (%d): %s", errno, strerror(errno));
        return Status::UNKNOWN_ERROR;
    }
    return Status::OK;
}

Return<bool> Vibrator::supportsAmplitudeControl() {
    return (mHwApi->hasRtpInput() ? true : false);
}

Return<Status> Vibrator::setAmplitude(uint8_t amplitude) {
    if (amplitude == 0) {
        return Status::BAD_VALUE;
    }

    int32_t rtp_input =
            std::round((amplitude - 1) / 254.0 * (MAX_RTP_INPUT - MIN_RTP_INPUT) + MIN_RTP_INPUT);

    if (!mHwApi->setRtpInput(rtp_input)) {
        ALOGE("Failed to set amplitude (%d): %s", errno, strerror(errno));
        return Status::UNKNOWN_ERROR;
    }

    return Status::OK;
}

// Methods from ::android.hidl.base::V1_0::IBase follow.

Return<void> Vibrator::debug(const hidl_handle &handle,
                             const hidl_vec<hidl_string> & /* options */) {
    if (handle == nullptr || handle->numFds < 1 || handle->data[0] < 0) {
        ALOGE("Called debug() with invalid fd.");
        return Void();
    }

    int fd = handle->data[0];

    dprintf(fd, "HIDL:\n");

    dprintf(fd, "  Click Duration: %" PRIu32 "\n", mClickDuration);
    dprintf(fd, "  Tick Duration: %" PRIu32 "\n", mTickDuration);
    dprintf(fd, "  Double Click Duration: %" PRIu32 "\n", mDoubleClickDuration);
    dprintf(fd, "  Heavy Click Duration: %" PRIu32 "\n", mHeavyClickDuration);

    dprintf(fd, "\n");

    mHwApi->debug(fd);

    dprintf(fd, "\n");

    mHwCal->debug(fd);

    fsync(fd);
    return Void();
}

static uint8_t convertEffectStrength(EffectStrength strength) {
    uint8_t scale;

    switch (strength) {
        case EffectStrength::LIGHT:
            scale = 2;  // 50%
            break;
        case EffectStrength::MEDIUM:
        case EffectStrength::STRONG:
            scale = 0;  // 100%
            break;
    }

    return scale;
}

Return<void> Vibrator::perform(V1_0::Effect effect, EffectStrength strength, perform_cb _hidl_cb) {
    return performWrapper(effect, strength, _hidl_cb);
}

Return<void> Vibrator::perform_1_1(V1_1::Effect_1_1 effect, EffectStrength strength,
                                   perform_cb _hidl_cb) {
    return performWrapper(effect, strength, _hidl_cb);
}

Return<void> Vibrator::perform_1_2(Effect effect, EffectStrength strength, perform_cb _hidl_cb) {
    return performWrapper(effect, strength, _hidl_cb);
}

template <typename T>
Return<void> Vibrator::performWrapper(T effect, EffectStrength strength, perform_cb _hidl_cb) {
    auto validEffectRange = hidl_enum_range<T>();
    if (effect < *validEffectRange.begin() || effect > *std::prev(validEffectRange.end())) {
        _hidl_cb(Status::UNSUPPORTED_OPERATION, 0);
        return Void();
    }
    auto validStrengthRange = hidl_enum_range<EffectStrength>();
    if (strength < *validStrengthRange.begin() || strength > *std::prev(validStrengthRange.end())) {
        _hidl_cb(Status::UNSUPPORTED_OPERATION, 0);
        return Void();
    }
    return performEffect(static_cast<Effect>(effect), strength, _hidl_cb);
}

Return<void> Vibrator::performEffect(Effect effect, EffectStrength strength, perform_cb _hidl_cb) {
    Status status = Status::OK;
    uint32_t timeMS;

    switch (effect) {
        case Effect::CLICK:
            mHwApi->setSequencer(WAVEFORM_CLICK_EFFECT_SEQ);
            timeMS = mClickDuration;
            break;
        case Effect::DOUBLE_CLICK:
            mHwApi->setSequencer(WAVEFORM_DOUBLE_CLICK_EFFECT_SEQ);
            timeMS = mDoubleClickDuration;
            break;
        case Effect::TICK:
            mHwApi->setSequencer(WAVEFORM_TICK_EFFECT_SEQ);
            timeMS = mTickDuration;
            break;
        case Effect::HEAVY_CLICK:
            mHwApi->setSequencer(WAVEFORM_HEAVY_CLICK_EFFECT_SEQ);
            timeMS = mHeavyClickDuration;
            break;
        default:
            _hidl_cb(Status::UNSUPPORTED_OPERATION, 0);
            return Void();
    }
    mHwApi->setScale(convertEffectStrength(strength));
    on(timeMS, true /* forceOpenLoop */, true /* isWaveform */);
    _hidl_cb(status, timeMS);
    return Void();
}

}  // namespace implementation
}  // namespace V1_2
}  // namespace vibrator
}  // namespace hardware
}  // namespace android
