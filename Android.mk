LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := gsp.$(TARGET_BOARD_PLATFORM)

LOCAL_PROPRIETARY_MODULE := true

LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := liblog libutils libcutils libhardware libui libsync libdl


LOCAL_SHARED_LIBRARIES += libdrm

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../external/kernel-headers \
                    $(LOCAL_PATH)/../libmemion \
                    $(TOP)/system/core/libion/kernel-headers \
                    $(LOCAL_PATH)/../

LOCAL_C_INCLUDES += $(TOP)/external/libdrm \
           $(TOP)/external/libdrm/include/drm

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../hwcomposer/v2.x/

LOCAL_C_INCLUDES += $(GPU_GRALLOC_INCLUDES)
LOCAL_C_INCLUDES += $(TOP)/vendor/sprd/external/drivers/gpu

LOCAL_CFLAGS := -DLOG_TAG=\"GSP\"
LOCAL_CFLAGS += -DTARGET_GPU_PLATFORM=$(TARGET_GPU_PLATFORM)

LOCAL_SRC_FILES := GspPlane.cpp \
                   GspDevice.cpp \
                   GspModule.cpp

LOCAL_SRC_FILES += GspR6P0Plane/GspR6P0Plane.cpp \
                   GspR6P0Plane/GspR6P0PlaneArray.cpp \
                   GspR6P0Plane/GspR6P0PlaneDst.cpp \
                   GspR6P0Plane/GspR6P0PlaneImage.cpp \
                   GspR6P0Plane/GspR6P0PlaneOsd.cpp \
                   GspR7P0Plane/GspR7P0Plane.cpp \
                   GspR7P0Plane/GspR7P0PlaneArray.cpp \
                   GspR7P0Plane/GspR7P0PlaneDst.cpp \
                   GspR7P0Plane/GspR7P0PlaneImage.cpp \
                   GspR7P0Plane/GspR7P0PlaneOsd.cpp \
                   GspR8P0Plane/GspR8P0Plane.cpp \
                   GspR8P0Plane/GspR8P0PlaneArray.cpp \
                   GspR8P0Plane/GspR8P0PlaneDst.cpp \
                   GspR8P0Plane/GspR8P0PlaneImage.cpp \
                   GspR8P0Plane/GspR8P0PlaneOsd.cpp \
                   GspLiteR2P0Plane/GspLiteR2P0Plane.cpp \
                   GspLiteR2P0Plane/GspLiteR2P0PlaneArray.cpp \
                   GspLiteR2P0Plane/GspLiteR2P0PlaneDst.cpp \
                   GspLiteR2P0Plane/GspLiteR2P0PlaneImage.cpp \
                   GspLiteR2P0Plane/GspLiteR2P0PlaneOsd.cpp \
                   GspLiteR3P0Plane/GspLiteR3P0Plane.cpp \
                   GspLiteR3P0Plane/GspLiteR3P0PlaneArray.cpp \
                   GspLiteR3P0Plane/GspLiteR3P0PlaneDst.cpp \
                   GspLiteR3P0Plane/GspLiteR3P0PlaneImage.cpp \
                   GspLiteR3P0Plane/GspLiteR3P0PlaneOsd.cpp

include $(BUILD_SHARED_LIBRARY)

