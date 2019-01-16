/*
 * Copyright (C) 2018 The Android Open Source Project
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

#define ATRACE_TAG (ATRACE_TAG_POWER | ATRACE_TAG_HAL)
#define LOG_TAG "android.hardware.power@1.2-service.wahoo-libperfmgr"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/strings.h>
#include <android-base/stringprintf.h>

#include <mutex>

#include <utils/Log.h>
#include <utils/Trace.h>

#include "Power.h"

extern struct stat_pair rpm_stat_map[];

namespace android {
namespace hardware {
namespace power {
namespace V1_2 {
namespace implementation {

using ::android::hardware::power::V1_0::Feature;
using ::android::hardware::power::V1_0::Status;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;

Power::Power() :
        mHintManager(nullptr),
        mInteractionHandler(nullptr),
        mVRModeOn(false),
        mSustainedPerfModeOn(false),
        mEncoderModeOn(false),
        mReady(false) {
            mInitThread =
            std::thread([this](){
                            android::base::WaitForProperty(kPowerHalInitProp, "1");
                            mHintManager = HintManager::GetFromJSON(kPowerHalConfigPath);
                            if (!mHintManager) {
                                LOG(FATAL) << "Invalid config: " << kPowerHalConfigPath;
                            }
                            mInteractionHandler = std::make_unique<InteractionHandler>(mHintManager);
                            mInteractionHandler->Init();

                            std::string state = android::base::GetProperty(kPowerHalStateProp, "");
                            if (state == "VIDEO_ENCODE") {
                                ALOGI("Initialize with VIDEO_ENCODE on");
                                mHintManager->DoHint("VIDEO_ENCODE");
                                mEncoderModeOn = true;
                            } else if (state ==  "SUSTAINED_PERFORMANCE") {
                                ALOGI("Initialize with SUSTAINED_PERFORMANCE on");
                                mHintManager->DoHint("SUSTAINED_PERFORMANCE");
                                mSustainedPerfModeOn = true;
                            } else if (state == "VR_MODE") {
                                ALOGI("Initialize with VR_MODE on");
                                mHintManager->DoHint("VR_MODE");
                                mVRModeOn = true;
                            } else if (state == "VR_SUSTAINED_PERFORMANCE") {
                                ALOGI("Initialize with SUSTAINED_PERFORMANCE and VR_MODE on");
                                mHintManager->DoHint("VR_SUSTAINED_PERFORMANCE");
                                mSustainedPerfModeOn = true;
                                mVRModeOn = true;
                            } else {
                                ALOGI("Initialize PowerHAL");
                            }

                            state = android::base::GetProperty(kPowerHalAudioProp, "");
                            if (state == "AUDIO_LOW_LATENCY") {
                                ALOGI("Initialize with AUDIO_LOW_LATENCY on");
                                mHintManager->DoHint("AUDIO_LOW_LATENCY");
                            }

                            // Now start to take powerhint
                            mReady.store(true);
                        });
            mInitThread.detach();
}

// Methods from ::android::hardware::power::V1_0::IPower follow.
Return<void> Power::setInteractive(bool /* interactive */)  {
    return Void();
}

Return<void> Power::powerHint(PowerHint_1_0 hint, int32_t data) {
    if (!isSupportedGovernor() || !mReady) {
        return Void();
    }

    switch(hint) {
        case PowerHint_1_0::INTERACTION:
            if (mVRModeOn || mSustainedPerfModeOn) {
                ALOGV("%s: ignoring due to other active perf hints", __func__);
            } else {
                mInteractionHandler->Acquire(data);
            }
            break;
        case PowerHint_1_0::VIDEO_ENCODE:
            if (mVRModeOn || mSustainedPerfModeOn) {
                ALOGV("%s: ignoring due to other active perf hints", __func__);
                break;
            }
            ATRACE_BEGIN("video_encode");
            if (mVRModeOn || mSustainedPerfModeOn) {
                ALOGV("%s: ignoring due to other active perf hints", __func__);
            } else {
                if (data) {
                    // Hint until canceled
                    ATRACE_INT("video_encode_lock", 1);
                    mHintManager->DoHint("VIDEO_ENCODE");
                    ALOGD("VIDEO_ENCODE ON");
                    mEncoderModeOn = true;
                } else {
                    ATRACE_INT("video_encode_lock", 0);
                    mHintManager->EndHint("VIDEO_ENCODE");
                    ALOGD("VIDEO_ENCODE OFF");
                    mEncoderModeOn = false;
                }
            }
            ATRACE_END();
            break;
        case PowerHint_1_0::SUSTAINED_PERFORMANCE:
            if (data && !mSustainedPerfModeOn) {
                ALOGD("SUSTAINED_PERFORMANCE ON");
                if (!mVRModeOn) { // Sustained mode only.
                    mHintManager->DoHint("SUSTAINED_PERFORMANCE");
                } else { // Sustained + VR mode.
                    mHintManager->EndHint("VR_MODE");
                    mHintManager->DoHint("VR_SUSTAINED_PERFORMANCE");
                }
                mSustainedPerfModeOn = true;
            } else if (!data && mSustainedPerfModeOn) {
                ALOGD("SUSTAINED_PERFORMANCE OFF");
                mHintManager->EndHint("VR_SUSTAINED_PERFORMANCE");
                mHintManager->EndHint("SUSTAINED_PERFORMANCE");
                if (mVRModeOn) { // Switch back to VR Mode.
                    mHintManager->DoHint("VR_MODE");
                }
                mSustainedPerfModeOn = false;
            }
            break;
        case PowerHint_1_0::VR_MODE:
            if (data && !mVRModeOn) {
                ALOGD("VR_MODE ON");
                if (!mSustainedPerfModeOn) { // VR mode only.
                    mHintManager->DoHint("VR_MODE");
                } else { // Sustained + VR mode.
                    mHintManager->EndHint("SUSTAINED_PERFORMANCE");
                    mHintManager->DoHint("VR_SUSTAINED_PERFORMANCE");
                }
                mVRModeOn = true;
            } else if (!data && mVRModeOn) {
                ALOGD("VR_MODE OFF");
                mHintManager->EndHint("VR_SUSTAINED_PERFORMANCE");
                mHintManager->EndHint("VR_MODE");
                if (mSustainedPerfModeOn) { // Switch back to sustained Mode.
                    mHintManager->DoHint("SUSTAINED_PERFORMANCE");
                }
                mVRModeOn = false;
            }
            break;
        case PowerHint_1_0::LAUNCH:
            ATRACE_BEGIN("launch");
            if (mVRModeOn || mSustainedPerfModeOn) {
                ALOGV("%s: ignoring due to other active perf hints", __func__);
            } else {
                if (data) {
                    // Hint until canceled
                    ATRACE_INT("launch_lock", 1);
                    mHintManager->DoHint("LAUNCH");
                    ALOGD("LAUNCH ON");
                } else {
                    ATRACE_INT("launch_lock", 0);
                    mHintManager->EndHint("LAUNCH");
                    ALOGD("LAUNCH OFF");
                }
            }
            ATRACE_END();
            break;
        default:
            break;

    }
    return Void();
}

Return<void> Power::setFeature(Feature /*feature*/, bool /*activate*/)  {
    //Nothing to do
    return Void();
}

Return<void> Power::getPlatformLowPowerStats(getPlatformLowPowerStats_cb _hidl_cb) {
    LOG(ERROR) << "getPlatformLowPowerStats not supported. Use IPowerStats HAL.";
    _hidl_cb({}, Status::SUCCESS);
    return Void();
}

// Methods from ::android::hardware::power::V1_1::IPower follow.
Return<void> Power::getSubsystemLowPowerStats(getSubsystemLowPowerStats_cb _hidl_cb) {
    LOG(ERROR) << "getSubsystemLowPowerStats not supported. Use IPowerStats HAL.";
    _hidl_cb({}, Status::SUCCESS);
    return Void();
}

bool Power::isSupportedGovernor() {
    std::string buf;
    if (android::base::ReadFileToString("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", &buf)) {
        buf = android::base::Trim(buf);
    }
    // Only support EAS 1.2, legacy EAS
    if (buf == "schedutil" || buf == "sched") {
        return true;
    } else {
        LOG(ERROR) << "Governor not supported by powerHAL, skipping";
        return false;
    }
}

Return<void> Power::powerHintAsync(PowerHint_1_0 hint, int32_t data) {
    // just call the normal power hint in this oneway function
    return powerHint(hint, data);
}

// Methods from ::android::hardware::power::V1_2::IPower follow.
Return<void> Power::powerHintAsync_1_2(PowerHint_1_2 hint, int32_t data) {
    if (!isSupportedGovernor() || !mReady) {
        return Void();
    }

    switch(hint) {
        case PowerHint_1_2::AUDIO_LOW_LATENCY:
            ATRACE_BEGIN("audio_low_latency");
            if (data) {
                // Hint until canceled
                ATRACE_INT("audio_low_latency_lock", 1);
                mHintManager->DoHint("AUDIO_LOW_LATENCY");
                ALOGD("AUDIO LOW LATENCY ON");
            } else {
                ATRACE_INT("audio_low_latency_lock", 0);
                mHintManager->EndHint("AUDIO_LOW_LATENCY");
                ALOGD("AUDIO LOW LATENCY OFF");
            }
            ATRACE_END();
            break;
        case PowerHint_1_2::AUDIO_STREAMING:
            ATRACE_BEGIN("audio_streaming");
            if (mVRModeOn || mSustainedPerfModeOn) {
                ALOGV("%s: ignoring due to other active perf hints", __func__);
            } else {
                if (data) {
                    // Hint until canceled
                    ATRACE_INT("audio_streaming_lock", 1);
                    mHintManager->DoHint("AUDIO_STREAMING");
                    ALOGD("AUDIO STREAMING ON");
                } else {
                    ATRACE_INT("audio_streaming_lock", 0);
                    mHintManager->EndHint("AUDIO_STREAMING");
                    ALOGD("AUDIO STREAMING OFF");
                }
            }
            ATRACE_END();
            break;
        case PowerHint_1_2::CAMERA_LAUNCH:
            ATRACE_BEGIN("camera_launch");
            if (data > 0) {
                ATRACE_INT("camera_launch_lock", 1);
                mHintManager->DoHint("CAMERA_LAUNCH", std::chrono::milliseconds(data));
                ALOGD("CAMERA LAUNCH ON: %d MS, LAUNCH ON: 2500 MS", data);
                // boosts 2.5s for launching
                mHintManager->DoHint("LAUNCH", std::chrono::milliseconds(2500));
            } else if (data == 0) {
                ATRACE_INT("camera_launch_lock", 0);
                mHintManager->EndHint("CAMERA_LAUNCH");
                ALOGD("CAMERA LAUNCH OFF");
            } else {
                ALOGE("CAMERA LAUNCH INVALID DATA: %d", data);
            }
            ATRACE_END();
            break;
        case PowerHint_1_2::CAMERA_STREAMING:
            ATRACE_BEGIN("camera_streaming");
            if (data > 0) {
                ATRACE_INT("camera_streaming_lock", 1);
                mHintManager->DoHint("CAMERA_STREAMING", std::chrono::milliseconds(data));
                ALOGD("CAMERA STREAMING ON: %d MS", data);
            } else if (data == 0) {
                ATRACE_INT("camera_streaming_lock", 0);
                mHintManager->EndHint("CAMERA_STREAMING");
                ALOGD("CAMERA STREAMING OFF");
            } else {
                ALOGE("CAMERA STREAMING INVALID DATA: %d", data);
            }
            ATRACE_END();
            break;
        case PowerHint_1_2::CAMERA_SHOT:
            ATRACE_BEGIN("camera_shot");
            if (data > 0) {
                ATRACE_INT("camera_shot_lock", 1);
                mHintManager->DoHint("CAMERA_SHOT", std::chrono::milliseconds(data));
                ALOGD("CAMERA SHOT ON: %d MS", data);
            } else if (data == 0) {
                ATRACE_INT("camera_shot_lock", 0);
                mHintManager->EndHint("CAMERA_SHOT");
                ALOGD("CAMERA SHOT OFF");
            } else {
                ALOGE("CAMERA SHOT INVALID DATA: %d", data);
            }
            ATRACE_END();
            break;
        default:
            return powerHint(static_cast<PowerHint_1_0>(hint), data);
    }
    return Void();
}

constexpr const char* boolToString(bool b) {
    return b ? "true" : "false";
}

Return<void> Power::debug(const hidl_handle& handle, const hidl_vec<hidl_string>&) {
    if (handle != nullptr && handle->numFds >= 1 && mReady) {
        int fd = handle->data[0];

        std::string buf(android::base::StringPrintf("HintManager Running: %s\n"
                                                    "VRMode: %s\n"
                                                    "SustainedPerformanceMode: %s\n"
                                                    "VideoEncodeMode: %s\n",
                                                    boolToString(mHintManager->IsRunning()),
                                                    boolToString(mVRModeOn),
                                                    boolToString(mSustainedPerfModeOn),
                                                    boolToString(mEncoderModeOn)));
        // Dump nodes through libperfmgr
        mHintManager->DumpToFd(fd);
        if (!android::base::WriteStringToFd(buf, fd)) {
            PLOG(ERROR) << "Failed to dump state to fd";
        }
        fsync(fd);
    }
    return Void();
}

}  // namespace implementation
}  // namespace V1_2
}  // namespace power
}  // namespace hardware
}  // namespace android
