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

#include <android-base/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "Vibrator.h"
#include "mocks.h"
#include "types.h"
#include "utils.h"

namespace android {
namespace hardware {
namespace vibrator {
namespace V1_2 {
namespace implementation {

using ::android::hardware::vibrator::V1_0::EffectStrength;
using ::android::hardware::vibrator::V1_0::Status;

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AnyOf;
using ::testing::Assign;
using ::testing::Combine;
using ::testing::DoAll;
using ::testing::DoDefault;
using ::testing::Exactly;
using ::testing::Mock;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::SetArgPointee;
using ::testing::SetArgReferee;
using ::testing::Test;
using ::testing::TestParamInfo;
using ::testing::ValuesIn;
using ::testing::WithParamInterface;

// Constants With Prescribed Values

static const std::map<EffectTuple, EffectSequence> EFFECT_SEQUENCES{
        {{Effect::CLICK, EffectStrength::LIGHT}, {"1 0", 2}},
        {{Effect::CLICK, EffectStrength::MEDIUM}, {"1 0", 0}},
        {{Effect::CLICK, EffectStrength::STRONG}, {"1 0", 0}},
        {{Effect::TICK, EffectStrength::LIGHT}, {"2 0", 2}},
        {{Effect::TICK, EffectStrength::MEDIUM}, {"2 0", 0}},
        {{Effect::TICK, EffectStrength::STRONG}, {"2 0", 0}},
        {{Effect::DOUBLE_CLICK, EffectStrength::LIGHT}, {"3 0", 2}},
        {{Effect::DOUBLE_CLICK, EffectStrength::MEDIUM}, {"3 0", 0}},
        {{Effect::DOUBLE_CLICK, EffectStrength::STRONG}, {"3 0", 0}},
        {{Effect::HEAVY_CLICK, EffectStrength::LIGHT}, {"4 0", 2}},
        {{Effect::HEAVY_CLICK, EffectStrength::MEDIUM}, {"4 0", 0}},
        {{Effect::HEAVY_CLICK, EffectStrength::STRONG}, {"4 0", 0}},
};

class VibratorTest : public Test, public WithParamInterface<EffectTuple> {
  public:
    void SetUp() override {
        std::unique_ptr<MockApi> mockapi;
        std::unique_ptr<MockCal> mockcal;

        mEffectDurations[Effect::CLICK] = std::rand();
        mEffectDurations[Effect::TICK] = std::rand();
        mEffectDurations[Effect::DOUBLE_CLICK] = std::rand();
        mEffectDurations[Effect::HEAVY_CLICK] = std::rand();

        createMock(&mockapi, &mockcal);
        createVibrator(std::move(mockapi), std::move(mockcal));
    }

    void TearDown() override { deleteVibrator(); }

  protected:
    void createMock(std::unique_ptr<MockApi> *mockapi, std::unique_ptr<MockCal> *mockcal) {
        *mockapi = std::make_unique<MockApi>();
        *mockcal = std::make_unique<MockCal>();

        mMockApi = mockapi->get();
        mMockCal = mockcal->get();

        ON_CALL(*mMockApi, destructor()).WillByDefault(Assign(&mMockApi, nullptr));

        ON_CALL(*mMockCal, destructor()).WillByDefault(Assign(&mMockCal, nullptr));
        ON_CALL(*mMockCal, getClickDuration(_))
                .WillByDefault(DoAll(SetArgPointee<0>(mEffectDurations[Effect::CLICK]),
                                     ::testing::Return(true)));
        ON_CALL(*mMockCal, getTickDuration(_))
                .WillByDefault(DoAll(SetArgPointee<0>(mEffectDurations[Effect::TICK]),
                                     ::testing::Return(true)));
        ON_CALL(*mMockCal, getDoubleClickDuration(_))
                .WillByDefault(DoAll(SetArgPointee<0>(mEffectDurations[Effect::DOUBLE_CLICK]),
                                     ::testing::Return(true)));
        ON_CALL(*mMockCal, getHeavyClickDuration(_))
                .WillByDefault(DoAll(SetArgPointee<0>(mEffectDurations[Effect::HEAVY_CLICK]),
                                     ::testing::Return(true)));

        relaxMock(false);
    }

    void createVibrator(std::unique_ptr<MockApi> mockapi, std::unique_ptr<MockCal> mockcal,
                        bool relaxed = true) {
        if (relaxed) {
            relaxMock(true);
        }
        mVibrator = new Vibrator(std::move(mockapi), std::move(mockcal));
        if (relaxed) {
            relaxMock(false);
        }
    }

    void deleteVibrator(bool relaxed = true) {
        if (relaxed) {
            relaxMock(true);
        }
        mVibrator.clear();
    }

  private:
    void relaxMock(bool relax) {
        auto times = relax ? AnyNumber() : Exactly(0);

        Mock::VerifyAndClearExpectations(mMockApi);
        Mock::VerifyAndClearExpectations(mMockCal);

        EXPECT_CALL(*mMockApi, destructor()).Times(times);
        EXPECT_CALL(*mMockApi, setAutocal(_)).Times(times);
        EXPECT_CALL(*mMockApi, setOlLraPeriod(_)).Times(times);
        EXPECT_CALL(*mMockApi, setActivate(_)).Times(times);
        EXPECT_CALL(*mMockApi, setDuration(_)).Times(times);
        EXPECT_CALL(*mMockApi, setState(_)).Times(times);
        EXPECT_CALL(*mMockApi, hasRtpInput()).Times(times);
        EXPECT_CALL(*mMockApi, setRtpInput(_)).Times(times);
        EXPECT_CALL(*mMockApi, setMode(_)).Times(times);
        EXPECT_CALL(*mMockApi, setSequencer(_)).Times(times);
        EXPECT_CALL(*mMockApi, setScale(_)).Times(times);
        EXPECT_CALL(*mMockApi, setCtrlLoop(_)).Times(times);
        EXPECT_CALL(*mMockApi, setLpTriggerEffect(_)).Times(times);
        EXPECT_CALL(*mMockApi, debug(_)).Times(times);

        EXPECT_CALL(*mMockCal, destructor()).Times(times);
        EXPECT_CALL(*mMockCal, getAutocal(_)).Times(times);
        EXPECT_CALL(*mMockCal, getLraPeriod(_)).Times(times);
        EXPECT_CALL(*mMockCal, getClickDuration(_)).Times(times);
        EXPECT_CALL(*mMockCal, getTickDuration(_)).Times(times);
        EXPECT_CALL(*mMockCal, getDoubleClickDuration(_)).Times(times);
        EXPECT_CALL(*mMockCal, getHeavyClickDuration(_)).Times(times);
        EXPECT_CALL(*mMockCal, debug(_)).Times(times);
    }

  protected:
    MockApi *mMockApi;
    MockCal *mMockCal;
    sp<IVibrator> mVibrator;

    std::map<Effect, EffectDuration> mEffectDurations;
};

TEST_F(VibratorTest, Constructor) {
    std::unique_ptr<MockApi> mockapi;
    std::unique_ptr<MockCal> mockcal;
    std::string autocalVal = std::to_string(std::rand()) + " " + std::to_string(std::rand()) + " " +
                             std::to_string(std::rand());
    uint32_t lraPeriodVal = std::rand();
    Sequence autocalSeq, lraPeriodSeq;

    EXPECT_CALL(*mMockApi, destructor()).WillOnce(DoDefault());
    EXPECT_CALL(*mMockCal, destructor()).WillOnce(DoDefault());

    deleteVibrator(false);

    createMock(&mockapi, &mockcal);

    EXPECT_CALL(*mMockApi, setState(true)).WillOnce(::testing::Return(true));

    EXPECT_CALL(*mMockCal, getAutocal(_))
            .InSequence(autocalSeq)
            .WillOnce(DoAll(SetArgReferee<0>(autocalVal), ::testing::Return(true)));
    EXPECT_CALL(*mMockApi, setAutocal(autocalVal))
            .InSequence(autocalSeq)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*mMockCal, getLraPeriod(_))
            .InSequence(lraPeriodSeq)
            .WillOnce(DoAll(SetArgPointee<0>(lraPeriodVal), ::testing::Return(true)));
    EXPECT_CALL(*mMockApi, setOlLraPeriod(lraPeriodVal))
            .InSequence(lraPeriodSeq)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*mMockCal, getClickDuration(_)).WillOnce(DoDefault());
    EXPECT_CALL(*mMockCal, getTickDuration(_)).WillOnce(DoDefault());
    EXPECT_CALL(*mMockCal, getDoubleClickDuration(_)).WillOnce(DoDefault());
    EXPECT_CALL(*mMockCal, getHeavyClickDuration(_)).WillOnce(DoDefault());

    EXPECT_CALL(*mMockApi, setLpTriggerEffect(1)).WillOnce(::testing::Return(true));

    createVibrator(std::move(mockapi), std::move(mockcal), false);
}

TEST_F(VibratorTest, on) {
    EffectDuration duration = std::rand();
    Sequence s1, s2, s3;

    EXPECT_CALL(*mMockApi, setCtrlLoop(AnyOf(0, 1)))
            .InSequence(s1)
            .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mMockApi, setMode("rtp")).InSequence(s2).WillOnce(::testing::Return(true));
    EXPECT_CALL(*mMockApi, setDuration(duration)).InSequence(s3).WillOnce(::testing::Return(true));
    EXPECT_CALL(*mMockApi, setActivate(true))
            .InSequence(s1, s2, s3)
            .WillOnce(::testing::Return(true));

    EXPECT_EQ(Status::OK, mVibrator->on(duration));
}

TEST_F(VibratorTest, off) {
    EXPECT_CALL(*mMockApi, setActivate(false)).WillOnce(::testing::Return(true));

    EXPECT_EQ(Status::OK, mVibrator->off());
}

TEST_F(VibratorTest, supportsAmplitudeControl_supported) {
    EXPECT_CALL(*mMockApi, hasRtpInput()).WillOnce(::testing::Return(true));

    EXPECT_EQ(true, mVibrator->supportsAmplitudeControl());
}

TEST_F(VibratorTest, supportsAmplitudeControl_unsupported) {
    EXPECT_CALL(*mMockApi, hasRtpInput()).WillOnce(::testing::Return(false));

    EXPECT_EQ(false, mVibrator->supportsAmplitudeControl());
}

TEST_F(VibratorTest, setAmplitude) {
    EffectAmplitude amplitude = std::rand();

    EXPECT_CALL(*mMockApi, setRtpInput(amplitudeToRtpInput(amplitude)))
            .WillOnce(::testing::Return(true));

    EXPECT_EQ(Status::OK, mVibrator->setAmplitude(amplitude));
}

TEST_P(VibratorTest, perform) {
    auto param = GetParam();
    auto effect = std::get<0>(param);
    auto strength = std::get<1>(param);
    auto seqIter = EFFECT_SEQUENCES.find(param);
    auto durIter = mEffectDurations.find(effect);
    EffectDuration duration;

    if (seqIter != EFFECT_SEQUENCES.end() && durIter != mEffectDurations.end()) {
        auto sequence = std::get<0>(seqIter->second);
        auto scale = std::get<1>(seqIter->second);
        Sequence s1, s2, s3, s4, s5;

        duration = durIter->second;

        EXPECT_CALL(*mMockApi, setSequencer(sequence))
                .InSequence(s1)
                .WillOnce(::testing::Return(true));
        EXPECT_CALL(*mMockApi, setScale(scale)).InSequence(s2).WillOnce(::testing::Return(true));
        EXPECT_CALL(*mMockApi, setCtrlLoop(1)).InSequence(s3).WillOnce(::testing::Return(true));
        EXPECT_CALL(*mMockApi, setMode("waveform"))
                .InSequence(s4)
                .WillOnce(::testing::Return(true));
        EXPECT_CALL(*mMockApi, setDuration(duration))
                .InSequence(s5)
                .WillOnce(::testing::Return(true));

        EXPECT_CALL(*mMockApi, setActivate(true))
                .InSequence(s1, s2, s3, s4, s5)
                .WillOnce(::testing::Return(true));
    } else {
        duration = 0;
    }

    mVibrator->perform_1_2(effect, strength, [&](Status status, uint32_t lengthMs) {
        if (duration) {
            EXPECT_EQ(Status::OK, status);
            EXPECT_EQ(duration, lengthMs);
        } else {
            EXPECT_EQ(Status::UNSUPPORTED_OPERATION, status);
            EXPECT_EQ(0, lengthMs);
        }
    });
}

INSTANTIATE_TEST_CASE_P(VibratorEffects, VibratorTest,
                        Combine(ValuesIn(hidl_enum_range<Effect>().begin(),
                                         hidl_enum_range<Effect>().end()),
                                ValuesIn(hidl_enum_range<EffectStrength>().begin(),
                                         hidl_enum_range<EffectStrength>().end())),
                        [](const testing::TestParamInfo<VibratorTest::ParamType> &info) {
                            auto effect = std::get<0>(info.param);
                            auto strength = std::get<1>(info.param);
                            return toString(effect) + "_" + toString(strength);
                        });

}  // namespace implementation
}  // namespace V1_2
}  // namespace vibrator
}  // namespace hardware
}  // namespace android
