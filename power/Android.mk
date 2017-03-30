LOCAL_PATH := $(call my-dir)

ifeq ($(call is-vendor-board-platform,QCOM),true)

# HAL module implemenation stored in
# hw/<POWERS_HARDWARE_MODULE_ID>.<ro.hardware>.so
include $(CLEAR_VARS)

LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SHARED_LIBRARIES := liblog libcutils libdl libxml2
LOCAL_SRC_FILES := power.c metadata-parser.c utils.c list.c hint-data.c powerhintparser.c
LOCAL_C_INCLUDES := external/libxml2/include \
                    external/icu/icu4c/source/common

ifeq ($(call is-board-platform-in-list,msm8998), true)
LOCAL_SRC_FILES += power-8998.c
endif

# Enable interaction boost all the time
LOCAL_CFLAGS += -DINTERACTION_BOOST

LOCAL_MODULE := power.$(TARGET_DEVICE)
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -Wno-unused-parameter -Wno-unused-variable
include $(BUILD_SHARED_LIBRARY)

endif
