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
#ifndef ANDROID_HARDWARE_VIBRATOR_TEST_MOCKS_H
#define ANDROID_HARDWARE_VIBRATOR_TEST_MOCKS_H

#include "Vibrator.h"

class MockApi : public ::android::hardware::vibrator::V1_2::implementation::Vibrator::HwApi {
  public:
    MOCK_METHOD0(destructor, void());
    MOCK_METHOD1(setAutocal, bool(std::string value));
    MOCK_METHOD1(setOlLraPeriod, bool(uint32_t value));
    MOCK_METHOD1(setActivate, bool(bool value));
    MOCK_METHOD1(setDuration, bool(uint32_t value));
    MOCK_METHOD1(setState, bool(bool value));
    MOCK_METHOD0(hasRtpInput, bool());
    MOCK_METHOD1(setRtpInput, bool(int8_t value));
    MOCK_METHOD1(setMode, bool(std::string value));
    MOCK_METHOD1(setSequencer, bool(std::string value));
    MOCK_METHOD1(setScale, bool(uint8_t value));
    MOCK_METHOD1(setCtrlLoop, bool(bool value));
    MOCK_METHOD1(setLpTriggerEffect, bool(uint32_t value));
    MOCK_METHOD1(debug, void(int fd));

    ~MockApi() override { destructor(); };
};

class MockCal : public ::android::hardware::vibrator::V1_2::implementation::Vibrator::HwCal {
  public:
    MOCK_METHOD0(destructor, void());
    MOCK_METHOD1(getAutocal, bool(std::string &value));  // NOLINT
    MOCK_METHOD1(getLraPeriod, bool(uint32_t *value));
    MOCK_METHOD1(getClickDuration, bool(uint32_t *value));
    MOCK_METHOD1(getTickDuration, bool(uint32_t *value));
    MOCK_METHOD1(getDoubleClickDuration, bool(uint32_t *value));
    MOCK_METHOD1(getHeavyClickDuration, bool(uint32_t *value));
    MOCK_METHOD1(debug, void(int fd));

    ~MockCal() override { destructor(); };
    // b/132668253: Workaround gMock Compilation Issue
    bool getAutocal(std::string *value) { return getAutocal(*value); }
};

#endif  // ANDROID_HARDWARE_VIBRATOR_TEST_MOCKS_H
