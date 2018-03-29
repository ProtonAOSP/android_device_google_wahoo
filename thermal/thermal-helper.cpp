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

#include <cctype>
#include <cerrno>
#include <cinttypes>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/strings.h>
#include <android-base/stringprintf.h>

#include "sensors.h"
#include "thermal-helper.h"

namespace android {
namespace hardware {
namespace thermal {
namespace V1_1 {
namespace implementation {

constexpr const char kThermalSensorsRoot[] = "/sys/class/thermal";
constexpr char kThermalZoneDirSuffix[] = "thermal_zone";
constexpr char kSensorTypeFileSuffix[] = "type";
constexpr char kTemperatureFileSuffix[] = "temp";
// This is a golden set of thermal sensor names, their types, and their
// multiplier. Used when we read in sensor values. The tuple value stored is
// formatted as such:
// <temperature type, multiplier value for reading temp>
const std::unordered_map<std::string, std::tuple<TemperatureType, float>>
kValidThermalSensorsMap = {
    {"tsens_tz_sensor1", {TemperatureType::CPU, 0.1}},   // CPU0
    {"tsens_tz_sensor2", {TemperatureType::CPU, 0.1}},   // CPU1
    {"tsens_tz_sensor4", {TemperatureType::CPU, 0.1}},   // CPU2
    {"tsens_tz_sensor3", {TemperatureType::CPU, 0.1}},   // CPU3
    {"tsens_tz_sensor7", {TemperatureType::CPU, 0.1}},   // CPU4
    {"tsens_tz_sensor8", {TemperatureType::CPU, 0.1}},   // CPU5
    {"tsens_tz_sensor9", {TemperatureType::CPU, 0.1}},   // CPU6
    {"tsens_tz_sensor10", {TemperatureType::CPU, 0.1}},  // CPU7
    // GPU thermal sensor.
    {"tsens_tz_sensor13", {TemperatureType::GPU, 0.1}},
    // Battery thermal sensor.
    {"battery", {TemperatureType::BATTERY, 0.001}},
    // Skin thermal sensors. We use back_therm for walleye. For taimen we use
    // bd_therm and bd_therm2.
    {"back_therm", {TemperatureType::SKIN, 1.}},
    {"bd_therm", {TemperatureType::SKIN, 1.}},
    {"bd_therm2", {TemperatureType::SKIN, 1.}},
    // USBC thermal sensor.
    {"usb_port_temp", {TemperatureType::UNKNOWN, 0.1}},
};

namespace {

using ::android::hardware::thermal::V1_0::TemperatureType;

static std::string gSkinSensorType;
static unsigned int gSkinThrottlingThreshold;
static unsigned int gSkinShutdownThreshold;
static unsigned int gVrThrottledBelowMin;
Sensors gSensors;

// A map containing hardcoded thresholds per sensor type.  Its not const
// because initThermal() will modify the skin sensor thresholds depending on the
// hardware type. The tuple is formatted as follows:
// <throttling threshold, shutdown threshold, vr threshold>
std::unordered_map<TemperatureType, std::tuple<float, float, float>>
gSensorTypeToThresholdsMap = {
    {TemperatureType::CPU, {kCpuThrottlingThreshold, kCpuShutdownThreshold,
                             kCpuThrottlingThreshold}},
    {TemperatureType::GPU, {NAN, NAN, NAN}},
    {TemperatureType::BATTERY, {NAN, kBatteryShutdownThreshold, NAN}},
    {TemperatureType::SKIN, {NAN, NAN, NAN}},
    {TemperatureType::UNKNOWN, {NAN, NAN, NAN}}
};

bool initializeSensors() {
    auto thermal_zone_dir = std::unique_ptr<DIR, int (*)(DIR*)>(
        opendir(kThermalSensorsRoot), closedir);
    struct dirent* dp;
    size_t num_thermal_zones = 0;
    while ((dp = readdir(thermal_zone_dir.get())) != nullptr) {
        std::string dir_name(dp->d_name);
        if (dir_name.find(kThermalZoneDirSuffix) != std::string::npos) {
            ++num_thermal_zones;
        }
    }

    for (size_t sensor_zone_num = 0; sensor_zone_num < num_thermal_zones;
            ++sensor_zone_num) {
        std::string path = android::base::StringPrintf("%s/%s%zu",
                                                       kThermalSensorsRoot,
                                                       kThermalZoneDirSuffix,
                                                       sensor_zone_num);
        std::string sensor_name;
        if (android::base::ReadFileToString(
                path + "/" + kSensorTypeFileSuffix, &sensor_name)) {
            sensor_name = android::base::Trim(sensor_name);
            if (kValidThermalSensorsMap.find(sensor_name) !=
                kValidThermalSensorsMap.end()) {
                  TemperatureType type = std::get<0>(
                      kValidThermalSensorsMap.at(sensor_name));
                  auto thresholds = gSensorTypeToThresholdsMap.at(type);
                  if (!gSensors.addSensor(
                          sensor_name, path + "/" + kTemperatureFileSuffix,
                          std::get<0>(thresholds), std::get<1>(thresholds),
                          std::get<2>(thresholds), type)) {
                        LOG(ERROR) << "Could not add " << sensor_name
                                   << "to sensors map";
                  }
            }
        }
    }
    return (gSensors.getNumSensors() == kTemperatureNum);
}

}  // namespace

/**
 * Initialization constants based on platform
 *
 * @return true on success or false on error.
 */
bool initThermal() {
    std::string hardware = android::base::GetProperty("ro.hardware", "");
    if (hardware == "walleye") {
        LOG(ERROR) << "Initialization on Walleye";
        gSkinThrottlingThreshold = kWalleyeSkinThrottlingThreshold;
        gSkinShutdownThreshold = kWalleyeSkinShutdownThreshold;
        gVrThrottledBelowMin = kWalleyeVrThrottledBelowMin;
    } else if (hardware == "taimen") {
        std::string rev = android::base::GetProperty("ro.revision", "");
        if (rev == "rev_a" || rev == "rev_b") {
            LOG(ERROR) << "Initialization on Taimen pre revision C";
            gSkinThrottlingThreshold = kTaimenRabSkinThrottlingThreshold;
            gSkinShutdownThreshold = kTaimenRabSkinShutdownThreshold;
            gVrThrottledBelowMin = kTaimenRabVrThrottledBelowMin;
        } else {
            LOG(ERROR) << "Initialization on Taimen revision C and later";
            gSkinThrottlingThreshold = kTaimenRcSkinThrottlingThreshold;
            gSkinShutdownThreshold = kTaimenRcSkinShutdownThreshold;
            gVrThrottledBelowMin = kTaimenRcVrThrottledBelowMin;
        }
    } else {
        LOG(ERROR) << "Unsupported hardware: " << hardware;
        return false;
    }
    gSensorTypeToThresholdsMap[TemperatureType::SKIN] =
        std::make_tuple(gSkinThrottlingThreshold, gSkinShutdownThreshold,
                        gVrThrottledBelowMin);
    return initializeSensors();
}

ssize_t fillTemperatures(hidl_vec<Temperature>* temperatures) {
    temperatures->resize(gSensors.getNumSensors());
    ssize_t current_index = 0;
    for (const auto& name_type_mult_pair : kValidThermalSensorsMap) {
        Temperature temp;
        if (gSensors.readTemperature(name_type_mult_pair.first,
                                     std::get<1>(name_type_mult_pair.second),
                                     &temp)) {
            (*temperatures)[current_index] = temp;
            ++current_index;
        }
    }
    return current_index;
}

ssize_t fillCpuUsages(hidl_vec<CpuUsage> *cpuUsages) {
    int vals, cpu_num, online;
    ssize_t read;
    uint64_t user, nice, system, idle, active, total;
    char *line = NULL;
    size_t len = 0;
    size_t size = 0;
    char file_name[PATH_MAX];
    FILE *file;
    FILE *cpu_file;

    if (cpuUsages == NULL || cpuUsages->size() < kCpuNum ) {
        LOG(ERROR) << "fillCpuUsages: incorrect buffer";
        return -EINVAL;
    }

    file = fopen(kCpuUsageFile, "r");
    if (file == NULL) {
        PLOG(ERROR) << "fillCpuUsages: failed to open file (" << kCpuUsageFile << ")";
        return -errno;
    }

    while ((read = getline(&line, &len, file)) != -1) {
        // Skip non "cpu[0-9]" lines.
        if (strnlen(line, read) < 4 || strncmp(line, "cpu", 3) != 0 || !isdigit(line[3])) {
            free(line);
            line = NULL;
            len = 0;
            continue;
        }

        vals = sscanf(line, "cpu%d %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64, &cpu_num, &user,
                &nice, &system, &idle);

        free(line);
        line = NULL;
        len = 0;

        if (vals != 5 || size == kCpuNum) {
            if (vals != 5) {
                PLOG(ERROR) << "fillCpuUsages: failed to read CPU information from file ("
                            << kCpuUsageFile << ")";
            } else {
                PLOG(ERROR) << "fillCpuUsages: file has incorrect format ("
                            << kCpuUsageFile << ")";
            }
            fclose(file);
            return errno ? -errno : -EIO;
        }

        active = user + nice + system;
        total = active + idle;

        // Read online CPU information.
        snprintf(file_name, PATH_MAX, kCpuOnlineFileFormat, cpu_num);
        cpu_file = fopen(file_name, "r");
        online = 0;
        if (cpu_file == NULL) {
            PLOG(ERROR) << "fillCpuUsages: failed to open file (" << file_name << ")";
            fclose(file);
            return -errno;
        }
        if (1 != fscanf(cpu_file, "%d", &online)) {
            PLOG(ERROR) << "fillCpuUsages: failed to read CPU online information from file ("
                        << file_name << ")";
            fclose(file);
            fclose(cpu_file);
            return errno ? -errno : -EIO;
        }
        fclose(cpu_file);

        (*cpuUsages)[size].name = kCpuLabel[size];
        (*cpuUsages)[size].active = active;
        (*cpuUsages)[size].total = total;
        (*cpuUsages)[size].isOnline = static_cast<bool>(online);

        LOG(DEBUG) << "fillCpuUsages: "<< kCpuLabel[size] << ": "
                   << active << " " << total << " " <<  online;
        size++;
    }
    fclose(file);

    if (size != kCpuNum) {
        PLOG(ERROR) << "fillCpuUsages: file has incorrect format (" << kCpuUsageFile << ")";
        return -EIO;
    }
    return kCpuNum;
}

std::string getTargetSkinSensorType() {
    return gSkinSensorType;
}

}  // namespace implementation
}  // namespace V1_1
}  // namespace thermal
}  // namespace hardware
}  // namespace android
