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
#ifndef GSPR6P0PLANE_GSPR6P0PLANEOSD_H_
#define GSPR6P0PLANE_GSPR6P0PLANEOSD_H_

#include "GspR6P0Plane.h"
#include "gralloc_public.h"
#include "SprdHWLayer.h"
#include "gsp_r6p0_cfg.h"

namespace android {

class GspR6P0PlaneOsd : public GspR6P0Plane {
public:
  GspR6P0PlaneOsd(bool async, const GspRangeSize &range);
  virtual ~GspR6P0PlaneOsd() {}

  void reset(int flag);

  bool adapt(SprdHWLayer *layer, int index);

  bool parcel(SprdHWLayer *layer);

  bool parcel(uint32_t w, uint32_t h);

  struct gsp_r6p0_osd_layer_user &getConfig();

private:
  void configCommon(int wait_fd, int share_fd, int enable);

  void configSize(struct sprdRect *srcRect, struct sprdRect *dstRect,
                  uint32_t pitch, uint32_t height);

  void configSize(uint32_t w, uint32_t h);

  void configFormat(enum gsp_r6p0_osd_layer_format format);

  void configAlpha(uint8_t alpha);

  void configZorder(uint8_t zorder);

  void configPmargbMode(uint32_t blendMode);

  void configEndian(native_handle_t *handle);

  void configPallet(int enable);

  void configFBC(bool inFBC, int format, native_handle_t *handle);

  bool checkFBC(uint32_t pitch, uint32_t height,
                enum gsp_r6p0_osd_layer_format format, bool inFBC);

  GspRangeSize mRangeSize;

  struct gsp_r6p0_osd_layer_user mConfig;
};
} // namespace android

#endif // GSPR6P0PLANE_GSPR6P0PLANEOSD_H_
