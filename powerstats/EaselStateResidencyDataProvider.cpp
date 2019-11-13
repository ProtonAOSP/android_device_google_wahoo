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

#define LOG_TAG "easelstateresidency"

#include <android-base/logging.h>
#include <fstream>
#include "EaselStateResidencyDataProvider.h"

namespace android {
namespace device {
namespace google {
namespace wahoo {
namespace powerstats {

const uint32_t EASEL_SYNTHETIC_SLEEP_ID = 0;

EaselStateResidencyDataProvider::EaselStateResidencyDataProvider(uint32_t id) :
    mPowerEntityId(id), mTotalOnSnapshotCount(0), mTotalNotOnSnapshotCount(0) {}

bool EaselStateResidencyDataProvider::getResults(
    std::unordered_map<uint32_t, PowerEntityStateResidencyResult> &results) {
    const std::string path = "/sys/devices/virtual/misc/mnh_sm/state";

    enum easel_state {
        EASEL_OFF = 0,
        EASEL_ON,
        EASEL_SUSPENDED,
        NUM_EASEL_STATES
    };

    // Since we are storing stats locally but can have multiple parallel
    // callers, locking is required to ensure stats are not corrupted.
    std::lock_guard<std::mutex> lock(mLock);

    std::ifstream inFile(path, std::ifstream::in);
    if (!inFile.is_open()) {
        PLOG(ERROR) << __func__ << ":Failed to open file " << path;
        return false;
    }

    unsigned long currentState;
    if(!(inFile >> currentState) || currentState >= NUM_EASEL_STATES) {
        PLOG(ERROR) << __func__ << ":Failed to parse " << path;
        return false;
    }

    // Update statistics for synthetic sleep state.  We combine OFF and
    // SUSPENDED to act as a composite "not on" state so the numbers will behave
    // like a real sleep state.
    if ((currentState == EASEL_OFF) || (currentState == EASEL_SUSPENDED)) {
        mTotalNotOnSnapshotCount++;
    } else {
        mTotalOnSnapshotCount++;
    }

    // Update statistics for synthetic sleep state, where
    // totalStateEntryCount = cumulative count of Easel state0 and state2 
    // (as seen by power.stats HAL)
    // totalTimeInStateMs = cumulative count of Easel state1 (as seen by
    //   power.stats HAL)
    PowerEntityStateResidencyResult result = {
        .powerEntityId = mPowerEntityId,
        .stateResidencyData = {{.powerEntityStateId = EASEL_SYNTHETIC_SLEEP_ID,
                                .totalTimeInStateMs = mTotalNotOnSnapshotCount,
                                .totalStateEntryCount = mTotalOnSnapshotCount,
                                .lastEntryTimestampMs = 0}}
    };

    results.emplace(std::make_pair(mPowerEntityId, result));
    return true;
}


std::vector<PowerEntityStateSpace> EaselStateResidencyDataProvider::getStateSpaces() {
    return {
        {.powerEntityId = mPowerEntityId,
            .states = {
                {
                 .powerEntityStateId = EASEL_SYNTHETIC_SLEEP_ID,
                 .powerEntityStateName = "SyntheticSleep"
                }
            }
        }
    };
}

}  // namespace powerstats
}  // namespace wahoo
}  // namespace google
}  // namespace device
}  // namespace android