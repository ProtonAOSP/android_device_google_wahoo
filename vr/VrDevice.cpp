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

#define LOG_TAG "VrDevice"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>

#include "VrDevice.h"

namespace android {
namespace hardware {
namespace vr {
namespace V1_0 {
namespace implementation {

constexpr char kTouchVRModeSysfs[] = "/sys/devices/virtual/input/ftm4_touch/vrmode";

VrDevice::VrDevice() : mVRmode(false) {
    std::string hardware = android::base::GetProperty("ro.hardware", "");
    if (hardware == "taimen") {
        mFtm4Touch = true;
    } else {
        mFtm4Touch = false;
    }
}

Return<void> VrDevice::init() {
    // NOOP
    return Void();
}

Return<void> VrDevice::setVrMode(bool enabled) {
    mVRmode = enabled;
    if (enabled) {
        if (!android::base::SetProperty("sys.qcom.thermalcfg",
                                        "/vendor/etc/thermal-engine-vr.conf")) {
            LOG(ERROR) << "Couldn't set thermal_engine enable property";
            return Void();
        }
    } else {
        if (!android::base::SetProperty("sys.qcom.thermalcfg",
                                        "/vendor/etc/thermal-engine.conf")) {
            LOG(ERROR) << "Couldn't set thermal_engine disable property";
            return Void();
        }
    }
    if (!android::base::SetProperty("ctl.restart", "vendor.thermal-engine")) {
        LOG(ERROR) << "Couldn't set thermal_engine restart property";
    }

    if (mFtm4Touch &&
        !android::base::WriteStringToFile((enabled ? "1" : "0"), kTouchVRModeSysfs)) {
        PLOG(ERROR) <<  "Failed to write to vrmode sysfs node with :" << enabled;
    }

    return Void();
}

Return<void> VrDevice::debug(const hidl_handle& handle, const hidl_vec<hidl_string>&) {
    if (handle != nullptr && handle->numFds >= 1) {
        int fd = handle->data[0];
        std::string buf(android::base::StringPrintf("VRMode: %s\n",
                                                    (mVRmode ? "true" : "false")));
        if (!android::base::WriteStringToFd(buf, fd)) {
            PLOG(ERROR) << "Failed to dump state to fd";
        }
        fsync(fd);
    }
    return Void();
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace vr
}  // namespace hardware
}  // namespace android
