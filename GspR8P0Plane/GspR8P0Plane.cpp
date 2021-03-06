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

#include "GspR8P0Plane.h"

namespace android {

enum gsp_r8p0_img_layer_format GspR8P0Plane::imgFormatConvert(int f) {
  enum gsp_r8p0_img_layer_format format = GSP_R8P0_IMG_FMT_MAX_NUM;

  ALOGI_IF(mDebugFlag, "source img format: 0x%x", f);
  switch (f) {
  case HAL_PIXEL_FORMAT_YCbCr_420_SP: // 0x19
    format = GSP_R8P0_IMG_FMT_YUV420_2P;
    break;
  case HAL_PIXEL_FORMAT_YCrCb_420_SP:
  case HAL_PIXEL_FORMAT_YCbCr_420_888:
  case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
    format = GSP_R8P0_IMG_FMT_YUV420_2P;
    break;
  case HAL_PIXEL_FORMAT_YV12:
    format = GSP_R8P0_IMG_FMT_YV12;
    break;
  case HAL_PIXEL_FORMAT_RGBA_8888:
    format = GSP_R8P0_IMG_FMT_ARGB888;
    break;
  case HAL_PIXEL_FORMAT_RGBX_8888:
    format = GSP_R8P0_IMG_FMT_RGB888;
    break;
  case HAL_PIXEL_FORMAT_RGB_565:
    format = GSP_R8P0_IMG_FMT_RGB565;
    break;
  case HAL_PIXEL_FORMAT_YCbCr_422_SP:
    format = GSP_R8P0_IMG_FMT_YUV422_2P;
    break;
  case HAL_PIXEL_FORMAT_YCrCb_422_SP:
    format = GSP_R8P0_IMG_FMT_YUV422_2P;
    break;
  case HAL_PIXEL_FORMAT_BGRA_8888:
    format = GSP_R8P0_IMG_FMT_ARGB888;
    break;
  case HAL_PIXEL_FORMAT_BGRX_8888:
    format = GSP_R8P0_IMG_FMT_RGB888;
    break;
  default:
    ALOGE("imgFormatConvert, unsupport img format:0x%x.", f);
    break;
  }

  return format;
}

enum gsp_r8p0_osd_layer_format GspR8P0Plane::osdFormatConvert(int f) {
  enum gsp_r8p0_osd_layer_format format = GSP_R8P0_OSD_FMT_MAX_NUM;

  ALOGI_IF(mDebugFlag, "source osd format: 0x%x", f);
  switch (f) {
  case HAL_PIXEL_FORMAT_RGBA_8888:
  case HAL_PIXEL_FORMAT_BGRA_8888:
    format = GSP_R8P0_OSD_FMT_ARGB888;
    break;
  case HAL_PIXEL_FORMAT_RGBX_8888:
  case HAL_PIXEL_FORMAT_BGRX_8888:
    format = GSP_R8P0_OSD_FMT_RGB888;
    break;
  case HAL_PIXEL_FORMAT_RGB_565:
    format = GSP_R8P0_OSD_FMT_RGB565;
    break;
  default:
    ALOGE("osdFormatConvert unsupport osd format:0x%x.", f);
    break;
  }

  return format;
}

enum gsp_rot_angle GspR8P0Plane::rotationTypeConvert(int32_t angle) {
  enum gsp_rot_angle rot;

  ALOGI_IF(mDebugFlag, "source rotation: %d", angle);
  switch (angle) {
  case 0:
    rot = GSP_ROT_ANGLE_0;
    break;
  case HAL_TRANSFORM_FLIP_H: // 1
    rot = GSP_ROT_ANGLE_180_M;
    break;
  case HAL_TRANSFORM_FLIP_V: // 2
    rot = GSP_ROT_ANGLE_0_M;
    break;
  case HAL_TRANSFORM_ROT_180: // 3
    rot = GSP_ROT_ANGLE_180;
    break;
  case HAL_TRANSFORM_ROT_90: // 4
    rot = GSP_ROT_ANGLE_270;
    break;
  case (HAL_TRANSFORM_ROT_90 | HAL_TRANSFORM_FLIP_H): // 5
    rot = GSP_ROT_ANGLE_270_M;
    break;
  case (HAL_TRANSFORM_ROT_90 | HAL_TRANSFORM_FLIP_V): // 6
    rot = GSP_ROT_ANGLE_90_M;
    break;
  case HAL_TRANSFORM_ROT_270: // 7
    rot = GSP_ROT_ANGLE_90;
    break;
  default:
    rot = GSP_ROT_ANGLE_MAX_NUM;
    ALOGE("rotationTypeConvert, unsupport angle=%d.", angle);
    break;
  }

  return rot;
}

bool GspR8P0Plane::checkRangeSize(struct sprdRect *srcRect,
                                  struct sprdRect *dstRect,
                                  const GspRangeSize &range) {
  bool result = true;

  ALOGI_IF(mDebugFlag, "src rect[%d %d %d %d] dst rect[%d %d %d %d]",
           srcRect->x, srcRect->y, srcRect->w, srcRect->h, dstRect->x,
           dstRect->y, dstRect->w, dstRect->h);
  // Source and destination rectangle size check.
  if (srcRect->w < range.crop_min.rect_w ||
      srcRect->h < range.crop_min.rect_h ||
      srcRect->w > range.crop_max.rect_w ||
      srcRect->h > range.crop_max.rect_h || dstRect->w < range.out_min.rect_w ||
      dstRect->h < range.out_min.rect_h || dstRect->w > range.out_max.rect_w ||
      dstRect->h > range.out_max.rect_h) {
    ALOGI_IF(mDebugFlag, "clip or dst rect is not supported.");
    result = false;
  }

  return result;
}

bool GspR8P0Plane::checkBlending(SprdHWLayer *layer) {
  bool result = false;

  switch (layer->getBlendMode()) {
  case HWC_BLENDING_NONE:
  case HWC_BLENDING_PREMULT:
  case HWC_BLENDING_COVERAGE:
    ALOGI_IF(mDebugFlag, "r8p0 support blending flag: 0x%x",
             layer->getBlendMode());
    result = true;
    break;
  default:
    ALOGI_IF(mDebugFlag, "checkBlending, unsupport blending flag: 0x%x",
             layer->getBlendMode());
    break;
  }

  return result;
}

bool GspR8P0Plane::isLandScapeTransform(enum gsp_rot_angle rot) {
  bool result = false;

  if (rot == GSP_ROT_ANGLE_90 || rot == GSP_ROT_ANGLE_270 ||
      rot == GSP_ROT_ANGLE_90_M || rot == GSP_ROT_ANGLE_270_M)
    result = true;

  return result;
}

bool GspR8P0Plane::needScale(struct sprdRect *srcRect, struct sprdRect *dstRect,
                             enum gsp_rot_angle rot) {
  bool result = false;

  if (isLandScapeTransform(rot) == true) {
    if (srcRect->w != dstRect->h || srcRect->h != dstRect->w)
      result = true;
  } else {
    if (srcRect->w != dstRect->w || srcRect->h != dstRect->h)
      result = true;
  }

  return result;
}

bool GspR8P0Plane::isVideoLayer(enum gsp_r8p0_img_layer_format format) {
  bool result = false;

  if ((format >= GSP_R8P0_IMG_FMT_YUV422_2P &&
       format <= GSP_R8P0_IMG_FMT_YUV420_3P) ||
      (format == GSP_R8P0_IMG_FMT_YV12))
    result = true;

  return result;
}

bool GspR8P0Plane::isVideoLayer(enum gsp_r8p0_osd_layer_format format) {
  bool result = false;

  if (format > GSP_R8P0_OSD_FMT_RGB565)
    result = true;

  return result;
}

} // namespace android
