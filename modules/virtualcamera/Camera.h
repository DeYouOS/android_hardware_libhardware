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

#ifndef CAMERA_H_
#define CAMERA_H_

#include <CameraMetadata.h>
#include <HandleImporter.h>
#include <hardware/camera3.h>
#include <hardware/hardware.h>
#include <sync/sync.h>
#include <utils/Mutex.h>
#include <utils/Vector.h>

#include <memory>
#include <vector>

#include "Stream.h"

#define CAMERA_SYNC_TIMEOUT_MS 5000

namespace virtual_camera_hal {

using ::android::hardware::camera::common::V1_0::helper::CameraMetadata;
using ::android::hardware::camera::common::V1_0::helper::HandleImporter;
using ::android::hardware::graphics::mapper::V2_0::YCbCrLayout;

// A simple wrapper for camera3_capture_request_t,
// with a constructor that makes a deep copy from the original struct.
struct CaptureRequest {
    uint32_t frame_number;
    CameraMetadata settings;
    std::unique_ptr<camera3_stream_buffer_t> input_buffer;
    std::vector<camera3_stream_buffer_t> output_buffers;

    CaptureRequest() : CaptureRequest(nullptr) {}
    // Create a deep copy of |request|.
    CaptureRequest(const camera3_capture_request_t *request) {
        if (!request)
            return;

        frame_number = request->frame_number;

        // CameraMetadata makes copies of camera_metadata_t through the
        // assignment operator (the constructor taking a camera_metadata_t*
        // takes ownership instead).
        settings = request->settings;

        // camera3_stream_buffer_t can be default copy constructed,
        // as its pointer values are handles, not ownerships.

        // Copy the input buffer.
        if (request->input_buffer)
            input_buffer = std::make_unique<camera3_stream_buffer_t>(
                *request->input_buffer);

        // Safely copy all the output buffers.
        uint32_t num_output_buffers = request->num_output_buffers;
        if (num_output_buffers < 0 || !request->output_buffers)
            num_output_buffers = 0;

        output_buffers.insert(output_buffers.end(), request->output_buffers,
                              request->output_buffers + num_output_buffers);
    }
};

// Camera represents a physical camera on a device.
// This is constructed when the HAL module is loaded, one per physical camera.
// It is opened by the framework, and must be closed before it can be opened
// again.
// This is an abstract class, containing all logic and data shared between all
// camera devices.
class Camera : virtual public android::RefBase {
  public:
    // id is used to distinguish cameras. 0 <= id < NUM_CAMERAS.
    // module is a handle to the HAL module, used when the device is opened.
    explicit Camera(int id);
    virtual ~Camera();

    // Common Camera Device Operations (see <hardware/camera_common.h>)
    int open(const hw_module_t *module, hw_device_t **device);
    int getInfo(struct camera_info *info);
    int close();

    // Camera v3 Device Operations (see <hardware/camera3.h>)
    int initialize(const camera3_callback_ops_t *callback_ops);
    int configureStreams(camera3_stream_configuration_t *stream_list);
    const camera_metadata_t *constructDefaultRequestSettings(int type);
    int processCaptureRequest(camera3_capture_request_t *request);
    int flush();
    void dump(int fd);

    // Update static camera characteristics. This method could be called by
    // HAL hotplug thread when camera is plugged.
    void updateInfo();
    void sendResult(std::shared_ptr<CaptureRequest> request);

  protected:
    // Initialize static camera characteristics.
    virtual android::status_t initStaticInfo(CameraMetadata &) = 0;
    // Verify settings are valid for a capture
    virtual bool isValidCaptureSettings(const camera_metadata_t *) = 0;
    // Separate open method for individual devices
    virtual int openDevice() = 0;
    // Separate initialization method for individual devices when opened
    virtual android::status_t initDevice() = 0;
    // Flush camera pipeline for each individual device
    virtual int flushDevice() = 0;
    // Separate close method for individual devices
    virtual int closeDevice() = 0;
    // Capture and file an output buffer for an input buffer.
    virtual int preprocessCaptureBuffer(camera3_stream_buffer_t *buffer) = 0;
    // Enqueue a request to receive data from the camera
    virtual int enqueueRequest(std::shared_ptr<CaptureRequest> request) = 0;
    virtual android::status_t upRequestMetadata(CameraMetadata &) = 0;
    // Accessor method used by initDevice() to set the templates' metadata
    android::status_t setTemplate(int type, camera_metadata_t *settings);
    // Prettyprint template names
    const char *templateToString(int type);
    // Process an output buffer

    // Identifier used by framework to distinguish cameras
    const int mId;

    // camera_metadata structure containing static characteristics
    CameraMetadata *mCameraMetadata;

  protected:
    // Camera device handle returned to framework for use
    camera3_device_t mDevice;
    // Reuse a stream already created by this device. Must be called with
    // mDeviceLock held.
    Stream *reuseStreamLocked(camera3_stream_t *astream);
    // Destroy all streams in a stream array, and the array itself. Must be
    // called with mDeviceLock held.
    void destroyStreamsLocked(android::Vector<Stream *> &streams);
    // Verify a set of streams is valid in aggregate. Must be called with
    // mDeviceLock held.
    bool isValidStreamSetLocked(const android::Vector<Stream *> &streams);
    // Calculate usage and max_bufs of each stream. Must be called with
    // mDeviceLock held.
    void setupStreamsLocked(android::Vector<Stream *> &streams);
    // Update new settings for re-use and clean up old settings. Must be called
    // with mDeviceLock held.
    void updateSettingsLocked(const camera_metadata_t *new_settings);
    // Send a shutter notify message with start of exposure time
    void notifyShutter(uint32_t frame_number, uint64_t timestamp);

    // Is type a valid template type (and valid index into mTemplates)
    bool isValidTemplateType(int type);

    // Busy flag indicates camera is in use
    bool mBusy;
    // Camera device operations handle shared by all devices
    const static camera3_device_ops_t sOps;
    // Methods used to call back into the framework
    const camera3_callback_ops_t *mCallbackOps;
    // Lock protecting the Camera object for modifications
    android::Mutex mDeviceLock;
    // Lock protecting only static camera characteristics, which may
    // be accessed without the camera device open
    android::Mutex mStaticInfoLock;
    // Array of handles to streams currently in use by the device
    android::Vector<Stream *> mStreams;
    // Static array of standard camera settings templates
    camera_metadata_t *mTemplates[CAMERA3_TEMPLATE_COUNT];
    // Most recent request settings seen, memoized to be reused
    camera_metadata_t *mSettings;
    bool mIsInitialized;
};
} // namespace virtual_camera_hal

#endif // CAMERA_H_
