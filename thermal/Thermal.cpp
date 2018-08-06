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

#include <cerrno>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>

#include "Thermal.h"
#include "thermal-helper.h"

namespace android {
namespace hardware {
namespace thermal {
namespace V1_1 {
namespace implementation {

Thermal::Thermal() : enabled(initThermal()) {}

namespace {

// Saves the IThermalCallback client object registered from the
// framework for sending thermal events to the framework thermal event bus.
sp<IThermalCallback> gThermalCallback;

struct ThermalDeathRecipient : hidl_death_recipient {
    virtual void serviceDied(
        uint64_t cookie __unused, const wp<IBase>& who __unused) {
        gThermalCallback = nullptr;
        LOG(ERROR) << "IThermalCallback HIDL service died";
    }
};

sp<ThermalDeathRecipient> gThermalCallbackDied = nullptr;

} // anonymous namespace

// Methods from ::android::hardware::thermal::V1_0::IThermal follow.
Return<void> Thermal::getTemperatures(getTemperatures_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;
    hidl_vec<Temperature> temperatures;
    temperatures.resize(kTemperatureNum);

    if (!enabled) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Unsupported hardware";
        _hidl_cb(status, temperatures);
        LOG(ERROR) << "ThermalHAL not initialized properly.";
        return Void();
    }

    if (fillTemperatures(&temperatures) != kTemperatureNum) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Error reading thermal sensors.";
    }
    _hidl_cb(status, temperatures);

    for (auto& t : temperatures) {
        LOG(DEBUG) << "getTemperatures "
                   << " Type: " << static_cast<int>(t.type)
                   << " Name: " << t.name
                   << " CurrentValue: " << t.currentValue
                   << " ThrottlingThreshold: " << t.throttlingThreshold
                   << " ShutdownThreshold: " << t.shutdownThreshold
                   << " VrThrottlingThreshold: " << t.vrThrottlingThreshold;
    }

    return Void();
}

Return<void> Thermal::getCpuUsages(getCpuUsages_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;
    hidl_vec<CpuUsage> cpuUsages;
    cpuUsages.resize(kCpuNum);

    if (!enabled) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Unsupported hardware";
        _hidl_cb(status, cpuUsages);
        LOG(ERROR) << "ThermalHAL not initialized properly.";
        return Void();
    }

    ssize_t ret = fillCpuUsages(&cpuUsages);
    if (ret < 0) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = strerror(-ret);
    }

    for (auto& u : cpuUsages) {
        LOG(DEBUG) << "getCpuUsages "
                   << " Name: " << u.name
                   << " Active: " << u.active
                   << " Total: " << u.total
                   << " IsOnline: " << u.isOnline;
    }

    _hidl_cb(status, cpuUsages);
    return Void();
}

Return<void> Thermal::getCoolingDevices(getCoolingDevices_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;
    hidl_vec<CoolingDevice> coolingDevices;

    if (!enabled) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Unsupported hardware";
        _hidl_cb(status, coolingDevices);
        LOG(ERROR) << "ThermalHAL not initialized properly.";
        return Void();
    }

    LOG(DEBUG) << "No Cooling Device";
    _hidl_cb(status, coolingDevices);
    return Void();
}

// Methods from ::android::hardware::thermal::V1_1::IThermal follow.

Return<void> Thermal::registerThermalCallback(
    const sp<IThermalCallback>& callback) {
    gThermalCallback = callback;

    if (gThermalCallback != nullptr) {
        if (gThermalCallbackDied == nullptr)
            gThermalCallbackDied = new ThermalDeathRecipient();

        if (gThermalCallbackDied != nullptr)
            gThermalCallback->linkToDeath(
                gThermalCallbackDied, 0x451F /* cookie, unused */);
        LOG(INFO) << "ThermalCallback registered";
    } else {
        LOG(INFO) << "ThermalCallback unregistered";
    }
    return Void();
}

// Local functions used internally by thermal-engine follow.

std::string Thermal::getSkinSensorType() {
    return getTargetSkinSensorType();
}

void Thermal::notifyThrottling(
    bool isThrottling, const Temperature& temperature) {
    if (gThermalCallback != nullptr) {
        Return<void> ret =
            gThermalCallback->notifyThrottling(isThrottling, temperature);
        if (!ret.isOk()) {
          if (ret.isDeadObject()) {
              gThermalCallback = nullptr;
              LOG(WARNING) << "Dropped throttling event, ThermalCallback died";
          } else {
              LOG(WARNING) <<
                  "Failed to send throttling event to ThermalCallback";
          }
        }
    } else {
        LOG(WARNING) <<
            "Dropped throttling event, no ThermalCallback registered";
    }
}

Return<void> Thermal::debug(const hidl_handle& handle, const hidl_vec<hidl_string>&) {
    if (handle != nullptr && handle->numFds >= 1) {
        int fd = handle->data[0];
        std::ostringstream dump_buf;

        if (!enabled) {
            dump_buf << "ThermalHAL not initialized properly." << std::endl;
        } else {
            hidl_vec<Temperature> temperatures;
            hidl_vec<CpuUsage> cpu_usages;
            cpu_usages.resize(kCpuNum);
            temperatures.resize(kTemperatureNum);

            dump_buf << "getTemperatures:" << std::endl;
            if (fillTemperatures(&temperatures) != kTemperatureNum) {
                dump_buf << "Failed to read thermal sensors." << std::endl;
            } else {
                for (const auto& t : temperatures) {
                    dump_buf << "Name: " << t.name
                             << " Type: " << android::hardware::thermal::V1_0::toString(t.type)
                             << " CurrentValue: " << t.currentValue
                             << " ThrottlingThreshold: " << t.throttlingThreshold
                             << " ShutdownThreshold: " << t.shutdownThreshold
                             << " VrThrottlingThreshold: " << t.vrThrottlingThreshold
                             << std::endl;
                }
            }

            dump_buf << "getCpuUsages:" << std::endl;
            ssize_t ret = fillCpuUsages(&cpu_usages);
            if (ret < 0) {
                dump_buf << "Failed to get CPU usages." << std::endl;
            } else {
                for (const auto& usage : cpu_usages) {
                    dump_buf << "Name: " << usage.name
                             << " Active: " << usage.active
                             << " Total: " << usage.total
                             << " IsOnline: " << usage.isOnline
                             << std::endl;
                }
            }

        }
        std::string buf = dump_buf.str();
        if (!android::base::WriteStringToFd(buf, fd)) {
            PLOG(ERROR) << "Failed to dump state to fd";
        }
        fsync(fd);
    }
    return Void();
}

}  // namespace implementation
}  // namespace V1_1
}  // namespace thermal
}  // namespace hardware
}  // namespace android
