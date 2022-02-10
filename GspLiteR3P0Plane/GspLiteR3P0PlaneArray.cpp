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

#include "GspLiteR3P0PlaneArray.h"
#include <algorithm>
#include "GspLiteR3P0PlaneImage.h"
#include "GspLiteR3P0PlaneOsd.h"
#include "GspLiteR3P0PlaneDst.h"
#include "../GspDevice.h"
#include "dump.h"
#include "SprdUtil.h"
#include "gsp_lite_r3p0_cfg.h"
#include "sprd_drm_gsp.h"
#include <xf86drm.h>

#define GSP_LITE_R3P0_PLANE_TRIGGER                                            \
  GSP_IO_TRIGGER(mAsync, (mConfigIndex + 1), mSplitSupport,                    \
                 struct gsp_lite_r3p0_cfg_user)

#define GSP_LOG_PATH "/data/gsp.cfg"

namespace android {

GspLiteR3P0PlaneArray::GspLiteR3P0PlaneArray(bool async) {
  mAsync = async;
  mSplitSupport = 0;
  mDebugFlag = 0;
  mInputRotMode = false;
}

GspLiteR3P0PlaneArray::~GspLiteR3P0PlaneArray() {
  for (int i = 0; i < LITE_R3P0_IMGL_NUM; i++) {
    if (mImagePlane[i])
      delete mImagePlane[i];
  }
  for (int i = 0; i < LITE_R3P0_OSDL_NUM; i++) {
    if (mOsdPlane[i])
      delete mOsdPlane[i];
  }

  if (mDstPlane)
    delete mDstPlane;

  if (mConfigs)
    delete mConfigs;
}
int GspLiteR3P0PlaneArray::init(int fd) {
  int ret = -1;
  struct drm_gsp_capability drm_cap;

  drm_cap.size = sizeof(gsp_lite_r3p0_capability);
  drm_cap.cap = &mCapability;

  ret = drmIoctl(fd, DRM_IOCTL_SPRD_GSP_GET_CAPABILITY, &drm_cap);
  if (ret < 0) {
    ALOGE("gsplite_r3p0plane array get capability failed, ret=%d.\n", ret);
    return ret;
  }

  if (mCapability.common.max_layer < 1) {
    ALOGE("max layer params error");
    return -1;
  }

  GspRangeSize range(mCapability.common.crop_max, mCapability.common.crop_min,
                     mCapability.common.out_max, mCapability.common.out_min);

  for (int i = 0; i < LITE_R3P0_IMGL_NUM; i++) {
    mImagePlane[i] = new GspLiteR3P0PlaneImage(
        mAsync, mCapability.yuv_xywh_even, mCapability.scale_updown_sametime,
        mCapability.max_video_size, mCapability.scale_range_up,
        mCapability.scale_range_down, range);
    if (mImagePlane[i] == NULL) {
      ALOGE("LiteR3P0 ImgPlane alloc fail!");
      return -1;
    }
  }

  for (int i = 0; i < LITE_R3P0_OSDL_NUM; i++) {
    mOsdPlane[i] = new GspLiteR3P0PlaneOsd(mAsync, range);
    if (mOsdPlane[i] == NULL) {
      ALOGE("LiteR3P0 OsdPlane[%d] alloc fail!", i);
      return -1;
    }
  }

  mDstPlane = new GspLiteR3P0PlaneDst(mAsync, mCapability.max_gspmmu_size,
                                      mCapability.max_gsp_bandwidth);
  if (mDstPlane == NULL) {
    ALOGE("LiteR3P0 DstPlane alloc fail!");
    return -1;
  }

  mPlaneNum = LITE_R3P0_IMGL_NUM + LITE_R3P0_OSDL_NUM;

  mConfigs = new gsp_lite_r3p0_cfg_user[mCapability.common.io_cnt];
  if (mConfigs == NULL) {
    ALOGE("LiteR3P0 mConfigs alloc fail, io_cnt:%d !",
          mCapability.common.io_cnt);
    return -1;
  }

  drm_mConfigs = (struct drm_gsp_lite_r3p0_cfg_user *)malloc(
      sizeof(drm_gsp_lite_r3p0_cfg_user));
  drm_mConfigs->async = mAsync;
  drm_mConfigs->config = mConfigs;
  drm_mConfigs->size = sizeof(gsp_lite_r3p0_cfg_user);
  drm_mConfigs->num = 1;
  drm_mConfigs->split = false;

  mDevice = fd;
  reset();

  return 0;
}

void GspLiteR3P0PlaneArray::reset() {
  mAttachedNum = 0;
  mConfigIndex = 0;
  mInputRotMode = false;

  for (int i = 0; i < LITE_R3P0_IMGL_NUM; i++) {
    mImagePlane[i]->reset(mDebugFlag);
  }
  for (int i = 0; i < LITE_R3P0_OSDL_NUM; i++) {
    mOsdPlane[i]->reset(mDebugFlag);
  }
  mDstPlane->reset(mDebugFlag);
  memset(&mConfigs->misc, 0, sizeof(struct gsp_lite_r3p0_misc_cfg_user));
}

void GspLiteR3P0PlaneArray::queryDebugFlag(int *debugFlag) {
  char value[PROPERTY_VALUE_MAX];
  static int openFileFlag = 0;

  if (debugFlag == NULL) {
    ALOGE("queryDebugFlag, input parameter is NULL");
    return;
  }

  property_get("debug.gsp.info", value, "0");

  if (atoi(value) == 1) {
    *debugFlag = 1;
  }
  if (access(GSP_LOG_PATH, R_OK) != 0) {
    return;
  }

  FILE *fp = NULL;
  char *pch;
  char cfg[100];

  fp = fopen(GSP_LOG_PATH, "r");
  if (fp != NULL) {
    if (openFileFlag == 0) {
      int ret;
      memset(cfg, '\0', 100);
      ret = fread(cfg, 1, 99, fp);
      if (ret < 1) {
        ALOGI_IF(mDebugFlag, "fread return size is wrong %d", ret);
      }
      cfg[sizeof(cfg) - 1] = 0;
      pch = strstr(cfg, "enable");
      if (pch != NULL) {
        *debugFlag = 1;
        openFileFlag = 1;
      }
    } else {
      *debugFlag = 1;
    }
    fclose(fp);
  }
}

bool GspLiteR3P0PlaneArray::checkLayerCount(int count) {
  return count > mPlaneNum ? false : true;
}

bool GspLiteR3P0PlaneArray::isExhausted() {
  return mAttachedNum == mMaxAttachedNum ? true : false;
}

bool GspLiteR3P0PlaneArray::isSupportBufferType(enum gsp_addr_type type) {
  if (type == GSP_ADDR_TYPE_IOVIRTUAL && mAddrType == GSP_ADDR_TYPE_PHYSICAL)
    return false;
  else
    return true;
}

bool GspLiteR3P0PlaneArray::isSupportSplit() { return mSplitSupport; }

void GspLiteR3P0PlaneArray::planeAttached() {
  mAttachedNum++;
  ALOGI_IF(mDebugFlag, "has attached total %d plane", mAttachedNum);
}

bool GspLiteR3P0PlaneArray::isImagePlaneAttached(int ImgPlaneIndex) {
  return mImagePlane[ImgPlaneIndex]->isAttached();
}

bool GspLiteR3P0PlaneArray::isOsdPlaneAttached(int OsdPlaneIndex) {
  return mOsdPlane[OsdPlaneIndex]->isAttached();
}

bool GspLiteR3P0PlaneArray::imagePlaneAdapt(SprdHWLayer *layer,
                                            int ImgPlaneIndex, int LayerIndex,
                                            bool InputRotMode) {
  if (isImagePlaneAttached(ImgPlaneIndex) == true)
    return false;

  return mImagePlane[ImgPlaneIndex]->adapt(layer, LayerIndex, InputRotMode);
}

bool GspLiteR3P0PlaneArray::osdPlaneAdapt(SprdHWLayer *layer, int OsdPlaneIndex,
                                          int LayerIndex, bool InputRotMode) {
  if (isOsdPlaneAttached(OsdPlaneIndex) == true)
    return false;

  return mOsdPlane[OsdPlaneIndex]->adapt(layer, LayerIndex, InputRotMode);
}

bool GspLiteR3P0PlaneArray::dstPlaneAdapt(SprdHWLayer **list, int count) {
  return mDstPlane->adapt(list, count);
}

bool GspLiteR3P0PlaneArray::needImgLayer(SprdHWLayer *layer) {
  native_handle_t *handle = NULL;
  struct sprdRect *srcRect = layer->getSprdSRCRect();
  struct sprdRect *dstRect = layer->getSprdFBRect();
  handle = layer->getBufferHandle();
  uint32_t transform = layer->getTransform();

  if ((ADP_FORMAT(handle) == HAL_PIXEL_FORMAT_YCbCr_420_SP) ||
      (ADP_FORMAT(handle) == HAL_PIXEL_FORMAT_YCrCb_420_SP) ||
      (ADP_FORMAT(handle) == HAL_PIXEL_FORMAT_YV12))
    return true;

  if (((transform & HAL_TRANSFORM_ROT_90) == HAL_TRANSFORM_ROT_90) ||
      ((transform & HAL_TRANSFORM_ROT_270) == HAL_TRANSFORM_ROT_270)) {
    if (srcRect->w != dstRect->h || srcRect->h != dstRect->w)
      return true;
  } else {
    if (srcRect->w != dstRect->w || srcRect->h != dstRect->h)
      return true;
  }

#if 0
/* for gsp module with input rotation. */
  if (transform > 0) {
    return true;
#endif

  return false;
}

bool GspLiteR3P0PlaneArray::needRotation(SprdHWLayer *layer) {
  uint32_t transform = layer->getTransform();

  return transform == 0 ? false : true;
}

int GspLiteR3P0PlaneArray::adapt(SprdHWLayer **list, int count, uint32_t w,
                                 uint32_t h) {
  int DPULayerNum = 0;
  int i = 0;
  int ret = 0;
  for (i = 0; i < count; i++) {
    if (list[i]->getAccelerator() == ACCELERATOR_DISPC)
      DPULayerNum++;
  }
  if (DPULayerNum == count) {
    ALOGI_IF(mDebugFlag, "DPU handle all the input layers, GSP do nothing");
    ret = 0;
  } else if (DPULayerNum == (count - 1)) {
    ret = partial_adapt(list, count, w, h);
  } else {
    ret = full_adapt(list, count, w, h);
  }

  return ret;
}

int GspLiteR3P0PlaneArray::full_adapt(SprdHWLayer **list, int count, uint32_t w,
                                      uint32_t h) {
  int ret = 0;
  bool result = false;
  native_handle_t *privateH = NULL;
  int32_t yuv_index = 0; // yuv layer index
  int32_t osd_index = 0;
  int32_t img_index = 0;
  int32_t img_index0 = 0;

  queryDebugFlag(&mDebugFlag);
  reset();
  if (checkLayerCount(count) == false) {
    ALOGI_IF(mDebugFlag, "layer count: %d  over capability", count);
    return -1;
  }

  int i = 0;
  while (i < count) {
    privateH = list[i]->getBufferHandle();

    if (needImgLayer(list[i]) == true) {
      yuv_index++;
    }
    i++;
  }

  ALOGI_IF(mDebugFlag, "layer count:%d, yuv_index:%d.", count, yuv_index);

  if (yuv_index > LITE_R3P0_IMGL_NUM) {
    ALOGI_IF(mDebugFlag, "PlaneAdapt, too many special layers(%d).", yuv_index);
    return -1;
  }

  /*  imglayer and osdlayer configure */
  img_index = 0;
  img_index0 = 0;
  osd_index = 0;
  for (i = 0; i < count; i++) {
    if (needImgLayer(list[i]) == true) {
      if (img_index < yuv_index) {
        result = imagePlaneAdapt(list[i], img_index, i, false);
        if (result == false) {
          ALOGI_IF(mDebugFlag, "imagePlaneAdapt, unsupport case (YUV format).");
          return -1;
        } else {
          list[i]->setLayerAccelerator(ACCELERATOR_GSP);
        }
        img_index++;
      }
    } else {
      if (img_index0 < LITE_R3P0_IMGL_NUM - yuv_index) {
        result = imagePlaneAdapt(list[i], img_index0 + yuv_index, i, false);
        if (result == false) {
          ALOGI_IF(mDebugFlag, "imagePlaneAdapt, unsupport case LayerIndex:%d",
                   i);
          return -1;
        } else {
          list[i]->setLayerAccelerator(ACCELERATOR_GSP);
        }
        img_index0++;
      } else {
        result = osdPlaneAdapt(list[i], osd_index, i, false);
        if (result == false) {
          ALOGI_IF(mDebugFlag, "osdPlaneAdapt, unsupport case LayerIndex: %d ",
                   i);
          return -1;
        } else {
          list[i]->setLayerAccelerator(ACCELERATOR_GSP);
        }
        osd_index++;
      }
    }
  }

  result = dstPlaneAdapt(list, count);
  if (result == false) {
    if (!input_rot_adapt(list, count)) {
      mInputRotMode = true;
      ALOGI_IF(mDebugFlag, "gsp lite_r3p0 input rotation mode.");
    } else {
      ALOGI_IF(mDebugFlag, "dstPlaneAdapt unsupport case.");
      return -1;
    }
  }

  ALOGI_IF(mDebugFlag, "gsp lite_r3p0plane full adapt success");

  return ret;
}

int GspLiteR3P0PlaneArray::partial_adapt(SprdHWLayer **list, int count,
                                         uint32_t w, uint32_t h) {
  int ret = 0;
  bool result = false;
  native_handle_t *privateH = NULL;
  int img_index = 0;

  queryDebugFlag(&mDebugFlag);
  reset();

  for (int i = 0; i < count; i++) {
    privateH = list[i]->getBufferHandle();

    if (list[i]->getAccelerator() != ACCELERATOR_DISPC) {
      if (img_index >= LITE_R3P0_IMGL_NUM) {
        ALOGI_IF(mDebugFlag, "imagePlaneAdapt, too many img layers(%d).",
                 img_index);
        return -1;
      }
      result = imagePlaneAdapt(list[i], img_index, img_index, false);
      if (result == false) {
        ALOGI_IF(mDebugFlag, "imagePlaneAdapt, unsupport case (YUV format).");
        return -1;
      } else {
        list[i]->setLayerAccelerator(ACCELERATOR_GSP);
      }
      img_index++;
      ALOGI_IF(mDebugFlag, "gsp lite_r3p0 partial adapt success");
      return 0;
    }
  }

  ALOGI_IF(mDebugFlag, "gsp lite_r3p0 partial adapt: strange to go here!");
  return ret;
}

int GspLiteR3P0PlaneArray::input_rot_adapt(SprdHWLayer **list, int count) {
  int ret = 0;
  bool result = false;
  int32_t yuv_index = 0; // yuv layer index
  int32_t osd_index = 0;
  int32_t img_index = 0;
  int32_t img_index0 = 0;

  queryDebugFlag(&mDebugFlag);
  reset();
  if (checkLayerCount(count) == false) {
    ALOGI_IF(mDebugFlag, "layer count: %d  over capability", count);
    return -1;
  }

  int i = 0;
  while (i < count) {
    if (needImgLayer(list[i]) || needRotation(list[i])) {
      yuv_index++;
    }
    i++;
  }

  ALOGI_IF(mDebugFlag, "layer count:%d, yuv_index:%d.", count, yuv_index);

  if (yuv_index > LITE_R3P0_IMGL_NUM) {
    ALOGI_IF(mDebugFlag, "PlaneAdapt, too many special layers(%d).", yuv_index);
    return -1;
  }

  /*  imglayer and osdlayer configure */
  img_index = 0;
  img_index0 = 0;
  osd_index = 0;
  for (i = 0; i < count; i++) {
    if (needImgLayer(list[i]) || needRotation(list[i])) {
      native_handle_t *handle = NULL;
      uint32_t transform = list[i]->getTransform();
      handle = list[i]->getBufferHandle();

      if (img_index < yuv_index) {
        result = imagePlaneAdapt(list[i], img_index, i, true);
        if (result == false) {
          ALOGI_IF(mDebugFlag, "imagePlaneAdapt, unsupport case (YUV format).");
          return -1;
        } else if ((ADP_FORMAT(handle) == HAL_PIXEL_FORMAT_YV12) &&
                   transform != 0 && (img_index == 0 && count > 2 ||
                                      img_index == 1 && count > 3)) {
          ALOGI_IF(mDebugFlag,
                   "imagePlaneAdapt, unsupport some yv12 input rotation");
          return -1;
        } else {
          list[i]->setLayerAccelerator(ACCELERATOR_GSP);
        }
        img_index++;
      }
    } else {
      if (img_index0 < LITE_R3P0_IMGL_NUM - yuv_index) {
        result = imagePlaneAdapt(list[i], img_index0 + yuv_index, i, true);
        if (result == false) {
          ALOGI_IF(mDebugFlag, "imagePlaneAdapt, unsupport case LayerIndex:%d",
                   i);
          return -1;
        } else {
          list[i]->setLayerAccelerator(ACCELERATOR_GSP);
        }
        img_index0++;
      } else {
        result = osdPlaneAdapt(list[i], osd_index, i, true);
        if (result == false) {
          ALOGI_IF(mDebugFlag, "osdPlaneAdapt, unsupport case LayerIndex: %d ",
                   i);
          return -1;
        } else {
          list[i]->setLayerAccelerator(ACCELERATOR_GSP);
        }
        osd_index++;
      }
    }
  }

  ALOGI_IF(mDebugFlag, "gsp lite_r3p0 input rotation adapt success");
  return ret;
}

bool GspLiteR3P0PlaneArray::imagePlaneParcel(SprdHWLayer **list,
                                             int ImgPlaneIndex,
                                             bool InputRotMode) {
  int index = mImagePlane[ImgPlaneIndex]->getIndex();
  bool result = false;

  ALOGI_IF(mDebugFlag, "image plane index: %d", index);
  if (index < 0) {
    result = mImagePlane[ImgPlaneIndex]->parcel(mOutputWidth, mOutputHeight);
  } else {
    result = mImagePlane[ImgPlaneIndex]->parcel(list[index], InputRotMode);
  }

  return result;
}

bool GspLiteR3P0PlaneArray::osdPlaneParcel(SprdHWLayer **list,
                                           int OsdPlaneIndex,
                                           bool InputRotMode) {
  int index = mOsdPlane[OsdPlaneIndex]->getIndex();
  bool result = false;

  ALOGI_IF(mDebugFlag, "osd plane index: %d", index);
  if (index >= 0) {
    result = mOsdPlane[OsdPlaneIndex]->parcel(list[index], InputRotMode);
  } else {
    result = mOsdPlane[OsdPlaneIndex]->parcel(mOutputWidth, mOutputHeight);
  }

  return result;
}

bool GspLiteR3P0PlaneArray::dstPlaneParcel(native_handle_t *handle, uint32_t w,
                                           uint32_t h, int format, int wait_fd,
                                           int32_t angle) {
  return mDstPlane->parcel(handle, w, h, format, wait_fd, angle);
}

int GspLiteR3P0PlaneArray::get_tap_var0(int srcPara, int destPara) {
  int retTap = 0;
  if ((srcPara < 3 * destPara) && (destPara <= 4 * srcPara))
    retTap = 4;
  else if ((srcPara >= 3 * destPara) && (srcPara < 4 * destPara))
    retTap = 6;
  else if (srcPara == 4 * destPara)
    retTap = 8;
  else if ((srcPara > 4 * destPara) && (srcPara < 6 * destPara))
    retTap = 4;
  else if ((srcPara >= 6 * destPara) && (srcPara < 8 * destPara))
    retTap = 6;
  else if (srcPara == 8 * destPara)
    retTap = 8;
  else if ((srcPara > 8 * destPara) && (srcPara < 12 * destPara))
    retTap = 4;
  else if ((srcPara >= 12 * destPara) && (srcPara < 16 * destPara))
    retTap = 6;
  else if (srcPara == 16 * destPara)
    retTap = 8;
  else
    retTap = 2;

  retTap = (8 - retTap) / 2;
  // retTap = 3 is null for htap & vtap on lite_r3p0

  return retTap;
}

bool GspLiteR3P0PlaneArray::miscCfgParcel(int mode_type, uint32_t transform) {
  bool status = false;
  int icnt = 0;
  struct gsp_lite_r3p0_misc_cfg_user &lite_r3p0_misc_cfg =
      mConfigs[mConfigIndex].misc;
  struct gsp_lite_r3p0_img_layer_user lite_r3p0_limg_cfg;
  uint32_t dstw = 0;
  uint32_t dsth = 0;

  switch (mode_type) {
  case 0: {
    /* run_mod = 0, scale_seq = 0 */
    lite_r3p0_misc_cfg.work_mod = 0;
    lite_r3p0_misc_cfg.core_num = 0;

    lite_r3p0_misc_cfg.workarea_src_rect.st_x = 0;
    lite_r3p0_misc_cfg.workarea_src_rect.st_y = 0;
    lite_r3p0_misc_cfg.workarea_src_rect.rect_w = mOutputWidth;
    lite_r3p0_misc_cfg.workarea_src_rect.rect_h = mOutputHeight;
  }
    status = true;
    break;
  default:
    ALOGE("gsp lite_r3p0 not implement other mode(%d) yet! ", mode_type);
    break;
  }

  return status;
}

/* TODO implement this function later for multi-configs */
int GspLiteR3P0PlaneArray::split(SprdHWLayer **list, int count) {
  if (list == NULL || count < 1) {
    ALOGE("split params error");
    return -1;
  }

  return 0;
}

int GspLiteR3P0PlaneArray::rotAdjustSingle(uint16_t *dx, uint16_t *dy,
                                           uint16_t *dw, uint16_t *dh,
                                           uint32_t pitch, uint32_t transform) {
  uint32_t x = *dx;
  uint32_t y = *dy;

  /*first adjust dest x y*/
  switch (transform) {
  case 0:
    break;
  case HAL_TRANSFORM_FLIP_H: // 1
    *dx = pitch - x - *dw;
    break;
  case HAL_TRANSFORM_FLIP_V: // 2
    *dy = mOutputHeight - y - *dh;
    break;
  case HAL_TRANSFORM_ROT_180: // 3
    *dx = pitch - x - *dw;
    *dy = mOutputHeight - y - *dh;
    break;
  case HAL_TRANSFORM_ROT_90: // 4
    *dx = y;
    *dy = pitch - x - *dw;
    break;
  case (HAL_TRANSFORM_ROT_90 | HAL_TRANSFORM_FLIP_H): // 5
    *dx = mOutputHeight - y - *dh;
    *dy = pitch - x - *dw;
    break;
  case (HAL_TRANSFORM_ROT_90 | HAL_TRANSFORM_FLIP_V): // 6
    *dx = y;
    *dy = x;
    break;
  case HAL_TRANSFORM_ROT_270: // 7
    *dx = mOutputHeight - y - *dh;
    *dy = x;
    break;
  default:
    ALOGE("rotAdjustSingle, unsupport angle=%d.", transform);
    break;
  }

  /*then adjust dest width height*/
  if (transform & HAL_TRANSFORM_ROT_90) {
    std::swap(*dw, *dh);
  }

  return 0;
}

int GspLiteR3P0PlaneArray::rotAdjust(struct gsp_lite_r3p0_cfg_user *cmd_info,
                                     SprdHWLayer **LayerList) {
  int32_t ret = 0;
  uint16_t w = 0;
  uint16_t h = 0;
  uint32_t transform = 0;
  int32_t icnt = 0;
  struct gsp_lite_r3p0_osd_layer_user *osd_info = NULL;
  struct gsp_lite_r3p0_img_layer_user *img_info = NULL;
  transform = LayerList[0]->getTransform();

  img_info = cmd_info->limg;
  for (icnt = 0; icnt < LITE_R3P0_IMGL_NUM; icnt++) {
    if (img_info[icnt].common.enable == 1) {
      ret = rotAdjustSingle(
          &img_info[icnt].params.des_rect.st_x,
          &img_info[icnt].params.des_rect.st_y,
          &img_info[icnt].params.scale_para.scale_rect_out.rect_w,
          &img_info[icnt].params.scale_para.scale_rect_out.rect_h,
          cmd_info->ld1.params.pitch, transform);
      if (ret) {
        ALOGE("rotAdjust img layer[%d] rotation adjust failed, ret=%d.", icnt,
              ret);
        return ret;
      }
    }
  }

  osd_info = cmd_info->losd;
  for (icnt = 0; icnt < LITE_R3P0_OSDL_NUM; icnt++) {
    if (osd_info[icnt].common.enable == 1) {
      w = osd_info[icnt].params.clip_rect.rect_w;
      h = osd_info[icnt].params.clip_rect.rect_h;
      if (transform & HAL_TRANSFORM_ROT_90) {
        std::swap(w, h);
      }
      ret = rotAdjustSingle(&osd_info[icnt].params.des_pos.pt_x,
                            &osd_info[icnt].params.des_pos.pt_y, &w, &h,
                            cmd_info->ld1.params.pitch, transform);
      if (ret) {
        ALOGE("rotAdjust OSD[%d] rotation adjust failed, ret=%d.", icnt, ret);
        return ret;
      }
    }
  }

  if (transform & HAL_TRANSFORM_ROT_90) {
    std::swap(cmd_info->ld1.params.pitch, cmd_info->ld1.params.height);
    std::swap(cmd_info->misc.workarea_src_rect.rect_w,
              cmd_info->misc.workarea_src_rect.rect_h);
  }
  return ret;
}

bool GspLiteR3P0PlaneArray::parcel(SprdUtilSource *source,
                                   SprdUtilTarget *target) {
  int count = source->LayerCount;
  SprdHWLayer **list = source->LayerList;
  native_handle_t *handle = NULL;
  bool result = false;
  uint32_t transform = list[0]->getTransform();
  handle = list[0]->getBufferHandle();

  if (target->buffer != NULL)
    handle = target->buffer;
  else if (target->buffer2 != NULL)
    handle = target->buffer2;
  else
    ALOGE("parcel buffer params error");

  for (int i = 0; i < LITE_R3P0_IMGL_NUM; i++) {
    result = imagePlaneParcel(list, i, mInputRotMode);
    if (result == false)
      return result;
  }

  for (int i = 0; i < LITE_R3P0_OSDL_NUM; i++) {
    result = osdPlaneParcel(list, i, mInputRotMode);
    if (result == false)
      return result;
  }

  if (mInputRotMode)
    result = dstPlaneParcel(handle, mOutputWidth, mOutputHeight, target->format,
                            target->releaseFenceFd, 0);
  else
    result = dstPlaneParcel(handle, mOutputWidth, mOutputHeight, target->format,
                            target->releaseFenceFd, transform);
  if (result == false)
    return result;

  result = miscCfgParcel(0, transform);

  return result;
}

int GspLiteR3P0PlaneArray::run() {
  drm_mConfigs->num = mConfigIndex + 1;
  return drmIoctl(mDevice, DRM_IOCTL_SPRD_GSP_TRIGGER, drm_mConfigs);
}

int GspLiteR3P0PlaneArray::acquireSignalFd() {
  return mConfigs[mConfigIndex].ld1.common.sig_fd;
}

int GspLiteR3P0PlaneArray::set(SprdUtilSource *source, SprdUtilTarget *target,
                               uint32_t w, uint32_t h) {
  int ret = -1;
  int fd = -1;
  int icnt = 0;

  mOutputWidth = w;
  mOutputHeight = h;

  if (parcel(source, target) == false) {
    ALOGE("gsp lite_r3p0plane parcel failed");
    return ret;
  }

  for (icnt = 0; icnt < LITE_R3P0_IMGL_NUM; icnt++)
    mConfigs[mConfigIndex].limg[icnt] = mImagePlane[icnt]->getConfig();
  for (icnt = 0; icnt < LITE_R3P0_OSDL_NUM; icnt++)
    mConfigs[mConfigIndex].losd[icnt] = mOsdPlane[icnt]->getConfig();

  mConfigs[mConfigIndex].ld1 = mDstPlane->getConfig();

  if (!mInputRotMode) {
    ret = rotAdjust(&(mConfigs[mConfigIndex]), source->LayerList);
    if (ret) {
      ALOGE("gsp rotAdjust fail ret=%d.", ret);
      return ret;
    }
  }

  ret = run();
  if (ret < 0) {
    ALOGE("trigger gsp device failed ret=%d.", ret);
    return ret;
  } else {
    ALOGI_IF(mDebugFlag, "trigger gsp device success");
  }

  if (mAsync == false) {
    target->acquireFenceFd = source->releaseFenceFd = -1;
    ALOGI_IF(mDebugFlag, "sync mode set fd to -1");
    return ret;
  }

  fd = acquireSignalFd();
  if (fd <= 0) {
    ALOGE("acquire signal fence fd: %d error", fd);
    return -1;
  }

  target->acquireFenceFd = fd;
  source->releaseFenceFd = dup(fd);
  ALOGI_IF(mDebugFlag,
           "source release fence fd: %d, target acquire fence fd: %d",
           source->releaseFenceFd, target->acquireFenceFd);

  return ret;
}

} // namespace android
