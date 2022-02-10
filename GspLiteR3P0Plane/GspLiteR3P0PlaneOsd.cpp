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

#include "GspLiteR3P0PlaneOsd.h"
#include "gralloc_public.h"
#include "SprdHWLayer.h"
#include "gsp_lite_r3p0_cfg.h"

namespace android {

GspLiteR3P0PlaneOsd::GspLiteR3P0PlaneOsd(bool async,
                                         const GspRangeSize &range) {
  mIndex = -1;
  mAttached = false;
  mAsync = async;
  mRangeSize = range;
  mRangeSize.print();
  ALOGI("create GspLiteR3P0PlaneOsd");
}

void GspLiteR3P0PlaneOsd::reset(int flag) {
  mAttached = false;
  mIndex = -1;
  mDebugFlag = flag;
  memset(&mConfig, 0, sizeof(struct gsp_lite_r3p0_osd_layer_user));
}

bool GspLiteR3P0PlaneOsd::checkFBC(uint32_t pitch, uint32_t height,
                                   enum gsp_lite_r3p0_osd_layer_format format,
                                   bool inFBC) {
  bool isIFBC = false;
  uint32_t horAlign, verAlign;

  if (inFBC) {
#if 0
    if ((GSP_LITE_R3P0_OSD_FMT_ARGB888 == format) ||
        (GSP_LITE_R3P0_OSD_FMT_RGB888 == format) ||
        (GSP_LITE_R3P0_OSD_FMT_RGB565 == format)) {
      isIFBC = true;
      if (GSP_LITE_R3P0_OSD_FMT_RGB565 == format)
        horAlign = 16;
      else
        horAlign = 8;
      verAlign = 8;
    } else {
      horAlign = 1;
      verAlign = 1;
      ALOGI_IF(mDebugFlag, "osd plane format(%d) unsuitable for FBC\n", format);
      return false;
    }

    if ((0 != pitch % horAlign) || (0 != height % verAlign)) {
      ALOGI_IF(mDebugFlag, "pitch/height un aligned %dx%d\n", horAlign,
               verAlign);
      return false;
    }
#endif
    return false;
  }

  return true;
}

bool GspLiteR3P0PlaneOsd::checkInputRotation(
    struct sprdRect *srcRect, struct sprdRect *dstRect, enum gsp_rot_angle rot,
    enum gsp_lite_r3p0_osd_layer_format format) {
  bool result = true;

  if (rot != 0) {
    switch (format) {
    case GSP_LITE_R3P0_OSD_FMT_ARGB888:
    case GSP_LITE_R3P0_OSD_FMT_RGB888:
    case GSP_LITE_R3P0_OSD_FMT_RGB565:
      break;
    default:
      ALOGI_IF(mDebugFlag, "input rotation unsupport osd format:0x%x.", format);
      result = false;
      break;
    }
  }

  return result;
}

bool GspLiteR3P0PlaneOsd::adapt(SprdHWLayer *layer, int LayerIndex,
                                bool InputRotMode) {
  struct sprdRect *srcRect = layer->getSprdSRCRect();
  struct sprdRect *dstRect = layer->getSprdFBRect();
  native_handle_t *handle = layer->getBufferHandle();
  // mali_gralloc_yuv_info yuv_info = ADP_YINFO(handle);
  enum gsp_lite_r3p0_osd_layer_format format =
      osdFormatConvert(ADP_FORMAT(handle));
  enum gsp_rot_angle rot = rotationTypeConvert(layer->getTransform());
  bool inFBC = ADP_COMPRESSED(handle);
  uint32_t pitch = ADP_STRIDE(handle);
  uint32_t height = (inFBC ? ADP_VSTRIDE(handle) : ADP_HEIGHT(handle));
  uint32_t fbc_mode;

  ALOGI_IF(mDebugFlag, "osd plane start to adapt, index: %d ", LayerIndex);
  ALOGI_IF(mDebugFlag, "osd inFBC(%d)\n", inFBC);

  if (isAttached() == true)
    return false;

  if (isVideoLayer(format) == true)
    return false;

  if (checkRangeSize(srcRect, dstRect, mRangeSize) == false)
    return false;

  if (checkFBC(pitch, height, format, inFBC) == false)
    return false;
  if (needScale(srcRect, dstRect, rot) == true)
    return false;

  if (checkBlending(layer) == false)
    return false;

  if (InputRotMode && (1 == layer->getLayerIndex()) && rot &&
      !checkInputRotation(srcRect, dstRect, rot, format))
    return false;

  attached(LayerIndex);
  ALOGI_IF(mDebugFlag, "osd plane attached layer index: %d", LayerIndex);

  return true;
}

void GspLiteR3P0PlaneOsd::configCommon(int wait_fd, int share_fd, int enable) {
  struct gsp_layer_user *common = &mConfig.common;

  common->type = GSP_OSD_LAYER;
  common->enable = enable;
  common->wait_fd = mAsync == true ? wait_fd : -1;
  common->share_fd = share_fd;
  ALOGI_IF(mDebugFlag, "config osd plane enable: %d, wait_fd: %d, share_fd: %d",
           common->enable, common->wait_fd, common->share_fd);
}

void GspLiteR3P0PlaneOsd::configSize(struct sprdRect *srcRect,
                                     struct sprdRect *dstRect, uint32_t pitch,
                                     uint32_t height) {
  struct gsp_lite_r3p0_osd_layer_params *params = &mConfig.params;

  // configure source clip size
  params->clip_rect.st_x = srcRect->x;
  params->clip_rect.st_y = srcRect->y;
  params->clip_rect.rect_w = srcRect->w;
  params->clip_rect.rect_h = srcRect->h;

  params->des_pos.pt_x = dstRect->x;
  params->des_pos.pt_y = dstRect->y;

  params->pitch = pitch;
  params->height = height;

  ALOGI_IF(mDebugFlag, "config osd plane pitch: %u, height: %u ", pitch,
           height);
  ALOGI_IF(mDebugFlag, "config osd plane clip rect[%d %d %d %d] dst pos[%d %d]",
           params->clip_rect.st_x, params->clip_rect.st_y,
           params->clip_rect.rect_w, params->clip_rect.rect_h,
           params->des_pos.pt_x, params->des_pos.pt_y);
}

void GspLiteR3P0PlaneOsd::configSize(uint32_t w, uint32_t h) {
  struct gsp_lite_r3p0_osd_layer_params *params = &mConfig.params;

  params->clip_rect.st_x = 0;
  params->clip_rect.st_y = 0;
  params->clip_rect.rect_w = w;
  params->clip_rect.rect_h = h;

  params->des_pos.pt_x = 0;
  params->des_pos.pt_y = 0;

  params->pitch = w;

  ALOGI_IF(mDebugFlag, "config osd plane pitch: %u", w);
  ALOGI_IF(mDebugFlag, "config osd plane clip rect[%d %d %d %d] dst pos[%d %d]",
           params->clip_rect.st_x, params->clip_rect.st_y,
           params->clip_rect.rect_w, params->clip_rect.rect_h,
           params->des_pos.pt_x, params->des_pos.pt_y);
}

void GspLiteR3P0PlaneOsd::configFormat(
    enum gsp_lite_r3p0_osd_layer_format format) {
  struct gsp_lite_r3p0_osd_layer_params *params = &mConfig.params;

  params->osd_format = format;
  ALOGI_IF(mDebugFlag, "config osd plane format: %d", format);
}

void GspLiteR3P0PlaneOsd::configFBC(bool inFBC, int format,
                                    native_handle_t *handle) {
  struct gsp_lite_r3p0_osd_layer_params *params = &mConfig.params;
  uint32_t fbc_mode;

  if (inFBC && ((GSP_LITE_R3P0_OSD_FMT_ARGB888 == format) ||
                (GSP_LITE_R3P0_OSD_FMT_RGB888 == format) ||
                (GSP_LITE_R3P0_OSD_FMT_RGB565 == format)))
    fbc_mode = 1;
  else
    fbc_mode = 0;
  params->fbcd_mod = fbc_mode;
  params->header_size_r = ADP_HEADERSIZER(handle);
  ALOGI_IF(mDebugFlag, "config osd plane vfbc_mode: %d", fbc_mode);
}

void GspLiteR3P0PlaneOsd::configPmargbMode(uint32_t blendMode) {
  struct gsp_lite_r3p0_osd_layer_params *params = &mConfig.params;

  if (blendMode == HWC_BLENDING_PREMULT || blendMode == HWC_BLENDING_COVERAGE)
    params->pmargb_mod = 1;
  else
    params->pmargb_mod = 0;

  ALOGI_IF(mDebugFlag, "config osd plane pmargb mode: %d", params->pmargb_mod);
}

void GspLiteR3P0PlaneOsd::configAlpha(uint8_t alpha) {
  struct gsp_lite_r3p0_osd_layer_params *params = &mConfig.params;

  params->alpha = alpha;
  ALOGI_IF(mDebugFlag, "config osd plane apha: 0x%x", alpha);
}

void GspLiteR3P0PlaneOsd::configZorder(uint8_t zorder) {
  struct gsp_lite_r3p0_osd_layer_params *params = &mConfig.params;

  params->zorder = zorder;
  ALOGI_IF(mDebugFlag, "config osd zorder: %d", zorder);
}

void GspLiteR3P0PlaneOsd::configEndian(native_handle_t *handle) {
  struct gsp_lite_r3p0_osd_layer_params *params = &mConfig.params;
  struct gsp_layer_user *common = &mConfig.common;
  int format = ADP_FORMAT(handle);
  common->offset.v_offset = common->offset.uv_offset = 0;

  params->endian.y_rgb_word_endn = GSP_LITE_R3P0_WORD_ENDN_0;
  params->endian.y_rgb_dword_endn = GSP_LITE_R3P0_DWORD_ENDN_0;
  params->endian.y_rgb_qword_endn = GSP_LITE_R3P0_QWORD_ENDN_0;
  params->endian.a_swap_mode = GSP_LITE_R3P0_A_SWAP_ARGB;
  switch (format) {
  case HAL_PIXEL_FORMAT_RGBA_8888:
  case HAL_PIXEL_FORMAT_RGBX_8888:
    params->endian.rgb_swap_mode = GSP_LITE_R3P0_RGB_SWP_BGR;
    break;
  case HAL_PIXEL_FORMAT_RGB_565:
    params->endian.rgb_swap_mode = GSP_LITE_R3P0_RGB_SWP_RGB;
    break;
  /*case HAL_PIXEL_FORMAT_BGRA_8888:
  case HAL_PIXEL_FORMAT_BGRX_8888:
    params->endian.rgb_swap_mode = GSP_LITE_R3P0_RGB_SWP_RGB;
    break;*/
  default:
    ALOGE("osd configEndian, unsupport format=0x%x.", format);
    break;
  }

  ALOGI_IF(mDebugFlag, "osd plane y_rgb_word_endn: %d, uv_word_endn: %d",
           params->endian.y_rgb_word_endn, params->endian.uv_word_endn);

  ALOGI_IF(mDebugFlag, "osd plane rgb_swap_mode: %d, a_swap_mode: %d",
           params->endian.rgb_swap_mode, params->endian.a_swap_mode);
}

void GspLiteR3P0PlaneOsd::configPallet(int enable) {
  struct gsp_lite_r3p0_osd_layer_params *params = &mConfig.params;

  params->pallet_en = enable;
}

void GspLiteR3P0PlaneOsd::configInputRot(enum gsp_rot_angle rot) {
  struct gsp_lite_r3p0_osd_layer_params *params = &mConfig.params;

  params->rot_angle = rot;
  ALOGI_IF(mDebugFlag, "config osd input rotation : 0x%d", rot);
}

struct gsp_lite_r3p0_osd_layer_user &GspLiteR3P0PlaneOsd::getConfig() {
  return mConfig;
}

bool GspLiteR3P0PlaneOsd::parcel(SprdHWLayer *layer, bool InputRotMode) {
  if (layer == NULL) {
    ALOGE("osd plane parcel params layer=NULL.");
    return false;
  }

  ALOGI_IF(mDebugFlag, "osd plane start to parcel");
  native_handle_t *handle = layer->getBufferHandle();
  enum gsp_rot_angle rot = rotationTypeConvert(layer->getTransform());
  struct sprdRect *srcRect = layer->getSprdSRCRect();
  struct sprdRect *dstRect = layer->getSprdFBRect();
  bool inFBC = ADP_COMPRESSED(handle);
  uint32_t height = (inFBC ? ADP_VSTRIDE(handle) : ADP_HEIGHT(handle));
  uint32_t fbc_mode;
  enum gsp_lite_r3p0_osd_layer_format format =
      osdFormatConvert(ADP_FORMAT(handle));

  // configure format
  configFormat(format);
  configFBC(inFBC, format, handle);

  // configure clip size and dst size
  configSize(srcRect, dstRect, ADP_STRIDE(handle), height);
  // configure pmargb mode
  configPmargbMode(layer->getBlendMode());

  int acquireFenceFd = layer->getAcquireFence();
  int share_fd = ADP_BUFFD(handle);
  // configure acquire fence fd and dma buffer share fd
  configCommon(acquireFenceFd, share_fd, true);

  uint8_t alpha = layer->getPlaneAlpha();
  // configure alpha
  configAlpha(alpha);

  configZorder(layer->getLayerIndex());

  configEndian(handle);

  configPallet(0);

  if (InputRotMode && (1 == layer->getLayerIndex()))
    configInputRot(rot);

  return true;
}

bool GspLiteR3P0PlaneOsd::parcel(uint32_t w, uint32_t h) {
  configPallet(1);

  configCommon(-1, -1, false);

  configSize(w, h);

  return true;
}

} // namespace android
