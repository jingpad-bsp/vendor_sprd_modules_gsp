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
#ifndef GSPR7P0PLANE_GSPR7P0PLANEIMAGE_H_
#define GSPR7P0PLANE_GSPR7P0PLANEIMAGE_H_

#include "GspR7P0Plane.h"
#include "gralloc_public.h"
#include "SprdHWLayer.h"
#include "gsp_r7p0_cfg.h"

namespace android {

class GspR7P0PlaneImage : public GspR7P0Plane {
public:
  GspR7P0PlaneImage(bool async, bool odd, bool support_up_down, bool size,
                    uint16_t up, uint16_t down, const GspRangeSize &range);
  virtual ~GspR7P0PlaneImage() {}

  bool adapt(SprdHWLayer *layer, int count, int index, bool InputRotMode);

  void reset(int flag);

  bool parcel(SprdHWLayer *layer, bool InputRotMode);

  bool parcel(uint32_t w, uint32_t h);
  struct gsp_r7p0_img_layer_user &getConfig();

private:
  bool hasMaxVideoSize();

  // bool checkYuvInfo(mali_gralloc_yuv_info info);

  bool checkOddBoundary(struct sprdRect *srcRect,
                        enum gsp_r7p0_img_layer_format format);

  bool checkVideoSize(struct sprdRect *srcRect);

  bool checkImgLayerFormat(enum gsp_r7p0_img_layer_format format);

  bool checkScaleSize(struct sprdRect *srcRect, struct sprdRect *dstRect,
                      enum gsp_rot_angle rot, bool inFBC);

  bool needScaleUpDown(struct sprdRect *srcRect, struct sprdRect *dstRect,
                       enum gsp_rot_angle rot);

  bool checkScale(struct sprdRect *srcRect, struct sprdRect *dstRect,
                  enum gsp_rot_angle rot, bool inFBC);

  void configCommon(int wait_fd, int share_fd, int enable);

  int get_tap_var0(int srcPara, int destPara);

  void configScale(struct sprdRect *srcRect, struct sprdRect *dstRect,
                   enum gsp_rot_angle rot);

  void configFormat(enum gsp_r7p0_img_layer_format format);

  void configPmargbMode(uint32_t blendMode);

  void configAlpha(uint8_t alpha);

  void configZorder(uint8_t zorder);

  void configCSCMatrix(native_handle_t *handle);

  void configY2Y(uint8_t y2y_mod);

  void configSize(struct sprdRect *srcRect, struct sprdRect *dstRect,
                  uint32_t pitch, uint32_t height,
                  enum gsp_r7p0_img_layer_format format,
                  enum gsp_rot_angle rot);

  void configEndian(native_handle_t *handle);

  void configFBC(bool inFBC, int format, native_handle_t *handle);

  bool checkFBC(uint32_t pitch, uint32_t height,
                enum gsp_r7p0_img_layer_format format, bool inFBC);

  bool checkInputRotation(struct sprdRect *srcRect, struct sprdRect *dstRect,
                          enum gsp_rot_angle rot,
                          enum gsp_r7p0_img_layer_format format);
  void configInputRot(enum gsp_rot_angle rot);

  bool mAsync;
  bool mOddSupport;
  bool mVideoMaxSize;
  bool mScale_Updown_Sametime;
  uint16_t mScaleRangeUp;
  uint16_t mScaleRangeDown;
  uint16_t mZorder;
  uint16_t mLayer_Count;

  GspRangeSize mRangeSize;

  struct gsp_r7p0_img_layer_user mConfig;
};
} // namespace android

#endif // GSPR7P0PLANE_GSPR7P0PLANEIMAGE_H_
