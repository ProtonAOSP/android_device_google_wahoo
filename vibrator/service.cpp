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
#define LOG_TAG "android.hardware.vibrator@1.0-service.wahoo"

#include <android/hardware/vibrator/1.0/IVibrator.h>
#include <hidl/HidlSupport.h>
#include <hidl/HidlTransportSupport.h>
#include <utils/Errors.h>
#include <utils/StrongPointer.h>

#include "Vibrator.h"

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::hardware::vibrator::V1_0::IVibrator;
using android::hardware::vibrator::V1_0::implementation::Vibrator;
using namespace android;

// Refer to Documentation/ABI/testing/sysfs-class-led-driver-drv2624
// kernel documentation on the detail usages for ABIs below
static const char *ACTIVATE_PATH = "/sys/class/leds/vibrator/activate";
static const char *DURATION_PATH = "/sys/class/leds/vibrator/duration";
static const char *STATE_PATH = "/sys/class/leds/vibrator/state";
static const char *RTP_INPUT_PATH = "/sys/class/leds/vibrator/device/rtp_input";
static const char *MODE_PATH = "/sys/class/leds/vibrator/device/mode";
static const char *SEQUENCER_PATH = "/sys/class/leds/vibrator/device/set_sequencer";
static const char *SCALE_PATH = "/sys/class/leds/vibrator/device/scale";
static const char *CTRL_LOOP_PATH = "/sys/class/leds/vibrator/device/ctrl_loop";

status_t registerVibratorService() {
    // ostreams below are required
    std::ofstream activate{ACTIVATE_PATH};
    if (!activate) {
        int error = errno;
        ALOGE("Failed to open %s (%d): %s", ACTIVATE_PATH, error, strerror(error));
        return -error;
    }

    std::ofstream duration{DURATION_PATH};
    if (!duration) {
        int error = errno;
        ALOGE("Failed to open %s (%d): %s", DURATION_PATH, error, strerror(error));
        return -error;
    }

    std::ofstream state{STATE_PATH};
    if (!state) {
        int error = errno;
        ALOGE("Failed to open %s (%d): %s", STATE_PATH, error, strerror(error));
        return -error;
    }

    state << 1 << std::endl;
    if (!state) {
        int error = errno;
        ALOGE("Failed to set state (%d): %s", errno, strerror(errno));
        return -error;
    }

    // ostreams below are optional
    std::ofstream rtpinput{RTP_INPUT_PATH};
    if (!state) {
        int error = errno;
        ALOGW("Failed to open %s (%d): %s", RTP_INPUT_PATH, error, strerror(error));
    }

    std::ofstream mode{MODE_PATH};
    if (!state) {
        int error = errno;
        ALOGW("Failed to open %s (%d): %s", MODE_PATH, error, strerror(error));
    }

    std::ofstream sequencer{SEQUENCER_PATH};
    if (!state) {
        int error = errno;
        ALOGW("Failed to open %s (%d): %s", SEQUENCER_PATH, error, strerror(error));
    }

    std::ofstream scale{SCALE_PATH};
    if (!state) {
        int error = errno;
        ALOGW("Failed to open %s (%d): %s", SCALE_PATH, error, strerror(error));
    }

    std::ofstream ctrlloop{CTRL_LOOP_PATH};
    if (!state) {
        int error = errno;
        ALOGW("Failed to open %s (%d): %s", CTRL_LOOP_PATH, error, strerror(error));
    }

    sp<IVibrator> vibrator = new Vibrator(std::move(activate), std::move(duration),
            std::move(state), std::move(rtpinput), std::move(mode),
            std::move(sequencer), std::move(scale), std::move(ctrlloop));

    vibrator->registerAsService();

    return OK;
}

int main() {
    configureRpcThreadpool(1, true);
    status_t status = registerVibratorService();

    if (status != OK) {
        return status;
    }

    joinRpcThreadpool();
}
