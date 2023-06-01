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

#ifndef EXAMPLE_CAMERA_H_
#define EXAMPLE_CAMERA_H_

#include <hardware/gralloc1.h>
#include <system/camera_metadata.h>
#include <utils/Thread.h>

#include <condition_variable>
#include <functional>
#include <queue>
#include <string>

#include "Camera.h"
#include "VirtualCameraService.h"
#include "arc/frame_buffer.h"

namespace virtual_camera_hal {
#define ADD_STATIC_ENTRY(TYPE, TAG, args...)                                   \
    TYPE tag_##TAG[] = args;                                                   \
    res = base.update(TAG, tag_##TAG, ARRAY_SIZE(tag_##TAG));                  \
    if (res != android::OK) {                                                  \
        LOGF(ERROR) << " " << mId << " CameraMetadata update fail " << #TAG;   \
        return res;                                                            \
    }

#define ADD_STATIC_ENTRY_UINT8(TAG, args...)                                   \
    ADD_STATIC_ENTRY(uint8_t, TAG, args)

#define ADD_STATIC_ENTRY_INT32(TAG, args...)                                   \
    ADD_STATIC_ENTRY(int32_t, TAG, args)

#define ADD_STATIC_ENTRY_FLOAT(TAG, args...) ADD_STATIC_ENTRY(float, TAG, args)

#define ADD_STATIC_ENTRY_INT64(TAG, args...)                                   \
    ADD_STATIC_ENTRY(int64_t, TAG, args)

#define ADD_STATIC_ENTRY_DOUBLE(TAG, args...)                                  \
    ADD_STATIC_ENTRY(double, TAG, args)

#define ADD_STATIC_ENTRY_RATIONAL(TAG, args...)                                \
    ADD_STATIC_ENTRY(camera_metadata_rational_t, TAG, args)

class FunctionThread : public android::Thread {
  public:
    FunctionThread(std::function<bool()> function) : function_(function){};

  private:
    bool threadLoop() override {
        bool result = function_();
        return result;
    };

    std::function<bool()> function_;
};

/**
 * VirtualCamera is an example for a specific camera device. The Camera instance
 * contains a specific camera device (e.g. VirtualCamera) holds all specific
 * metadata and logic about that device.
 */
class VirtualCamera : public Camera {
  public:
    explicit VirtualCamera(std::shared_ptr<VirtualCameraService> &service,
                           int id);
    ~VirtualCamera();

  private:
    // Initialize static camera characteristics for individual device
    android::status_t initStaticInfo(CameraMetadata &base);
    int openDevice();
    // Initialize whole device (templates/etc) when opened
    int initDevice();
    int flushDevice();
    int closeDevice();
    int preprocessCaptureBuffer(camera3_stream_buffer_t *buffer);
    // Enqueue a request to receive data from the camera.
    int enqueueRequest(std::shared_ptr<CaptureRequest> request) override;
    android::status_t upRequestMetadata(CameraMetadata &base) override;

    // Async request processing helpers.
    // Dequeue a request from the waiting queue.
    // Blocks until a request is available.
    std::shared_ptr<CaptureRequest> dequeueRequest();
    bool enqueueRequestBuffers();

    // Initialize each template metadata controls
    android::status_t initPreviewTemplate(CameraMetadata &base);
    android::status_t initStillTemplate(CameraMetadata &base);
    android::status_t initRecordTemplate(CameraMetadata &base);
    android::status_t initSnapshotTemplate(CameraMetadata &base);
    android::status_t initZslTemplate(CameraMetadata &base);
    android::status_t initManualTemplate(CameraMetadata &base);

    // Verify settings are valid for a capture with this device
    bool isValidCaptureSettings(const camera_metadata_t *settings);
    uint32_t HalToV4L2PixelFormat(int hal_pixel_format);

  private:
    std::mutex request_queue_lock_;
    std::queue<std::shared_ptr<CaptureRequest>> request_queue_;
    std::condition_variable requests_available_;
    android::sp<android::Thread> buffer_enqueuer_;
    std::shared_ptr<VirtualCameraService> m_VirtualCameraService;
};
} // namespace virtual_camera_hal

#endif // CAMERA_H_
