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

#include "GspR3P0PlaneOsd.h"
#include "gralloc_public.h"
#include "SprdHWLayer.h"
#include "gsp_r3p0_cfg.h"

namespace android {

GspR3P0PlaneOsd::GspR3P0PlaneOsd(bool async, const GspRangeSize &range) {
  mIndex = -1;
  mAttached = false;
  mAsync = async;
  mRangeSize = range;
  mRangeSize.print();
  ALOGI("create GspR3P0PlaneOsd");
}

void GspR3P0PlaneOsd::reset(int flag) {
  mAttached = false;
  mIndex = -1;
  mDebugFlag = flag;
  memset(&mConfig, 0, sizeof(struct gsp_r3p0_osd_layer_user));
}

bool GspR3P0PlaneOsd::adapt(SprdHWLayer *layer, int LayerIndex) {
  struct sprdRect *srcRect = layer->getSprdSRCRect();
  struct sprdRect *dstRect = layer->getSprdFBRect();
  native_handle_t *handle = layer->getBufferHandle();
  //  mali_gralloc_yuv_info yuv_info = handle->yuv_info;
  //  enum gsp_addr_type bufType = bufTypeConvert(handle->flags);
  enum gsp_r3p0_osd_layer_format format = osdFormatConvert(ADP_FORMAT(handle));
  enum gsp_rot_angle rot = rotationTypeConvert(layer->getTransform());

  ALOGI_IF(mDebugFlag, "osd plane start to adapt, index: %d ", LayerIndex);
  if (isAttached() == true)
    return false;

  if (isVideoLayer(format) == true)
    return false;

  if (checkRangeSize(srcRect, dstRect, mRangeSize) == false)
    return false;

  if (needScale(srcRect, dstRect, rot) == true)
    return false;

  if (checkBlending(layer) == false)
    return false;

  attached(LayerIndex);
  ALOGI_IF(mDebugFlag, "osd plane attached layer index: %d", LayerIndex);

  return true;
}

void GspR3P0PlaneOsd::configCommon(int wait_fd, int share_fd, int enable) {
  struct gsp_layer_user *common = &mConfig.common;

  common->type = GSP_OSD_LAYER;
  common->enable = enable;
  common->wait_fd = mAsync == true ? wait_fd : -1;
  common->share_fd = share_fd;
  ALOGI_IF(mDebugFlag, "config osd plane enable: %d, wait_fd: %d, share_fd: %d",
           common->enable, common->wait_fd, common->share_fd);
}

void GspR3P0PlaneOsd::configSize(struct sprdRect *srcRect,
                                 struct sprdRect *dstRect, uint32_t pitch,
                                 uint32_t height) {
  struct gsp_r3p0_osd_layer_params *params = &mConfig.params;

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

void GspR3P0PlaneOsd::configSize(uint32_t w, uint32_t h) {
  struct gsp_r3p0_osd_layer_params *params = &mConfig.params;

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

void GspR3P0PlaneOsd::configFormat(enum gsp_r3p0_osd_layer_format format) {
  struct gsp_r3p0_osd_layer_params *params = &mConfig.params;

  params->osd_format = format;
  ALOGI_IF(mDebugFlag, "config osd plane format: %d", format);
}

void GspR3P0PlaneOsd::configPmargbMode(uint32_t blendMode) {
  struct gsp_r3p0_osd_layer_params *params = &mConfig.params;

  if (blendMode == HWC_BLENDING_PREMULT || blendMode == HWC_BLENDING_COVERAGE)
    params->pmargb_mod = 1;
  else
    params->pmargb_mod = 0;

  ALOGI_IF(mDebugFlag, "config osd plane pmargb mode: %d", params->pmargb_mod);
}

void GspR3P0PlaneOsd::configAlpha(uint8_t alpha) {
  struct gsp_r3p0_osd_layer_params *params = &mConfig.params;

  params->alpha = alpha;
  ALOGI_IF(mDebugFlag, "config osd plane apha: %d", alpha);
}

void GspR3P0PlaneOsd::configEndian(native_handle_t *handle) {
  struct gsp_r3p0_osd_layer_params *params = &mConfig.params;
  struct gsp_layer_user *common = &mConfig.common;
  int format = ADP_FORMAT(handle);
  common->offset.v_offset = common->offset.uv_offset = 0;

  switch (format) {
  case HAL_PIXEL_FORMAT_RGBA_8888:
  case HAL_PIXEL_FORMAT_RGBX_8888:
    params->endian.y_rgb_word_endn = GSP_R3P0_WORD_ENDN_1;
    params->endian.y_rgb_dword_endn = GSP_R3P0_DWORD_ENDN_0;
    params->endian.y_rgb_qword_endn = GSP_R3P0_QWORD_ENDN_0;
    params->endian.a_swap_mode = GSP_R3P0_A_SWAP_RGBA;
    break;
  case HAL_PIXEL_FORMAT_RGB_565:
    params->endian.rgb_swap_mode = GSP_R3P0_RGB_SWP_RGB;
    break;
  default:
    ALOGE("osd configEndian, unsupport format=%d.", format);
    break;
  }

  ALOGI_IF(mDebugFlag,
           "osd plane y_rgb_word_endn: %d, uv_word_endn: %d, va_word_endn: %d",
           params->endian.y_rgb_word_endn, params->endian.uv_word_endn,
           params->endian.va_word_endn);
  ALOGI_IF(mDebugFlag, "osd plane rgb_swap_mode: %d, a_swap_mode: %d",
           params->endian.rgb_swap_mode, params->endian.a_swap_mode);
}

void GspR3P0PlaneOsd::configPallet(int enable) {
  struct gsp_r3p0_osd_layer_params *params = &mConfig.params;

  params->pallet_en = enable;
}

struct gsp_r3p0_osd_layer_user &GspR3P0PlaneOsd::getConfig() {
  return mConfig;
}

bool GspR3P0PlaneOsd::parcel(SprdHWLayer *layer) {
  if (layer == NULL) {
    ALOGE("osd plane parcel params layer=NULL.");
    return false;
  }

  ALOGI_IF(mDebugFlag, "osd plane start to parcel");
  native_handle_t *handle = layer->getBufferHandle();
  enum gsp_rot_angle rot = rotationTypeConvert(layer->getTransform());

  struct sprdRect *srcRect = layer->getSprdSRCRect();
  struct sprdRect *dstRect = layer->getSprdFBRect();
  // configure clip size and dst size
  configSize(srcRect, dstRect, ADP_STRIDE(handle), ADP_HEIGHT(handle));

  enum gsp_r3p0_osd_layer_format format = osdFormatConvert(ADP_FORMAT(handle));
  // configure format
  configFormat(format);
  // configure pmargb mode
  configPmargbMode(layer->getBlendMode());

  int acquireFenceFd = layer->getAcquireFence();
  int share_fd = ADP_BUFFD(handle);
  // configure acquire fence fd and dma buffer share fd
  configCommon(acquireFenceFd, share_fd, true);

  uint8_t alpha = layer->getPlaneAlpha();
  // configure alpha
  configAlpha(alpha);

  configEndian(handle);

  configPallet(0);

  return true;
}

bool GspR3P0PlaneOsd::parcel(uint32_t w, uint32_t h) {
  configPallet(1);

  configCommon(-1, -1, false);

  configSize(w, h);

  return true;
}

} // namespace android
