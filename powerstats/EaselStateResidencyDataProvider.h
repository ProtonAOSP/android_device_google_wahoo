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
#ifndef DEVICE_GOOGLE_WAHOO_POWERSTATS_EASELSTATERESIDENCYDATAPROVIDER_H
#define DEVICE_GOOGLE_WAHOO_POWERSTATS_EASELSTATERESIDENCYDATAPROVIDER_H

#include <pixelpowerstats/PowerStats.h>

using android::hardware::power::stats::V1_0::PowerEntityStateResidencyResult;
using android::hardware::power::stats::V1_0::PowerEntityStateSpace;
using android::hardware::google::pixel::powerstats::IStateResidencyDataProvider;

namespace android {
namespace device {
namespace google {
namespace wahoo {
namespace powerstats {

class EaselStateResidencyDataProvider : public IStateResidencyDataProvider {
  public:
    EaselStateResidencyDataProvider(uint32_t id);
    ~EaselStateResidencyDataProvider() = default;
    bool getResults(std::unordered_map<uint32_t, PowerEntityStateResidencyResult>
            &results) override;
    std::vector<PowerEntityStateSpace> getStateSpaces() override;

  private:
    std::mutex mLock;
    const uint32_t mPowerEntityId;
    uint64_t mTotalOnSnapshotCount;
    uint64_t mTotalNotOnSnapshotCount;
};

}  // namespace powerstats
}  // namespace wahoo
}  // namespace google
}  // namespace device
}  // namespace android

#endif  // DEVICE_GOOGLE_WAHOO_POWERSTATS_EASELSTATERESIDENCYDATAPROVIDER_H
