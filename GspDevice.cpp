/*
 * Copyright (C) 2010 The Android Open Source Project
 * Copyright (C) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * Not a Contribution, Apache license notifications and license are retained
 * for attribution purposes only.
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

#include <fcntl.h>
#include <hardware/hardware.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "GspDevice.h"
#include "GspPlane.h"
#include "GspPlaneArray.h"
#include "./GspLiteR2P0Plane/GspLiteR2P0PlaneArray.h"
#include "./GspLiteR3P0Plane/GspLiteR3P0PlaneArray.h"
#include "./GspR6P0Plane/GspR6P0PlaneArray.h"
#include "./GspR7P0Plane/GspR7P0PlaneArray.h"
#include "./GspR8P0Plane/GspR8P0PlaneArray.h"
#include "gsp_cfg.h"
#include "sprd_drm_gsp.h"
#include <xf86drm.h>

#define GSP_DEVICE "/dev/dri/card0"

#define GSP_LITE_R2P0_NAME "LITE_R2P0"
#define GSP_R6P0_NAME "R6P0"
#define GSP_LITE_R3P0_NAME "LITE_R3P0"
#define GSP_R7P0_NAME "R7P0"
#define GSP_R8P0_NAME "R8P0"

#define GSP_SYNC false
#define GSP_ASYNC true

namespace android {

GspDevice::GspDevice() {
  mDevice = -1;
  mOutputWidth = -1;
  mOutputHeight = -1;
}

GspDevice::~GspDevice() {
  if (mDevice > 0)
    close(mDevice);

  if (mPlaneArray)
    delete mPlaneArray;
}

bool GspDevice::init() {
  int fd;
  struct drm_gsp_capability drm_cap;

  fd = open(GSP_DEVICE, O_RDWR, S_IRWXU);

  if (fd < 0) {
    ALOGE("open gsp device failed fd=%d.", fd);
    return false;
  }

  drm_cap.size = sizeof(gsp_capability);
  drm_cap.cap = &mCapability;

  int ret = drmIoctl(fd, DRM_IOCTL_SPRD_GSP_GET_CAPABILITY, &drm_cap);

  if (ret < 0) {
    ALOGE("get gsp device capability failed ret=%d.", ret);
    close(fd);
    return false;
  }

  if (mCapability.magic != GSP_CAPABILITY_MAGIC) {
    ALOGE("gsp device capability has not been initialized");
    close(fd);
    return false;
  }

  if (strcmp(GSP_LITE_R3P0_NAME, mCapability.version) == 0) {
    mPlaneArray = new GspLiteR3P0PlaneArray(GSP_ASYNC);
  } else if (strcmp(GSP_R6P0_NAME, mCapability.version) == 0) {
    mPlaneArray = new GspR6P0PlaneArray(GSP_ASYNC);
  } else if (strcmp(GSP_R7P0_NAME, mCapability.version) == 0) {
    mPlaneArray = new GspR7P0PlaneArray(GSP_ASYNC);
  } else if (strcmp(GSP_R8P0_NAME, mCapability.version) == 0) {
    mPlaneArray = new GspR8P0PlaneArray(GSP_ASYNC);
  } else if (strcmp(GSP_LITE_R2P0_NAME, mCapability.version) == 0) {
    mPlaneArray = new GspLiteR2P0PlaneArray(GSP_ASYNC);
  } else {
    ALOGE("gsp version map fail ! version: %s ", mCapability.version);
    close(fd);
    return false;
  }

  ALOGI("gsp device version: %s, io count: %d, fd: %d", mCapability.version,
        mCapability.io_cnt, fd);

  if (mPlaneArray->init(fd) < 0) {
    ALOGE("plane array initialized failed");
    close(fd);
    return false;
  }

  mDevice = fd;
  return true;
}

int GspDevice::prepare(SprdHWLayer **list, int count, bool *support) {
  int ret = -1;
  *support = true;

  if (list == NULL) {
    ALOGE("no list to prepare");
    return ret;
  }

  ret = mPlaneArray->adapt(list, count, mOutputWidth, mOutputHeight);
  if (ret < 0) {
    *support = false;
  }

  return ret;
}

int GspDevice::set(SprdUtilSource *source, SprdUtilTarget *target) {
  if (source->LayerList == NULL) {
    ALOGE("no layer list for set");
    return -1;
  }

  return mPlaneArray->set(source, target, mOutputWidth, mOutputHeight);
}

int GspDevice::updateOutputSize(uint32_t w, uint32_t h) {
  if (w < 4 || h < 4) {
    ALOGE("update output size params error");
    return -1;
  }

  mOutputWidth = w;
  mOutputHeight = h;

  return 0;
}

} // namespace android
