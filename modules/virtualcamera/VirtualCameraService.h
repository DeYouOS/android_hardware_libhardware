/*
 * Copyright (C) 2015 The Android Open Source Project
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
#pragma once
#ifndef VIRTUAL_CAMERA_SERVICE_H_
#define VIRTUAL_CAMERA_SERVICE_H_

#include "arc/frame_buffer.h"
#include <aidl/android/hardware/virtualmedia/BnNuPlayerService.h>
#include <aidl/android/hardware/virtualmedia/BnVirtualMedia.h>
#include <utils/RefBase.h>

namespace virtual_camera_hal {

/**
 * VirtualCamera is an example for a specific camera device. The Camera instance
 * contains a specific camera device (e.g. VirtualCamera) holds all specific
 * metadata and logic about that device.
 */
class VirtualCameraService : virtual public android::RefBase {
  public:
    explicit VirtualCameraService();
    ~VirtualCameraService();

  public:
    void OnDeath();
    void OnUnlink();
    void OnDeathNuPlayer();
    void OnUnlinkNuPlayer();

  public:
    std::shared_ptr<arc::FrameBuffer> getCameraBuffer();

    std::shared_ptr<::aidl::android::hardware::virtualmedia::IVirtualMedia> &
    getVirtualMedia();

    std::shared_ptr<::aidl::android::hardware::virtualmedia::INuPlayerService> &
    getNuPlayerService();

  private:
    std::shared_ptr<::aidl::android::hardware::virtualmedia::IVirtualMedia>
        m_IVirtualMedia;
    std::shared_ptr<::aidl::android::hardware::virtualmedia::INuPlayerService>
        m_INuPlayerService;
};
} // namespace virtual_camera_hal

#endif // VIRTUAL_CAMERA_SERVICE_H_
