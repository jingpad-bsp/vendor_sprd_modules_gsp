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

#include "GspR8P0PlaneDst.h"
#include "gralloc_public.h"

namespace android {

GspR8P0PlaneDst::GspR8P0PlaneDst(bool async, int max_gspmmu_size,
                                 int max_gsp_bandwidth) {
  mAsync = async;
  maxGspMMUSize = max_gspmmu_size;
  maxGspBandwidth = max_gsp_bandwidth;

  ALOGI("create GspR8P0PlaneDst");
}

enum gsp_r8p0_des_layer_format GspR8P0PlaneDst::dstFormatConvert(int format) {
  enum gsp_r8p0_des_layer_format f = GSP_R8P0_DST_FMT_MAX_NUM;

  ALOGI_IF(mDebugFlag, "source dst format: 0x%x", format);

  switch (format) {
  case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    f = GSP_R8P0_DST_FMT_YUV420_2P;
    break;
  case HAL_PIXEL_FORMAT_YCbCr_422_SP:
    f = GSP_R8P0_DST_FMT_YUV422_2P;
    break;
  case HAL_PIXEL_FORMAT_YV12:
    f = GSP_R8P0_DST_FMT_YUV420_3P;
    break;
  case HAL_PIXEL_FORMAT_RGBA_8888:
    f = GSP_R8P0_DST_FMT_ARGB888;
    break;
  case HAL_PIXEL_FORMAT_RGB_565:
    f = GSP_R8P0_DST_FMT_RGB565;
    break;
  case HAL_PIXEL_FORMAT_RGBX_8888:
    f = GSP_R8P0_DST_FMT_RGB888;
    break;
  default:
    ALOGE("dstFormatConvert,unsupport output format:0x%x.", format);
    break;
  }
  ALOGI_IF(mDebugFlag, "output format: 0x%x", f);

  return f;
}

void GspR8P0PlaneDst::reset(int flag) {
  mDebugFlag = flag;
  memset(&mConfig, 0, sizeof(struct gsp_r8p0_des_layer_user));
}

bool GspR8P0PlaneDst::checkOutputRotation(SprdHWLayer **list, int count) {
  bool result = true;

  for (int i = 0; i < count - 1; i++) {
    if (list[i]->getAccelerator() != ACCELERATOR_DISPC &&
        list[i + 1]->getAccelerator() != ACCELERATOR_DISPC &&
        list[i]->getTransform() != list[i + 1]->getTransform()) {
      result = false;
      break;
    }
  }

  return result;
}

bool GspR8P0PlaneDst::checkGspMMUSize(SprdHWLayer **list, int count) {
  bool result = true;
  native_handle_t *privateH = NULL;
  int i = 0;
  int needGspMmuSize = R8P0_DST_GSP_MMU_SIZE;

  i = 0;
  while (i < count) {
    privateH = list[i]->getBufferHandle();
    i++;
  }

  if (needGspMmuSize > maxGspMMUSize) {
    ALOGI_IF(mDebugFlag, "checkGspMMUSize exceed the max GSP_MMU size.");
    result = false;
  }

  return result;
}

bool GspR8P0PlaneDst::checkGspBandwidth(SprdHWLayer **list, int count) {
  bool result = true;
  native_handle_t *privateH = NULL;
  int i = 0;
  int needGspBandWidth = 0;
  struct sprdRect *srcRect = NULL;

  i = 0;
  while (i < count) {
    srcRect = list[i]->getSprdSRCRect();
    privateH = list[i]->getBufferHandle();

    if ((ADP_FORMAT(privateH) == HAL_PIXEL_FORMAT_YCbCr_420_SP) ||
        (ADP_FORMAT(privateH) == HAL_PIXEL_FORMAT_YCrCb_420_SP)) {
      needGspBandWidth += (srcRect->w * srcRect->h * 3 / 2);
    } else if ((ADP_FORMAT(privateH) == HAL_PIXEL_FORMAT_RGBA_8888) ||
               (ADP_FORMAT(privateH) == HAL_PIXEL_FORMAT_RGBX_8888)) {
      needGspBandWidth += (srcRect->w * srcRect->h * 4);
    } else if (ADP_FORMAT(privateH) == HAL_PIXEL_FORMAT_RGB_565) {
      needGspBandWidth += (srcRect->w * srcRect->h * 2);
    }
    ALOGI_IF(mDebugFlag, "checkGspBandwidth needGspBandWidth=%d.",
             needGspBandWidth);

    i++;
  }

  if (needGspBandWidth > maxGspBandwidth) {
    ALOGI_IF(mDebugFlag,
             "checkGspBandwidth needGspBandWidth=%d, exceed the max gsp "
             "bandwidth size.",
             needGspBandWidth);
    result = false;
  }

  return result;
}

bool GspR8P0PlaneDst::adapt(SprdHWLayer **list, int count) {
  if (checkOutputRotation(list, count) == false)
    return false;

  //  if (checkGspMMUSize(list, count) == false) return false;

  //  if (checkGspBandwidth(list, count) == false) return false;

  return true;
}

void GspR8P0PlaneDst::configCommon(int wait_fd, int share_fd) {
  struct gsp_layer_user *common = &mConfig.common;

  common->type = GSP_DES_LAYER;
  common->enable = 1;
  common->wait_fd = mAsync == true ? wait_fd : -1;
  common->share_fd = share_fd;
  ALOGI_IF(mDebugFlag, "conifg dst plane enable: %d, wait_fd: %d, share_fd: %d",
           common->enable, common->wait_fd, common->share_fd);
}

void GspR8P0PlaneDst::configFormat(int format) {
  struct gsp_r8p0_des_layer_params *params = &mConfig.params;

  params->img_format = format;
}

void GspR8P0PlaneDst::configFBC(bool outAFBC, int format,
                                native_handle_t *handle) {
  struct gsp_r8p0_des_layer_params *params = &mConfig.params;
  uint32_t fbc_mode;

  if (outAFBC && ((GSP_R8P0_DST_FMT_ARGB888 == format) ||
                  (GSP_R8P0_DST_FMT_YUV420_2P == format)))
    fbc_mode = 2;
  else
    fbc_mode = 0;
  params->fbc_mod = fbc_mode;
  params->header_size_r = ADP_HEADERSIZER(handle);
}

void GspR8P0PlaneDst::configPitch(uint32_t pitch) {
  struct gsp_r8p0_des_layer_params *params = &mConfig.params;

  params->pitch = pitch;
  ALOGI_IF(mDebugFlag, "dst plane pitch: %u", pitch);
}

void GspR8P0PlaneDst::configHeight(uint32_t height) {
  struct gsp_r8p0_des_layer_params *params = &mConfig.params;

  params->height = height;
  ALOGI_IF(mDebugFlag, "dst plane height: %u", height);
}

void GspR8P0PlaneDst::configEndian(int f, uint32_t w, uint32_t h) {
  struct gsp_r8p0_des_layer_params *params = &mConfig.params;
  struct gsp_layer_user *common = &mConfig.common;

  params->endian.uv_dword_endn = GSP_R8P0_DWORD_ENDN_0;
  params->endian.uv_word_endn = GSP_R8P0_WORD_ENDN_0;
  params->endian.y_rgb_dword_endn = GSP_R8P0_DWORD_ENDN_0;
  params->endian.y_rgb_word_endn = GSP_R8P0_WORD_ENDN_0;
  params->endian.a_swap_mode = GSP_R8P0_A_SWAP_ARGB;
  switch (f) {
  case HAL_PIXEL_FORMAT_RGBA_8888:
  case HAL_PIXEL_FORMAT_RGBX_8888:
    params->endian.rgb_swap_mode = GSP_R8P0_RGB_SWP_BGR;
    break;
  case HAL_PIXEL_FORMAT_RGB_565:
    params->endian.rgb_swap_mode = GSP_R8P0_RGB_SWP_RGB;
    break;
  case HAL_PIXEL_FORMAT_BGRA_8888:
    params->endian.rgb_swap_mode = GSP_R8P0_RGB_SWP_RGB;
    break;
  default:
    ALOGE("dst configEndian, unsupport format=0x%x.", f);
    break;
  }

  common->offset.uv_offset = w * h;
  common->offset.v_offset = w * h;

  ALOGI_IF(mDebugFlag, "dst plane y_word_endn: %d, uv_word_endn: %d",
           params->endian.y_rgb_word_endn, params->endian.uv_word_endn);
  ALOGI_IF(mDebugFlag, "dst plane rgb_swap_mode: %d, a_swap_mode: %d",
           params->endian.rgb_swap_mode, params->endian.a_swap_mode);
}

void GspR8P0PlaneDst::configBackGround(struct gsp_background_para bg_para) {
  struct gsp_r8p0_des_layer_params *params = &mConfig.params;
  params->bk_para.bk_enable = bg_para.bk_enable;
  params->bk_para.bk_blend_mod = bg_para.bk_blend_mod;
  params->bk_para.background_rgb = bg_para.background_rgb;
}

bool GspR8P0PlaneDst::configOutputRotation(enum gsp_rot_angle rot_angle,
                                           bool outAFBC, int format) {
  if (!rot_angle && outAFBC && GSP_R8P0_DST_FMT_YUV420_2P == format)
    return false;
  struct gsp_r8p0_des_layer_params *params = &mConfig.params;
  params->rot_angle = rot_angle;
  return true;
}

void GspR8P0PlaneDst::configDither(bool enable) {
  struct gsp_r8p0_des_layer_params *params = &mConfig.params;
  params->dither_en = enable;
}

void GspR8P0PlaneDst::configCSCMode(uint8_t r2y_mod) {
  struct gsp_r8p0_des_layer_params *params = &mConfig.params;
  params->r2y_mod = r2y_mod;
}

struct gsp_r8p0_des_layer_user &GspR8P0PlaneDst::getConfig() {
  return mConfig;
}

bool GspR8P0PlaneDst::parcel(native_handle_t *handle, uint32_t w, uint32_t h,
                             int format, int wait_fd, int32_t transform) {
  int f = dstFormatConvert(format);
  bool outAFBC = ADP_COMPRESSED(handle);
  uint32_t height = (outAFBC ? ADP_VSTRIDE(handle) : ADP_HEIGHT(handle));
  bool result = false;

  if (handle == NULL) {
    ALOGE("dst plane parcel params handle=NULL.");
    return false;
  }

  ALOGI_IF(mDebugFlag, "dst outAFBC(%d)\n", outAFBC);

  configCommon(wait_fd, ADP_BUFFD(handle));

  configPitch(ADP_STRIDE(handle));

  configHeight(height);

  configFormat(f);

  configFBC(outAFBC, f, handle);

  configEndian(format, ADP_STRIDE(handle), height);
  if (w != (uint32_t)ADP_STRIDE(handle) || h != (uint32_t)ADP_HEIGHT(handle)) {
    ALOGI_IF(mDebugFlag, "dst plane cfg stride: %d, width: %d",
             ADP_STRIDE(handle), w);
  }

  enum gsp_rot_angle rot = rotationTypeConvert(transform);
  result = configOutputRotation(rot, outAFBC, f);
  if (result == false) {
    ALOGE("No support YUV420_2P IFBCE output rotation.");
    return false;
  }

  struct gsp_background_para bg_para;
  bg_para.bk_enable = 1;
  bg_para.bk_blend_mod = 0;
  bg_para.background_rgb.a_val = 0;
  bg_para.background_rgb.r_val = 0;
  bg_para.background_rgb.g_val = 0;
  bg_para.background_rgb.b_val = 0;
  configBackGround(bg_para);

  configCSCMode(0);

  return true;
}

} // namespace android
