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

#include <algorithm>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <android-base/stringprintf.h>
#include "sensors.h"

namespace android {
namespace hardware {
namespace thermal {
namespace V1_1 {
namespace implementation {

std::string Sensors::getSensorPath(const std::string& sensor_name) {
    if (sensor_name_to_data_map_.find(sensor_name) !=
            sensor_name_to_data_map_.end()) {
        return std::get<0>(sensor_name_to_data_map_.at(sensor_name));
    }
    return "";
}

bool Sensors::addSensor(
        const std::string& sensor_name, const std::string& path,
        const float throttling_threshold, const float shutdown_threshold,
        const float vr_threshold, const TemperatureType& type) {
    return sensor_name_to_data_map_.emplace(
        sensor_name, std::make_tuple(
            path, throttling_threshold, shutdown_threshold,
            vr_threshold, type)).second;
}

bool Sensors::readSensorFile(
        const std::string& sensor_name, std::string* data,
        std::string* file_path) const {
    std::string sensor_reading;
    if (sensor_name_to_data_map_.find(sensor_name) ==
            sensor_name_to_data_map_.end()) {
        *data = "";
        *file_path = "";
        return false;
    }

    android::base::ReadFileToString(
        std::get<0>(sensor_name_to_data_map_.at(sensor_name)), &sensor_reading);
    // Strip the newline.
    *data = ::android::base::Trim(sensor_reading);
    *file_path = std::get<0>(sensor_name_to_data_map_.at(sensor_name));
    return true;
}

bool Sensors::readTemperature(
        const std::string& sensor_name, const float mult,
        Temperature* out) const {
    if (sensor_name_to_data_map_.find(sensor_name) ==
            sensor_name_to_data_map_.end()) {
        return false;
    }

    std::string sensor_reading;
    std::string path;
    readSensorFile(sensor_name, &sensor_reading, &path);

    auto sensor = sensor_name_to_data_map_.at(sensor_name);
    out->name = sensor_name;
    out->currentValue = std::stoi(sensor_reading) * mult;
    out->throttlingThreshold = std::get<1>(sensor);
    out->shutdownThreshold = std::get<2>(sensor);
    out->vrThrottlingThreshold = std::get<3>(sensor);
    out->type = std::get<4>(sensor);

    LOG(DEBUG) << android::base::StringPrintf(
        "readTemperature: %s, %d, %s, %g, %g, %g, %g",
        path.c_str(), out->type, out->name.c_str(), out->currentValue,
        out->throttlingThreshold, out->shutdownThreshold,
        out->vrThrottlingThreshold);
    return true;
}

}  // namespace implementation
}  // namespace V1_1
}  // namespace thermal
}  // namespace hardware
}  // namespace android
