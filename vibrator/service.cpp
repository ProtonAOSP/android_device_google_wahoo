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
#define LOG_TAG "android.hardware.vibrator@1.2-service.drv2624"

#include <android/hardware/vibrator/1.2/IVibrator.h>
#include <hidl/HidlSupport.h>
#include <hidl/HidlTransportSupport.h>
#include <utils/Errors.h>
#include <utils/StrongPointer.h>

#include "Hardware.h"
#include "Vibrator.h"

using android::OK;
using android::sp;
using android::status_t;
using android::UNKNOWN_ERROR;
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::hardware::vibrator::V1_2::implementation::HwApi;
using android::hardware::vibrator::V1_2::implementation::HwCal;
using android::hardware::vibrator::V1_2::implementation::Vibrator;

status_t registerVibratorService() {
    auto hwapi = HwApi::Create();

    if (!hwapi) {
        return UNKNOWN_ERROR;
    }

    sp<Vibrator> vibrator = new Vibrator(std::move(hwapi), std::make_unique<HwCal>());

    return vibrator->registerAsService();
}

int main() {
    configureRpcThreadpool(1, true);
    status_t status = registerVibratorService();

    if (status != OK) {
        return status;
    }

    joinRpcThreadpool();
}
