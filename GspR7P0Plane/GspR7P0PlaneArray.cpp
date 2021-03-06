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

#include "GspR7P0PlaneArray.h"
#include <algorithm>
#include "GspR7P0PlaneImage.h"
#include "GspR7P0PlaneOsd.h"
#include "GspR7P0PlaneDst.h"
#include "../GspDevice.h"
#include "dump.h"
#include "SprdUtil.h"
#include "gsp_r7p0_cfg.h"
#include "sprd_drm_gsp.h"
#include <xf86drm.h>

#define GSP_R7P0_PLANE_TRIGGER                                                 \
  GSP_IO_TRIGGER(mAsync, (mConfigIndex + 1), mSplitSupport,                    \
                 struct gsp_r7p0_cfg_user)

#define GSP_LOG_PATH "/data/gsp.cfg"

namespace android {

GspR7P0PlaneArray::GspR7P0PlaneArray(bool async) {
  mAsync = async;
  mSplitSupport = 0;
  mDebugFlag = 0;
  mInputRotMode = false;
}

GspR7P0PlaneArray::~GspR7P0PlaneArray() {
  for (int i = 0; i < R7P0_IMGL_NUM; i++) {
    if (mImagePlane[i])
      delete mImagePlane[i];
  }
  for (int i = 0; i < R7P0_OSDL_NUM; i++) {
    if (mOsdPlane[i])
      delete mOsdPlane[i];
  }

  if (mDstPlane)
    delete mDstPlane;

  if (mConfigs)
    delete mConfigs;
}
int GspR7P0PlaneArray::init(int fd) {
  int ret = -1;
  struct drm_gsp_capability drm_cap;

  drm_cap.size = sizeof(gsp_r7p0_capability);
  drm_cap.cap = &mCapability;

  ret = drmIoctl(fd, DRM_IOCTL_SPRD_GSP_GET_CAPABILITY, &drm_cap);
  if (ret < 0) {
    ALOGE("gspr7p0plane array get capability failed, ret=%d.\n", ret);
    return ret;
  }

  if (mCapability.common.max_layer < 1) {
    ALOGE("max layer params error");
    return -1;
  }

  GspRangeSize range(mCapability.common.crop_max, mCapability.common.crop_min,
                     mCapability.common.out_max, mCapability.common.out_min);

  for (int i = 0; i < R7P0_IMGL_NUM; i++) {
    mImagePlane[i] = new GspR7P0PlaneImage(
        mAsync, mCapability.yuv_xywh_even, mCapability.scale_updown_sametime,
        mCapability.max_video_size, mCapability.scale_range_up,
        mCapability.scale_range_down, range);
    if (mImagePlane[i] == NULL) {
      ALOGE("R7P0 ImgPlane alloc fail!");
      return -1;
    }
  }

  for (int i = 0; i < R7P0_OSDL_NUM; i++) {
    mOsdPlane[i] = new GspR7P0PlaneOsd(mAsync, range);
    if (mOsdPlane[i] == NULL) {
      ALOGE("R7P0 OsdPlane[%d] alloc fail!", i);
      return -1;
    }
  }

  mDstPlane = new GspR7P0PlaneDst(mAsync, mCapability.max_gspmmu_size,
                                  mCapability.max_gsp_bandwidth);
  if (mDstPlane == NULL) {
    ALOGE("R7P0 DstPlane alloc fail!");
    return -1;
  }

  mPlaneNum = R7P0_IMGL_NUM + R7P0_OSDL_NUM;

  mConfigs = new gsp_r7p0_cfg_user[mCapability.common.io_cnt];
  if (mConfigs == NULL) {
    ALOGE("R7P0 mConfigs alloc fail, io_cnt:%d !", mCapability.common.io_cnt);
    return -1;
  }

  drm_mConfigs =
      (struct drm_gsp_r7p0_cfg_user *)malloc(sizeof(drm_gsp_r7p0_cfg_user));
  drm_mConfigs->async = mAsync;
  drm_mConfigs->config = mConfigs;
  drm_mConfigs->size = sizeof(gsp_r7p0_cfg_user);
  drm_mConfigs->num = 1;
  drm_mConfigs->split = false;

  mDevice = fd;
  reset();

  return 0;
}

void GspR7P0PlaneArray::reset() {
  mAttachedNum = 0;
  mConfigIndex = 0;
  mInputRotMode = false;

  for (int i = 0; i < R7P0_IMGL_NUM; i++) {
    mImagePlane[i]->reset(mDebugFlag);
  }
  for (int i = 0; i < R7P0_OSDL_NUM; i++) {
    mOsdPlane[i]->reset(mDebugFlag);
  }
  mDstPlane->reset(mDebugFlag);
  memset(&mConfigs->misc, 0, sizeof(struct gsp_r7p0_misc_cfg_user));
}

void GspR7P0PlaneArray::queryDebugFlag(int *debugFlag) {
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

bool GspR7P0PlaneArray::checkLayerCount(int count) {
  return count > mPlaneNum ? false : true;
}

bool GspR7P0PlaneArray::isExhausted() {
  return mAttachedNum == mMaxAttachedNum ? true : false;
}

bool GspR7P0PlaneArray::isSupportBufferType(enum gsp_addr_type type) {
  if (type == GSP_ADDR_TYPE_IOVIRTUAL && mAddrType == GSP_ADDR_TYPE_PHYSICAL)
    return false;
  else
    return true;
}

bool GspR7P0PlaneArray::isSupportSplit() { return mSplitSupport; }

void GspR7P0PlaneArray::planeAttached() {
  mAttachedNum++;
  ALOGI_IF(mDebugFlag, "has attached total %d plane", mAttachedNum);
}

bool GspR7P0PlaneArray::isImagePlaneAttached(int ImgPlaneIndex) {
  return mImagePlane[ImgPlaneIndex]->isAttached();
}

bool GspR7P0PlaneArray::isOsdPlaneAttached(int OsdPlaneIndex) {
  return mOsdPlane[OsdPlaneIndex]->isAttached();
}

bool GspR7P0PlaneArray::imagePlaneAdapt(SprdHWLayer *layer, int count,
                                        int ImgPlaneIndex, int LayerIndex,
                                        bool InputRotMode) {
  if (isImagePlaneAttached(ImgPlaneIndex) == true)
    return false;

  return mImagePlane[ImgPlaneIndex]->adapt(layer, count, LayerIndex, InputRotMode);
}

bool GspR7P0PlaneArray::osdPlaneAdapt(SprdHWLayer *layer, int OsdPlaneIndex,
                                      int LayerIndex) {
  if (isOsdPlaneAttached(OsdPlaneIndex) == true)
    return false;

  return mOsdPlane[OsdPlaneIndex]->adapt(layer, LayerIndex);
}

bool GspR7P0PlaneArray::dstPlaneAdapt(SprdHWLayer **list, int count) {
  return mDstPlane->adapt(list, count);
}

bool GspR7P0PlaneArray::needImgLayer(SprdHWLayer *layer) {
  native_handle_t *handle = NULL;
  struct sprdRect *srcRect = layer->getSprdSRCRect();
  struct sprdRect *dstRect = layer->getSprdFBRect();
  handle = layer->getBufferHandle();
  uint32_t transform = layer->getTransform();

  if ((ADP_FORMAT(handle) == HAL_PIXEL_FORMAT_YCbCr_420_SP) ||
      (ADP_FORMAT(handle) == HAL_PIXEL_FORMAT_YCrCb_420_SP) ||
      (ADP_FORMAT(handle) == HAL_PIXEL_FORMAT_YV12) ||
      (ADP_FORMAT(handle) == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED))
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

bool GspR7P0PlaneArray::needRotation(SprdHWLayer *layer) {
  uint32_t transform = layer->getTransform();

  return transform == 0 ? false : true;
}

int GspR7P0PlaneArray::adapt(SprdHWLayer **list, int count, uint32_t w,
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
  } else {
    ret = gsp_choose_layer(list, count);
    if (ret >= 0)
      ret = gsp_adapt(list, count, w, h);
  }

  return ret;
}

int GspR7P0PlaneArray::gsp_choose_layer(SprdHWLayer **list, int count) {
  int ret = 0;
  int i, j = 0;
  int DPULayerNum = 0;
  int skip_index = -1;
  int first_skip = -1;
  int revert_num = 0;
  int backup_layer = 0;

  queryDebugFlag(&mDebugFlag);
  reset();

  for (j = 0; j < count; j++) {
    if (list[j]->getAccelerator() != ACCELERATOR_DISPC) {
      if (skip_index == -1) {
        skip_index = j;
        first_skip = j;
      } else {
        for (i = skip_index + 1; i < j; i++) {
          if (list[i]->getAccelerator() == ACCELERATOR_DISPC) {
            list[i]->setLayerAccelerator(ACCELERATOR_DISPC_BACKUP);
            revert_num++;
          }
        }
        skip_index = j;
      }
    }
  }

  for (j = count - 1; j > first_skip; j--) {
    if (revert_num == 0) {
      break;
    }

    if (list[j]->getAccelerator() == ACCELERATOR_DISPC_BACKUP) {
      list[j]->setLayerAccelerator(ACCELERATOR_DISPC);
      revert_num--;
    } else {
      break;
    }
  }

  for (i = 0; i < count; i++) {
    if (list[i]->getAccelerator() == ACCELERATOR_DISPC)
      DPULayerNum++;
  }

  if (count - DPULayerNum > R7P0_IMGL_NUM + R7P0_OSDL_NUM) {
    ALOGI_IF(mDebugFlag, "layer count: %d  over capability", count);
    return -1;
  }
  return ret;
}

int GspR7P0PlaneArray::gsp_adapt(SprdHWLayer **list, int count, uint32_t w,
                                 uint32_t h) {
  int ret = 0;
  bool result = false;
  int32_t yuv_index = 0; // yuv layer index
  int32_t osd_index = 0;
  int32_t img_index = 0;
  int32_t img_index0 = 0;
  int32_t layer_index = 0;
  uint32_t transform = 0;
  bool input_rot_mode = false;
  bool output_rot_mode = false;
  int input_rot_num = 0;
  int i = 0;

  queryDebugFlag(&mDebugFlag);

  if (dstPlaneAdapt(list, count)) {
    output_rot_mode = true;
  }

  while (i < count) {
    if (list[i]->getAccelerator() != ACCELERATOR_DISPC &&
        (needImgLayer(list[i]) == true)) {
      yuv_index++;
    } else if (!output_rot_mode &&
               list[i]->getAccelerator() != ACCELERATOR_DISPC &&
               (needRotation(list[i]) == true)) {
      input_rot_num++;
    }
    i++;
  }

  ALOGI_IF(mDebugFlag, "layer count:%d, yuv_index:%d.", count, yuv_index);

  if (yuv_index + input_rot_num > R7P0_IMGL_NUM) {
    ALOGI_IF(mDebugFlag, "PlaneAdapt, too many special layers(%d).", yuv_index);
    return -1;
  }

  /*  imglayer and osdlayer configure */
  for (i = 0; i < count; i++) {
    if (list[i]->getAccelerator() == ACCELERATOR_DISPC) {
      continue;
    }

    transform = list[i]->getTransform();
    if (!output_rot_mode && transform) {
      input_rot_mode = true;
    }

    if (needImgLayer(list[i]) || input_rot_mode) {
      if (img_index < (yuv_index + input_rot_num)) {
        result = imagePlaneAdapt(list[i], count, img_index, layer_index,
                                 input_rot_mode);
        if (result == false) {
          ALOGI_IF(mDebugFlag, "imagePlaneAdapt, unsupport case (YUV format).");
          return -1;
        } else {
          list[i]->setLayerAccelerator(ACCELERATOR_GSP);
        }
        img_index++;
      }
    } else {
      if (img_index0 < (R7P0_IMGL_NUM - (yuv_index + input_rot_num))) {
        result = imagePlaneAdapt(list[i], count, img_index0 + yuv_index + input_rot_num,
                            layer_index, false);
        if (result == false) {
          ALOGI_IF(mDebugFlag, "imagePlaneAdapt, unsupport case LayerIndex:%d",
                   i);
          return -1;
        } else {
          list[i]->setLayerAccelerator(ACCELERATOR_GSP);
        }
        img_index0++;
      } else {
        result = osdPlaneAdapt(list[i], osd_index, layer_index);
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

    if (input_rot_mode == true) {
      mInputRotMode = true;
    }
    input_rot_mode = false;
    layer_index++;
  }

  ALOGI_IF(mDebugFlag, "gsp r7p0plane full adapt success");

  return ret;
}

bool GspR7P0PlaneArray::imagePlaneParcel(SprdHWLayer **list, int ImgPlaneIndex,
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

bool GspR7P0PlaneArray::osdPlaneParcel(SprdHWLayer **list, int OsdPlaneIndex) {
  int index = mOsdPlane[OsdPlaneIndex]->getIndex();
  bool result = false;

  ALOGI_IF(mDebugFlag, "osd plane index: %d", index);
  if (index >= 0) {
    result = mOsdPlane[OsdPlaneIndex]->parcel(list[index]);
  } else {
    result = mOsdPlane[OsdPlaneIndex]->parcel(mOutputWidth, mOutputHeight);
  }

  return result;
}

bool GspR7P0PlaneArray::dstPlaneParcel(native_handle_t *handle, uint32_t w,
                                       uint32_t h, int format, int wait_fd,
                                       int32_t angle) {
  return mDstPlane->parcel(handle, w, h, format, wait_fd, angle);
}

int GspR7P0PlaneArray::get_tap_var0(int srcPara, int destPara) {
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
  // retTap = 3 is null for htap & vtap on r7p0

  return retTap;
}

bool GspR7P0PlaneArray::miscCfgParcel(int mode_type, uint32_t transform) {
  bool status = false;
  int icnt = 0;
  struct gsp_r7p0_misc_cfg_user &r7p0_misc_cfg = mConfigs[mConfigIndex].misc;
  struct gsp_r7p0_img_layer_user r7p0_limg_cfg;
  uint32_t dstw = 0;
  uint32_t dsth = 0;

  switch (mode_type) {
  case 0: {
    /* run_mod = 0, scale_seq = 0 */
    r7p0_misc_cfg.work_mod = 0;
    r7p0_misc_cfg.core_num = 0;

    r7p0_misc_cfg.workarea_src_rect.st_x = 0;
    r7p0_misc_cfg.workarea_src_rect.st_y = 0;
    r7p0_misc_cfg.workarea_src_rect.rect_w = mOutputWidth;
    r7p0_misc_cfg.workarea_src_rect.rect_h = mOutputHeight;
  }
    status = true;
    break;
  default:
    ALOGE("gsp r7p0 not implement other mode(%d) yet! ", mode_type);
    break;
  }

  return status;
}

/* TODO implement this function later for multi-configs */
int GspR7P0PlaneArray::split(SprdHWLayer **list, int count) {
  if (list == NULL || count < 1) {
    ALOGE("split params error");
    return -1;
  }

  return 0;
}

int GspR7P0PlaneArray::rotAdjustSingle(uint16_t *dx, uint16_t *dy, uint16_t *dw,
                                       uint16_t *dh, uint32_t pitch,
                                       uint32_t transform) {
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

int GspR7P0PlaneArray::rotAdjust(struct gsp_r7p0_cfg_user *cmd_info,
                                 SprdHWLayer **LayerList) {
  int32_t ret = 0;
  uint16_t w = 0;
  uint16_t h = 0;
  uint32_t transform = 0;
  int32_t icnt = 0;
  struct gsp_r7p0_osd_layer_user *osd_info = NULL;
  struct gsp_r7p0_img_layer_user *img_info = NULL;
  transform = LayerList[0]->getTransform();

  img_info = cmd_info->limg;
  for (icnt = 0; icnt < R7P0_IMGL_NUM; icnt++) {
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
  for (icnt = 0; icnt < R7P0_OSDL_NUM; icnt++) {
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

bool GspR7P0PlaneArray::parcel(SprdUtilSource *source, SprdUtilTarget *target) {
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

  for (int i = 0; i < R7P0_IMGL_NUM; i++) {
    result = imagePlaneParcel(list, i, mInputRotMode);
    if (result == false)
      return result;
  }

  for (int i = 0; i < R7P0_OSDL_NUM; i++) {
    result = osdPlaneParcel(list, i);
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

int GspR7P0PlaneArray::run() {
  drm_mConfigs->num = mConfigIndex + 1;
  return drmIoctl(mDevice, DRM_IOCTL_SPRD_GSP_TRIGGER, drm_mConfigs);
}

int GspR7P0PlaneArray::acquireSignalFd() {
  return mConfigs[mConfigIndex].ld1.common.sig_fd;
}

int GspR7P0PlaneArray::set(SprdUtilSource *source, SprdUtilTarget *target,
                           uint32_t w, uint32_t h) {
  int ret = -1;
  int fd = -1;
  int icnt = 0;

  mOutputWidth = w;
  mOutputHeight = h;

  if (parcel(source, target) == false) {
    ALOGE("gsp r7p0plane parcel failed");
    return ret;
  }

  for (icnt = 0; icnt < R7P0_IMGL_NUM; icnt++)
    mConfigs[mConfigIndex].limg[icnt] = mImagePlane[icnt]->getConfig();
  for (icnt = 0; icnt < R7P0_OSDL_NUM; icnt++)
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
