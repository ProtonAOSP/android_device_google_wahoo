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
#include <assert.h>
#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>

#include <cutils/uevent.h>
#include <sys/epoll.h>
#include <utils/Errors.h>
#include <utils/StrongPointer.h>

#include "Usb.h"

namespace android {
namespace hardware {
namespace usb {
namespace V1_0 {
namespace implementation {

// Set by the signal handler to destroy the thread
volatile bool destroyThread;

int32_t readFile(std::string filename, std::string *contents) {
  FILE *fp;
  ssize_t read = 0;
  char *line = NULL;
  size_t len = 0;

  fp = fopen(filename.c_str(), "r");
  if (fp != NULL) {
    if ((read = getline(&line, &len, fp)) != -1) {
      char *pos;
      if ((pos = strchr(line, '\n')) != NULL) *pos = '\0';
      *contents = line;
    }
    free(line);
    fclose(fp);
    return 0;
  } else {
    ALOGE("fopen failed");
  }

  return -1;
}

std::string appendRoleNodeHelper(const std::string portName,
                                 PortRoleType type) {
  std::string node("/sys/class/typec/" + portName);

  switch (type) {
    case PortRoleType::DATA_ROLE:
      return node + "/current_data_role";
    case PortRoleType::POWER_ROLE:
      return node + "/current_power_role";
    default:
      return node + "/mode";
  }
}

std::string convertRoletoString(PortRole role) {
  if (role.type == PortRoleType::POWER_ROLE) {
    if (role.role == static_cast<uint32_t>(PortPowerRole::SOURCE))
      return "source";
    else if (role.role == static_cast<uint32_t>(PortPowerRole::SINK))
      return "sink";
  } else if (role.type == PortRoleType::DATA_ROLE) {
    if (role.role == static_cast<uint32_t>(PortDataRole::HOST)) return "host";
    if (role.role == static_cast<uint32_t>(PortDataRole::DEVICE))
      return "device";
  } else if (role.type == PortRoleType::MODE) {
    if (role.role == static_cast<uint32_t>(PortMode::UFP)) return "ufp";
    if (role.role == static_cast<uint32_t>(PortMode::DFP)) return "dfp";
  }
  return "none";
}

void extractRole(std::string *roleName) {
  std::size_t first, last;

  first = roleName->find("[");
  last = roleName->find("]");

  if (first != std::string::npos && last != std::string::npos) {
    *roleName = roleName->substr(first + 1, last - first - 1);
  }
}

Return<void> Usb::switchRole(const hidl_string &portName,
                             const PortRole &newRole) {
  std::string filename =
      appendRoleNodeHelper(std::string(portName.c_str()), newRole.type);
  std::string written;
  FILE *fp;

  ALOGI("filename write: %s role:%s", filename.c_str(),
        convertRoletoString(newRole).c_str());

  fp = fopen(filename.c_str(), "w");
  if (fp != NULL) {
    int ret = fputs(convertRoletoString(newRole).c_str(), fp);
    fclose(fp);
    if ((ret != EOF) && !readFile(filename, &written)) {
      extractRole(&written);
      ALOGI("written: %s", written.c_str());
      if (written == convertRoletoString(newRole)) {
        pthread_mutex_lock(&mLock);
        if (mCallback != NULL) {
          Return<void> ret = mCallback->notifyRoleSwitchStatus(
              portName, newRole, Status::SUCCESS);
          if (!ret.isOk())
            ALOGE("RoleSwitch transaction error %s", ret.description().c_str());
        } else {
          ALOGE("Not notifying the userspace. Callback is not set");
        }
        pthread_mutex_unlock(&mLock);
        return Void();
      } else {
        ALOGE("Role switch failed");
      }
    } else {
      ALOGE("failed to update the new role");
    }
  } else {
    ALOGE("fopen failed");
  }

  pthread_mutex_lock(&mLock);
  if (mCallback != NULL) {
    Return<void> ret =
        mCallback->notifyRoleSwitchStatus(portName, newRole, Status::ERROR);
    if (!ret.isOk())
      ALOGE("RoleSwitchStatus error %s", ret.description().c_str());
  } else {
    ALOGE("Not notifying the userspace. Callback is not set");
  }
  pthread_mutex_unlock(&mLock);

  return Void();
}

Status getCurrentRoleHelper(std::string portName, bool connected,
                            PortRoleType type, uint32_t *currentRole) {
  std::string filename;
  std::string roleName;

  // Mode

  if (type == PortRoleType::POWER_ROLE) {
    filename = "/sys/class/typec/" + portName + "/current_power_role";
    *currentRole = static_cast<uint32_t>(PortPowerRole::NONE);
  } else if (type == PortRoleType::DATA_ROLE) {
    filename = "/sys/class/typec/" + portName + "/current_data_role";
    *currentRole = static_cast<uint32_t>(PortDataRole::NONE);
  } else if (type == PortRoleType::MODE) {
    filename = "/sys/class/typec/" + portName + "/current_data_role";
    *currentRole = static_cast<uint32_t>(PortMode::NONE);
  } else {
    return Status::ERROR;
  }

  if (!connected) return Status::SUCCESS;

  if (readFile(filename, &roleName)) {
    ALOGE("getCurrentRole: Failed to open filesystem node: %s",
          filename.c_str());
    return Status::ERROR;
  }

  extractRole(&roleName);

  if (roleName == "source") {
    *currentRole = static_cast<uint32_t>(PortPowerRole::SOURCE);
  } else if (roleName == "sink") {
    *currentRole = static_cast<uint32_t>(PortPowerRole::SINK);
  } else if (roleName == "host") {
    if (type == PortRoleType::DATA_ROLE)
      *currentRole = static_cast<uint32_t>(PortDataRole::HOST);
    else
      *currentRole = static_cast<uint32_t>(PortMode::DFP);
  } else if (roleName == "device") {
    if (type == PortRoleType::DATA_ROLE)
      *currentRole = static_cast<uint32_t>(PortDataRole::DEVICE);
    else
      *currentRole = static_cast<uint32_t>(PortMode::UFP);
  } else if (roleName != "none") {
    /* case for none has already been addressed.
     * so we check if the role isnt none.
     */
    return Status::UNRECOGNIZED_ROLE;
  }

  return Status::SUCCESS;
}

Status getTypeCPortNamesHelper(std::unordered_map<std::string, bool> *names) {
  DIR *dp;

  dp = opendir("/sys/class/typec");
  if (dp != NULL) {
    int32_t ports = 0;
    int32_t current = 0;
    struct dirent *ep;

    while ((ep = readdir(dp))) {
      if (ep->d_type == DT_LNK) {
        if (std::string::npos == std::string(ep->d_name).find("-partner")) {
          std::unordered_map<std::string, bool>::const_iterator portName =
              names->find(ep->d_name);
          if (portName == names->end()) {
            names->insert({ep->d_name, false});
          }
        } else {
          (*names)[std::strtok(ep->d_name, "-")] = true;
        }
      }
    }
    closedir(dp);
    return Status::SUCCESS;
  }

  ALOGE("Failed to open /sys/class/typec");
  return Status::ERROR;
}

bool canSwitchRoleHelper(const std::string portName, PortRoleType /*type*/) {
  std::string filename =
      "/sys/class/typec/" + portName + "-partner/supports_usb_power_delivery";
  std::string supportsPD;

  if (!readFile(filename, &supportsPD)) {
    if (supportsPD == "1") {
      return true;
    }
  }

  return false;
}

Status getPortStatusHelper(hidl_vec<PortStatus> *currentPortStatus) {
  std::unordered_map<std::string, bool> names;
  Status result = getTypeCPortNamesHelper(&names);
  int i = -1;

  if (result == Status::SUCCESS) {
    currentPortStatus->resize(names.size());
    for (std::pair<std::string, bool> port : names) {
      i++;
      ALOGI("%s", port.first.c_str());
      (*currentPortStatus)[i].portName = port.first;

      uint32_t currentRole;
      if (getCurrentRoleHelper(port.first, port.second,
                               PortRoleType::POWER_ROLE,
                               &currentRole) == Status::SUCCESS) {
        (*currentPortStatus)[i].currentPowerRole =
            static_cast<PortPowerRole>(currentRole);
      } else {
        ALOGE("Error while retreiving portNames");
        goto done;
      }

      if (getCurrentRoleHelper(port.first, port.second, PortRoleType::DATA_ROLE,
                               &currentRole) == Status::SUCCESS) {
        (*currentPortStatus)[i].currentDataRole =
            static_cast<PortDataRole>(currentRole);
      } else {
        ALOGE("Error while retreiving current port role");
        goto done;
      }

      if (getCurrentRoleHelper(port.first, port.second, PortRoleType::MODE,
                               &currentRole) == Status::SUCCESS) {
        (*currentPortStatus)[i].currentMode =
            static_cast<PortMode>(currentRole);
      } else {
        ALOGE("Error while retreiving current data role");
        goto done;
      }

      (*currentPortStatus)[i].canChangeMode = false;
      (*currentPortStatus)[i].canChangeDataRole =
          port.second ? canSwitchRoleHelper(port.first, PortRoleType::DATA_ROLE)
                      : false;
      (*currentPortStatus)[i].canChangePowerRole =
          port.second
              ? canSwitchRoleHelper(port.first, PortRoleType::POWER_ROLE)
              : false;

      ALOGI("connected:%d canChangeMode: %d canChagedata: %d canChangePower:%d",
            port.second, (*currentPortStatus)[i].canChangeMode,
            (*currentPortStatus)[i].canChangeDataRole,
            (*currentPortStatus)[i].canChangePowerRole);

      (*currentPortStatus)[i].supportedModes = PortMode::DRP;
    }
    return Status::SUCCESS;
  }
done:
  return Status::ERROR;
}

Return<void> Usb::queryPortStatus() {
  hidl_vec<PortStatus> currentPortStatus;
  Status status;

  status = getPortStatusHelper(&currentPortStatus);

  pthread_mutex_lock(&mLock);
  if (mCallback != NULL) {
    Return<void> ret =
        mCallback->notifyPortStatusChange(currentPortStatus, status);
    if (!ret.isOk())
      ALOGE("queryPortStatus error %s", ret.description().c_str());
  } else {
    ALOGI("Notifying userspace skipped. Callback is NULL");
  }
  pthread_mutex_unlock(&mLock);

  return Void();
}
struct data {
  int uevent_fd;
  android::hardware::usb::V1_0::implementation::Usb *usb;
};

static void uevent_event(uint32_t /*epevents*/, struct data *payload) {
  char msg[UEVENT_MSG_LEN + 2];
  char *cp;
  int n;

  n = uevent_kernel_multicast_recv(payload->uevent_fd, msg, UEVENT_MSG_LEN);
  if (n <= 0) return;
  if (n >= UEVENT_MSG_LEN) /* overflow -- discard */
    return;

  msg[n] = '\0';
  msg[n + 1] = '\0';
  cp = msg;

  while (*cp) {
    if (!strncmp(cp, "DEVTYPE=typec_", strlen("DEVTYPE=typec_"))) {
      ALOGI("uevent received %s", cp);
      pthread_mutex_lock(&payload->usb->mLock);
      if (payload->usb->mCallback != NULL) {
        hidl_vec<PortStatus> currentPortStatus;
        Status status = getPortStatusHelper(&currentPortStatus);
        Return<void> ret = payload->usb->mCallback->notifyPortStatusChange(
            currentPortStatus, status);
        if (!ret.isOk()) ALOGE("error %s", ret.description().c_str());
      } else {
        ALOGI("Notifying userspace skipped. Callback is NULL");
      }
      pthread_mutex_unlock(&payload->usb->mLock);
      break;
    }
    /* advance to after the next \0 */
    while (*cp++) {}
  }
}

void *work(void *param) {
  int epoll_fd, uevent_fd;
  struct epoll_event ev;
  int nevents = 0;
  struct data payload;

  ALOGE("creating thread");

  uevent_fd = uevent_open_socket(64 * 1024, true);

  if (uevent_fd < 0) {
    ALOGE("uevent_init: uevent_open_socket failed\n");
    return NULL;
  }

  payload.uevent_fd = uevent_fd;
  payload.usb = (android::hardware::usb::V1_0::implementation::Usb *)param;

  fcntl(uevent_fd, F_SETFL, O_NONBLOCK);

  ev.events = EPOLLIN;
  ev.data.ptr = (void *)uevent_event;

  epoll_fd = epoll_create(64);
  if (epoll_fd == -1) {
    ALOGE("epoll_create failed; errno=%d", errno);
    goto error;
  }

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, uevent_fd, &ev) == -1) {
    ALOGE("epoll_ctl failed; errno=%d", errno);
    goto error;
  }

  while (!destroyThread) {
    struct epoll_event events[64];

    nevents = epoll_wait(epoll_fd, events, 64, -1);
    if (nevents == -1) {
      if (errno == EINTR) continue;
      ALOGE("usb epoll_wait failed; errno=%d", errno);
      break;
    }

    for (int n = 0; n < nevents; ++n) {
      if (events[n].data.ptr)
        (*(void (*)(int, struct data *payload))events[n].data.ptr)(
            events[n].events, &payload);
    }
  }

  ALOGI("exiting worker thread");
error:
  close(uevent_fd);

  if (epoll_fd >= 0) close(epoll_fd);

  return NULL;
}

void sighandler(int sig) {
  if (sig == SIGUSR1) {
    destroyThread = true;
    ALOGI("destroy set");
    return;
  }
  signal(SIGUSR1, sighandler);
}

Return<void> Usb::setCallback(const sp<IUsbCallback> &callback) {
  pthread_mutex_lock(&mLock);
  /*
   * When both the old callback and new callback values are NULL,
   * there is no need to spin off the worker thread.
   * When both the values are not NULL, we would already have a
   * worker thread running, so updating the callback object would
   * be suffice.
   */
  if ((mCallback == NULL && callback == NULL) ||
      (mCallback != NULL && callback != NULL)) {
    mCallback = callback;
    pthread_mutex_unlock(&mLock);
    return Void();
  }

  mCallback = callback;
  ALOGI("registering callback");

  // Kill the worker thread if the new callback is NULL.
  if (mCallback == NULL) {
    pthread_mutex_unlock(&mLock);
    if (!pthread_kill(mPoll, SIGUSR1)) {
      pthread_join(mPoll, NULL);
      ALOGI("pthread destroyed");
    }
    return Void();
  }

  destroyThread = false;
  signal(SIGUSR1, sighandler);

  /*
   * Create a background thread if the old callback value is NULL
   * and being updated with a new value.
   */
  if (pthread_create(&mPoll, NULL, work, this)) {
    ALOGE("pthread creation failed %d", errno);
    mCallback = NULL;
  }

  pthread_mutex_unlock(&mLock);
  return Void();
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace usb
}  // namespace hardware
}  // namespace android
