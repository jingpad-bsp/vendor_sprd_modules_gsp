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

#ifndef GSPLITER2P0PLANE_GSPLITER2P0PLANE_H_
#define GSPLITER2P0PLANE_GSPLITER2P0PLANE_H_

#include "GspPlane.h"
#include "SprdHWLayer.h"
#include "SprdUtil.h"
#include "gsp_cfg.h"
#include "gsp_lite_r2p0_cfg.h"

namespace android {

class GspLiteR2P0Plane : public GspPlane {
public:
  GspLiteR2P0Plane() {}
  virtual ~GspLiteR2P0Plane() {}

protected:
  enum gsp_lite_r2p0_img_layer_format imgFormatConvert(int f);

  enum gsp_lite_r2p0_osd_layer_format osdFormatConvert(int f);

  enum gsp_rot_angle rotationTypeConvert(int32_t angle);

  bool checkRangeSize(struct sprdRect *srcRect, struct sprdRect *dstRect,
                      const GspRangeSize &range);

  bool checkBlending(SprdHWLayer *layer);

  bool checkOddBoundary(struct sprdRect *srcRect, bool even);

  bool isLandScapeTransform(enum gsp_rot_angle rot);

  bool needScale(struct sprdRect *srcRect, struct sprdRect *dstRect,
                 enum gsp_rot_angle rot);

  bool isVideoLayer(enum gsp_lite_r2p0_img_layer_format format);

  bool isVideoLayer(enum gsp_lite_r2p0_osd_layer_format format);

  uint32_t mOutputWidth;
  uint32_t mOutputHeight;
};

} // namespace android

#endif // GSPLITER2P0PLANE_GSPLITER2P0PLANEDST_H_
