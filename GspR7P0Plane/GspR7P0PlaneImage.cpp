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

#include <hardware/hwcomposer_defs.h>
#include "GspR7P0Plane.h"
#include "GspR7P0PlaneArray.h"
#include "gsp_cfg.h"
#include "gsp_r7p0_cfg.h"

namespace android {

GspR7P0PlaneImage::GspR7P0PlaneImage(bool async, bool is_need_even,
                                     bool support_up_down, bool videoSize,
                                     uint16_t up, uint16_t down,
                                     const GspRangeSize &range) {
  mAttached = false;
  mIndex = -1;
  mAsync = async;
  mOddSupport = is_need_even ? false : true;
  mScale_Updown_Sametime = support_up_down ? true : false;
  mVideoMaxSize = videoSize;
  mRangeSize = range;
  mScaleRangeUp = up;
  mScaleRangeDown = down;
  mZorder = 0;
  ALOGI("create GspR7P0PlaneImage");
  mRangeSize.print();
  ALOGI("scale range up: %d, scale range down: %d", mScaleRangeUp,
        mScaleRangeDown);
  ALOGI("odd support: %d", mOddSupport);
  ALOGI("up_down_sametime support: %d", mScale_Updown_Sametime);
  ALOGI("video max size: %d", mVideoMaxSize);
}

bool GspR7P0PlaneImage::checkVideoSize(struct sprdRect *srcRect) {
  bool result = true;

  // source video size check.
  if (mVideoMaxSize == true) {
    if (srcRect->w * srcRect->h > 1280 * 720) {
      ALOGI_IF(mDebugFlag, "not support > 720P video.");
      result = false;
    }
  } else if (srcRect->w * srcRect->h > 1920 * 1080) {
    ALOGI_IF(mDebugFlag, "not support > 1080P video.");
    result = false;
  }

  return result;
}

bool GspR7P0PlaneImage::checkOddBoundary(
    struct sprdRect *srcRect, enum gsp_r7p0_img_layer_format format) {
  bool result = true;

  // if yuv_xywh_even == 1, gsp do not support odd source layer.
  if (isVideoLayer(format) == true && mOddSupport == false) {
    if ((srcRect->x & 0x1) || (srcRect->y & 0x1) || (srcRect->w & 0x1) ||
        (srcRect->h & 0x1)) {
      ALOGD_IF(mDebugFlag, "do not support odd source layer xywh.");
      result = false;
    }
  }

  return result;
}

bool GspR7P0PlaneImage::needScaleUpDown(struct sprdRect *srcRect,
                                        struct sprdRect *dstRect,
                                        enum gsp_rot_angle rot) {
  bool result = false;
  uint32_t srcw = 0;
  uint32_t srch = 0;
  uint32_t dstw = dstRect->w;
  uint32_t dsth = dstRect->h;

  if (isLandScapeTransform(rot) == true) {
    srcw = srcRect->h;
    srch = srcRect->w;
  } else {
    srcw = srcRect->w;
    srch = srcRect->h;
  }

  if ((mScale_Updown_Sametime == false) &&
      ((srcw < dstw && srch > dsth) || (srcw > dstw && srch < dsth))) {
    ALOGI_IF(mDebugFlag,
             "need scale up and down at same time, which not support");
    result = true;
  }

  return result;
}

bool GspR7P0PlaneImage::checkScaleSize(struct sprdRect *srcRect,
                                       struct sprdRect *dstRect,
                                       enum gsp_rot_angle rot, bool inFBC) {
  bool result = true;

  uint16_t scaleUpLimit = (inFBC ? 4 : (mScaleRangeUp / 16));
  uint16_t scaleDownLimit = (inFBC ? 4 : (16 / mScaleRangeDown));

  uint32_t srcw = 0;
  uint32_t srch = 0;
  uint32_t dstw = dstRect->w;
  uint32_t dsth = dstRect->h;

  if ((mLayer_Count >= 6) && (inFBC == 0)) {
    scaleDownLimit = 12;
  }

  if (isLandScapeTransform(rot) == true) {
    srcw = srcRect->h;
    srch = srcRect->w;
  } else {
    srcw = srcRect->w;
    srch = srcRect->h;
  }

  if (scaleUpLimit * srcw < dstw || scaleUpLimit * srch < dsth ||
      scaleDownLimit * dstw < srcw || scaleDownLimit * dsth < srch) {
    // gsp support [1/16-gsp_scaling_up_limit] scaling
    ALOGI_IF(mDebugFlag, "GSP only support %d-%d scaling!", scaleDownLimit,
             scaleUpLimit);
    result = false;
  }

  return result;
}

bool GspR7P0PlaneImage::checkScale(struct sprdRect *srcRect,
                                   struct sprdRect *dstRect,
                                   enum gsp_rot_angle rot, bool inFBC) {
  if (needScaleUpDown(srcRect, dstRect, rot) == true)
    return false;

  if (checkScaleSize(srcRect, dstRect, rot, inFBC) == false)
    return false;

  return true;
}

bool GspR7P0PlaneImage::checkImgLayerFormat(
    enum gsp_r7p0_img_layer_format format) {
  bool result = true;

  if (format == GSP_R7P0_IMG_FMT_MAX_NUM) {
    ALOGI_IF(mDebugFlag, "gsp img layer meet unsupported format!");
    result = false;
  }

  return result;
}

void GspR7P0PlaneImage::reset(int flag) {
  mAttached = false;
  mIndex = -1;
  mDebugFlag = flag;
  memset(&mConfig, 0, sizeof(struct gsp_r7p0_img_layer_user));
}

bool GspR7P0PlaneImage::checkFBC(uint32_t pitch, uint32_t height,
                                 enum gsp_r7p0_img_layer_format format,
                                 bool inFBC) {
  bool isIFBC = false;
  uint32_t horAlign, verAlign;

  if (inFBC) {
    if ((GSP_R7P0_IMG_FMT_ARGB888 == format) ||
        (GSP_R7P0_IMG_FMT_RGB888 == format) ||
	(GSP_R7P0_IMG_FMT_RGB565 == format) ||
        (GSP_R7P0_IMG_FMT_YUV420_2P == format)) {
      isIFBC = true;
      if (GSP_R7P0_IMG_FMT_RGB565 == format) {
	horAlign = 16;
	verAlign = 8;
	}
      else if (GSP_R7P0_IMG_FMT_YUV420_2P == format) {
	horAlign = 32;
	verAlign = 16;
      }
      else {
	horAlign = 8;
	verAlign = 8;
      }
    } else {
      horAlign = 1;
      verAlign = 1;
      ALOGI_IF(mDebugFlag, "image plane format(%d) unsuitable for FBC\n",
               format);
      return false;
    }

    if ((0 != pitch % horAlign) || (0 != height % verAlign)) {
      ALOGI_IF(mDebugFlag, "pitch/height un aligned %dx%d\n", horAlign,
               verAlign);
      return false;
    }
  }

  return true;
}

bool GspR7P0PlaneImage::checkInputRotation(
    struct sprdRect *srcRect, struct sprdRect *dstRect, enum gsp_rot_angle rot,
    enum gsp_r7p0_img_layer_format format) {
  bool result = true;

  uint16_t scaleUpLimit = 4;
  uint16_t scaleDownLimit = 4;

  uint32_t srcw = 0;
  uint32_t srch = 0;
  uint32_t dstw = dstRect->w;
  uint32_t dsth = dstRect->h;

  if (isLandScapeTransform(rot) == true) {
    srcw = srcRect->h;
    srch = srcRect->w;
  } else {
    srcw = srcRect->w;
    srch = srcRect->h;
  }

  if (scaleUpLimit * srcw < dstw || scaleUpLimit * srch < dsth ||
      scaleDownLimit * dstw < srcw || scaleDownLimit * dsth < srch) {
    ALOGI_IF(mDebugFlag, "GSP input rotation only support %d-%d scaling!",
             scaleDownLimit, scaleUpLimit);
    return false;
  }

  if (rot != 0) {
    switch (format) {
    case GSP_R7P0_IMG_FMT_YUV420_2P:
    case GSP_R7P0_IMG_FMT_YV12:
    case GSP_R7P0_IMG_FMT_ARGB888:
    case GSP_R7P0_IMG_FMT_RGB888:
    case GSP_R7P0_IMG_FMT_RGB565:
      break;
    default:
      ALOGI_IF(mDebugFlag, "input rotation unsupport img format:0x%x.", format);
      result = false;
      break;
    }
  }

  if (rot != 0 && (srcw != dstw || srch != dsth)) {
    switch (format) {
    case GSP_R7P0_IMG_FMT_YUV420_2P:
    case GSP_R7P0_IMG_FMT_YV12:
      break;
    default:
      ALOGI_IF(mDebugFlag, "input rotation  scaling unsupport img format:0x%x.",
               format);
      result = false;
      break;
    }
  }

  return result;
}

bool GspR7P0PlaneImage::adapt(SprdHWLayer *layer, int count, int LayerIndex,
                              bool InputRotMode) {
  struct sprdRect *srcRect = layer->getSprdSRCRect();
  struct sprdRect *dstRect = layer->getSprdFBRect();
  native_handle_t *handle = layer->getBufferHandle();
  // mali_gralloc_yuv_info yuv_info = ADP_YINFO(handle);
  enum gsp_r7p0_img_layer_format format = imgFormatConvert(ADP_FORMAT(handle));
  enum gsp_rot_angle rot = rotationTypeConvert(layer->getTransform());
  bool inFBC = ADP_COMPRESSED(handle);
  uint32_t pitch = ADP_STRIDE(handle);
  uint32_t height = (inFBC ? ADP_VSTRIDE(handle) : ADP_HEIGHT(handle));
  uint32_t fbc_mode;

  ALOGI_IF(mDebugFlag, "image plane start to adapt");
  ALOGI_IF(mDebugFlag, "img inFBC(%d)\n", inFBC);

  if (isAttached() == true)
    return false;

  if (isProtectedVideo(handle) == true)
    return false;

  mLayer_Count = count;

  if (checkRangeSize(srcRect, dstRect, mRangeSize) == false)
    return false;

  if (checkImgLayerFormat(format) == false)
    return false;

  // if (checkVideoSize(srcRect) == false) return false;

  if (checkOddBoundary(srcRect, format) == false)
    return false;

  if (checkFBC(pitch, height, format, inFBC) == false)
    return false;

  if (checkScale(srcRect, dstRect, rot, inFBC) == false)
    return false;

  if (checkBlending(layer) == false)
    return false;

  if (InputRotMode && rot && !checkInputRotation(srcRect, dstRect, rot, format))
    return false;

  attached(LayerIndex);
  mZorder = LayerIndex;
  ALOGI_IF(mDebugFlag, "image plane attached, layer index: %d", LayerIndex);

  return true;
}

void GspR7P0PlaneImage::configCommon(int wait_fd, int share_fd, int enable) {
  struct gsp_layer_user *common = &mConfig.common;

  common->type = GSP_IMG_LAYER;
  common->enable = enable;
  common->wait_fd = mAsync == true ? wait_fd : -1;
  common->share_fd = share_fd;
  ALOGI_IF(mDebugFlag,
           "conifg image plane enable: %d, wait_fd: %d, share_fd: %d",
           common->enable, common->wait_fd, common->share_fd);
}

int GspR7P0PlaneImage::get_tap_var0(int srcPara, int destPara) {
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

void GspR7P0PlaneImage::configScale(struct sprdRect *srcRect,
                                    struct sprdRect *dstRect,
                                    enum gsp_rot_angle rot) {
  struct gsp_r7p0_img_layer_params *params = &mConfig.params;
  uint32_t dstw = 0;
  uint32_t dsth = 0;

  if (needScale(srcRect, dstRect, rot) == true) {
    params->scaling_en = 1;
    params->scale_para.scale_en = 1;

    params->scale_para.scale_rect_in.st_x = srcRect->x;
    params->scale_para.scale_rect_in.st_y = srcRect->y;
    params->scale_para.scale_rect_in.rect_w = srcRect->w;
    params->scale_para.scale_rect_in.rect_h = srcRect->h;

    params->scale_para.scale_rect_out.st_x = dstRect->x;
    params->scale_para.scale_rect_out.st_y = dstRect->y;
    params->scale_para.scale_rect_out.rect_w = dstRect->w;
    params->scale_para.scale_rect_out.rect_h = dstRect->h;

    if (isLandScapeTransform(rot) == true) {
      dstw = params->scale_para.scale_rect_out.rect_h;
      dsth = params->scale_para.scale_rect_out.rect_w;
    } else {
      dstw = params->scale_para.scale_rect_out.rect_w;
      dsth = params->scale_para.scale_rect_out.rect_h;
    }

    params->scale_para.htap_mod =
        get_tap_var0(params->scale_para.scale_rect_in.rect_w, dstw);

    params->scale_para.vtap_mod =
        get_tap_var0(params->scale_para.scale_rect_in.rect_h, dsth);
  }

  /*for output rotation dst coordinate calucuate, "rotAdjustSingle" */
  params->scale_para.scale_rect_out.rect_w = dstRect->w;
  params->scale_para.scale_rect_out.rect_h = dstRect->h;
}

void GspR7P0PlaneImage::configSize(struct sprdRect *srcRect,
                                   struct sprdRect *dstRect, uint32_t pitch,
                                   uint32_t height,
                                   enum gsp_r7p0_img_layer_format format,
                                   enum gsp_rot_angle rot) {
  struct gsp_r7p0_img_layer_params *params = &mConfig.params;

  // configure source clip size
  params->clip_rect.st_x = srcRect->x;
  params->clip_rect.st_y = srcRect->y;
  params->clip_rect.rect_w = srcRect->w;
  params->clip_rect.rect_h = srcRect->h;

  params->des_rect.st_x = dstRect->x;
  params->des_rect.st_y = dstRect->y;
  params->des_rect.rect_w = dstRect->w;
  params->des_rect.rect_h = dstRect->h;

  params->pitch = pitch;
  params->height = height;

  if (isVideoLayer(format) == true && mOddSupport == false) {
    // image plane not support yuv format osdd size
    if (needScale(srcRect, dstRect, rot) == true) {
      params->clip_rect.st_x &= 0xfffe;
      params->clip_rect.st_y &= 0xfffe;
      params->clip_rect.rect_w &= 0xfffe;
      params->clip_rect.rect_h &= 0xfffe;
    }
  }

  ALOGI_IF(mDebugFlag, "config image plane pitch: %u, height: %d ", pitch,
           height);
  ALOGI_IF(mDebugFlag,
           "config image plane clip rect[%d %d %d %d] dst rect[%d %d %d %d]",
           params->clip_rect.st_x, params->clip_rect.st_y,
           params->clip_rect.rect_w, params->clip_rect.rect_h,
           params->des_rect.st_x, params->des_rect.st_y,
           params->des_rect.rect_w, params->des_rect.rect_h);
}

void GspR7P0PlaneImage::configFormat(enum gsp_r7p0_img_layer_format format) {
  struct gsp_r7p0_img_layer_params *params = &mConfig.params;

  params->img_format = format;
  ALOGI_IF(mDebugFlag, "config image plane format: %d", format);
}

void GspR7P0PlaneImage::configFBC(bool inFBC, int format,
                                  native_handle_t *handle) {
  struct gsp_r7p0_img_layer_params *params = &mConfig.params;
  uint32_t fbc_mode;

  if (inFBC && ((GSP_R7P0_IMG_FMT_ARGB888 == format) ||
                (GSP_R7P0_IMG_FMT_RGB888 == format) ||
		(GSP_R7P0_IMG_FMT_RGB565 == format) ||
                (GSP_R7P0_IMG_FMT_YUV420_2P == format)))
    fbc_mode = 1;
  else
    fbc_mode = 0;
  params->fbcd_mod = fbc_mode;
  params->header_size_r = ADP_HEADERSIZER(handle);
  ALOGI_IF(mDebugFlag, "config image plane vfbc_mode: %d", fbc_mode);
}

void GspR7P0PlaneImage::configPmargbMode(uint32_t blendMode) {
  struct gsp_r7p0_img_layer_params *params = &mConfig.params;

  if (blendMode == HWC_BLENDING_PREMULT ||
      blendMode == HWC_BLENDING_NONE)
    params->pmargb_mod = 1;
  else
    params->pmargb_mod = 0;

  ALOGI_IF(mDebugFlag, "config image plane pmargb mode: %d",
           params->pmargb_mod);
}

void GspR7P0PlaneImage::configAlpha(uint8_t alpha) {
  struct gsp_r7p0_img_layer_params *params = &mConfig.params;

  params->alpha = alpha;
  ALOGI_IF(mDebugFlag, "config image plane apha: 0x%x", alpha);
}

void GspR7P0PlaneImage::configCSCMatrix(native_handle_t *handle) {
  struct gsp_r7p0_img_layer_params *params = &mConfig.params;
  uint8_t y2r_mod = 3;

  switch (ADP_YINFO(handle)) {
  case USC_YUV_BT601_NARROW:
    y2r_mod = 1;
    break;
  case USC_YUV_BT601_WIDE:
    y2r_mod = 0;
    break;
  case USC_YUV_BT709_NARROW:
    y2r_mod = 3;
    break;
  case USC_YUV_BT709_WIDE:
    y2r_mod = 2;
    break;
  case USC_YUV_BT2020_NARROW:
    y2r_mod = 5;
    break;
  case USC_YUV_BT2020_WIDE:
    y2r_mod = 4;
    break;
  default:
    break;
  }

  params->y2r_mod = y2r_mod;
  ALOGI_IF(mDebugFlag, "config image plane y2r_mod: %d", y2r_mod);
}

void GspR7P0PlaneImage::configY2Y(uint8_t y2y_mod) {
  struct gsp_r7p0_img_layer_params *params = &mConfig.params;

  params->y2y_mod = y2y_mod;
  ALOGI_IF(mDebugFlag, "config image plane y2y_mod: %d", y2y_mod);
}

void GspR7P0PlaneImage::configZorder(uint8_t zorder) {
  struct gsp_r7p0_img_layer_params *params = &mConfig.params;

  params->zorder = zorder;
  ALOGI_IF(mDebugFlag, "config image zorder: %d", zorder);
}

void GspR7P0PlaneImage::configEndian(native_handle_t *handle) {
  struct gsp_r7p0_img_layer_params *params = &mConfig.params;
  struct gsp_layer_user *common = &mConfig.common;
  int format = ADP_FORMAT(handle);
  uint32_t width = ADP_STRIDE(handle);
  uint32_t height = ADP_HEIGHT(handle);
  uint32_t pixel_cnt = ADP_STRIDE(handle) * ADP_HEIGHT(handle);
  common->offset.v_offset = common->offset.uv_offset = pixel_cnt;

  params->endian.uv_dword_endn = GSP_R7P0_DWORD_ENDN_0;
  params->endian.uv_word_endn = GSP_R7P0_WORD_ENDN_0;
  params->endian.y_rgb_dword_endn = GSP_R7P0_DWORD_ENDN_0;
  params->endian.y_rgb_word_endn = GSP_R7P0_WORD_ENDN_0;
  params->endian.a_swap_mode = GSP_R7P0_A_SWAP_ARGB;
  switch (format) {
  case HAL_PIXEL_FORMAT_RGBA_8888:
  case HAL_PIXEL_FORMAT_RGBX_8888:
    params->endian.rgb_swap_mode = GSP_R7P0_RGB_SWP_BGR;
    break;
  case HAL_PIXEL_FORMAT_YV12: // YUV420_3P, Y V U
    common->offset.uv_offset += (height + 1) / 2 * ALIGN((width + 1) / 2, 16);
    params->endian.uv_word_endn = GSP_R7P0_WORD_ENDN_0;
    params->endian.y_rgb_word_endn = GSP_R7P0_WORD_ENDN_0;
    params->endian.a_swap_mode = GSP_R7P0_A_SWAP_ARGB;
    break;
  case HAL_PIXEL_FORMAT_YCrCb_420_SP:
  case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
    params->endian.uv_word_endn = GSP_R7P0_WORD_ENDN_3;
    break;
  case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    params->endian.uv_word_endn = GSP_R7P0_WORD_ENDN_0;
    params->endian.y_rgb_word_endn = GSP_R7P0_WORD_ENDN_0;
    params->endian.a_swap_mode = GSP_R7P0_A_SWAP_ARGB;
    break;
  case HAL_PIXEL_FORMAT_RGB_565:
    params->endian.rgb_swap_mode = GSP_R7P0_RGB_SWP_RGB;
    break;
  case HAL_PIXEL_FORMAT_BGRA_8888:
  //case HAL_PIXEL_FORMAT_BGRX_8888:
    params->endian.rgb_swap_mode = GSP_R7P0_RGB_SWP_RGB;
    break;
  default:
    ALOGE("image configEndian, unsupport format=%d.", format);
    break;
  }

  ALOGI_IF(mDebugFlag, "image plane y_rgb_word_endn: %d, uv_word_endn: %d",
           params->endian.y_rgb_word_endn, params->endian.uv_word_endn);

  ALOGI_IF(mDebugFlag, "image plane rgb_swap_mode: %d, a_swap_mode: %d",
           params->endian.rgb_swap_mode, params->endian.a_swap_mode);
}

void GspR7P0PlaneImage::configInputRot(enum gsp_rot_angle rot) {
  struct gsp_r7p0_img_layer_params *params = &mConfig.params;

  params->rot_angle = rot;
  ALOGI_IF(mDebugFlag, "config input rotation : 0x%d", rot);
}
struct gsp_r7p0_img_layer_user &GspR7P0PlaneImage::getConfig() {
  return mConfig;
}

bool GspR7P0PlaneImage::parcel(SprdHWLayer *layer, bool InputRotMode) {
  if (layer == NULL) {
    ALOGE("image plane parcel params layer=NULL.");
    return false;
  }

  ALOGI_IF(mDebugFlag, "image plane start to parcel");
  native_handle_t *handle = layer->getBufferHandle();
  enum gsp_rot_angle rot = rotationTypeConvert(layer->getTransform());

  struct sprdRect *srcRect = layer->getSprdSRCRect();
  struct sprdRect *dstRect = layer->getSprdFBRect();
  enum gsp_r7p0_img_layer_format format = imgFormatConvert(ADP_FORMAT(handle));
  bool inFBC = ADP_COMPRESSED(handle);
  uint32_t pitch = ADP_STRIDE(handle);
  uint32_t height = (inFBC ? ADP_VSTRIDE(handle) : ADP_HEIGHT(handle));

  // configure format
  configFormat(format);
  configFBC(inFBC, format, handle);

  // configure clip size and dst size
  configSize(srcRect, dstRect, pitch, height, format, rot);

  // configure pmargb mode
  configPmargbMode(layer->getBlendMode());

  int acquireFenceFd = layer->getAcquireFence();
  int share_fd = ADP_BUFFD(handle);
  // configure acquire fence fd and dma buffer share fd
  configCommon(acquireFenceFd, share_fd, true);

  configScale(srcRect, dstRect, rot);

  uint8_t alpha = layer->getPlaneAlpha();
  // configure alpha
  configAlpha(alpha);

  configZorder(mZorder);

  configCSCMatrix(handle);

  // configure yuv adjust as bypass
  configY2Y(1);

  configEndian(handle);

  if (InputRotMode && rot)
    configInputRot(rot);

  return true;
}

bool GspR7P0PlaneImage::parcel(uint32_t w, uint32_t h) {
  configCommon(-1, -1, false);

  ALOGI_IF(mDebugFlag, "Image plane skip parcel!");
  return true;
}

} // namespace android
