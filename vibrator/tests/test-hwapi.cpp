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
#define LOG_TAG "PtsVibratorHalDrv2624TestSuite"

#include <android-base/file.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>

#include "Hardware.h"

namespace android {
namespace hardware {
namespace vibrator {
namespace V1_2 {
namespace implementation {

using ::testing::Test;
using ::testing::TestParamInfo;
using ::testing::ValuesIn;
using ::testing::WithParamInterface;

class HwApiTest : public Test {
  protected:
    static constexpr const char *FILE_NAMES[]{
            "AUTOCAL_FILEPATH", "OL_LRA_PERIOD_FILEPATH", "ACTIVATE_PATH",   "DURATION_PATH",
            "STATE_PATH",       "RTP_INPUT_PATH",         "MODE_PATH",       "SEQUENCER_PATH",
            "SCALE_PATH",       "CTRL_LOOP_PATH",         "LP_TRIGGER_PATH",
    };

    static constexpr const char *REQUIRED[]{
            "ACTIVATE_PATH",
            "DURATION_PATH",
            "STATE_PATH",
    };

  public:
    void SetUp() override {
        for (auto n : FILE_NAMES) {
            auto name = std::string(n);
            auto path = std::string(mFilesDir.path) + "/" + name;
            std::ofstream touch{path};
            setenv(name.c_str(), path.c_str(), true);
            mFileMap[name] = path;
        }
        mHwApi = HwApi::Create();

        for (auto n : REQUIRED) {
            auto name = std::string(n);
            auto path = std::string(mEmptyDir.path) + "/" + name;
            std::ofstream touch{path};
        }
        for (auto n : FILE_NAMES) {
            auto name = std::string(n);
            auto path = std::string(mEmptyDir.path) + "/" + name;
            setenv(name.c_str(), path.c_str(), true);
        }
        mNoApi = HwApi::Create();
    }

    void TearDown() override { verifyContents(); }

  protected:
    // Set expected file content for a test.
    template <typename T>
    void expectContent(const std::string &name, const T &value) {
        mExpectedContent[name] << value << std::endl;
    }

    // Set actual file content for an input test.
    template <typename T>
    void updateContent(const std::string &name, const T &value) {
        std::ofstream(mFileMap[name]) << value << std::endl;
    }

    template <typename T>
    void expectAndUpdateContent(const std::string &name, const T &value) {
        expectContent(name, value);
        updateContent(name, value);
    }

    // Compare all file contents against expected contents.
    void verifyContents() {
        for (auto &a : mFileMap) {
            std::ifstream file{a.second};
            std::string expect = mExpectedContent[a.first].str();
            std::string actual = std::string(std::istreambuf_iterator<char>(file),
                                             std::istreambuf_iterator<char>());
            EXPECT_EQ(expect, actual) << a.first;
        }
    }

    // TODO(eliptus): Determine how to induce errors in required files
    static bool isRequired(const std::string &name) {
        for (auto n : REQUIRED) {
            if (std::string(n) == name) {
                return true;
            }
        }
        return false;
    }

  protected:
    std::unique_ptr<Vibrator::HwApi> mHwApi;
    std::unique_ptr<Vibrator::HwApi> mNoApi;
    std::map<std::string, std::string> mFileMap;
    TemporaryDir mFilesDir;
    TemporaryDir mEmptyDir;
    std::map<std::string, std::stringstream> mExpectedContent;
};

class CreateTest : public HwApiTest, public testing::WithParamInterface<const char *> {
  public:
    void SetUp() override{};
    void TearDown() override{};

    static auto PrintParam(const testing::TestParamInfo<CreateTest::ParamType> &info) {
        return info.param;
    }
    static auto &AllParams() { return FILE_NAMES; }
};

TEST_P(CreateTest, env_missing) {
    auto skip = std::string(GetParam());
    TemporaryDir dir;
    std::unique_ptr<HwApi> hwapi;

    for (auto n : FILE_NAMES) {
        auto name = std::string(n);
        if (name == skip) {
            unsetenv(name.c_str());
            continue;
        }
        auto path = std::string(dir.path) + "/" + name;
        std::ofstream touch{path};
        setenv(name.c_str(), path.c_str(), true);
    }

    hwapi = HwApi::Create();
    if (isRequired(skip)) {
        EXPECT_EQ(nullptr, hwapi);
    } else {
        EXPECT_NE(nullptr, hwapi);
    }
}

TEST_P(CreateTest, file_missing) {
    auto skip = std::string(GetParam());
    TemporaryDir dir;
    std::unique_ptr<HwApi> hwapi;

    for (auto n : FILE_NAMES) {
        auto name = std::string(n);
        auto path = std::string(dir.path) + "/" + name;
        setenv(name.c_str(), path.c_str(), true);
        if (name == skip) {
            continue;
        }
        std::ofstream touch{path};
    }

    hwapi = HwApi::Create();
    if (isRequired(skip)) {
        EXPECT_EQ(nullptr, hwapi);
    } else {
        EXPECT_NE(nullptr, hwapi);
    }
}

INSTANTIATE_TEST_CASE_P(HwApiTests, CreateTest, ValuesIn(CreateTest::AllParams()),
                        CreateTest::PrintParam);

template <typename T>
class HwApiTypedTest
    : public HwApiTest,
      public testing::WithParamInterface<std::tuple<std::string, std::function<T>>> {
  public:
    static auto PrintParam(const testing::TestParamInfo<typename HwApiTypedTest::ParamType> &info) {
        return std::get<0>(info.param);
    }
    static auto MakeParam(std::string name, std::function<T> func) {
        return std::make_tuple(name, func);
    }
};

using HasTest = HwApiTypedTest<bool(Vibrator::HwApi &)>;

TEST_P(HasTest, success_returnsTrue) {
    auto param = GetParam();
    auto func = std::get<1>(param);

    EXPECT_TRUE(func(*mHwApi));
}

TEST_P(HasTest, success_returnsFalse) {
    auto param = GetParam();
    auto func = std::get<1>(param);

    EXPECT_FALSE(func(*mNoApi));
}

INSTANTIATE_TEST_CASE_P(HwApiTests, HasTest,
                        ValuesIn({
                                HasTest::MakeParam("RTP_INPUT_PATH", &Vibrator::HwApi::hasRtpInput),
                        }),
                        HasTest::PrintParam);

using SetBoolTest = HwApiTypedTest<bool(Vibrator::HwApi &, bool)>;

TEST_P(SetBoolTest, success_returnsTrue) {
    auto param = GetParam();
    auto name = std::get<0>(param);
    auto func = std::get<1>(param);

    expectContent(name, "1");

    EXPECT_TRUE(func(*mHwApi, true));
}

TEST_P(SetBoolTest, success_returnsFalse) {
    auto param = GetParam();
    auto name = std::get<0>(param);
    auto func = std::get<1>(param);

    expectContent(name, "0");

    EXPECT_TRUE(func(*mHwApi, false));
}

TEST_P(SetBoolTest, failure) {
    auto param = GetParam();
    auto name = std::get<0>(param);
    auto func = std::get<1>(param);

    if (isRequired(name)) {
        GTEST_SKIP();
    }

    EXPECT_FALSE(func(*mNoApi, true));
    EXPECT_FALSE(func(*mNoApi, false));
}

INSTANTIATE_TEST_CASE_P(
        HwApiTests, SetBoolTest,
        ValuesIn({
                SetBoolTest::MakeParam("ACTIVATE_PATH", &Vibrator::HwApi::setActivate),
                SetBoolTest::MakeParam("STATE_PATH", &Vibrator::HwApi::setState),
                SetBoolTest::MakeParam("CTRL_LOOP_PATH", &Vibrator::HwApi::setCtrlLoop),
        }),
        SetBoolTest::PrintParam);

using SetInt8Test = HwApiTypedTest<bool(Vibrator::HwApi &, int8_t)>;

TEST_P(SetInt8Test, success) {
    auto param = GetParam();
    auto name = std::get<0>(param);
    auto func = std::get<1>(param);
    int8_t value = std::rand();

    expectContent(name, +value);

    EXPECT_TRUE(func(*mHwApi, value));
}

TEST_P(SetInt8Test, failure) {
    auto param = GetParam();
    auto name = std::get<0>(param);
    auto func = std::get<1>(param);
    int8_t value = std::rand();

    if (isRequired(name)) {
        GTEST_SKIP();
    }

    EXPECT_FALSE(func(*mNoApi, value));
}

INSTANTIATE_TEST_CASE_P(HwApiTests, SetInt8Test,
                        ValuesIn({
                                SetInt8Test::MakeParam("RTP_INPUT_PATH",
                                                       &Vibrator::HwApi::setRtpInput),
                        }),
                        SetInt8Test::PrintParam);

using SetUint8Test = HwApiTypedTest<bool(Vibrator::HwApi &, uint8_t)>;

TEST_P(SetUint8Test, success) {
    auto param = GetParam();
    auto name = std::get<0>(param);
    auto func = std::get<1>(param);
    uint8_t value = std::rand();

    expectContent(name, +value);

    EXPECT_TRUE(func(*mHwApi, value));
}

TEST_P(SetUint8Test, failure) {
    auto param = GetParam();
    auto name = std::get<0>(param);
    auto func = std::get<1>(param);
    uint8_t value = std::rand();

    if (isRequired(name)) {
        GTEST_SKIP();
    }

    EXPECT_FALSE(func(*mNoApi, value));
}

INSTANTIATE_TEST_CASE_P(HwApiTests, SetUint8Test,
                        ValuesIn({
                                SetUint8Test::MakeParam("SCALE_PATH", &Vibrator::HwApi::setScale),
                        }),
                        SetUint8Test::PrintParam);

using SetUint32Test = HwApiTypedTest<bool(Vibrator::HwApi &, uint32_t)>;

TEST_P(SetUint32Test, success) {
    auto param = GetParam();
    auto name = std::get<0>(param);
    auto func = std::get<1>(param);
    uint32_t value = std::rand();

    expectContent(name, value);

    EXPECT_TRUE(func(*mHwApi, value));
}

TEST_P(SetUint32Test, failure) {
    auto param = GetParam();
    auto name = std::get<0>(param);
    auto func = std::get<1>(param);
    uint32_t value = std::rand();

    if (isRequired(name)) {
        GTEST_SKIP();
    }

    EXPECT_FALSE(func(*mNoApi, value));
}

INSTANTIATE_TEST_CASE_P(
        HwApiTests, SetUint32Test,
        ValuesIn({
                SetUint32Test::MakeParam("OL_LRA_PERIOD_FILEPATH",
                                         &Vibrator::HwApi::setOlLraPeriod),
                SetUint32Test::MakeParam("DURATION_PATH", &Vibrator::HwApi::setDuration),
                SetUint32Test::MakeParam("LP_TRIGGER_PATH", &Vibrator::HwApi::setLpTriggerEffect),
        }),
        SetUint32Test::PrintParam);

using SetStringTest = HwApiTypedTest<bool(Vibrator::HwApi &, std::string)>;

TEST_P(SetStringTest, success) {
    auto param = GetParam();
    auto name = std::get<0>(param);
    auto func = std::get<1>(param);
    std::string value = TemporaryFile().path;

    expectContent(name, value);

    EXPECT_TRUE(func(*mHwApi, value));
}

TEST_P(SetStringTest, failure) {
    auto param = GetParam();
    auto name = std::get<0>(param);
    auto func = std::get<1>(param);
    std::string value = TemporaryFile().path;

    if (isRequired(name)) {
        GTEST_SKIP();
    }

    EXPECT_FALSE(func(*mNoApi, value));
}

INSTANTIATE_TEST_CASE_P(
        HwApiTests, SetStringTest,
        ValuesIn({
                SetStringTest::MakeParam("AUTOCAL_FILEPATH", &Vibrator::HwApi::setAutocal),
                SetStringTest::MakeParam("MODE_PATH", &Vibrator::HwApi::setMode),
                SetStringTest::MakeParam("SEQUENCER_PATH", &Vibrator::HwApi::setSequencer),
        }),
        SetStringTest::PrintParam);

}  // namespace implementation
}  // namespace V1_2
}  // namespace vibrator
}  // namespace hardware
}  // namespace android
