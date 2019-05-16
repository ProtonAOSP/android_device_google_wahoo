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
#ifndef ANDROID_HARDWARE_VIBRATOR_UTILS_H
#define ANDROID_HARDWARE_VIBRATOR_UTILS_H

namespace android {
namespace hardware {
namespace vibrator {
namespace utils {

// override for default behavior of printing as a character
inline std::ostream &operator<<(std::ostream &stream, const int8_t value) {
    return stream << +value;
}
// override for default behavior of printing as a character
inline std::ostream &operator<<(std::ostream &stream, const uint8_t value) {
    return stream << +value;
}

}  // namespace utils
}  // namespace vibrator
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_VIBRATOR_UTILS_H
