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

#define LOG_TAG "android.hardware.vibrator@1.2-service.drv2624"

#include "Hardware.h"

#include <log/log.h>

#include <iostream>

#include "utils.h"

namespace android {
namespace hardware {
namespace vibrator {
namespace V1_2 {
namespace implementation {

template <typename T>
static void fileFromEnv(const char *env, T *outStream, std::string *outName = nullptr) {
    auto file = std::getenv(env);
    auto mode = std::is_base_of_v<std::ostream, T> ? std::ios_base::out : std::ios_base::in;

    if (file == nullptr) {
        ALOGE("Failed get env %s", env);
        return;
    }

    if (outName != nullptr) {
        *outName = std::string(file);
    }

    // Force 'in' mode to prevent file creation
    outStream->open(file, mode | std::ios_base::in);
    if (!*outStream) {
        ALOGE("Failed to open %s:%s (%d): %s", env, file, errno, strerror(errno));
    }
}

static auto pathsFromEnv(const char *env) {
    std::map<std::string, std::ifstream> ret;
    auto value = std::getenv(env);

    if (value == nullptr) {
        return ret;
    }

    std::istringstream paths{value};
    std::string path;

    while (paths >> path) {
        ret[path].open(path);
    }

    return ret;
}

static std::string trim(const std::string &str, const std::string &whitespace = " \t") {
    const auto str_begin = str.find_first_not_of(whitespace);
    if (str_begin == std::string::npos) {
        return "";
    }

    const auto str_end = str.find_last_not_of(whitespace);
    const auto str_range = str_end - str_begin + 1;

    return str.substr(str_begin, str_range);
}

static void unpack(std::istream &stream, uint32_t *value) {
    stream >> *value;
}

static void unpack(std::istream &stream, std::string *value) {
    *value = std::string(std::istreambuf_iterator(stream), {});
    stream.setstate(std::istream::eofbit);
}

HwApi::HwApi() {
    fileFromEnv("AUTOCAL_FILEPATH", &mAutocal, &mNames[&mAutocal]);
    fileFromEnv("OL_LRA_PERIOD_FILEPATH", &mOlLraPeriod, &mNames[&mOlLraPeriod]);
    fileFromEnv("ACTIVATE_PATH", &mActivate, &mNames[&mActivate]);
    fileFromEnv("DURATION_PATH", &mDuration, &mNames[&mDuration]);
    fileFromEnv("STATE_PATH", &mState, &mNames[&mState]);
    fileFromEnv("RTP_INPUT_PATH", &mRtpInput, &mNames[&mRtpInput]);
    fileFromEnv("MODE_PATH", &mMode, &mNames[&mMode]);
    fileFromEnv("SEQUENCER_PATH", &mSequencer, &mNames[&mSequencer]);
    fileFromEnv("SCALE_PATH", &mScale, &mNames[&mScale]);
    fileFromEnv("CTRL_LOOP_PATH", &mCtrlLoop, &mNames[&mCtrlLoop]);
    fileFromEnv("LP_TRIGGER_PATH", &mLpTrigger, &mNames[&mLpTrigger]);
}

std::unique_ptr<HwApi> HwApi::Create() {
    auto hwapi = std::unique_ptr<HwApi>(new HwApi());

    // the following streams are required
    if (!hwapi->mActivate.is_open() || !hwapi->mDuration.is_open() || !hwapi->mState.is_open()) {
        return nullptr;
    }

    return hwapi;
}

template <typename T>
bool HwApi::has(const T &stream) {
    return !!stream;
}

template <typename T, typename U>
bool HwApi::get(T *value, U *stream) {
    bool ret;
    stream->seekg(0);
    *stream >> *value;
    if (!(ret = !!*stream)) {
        ALOGE("Failed to read %s (%d): %s", mNames[stream].c_str(), errno, strerror(errno));
    }
    stream->clear();
    return ret;
}

template <typename T, typename U>
bool HwApi::set(const T &value, U *stream) {
    using utils::operator<<;
    bool ret;
    *stream << value << std::endl;
    if (!(ret = !!*stream)) {
        ALOGE("Failed to write %s (%d): %s", mNames[stream].c_str(), errno, strerror(errno));
        stream->clear();
    }
    return ret;
}

void HwApi::debug(int fd) {
    dprintf(fd, "Kernel:\n");

    for (auto &entry : pathsFromEnv("HWAPI_DEBUG_PATHS")) {
        auto &path = entry.first;
        auto &stream = entry.second;
        std::string line;

        dprintf(fd, "  %s:\n", path.c_str());
        while (std::getline(stream, line)) {
            dprintf(fd, "    %s\n", line.c_str());
        }
    }
}

HwCal::HwCal() {
    std::ifstream calfile;
    auto propertyPrefix = std::getenv("PROPERTY_PREFIX");

    if (propertyPrefix != NULL) {
        mPropertyPrefix = std::string(propertyPrefix);
    } else {
        ALOGE("Failed get property prefix!");
    }

    fileFromEnv("CALIBRATION_FILEPATH", &calfile);

    for (std::string line; std::getline(calfile, line);) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream is_line(line);
        std::string key, value;
        if (std::getline(is_line, key, ':') && std::getline(is_line, value)) {
            mCalData[trim(key)] = trim(value);
        }
    }
}

template <typename T>
bool HwCal::get(const char *key, T *value) {
    auto it = mCalData.find(key);
    if (it == mCalData.end()) {
        ALOGE("Missing %s config!", key);
        return false;
    }
    std::stringstream stream{it->second};
    unpack(stream, value);
    if (!stream || !stream.eof()) {
        ALOGE("Invalid %s config!", key);
        return false;
    }
    return true;
}

void HwCal::debug(int fd) {
    std::ifstream stream;
    std::string path;
    std::string line;
    struct context {
        HwCal *obj;
        int fd;
    } context{this, fd};

    dprintf(fd, "Properties:\n");

    property_list(
            [](const char *key, const char *value, void *cookie) {
                struct context *context = static_cast<struct context *>(cookie);
                HwCal *obj = context->obj;
                int fd = context->fd;
                const std::string expect{obj->mPropertyPrefix};
                const std::string actual{key, std::min(strlen(key), expect.size())};
                if (actual == expect) {
                    dprintf(fd, "  %s:\n", key);
                    dprintf(fd, "    %s\n", value);
                }
            },
            &context);

    dprintf(fd, "\n");

    dprintf(fd, "Persist:\n");

    fileFromEnv("CALIBRATION_FILEPATH", &stream, &path);

    dprintf(fd, "  %s:\n", path.c_str());
    while (std::getline(stream, line)) {
        dprintf(fd, "    %s\n", line.c_str());
    }
}

}  // namespace implementation
}  // namespace V1_2
}  // namespace vibrator
}  // namespace hardware
}  // namespace android
