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
#include <stdint.h>

#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL)
#include <libyuv/convert.h>
#include <linux/videodev2.h>
#include <system/camera_metadata.h>
#include <system/graphics.h>
#include <utils/Trace.h>

#include "Camera.h"
#include "VirtualCamera.h"
#include "arc/cached_frame.h"
#include "arc/common.h"
#include "arc/native_handle.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

namespace virtual_camera_hal {

VirtualCamera::VirtualCamera(std::shared_ptr<VirtualCameraService> &service,
                             int id)
    : Camera(id), buffer_enqueuer_(new FunctionThread(
                      std::bind(&VirtualCamera::enqueueRequestBuffers, this))),
      m_VirtualCameraService(service) {
    buffer_enqueuer_->run("virtual-camera-buffer");
}

VirtualCamera::~VirtualCamera() {
    // Stop hotplug thread
    if (buffer_enqueuer_ != NULL)
        buffer_enqueuer_->requestExit();

    // Joining done without holding mLock, otherwise deadlocks may ensue
    // as the threads try to access parent states.
    if (buffer_enqueuer_ != NULL)
        buffer_enqueuer_->join();
}

android::status_t VirtualCamera::initStaticInfo(CameraMetadata &base) {
    /*
     * Setup static camera info.  This will have to customized per camera
     * device.
     * TODO: this is just some sample code, need tailor for USB cameras.
     */
    if (base.isEmpty())
        base.clear();

    android::status_t res = android::OK;

    /* android.control */
    // android.colorCorrection.availableAberrationModes [static, byte[], public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES,
                           {0, 1, 2});

    // android.control.aeAvailableAntibandingModes [static, byte[], public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_CONTROL_AE_AVAILABLE_ANTIBANDING_MODES,
                           {0, 3});

    // android.control.aeAvailableModes [static, byte[], public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_CONTROL_AE_AVAILABLE_MODES, {0, 1, 2, 3});

    // android.control.aeAvailableTargetFpsRanges [static, int32[], public]
    ADD_STATIC_ENTRY_INT32(ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES,
                           {15, 15, 5, 30, 15, 30, 30, 30});

    // android.control.aeCompensationRange [static, int32[], public]
    ADD_STATIC_ENTRY_INT32(ANDROID_CONTROL_AE_COMPENSATION_RANGE, {-9, 9});

    // android.control.aeCompensationStep [static, rational, public]
    ADD_STATIC_ENTRY_RATIONAL(ANDROID_CONTROL_AE_COMPENSATION_STEP, {{1, 3}});

    // android.control.aeLockAvailable [static, enum, public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_CONTROL_AE_LOCK_AVAILABLE,
                           {ANDROID_CONTROL_AE_LOCK_AVAILABLE_FALSE});

    // android.control.afAvailableModes [static, byte[], public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_CONTROL_AF_AVAILABLE_MODES, {0, 1, 2, 3, 4});

    // android.control.availableExtendedSceneModeMaxSizes [static, int32[],
    // ndk_public]
    ADD_STATIC_ENTRY_INT32(
        ANDROID_CONTROL_AVAILABLE_EXTENDED_SCENE_MODE_MAX_SIZES,
        {0, 0, 0, 1, 1856, 1392, 64, 1856, 1392});

    // android.control.availableExtendedSceneModeZoomRatioRanges [static,
    // float[], ndk_public]
    ADD_STATIC_ENTRY_FLOAT(
        ANDROID_CONTROL_AVAILABLE_EXTENDED_SCENE_MODE_ZOOM_RATIO_RANGES,
        {1.0, 4.0, 1.0, 4.0});

    // android.control.availableEffects [static, byte[], public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_CONTROL_AVAILABLE_EFFECTS, {0});

    // android.control.availableModes [static, byte[], public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_CONTROL_AVAILABLE_MODES, {0, 1, 2, 4});

    // android.control.availableSceneModes [static, byte[], public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_CONTROL_AVAILABLE_SCENE_MODES, {1});

    // android.control.availableVideoStabilizationModes [static, byte[], public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES,
                           {0, 1});

    // android.control.awbAvailableModes [static, byte[], public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_CONTROL_AWB_AVAILABLE_MODES,
                           {0, 1, 2, 3, 5, 8});

    //  android.control.awbLockAvailable [static, enum, public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_CONTROL_AWB_LOCK_AVAILABLE,
                           {ANDROID_CONTROL_AWB_LOCK_AVAILABLE_FALSE});

    // android.control.maxRegions [static, int32[], ndk_public]
    ADD_STATIC_ENTRY_INT32(ANDROID_CONTROL_MAX_REGIONS, {1, 0, 1});

    // android.control.postRawSensitivityBoostRange [static, int32[], public]
    ADD_STATIC_ENTRY_INT32(ANDROID_CONTROL_POST_RAW_SENSITIVITY_BOOST_RANGE,
                           {100, 100});

    // android.control.sceneModeOverrides [static, byte[], system]
    ADD_STATIC_ENTRY_UINT8(ANDROID_CONTROL_SCENE_MODE_OVERRIDES, {1, 1, 1});

    // android.control.zoomRatioRange [static, float[], public]
    ADD_STATIC_ENTRY_FLOAT(ANDROID_CONTROL_ZOOM_RATIO_RANGE, {1.0, 10.0});

    // android.edge.availableEdgeModes [static, byte[], public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_EDGE_AVAILABLE_EDGE_MODES, {0, 1, 2, 3});

    // android.flash.info.available [static, enum, public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_FLASH_INFO_AVAILABLE,
                           {ANDROID_FLASH_INFO_AVAILABLE_FALSE});

    // android.flash.state [dynamic, enum, public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_FLASH_STATE,
                           {ANDROID_FLASH_STATE_UNAVAILABLE});

    // android.flash.mode [dynamic, enum, public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_FLASH_MODE, {ANDROID_FLASH_MODE_OFF});

    // android.hotPixel.availableHotPixelModes [static, byte[], public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_HOT_PIXEL_AVAILABLE_HOT_PIXEL_MODES,
                           {0, 1, 2});

    // android.info.supportedHardwareLevel [static, enum, public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL,
                           {ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_3});

    // android.jpeg.availableThumbnailSizes [static, int32[], public]
    ADD_STATIC_ENTRY_INT32(ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES,
                           {0, 0, 160, 120, 320, 240});

    // android.jpeg.maxSize [static, int32, system]
    ADD_STATIC_ENTRY_INT32(ANDROID_JPEG_MAX_SIZE, {13 * 1024 * 1024}); // 13MB

    // android.lens.facing [static, enum, public]
    if (mId == 0) {
        uint8_t android_lens_facing[] = {ANDROID_LENS_FACING_BACK};
        base.update(ANDROID_LENS_FACING, android_lens_facing,
                    ARRAY_SIZE(android_lens_facing));
    } else if (mId == 1) {
        uint8_t android_lens_facing[] = {ANDROID_LENS_FACING_FRONT};
        base.update(ANDROID_LENS_FACING, android_lens_facing,
                    ARRAY_SIZE(android_lens_facing));
    } else {
        uint8_t android_lens_facing[] = {ANDROID_LENS_FACING_EXTERNAL};
        base.update(ANDROID_LENS_FACING, android_lens_facing,
                    ARRAY_SIZE(android_lens_facing));
    }

    // android.lens.info.availableApertures [static, float[], public]
    ADD_STATIC_ENTRY_FLOAT(ANDROID_LENS_INFO_AVAILABLE_APERTURES, {2.79999995});

    // android.lens.info.availableFilterDensities [static, float[], public]
    ADD_STATIC_ENTRY_FLOAT(ANDROID_LENS_INFO_AVAILABLE_FILTER_DENSITIES,
                           {0.00000000});

    // android.lens.info.availableFocalLengths [static, float[], public]
    ADD_STATIC_ENTRY_FLOAT(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS,
                           {3.29999995});

    // android.lens.info.availableOpticalStabilization [static, byte[], public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION,
                           {0});

    // android.lens.info.focusDistanceCalibration [static, enum, public]
    ADD_STATIC_ENTRY_UINT8(
        ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION,
        {ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_APPROXIMATE});

    // android.lens.info.hyperfocalDistance [static, float, public]
    ADD_STATIC_ENTRY_FLOAT(ANDROID_LENS_INFO_HYPERFOCAL_DISTANCE, {0.20000000});

    // android.lens.info.minimumFocusDistance [static, float, public]
    ADD_STATIC_ENTRY_FLOAT(ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE,
                           {20.00000000});

    // android.lens.info.shadingMapSize [static, int32[], ndk_public]
    ADD_STATIC_ENTRY_INT32(ANDROID_LENS_INFO_SHADING_MAP_SIZE, {17, 13});

    // android.noiseReduction.availableNoiseReductionModes [static, byte[],
    // public]
    ADD_STATIC_ENTRY_UINT8(
        ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES, {0, 1, 2, 4});

    // android.request.availableCapabilities [static, enum[], public]
    ADD_STATIC_ENTRY_UINT8(
        ANDROID_REQUEST_AVAILABLE_CAPABILITIES,
        {ANDROID_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE,
         ANDROID_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR,
         ANDROID_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_POST_PROCESSING,
         ANDROID_REQUEST_AVAILABLE_CAPABILITIES_READ_SENSOR_SETTINGS,
         ANDROID_REQUEST_AVAILABLE_CAPABILITIES_BURST_CAPTURE,
         ANDROID_REQUEST_AVAILABLE_CAPABILITIES_PRIVATE_REPROCESSING,
         ANDROID_REQUEST_AVAILABLE_CAPABILITIES_YUV_REPROCESSING,
         ANDROID_REQUEST_AVAILABLE_CAPABILITIES_RAW});

    // android.sensor.referenceIlluminant1 [static, enum, public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_SENSOR_REFERENCE_ILLUMINANT1,
                           {ANDROID_SENSOR_REFERENCE_ILLUMINANT1_D50});

    // android.statistics.info.availableHotPixelMapModes [static, byte[],
    // public]
    ADD_STATIC_ENTRY_UINT8(
        ANDROID_STATISTICS_INFO_AVAILABLE_HOT_PIXEL_MAP_MODES, {0, 1});

    // android.request.maxNumInputStreams [static, int32, java_public]
    ADD_STATIC_ENTRY_INT32(ANDROID_REQUEST_MAX_NUM_INPUT_STREAMS, {1});

    // android.scaler.availableInputOutputFormatsMap [static, int32, hidden]
    ADD_STATIC_ENTRY_INT32(ANDROID_SCALER_AVAILABLE_INPUT_OUTPUT_FORMATS_MAP,
                           {34, 2, 33, 35, 35, 2, 33, 35});

    // android.reprocess.maxCaptureStall [static, int32, java_public]
    ADD_STATIC_ENTRY_INT32(ANDROID_REPROCESS_MAX_CAPTURE_STALL, {2});

    // android.request.availableCharacteristicsKeys [static, int32[],
    // ndk_public]
    ADD_STATIC_ENTRY_INT32(
        ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS,
        {983043,     983044, 983041,  917517,  983045,  983046,  983040,
         917518,     983048, 983042,  983047,  917516,  917529,  589826,
         589829,     589828, 589824,  589825,  589827,  589830,  589831,
         524293,     327680, 1245188, 1245189, 851978,  851979,  851980,
         851981,     851972, 458759,  458760,  1179648, 1179650, 1179655,
         1507329,    65574,  65561,   65560,   65564,   65555,   65558,
         65557,      65556,  65554,   65572,   65563,   65573,   65559,
         65562,      4,      196610,  1376256, 655362,  1048578, 786438,
         786442,     786443, 786444,  786445,  786446,  786447,  65575,
         65579,      65580,  65582,   983050,  393217,  1572865, 786440,
         851977,     917507, 917509,  917511,  917513,  1179654, 851984,
         -2080374781});

    // android.request.availableRequestKeys [static, int32[], ndk_public]
    ADD_STATIC_ENTRY_INT32(
        ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS,
        {786435,  786433,      786432,      524290,     524291,  524288,
         524289,  524292,      917504,      917505,     917506,  262146,
         262144,  262145,      393216,      131072,     655360,  1048576,
         0,       1245187,     196608,      1,          2,       1245186,
         1245185, 1245184,     851968,      458756,     458758,  458757,
         458752,  458753,      458754,      458755,     1114112, 1114115,
         65549,   65551,       65541,       65550,      65552,   65539,
         65538,   65540,       65537,       65536,      65542,   65547,
         65546,   65543,       65544,       65545,      65553,   1441792,
         1114128, 3,           917528,      65576,      65581,   65583,
         851985,  -2080374783, -2080374782, -2080374780});

    // android.request.availableResultKeys [static, int32[], ndk_public]
    ADD_STATIC_ENTRY_INT32(
        ANDROID_REQUEST_AVAILABLE_RESULT_KEYS,
        {786435,  786433,  786432, 524290,      524291,     524288,
         524289,  524292,  917504, 917505,      917506,     262146,
         262144,  262145,  393216, 131072,      655360,     1048576,
         0,       1245187, 196608, 1,           2,          1245186,
         1245185, 1245184, 851968, 458756,      458758,     458757,
         458752,  458753,  458754, 458755,      1114112,    1114115,
         65549,   65551,   65541,  65550,       65552,      65539,
         65538,   65540,   65537,  65536,       65542,      65547,
         65546,   65543,   65544,  65545,       65553,      1441792,
         1114128, 3,       917528, 65567,       65568,      65570,
         262149,  524297,  524296, 917530,      1114126,    1114123,
         786441,  917520,  917522, 65576,       65581,      65583,
         917523,  917526,  851985, -2080374783, -2080374782});

    // android.request.availableSessionKeys [static, int32[], ndk_public]
    ADD_STATIC_ENTRY_INT32(ANDROID_REQUEST_AVAILABLE_SESSION_KEYS,
                           {786435, -2080374782});

    // android.request.maxNumOutputStreams [static, int32[], ndk_public]
    ADD_STATIC_ENTRY_INT32(ANDROID_REQUEST_MAX_NUM_OUTPUT_STREAMS, {1, 3, 1});

    // android.request.partialResultCount [static, int32, public]
    ADD_STATIC_ENTRY_INT32(ANDROID_REQUEST_PARTIAL_RESULT_COUNT, {1});

    // android.request.pipelineMaxDepth [static, byte, public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_REQUEST_PIPELINE_MAX_DEPTH, {4});

    // android.scaler.availableMaxDigitalZoom [static, float, public]
    ADD_STATIC_ENTRY_FLOAT(ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM,
                           {10.00000000});

    // android.scaler.availableRotateAndCropModes [static, byte[], public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_SCALER_AVAILABLE_ROTATE_AND_CROP_MODES,
                           {0, 1, 4});

    // android.scaler.availableMinFrameDurations [static, int64[], ndk_public]
    ADD_STATIC_ENTRY_INT64(ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS,
                           {32, 1856, 1392, 33331760, 33, 1856, 1392, 33331760,
                            33, 1280, 720,  33331760, 34, 160,  120,  33331760,
                            34, 320,  240,  33331760, 35, 160,  120,  33331760,
                            35, 320,  240,  33331760, 33, 320,  240,  33331760,
                            34, 640,  480,  33331760, 35, 640,  480,  33331760,
                            33, 640,  480,  33331760, 34, 1280, 720,  33331760,
                            34, 1856, 1392, 33331760, 35, 1280, 720,  33331760,
                            35, 1856, 1392, 33331760, 1,  1600, 1200, 33331760,
                            34, 176,  144,  33331760, 35, 176,  144,  33331760,
                            33, 176,  144,  33331760, 34, 1024, 768,  33331760,
                            35, 1024, 768,  33331760, 33, 1024, 768,  33331760,
                            54, 1024, 768,  33331760});

    // android.scaler.availableStallDurations [static, int64[], ndk_public]
    ADD_STATIC_ENTRY_INT64(ANDROID_SCALER_AVAILABLE_STALL_DURATIONS,
                           {HAL_PIXEL_FORMAT_RAW16,
                            1856,
                            1392,
                            33331760,
                            HAL_PIXEL_FORMAT_BLOB,
                            1856,
                            1392,
                            33331760,
                            HAL_PIXEL_FORMAT_BLOB,
                            1280,
                            720,
                            33331760,
                            HAL_PIXEL_FORMAT_BLOB,
                            1024,
                            768,
                            33331760,
                            HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
                            160,
                            120,
                            0,
                            HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
                            320,
                            240,
                            0,
                            HAL_PIXEL_FORMAT_YCbCr_420_888,
                            160,
                            120,
                            0,
                            HAL_PIXEL_FORMAT_YCbCr_420_888,
                            320,
                            240,
                            0,
                            1,
                            320,
                            240,
                            0,
                            HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
                            640,
                            480,
                            0,
                            HAL_PIXEL_FORMAT_YCbCr_420_888,
                            640,
                            480,
                            0,
                            HAL_PIXEL_FORMAT_BLOB,
                            640,
                            480,
                            33331760,
                            HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
                            1280,
                            720,
                            0,
                            HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
                            1856,
                            1392,
                            0,
                            HAL_PIXEL_FORMAT_YCbCr_420_888,
                            1280,
                            720,
                            0,
                            HAL_PIXEL_FORMAT_YCbCr_420_888,
                            1856,
                            1392,
                            0,
                            1,
                            1600,
                            1200,
                            0,
                            54,
                            1024,
                            768,
                            0});

    // android.scaler.availableStreamConfigurations [static, enum[], ndk_public]
    ADD_STATIC_ENTRY_INT32(
        ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
        {HAL_PIXEL_FORMAT_YCbCr_420_888,
         1856,
         1392,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT,
         HAL_PIXEL_FORMAT_BLOB,
         1856,
         1392,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         HAL_PIXEL_FORMAT_BLOB,
         1280,
         720,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
         160,
         120,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
         320,
         240,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         HAL_PIXEL_FORMAT_YCbCr_420_888,
         160,
         120,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         HAL_PIXEL_FORMAT_YCbCr_420_888,
         320,
         240,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         HAL_PIXEL_FORMAT_BLOB,
         320,
         240,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
         640,
         480,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         HAL_PIXEL_FORMAT_YCbCr_420_888,
         640,
         480,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         HAL_PIXEL_FORMAT_BLOB,
         640,
         480,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
         1280,
         720,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
         1856,
         1392,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT,
         HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
         1856,
         1392,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         HAL_PIXEL_FORMAT_YCbCr_420_888,
         1280,
         720,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         HAL_PIXEL_FORMAT_YCbCr_420_888,
         1856,
         1392,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         HAL_PIXEL_FORMAT_RAW16,
         1856,
         1392,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT,
         1600,
         1200,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
         176,
         144,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         HAL_PIXEL_FORMAT_YCbCr_420_888,
         176,
         144,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         HAL_PIXEL_FORMAT_BLOB,
         176,
         144,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
         1024,
         768,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         HAL_PIXEL_FORMAT_YCbCr_420_888,
         1024,
         768,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         HAL_PIXEL_FORMAT_BLOB,
         1024,
         768,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT,
         54,
         1024,
         768,
         ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT});

    // android.scaler.croppingType [static, enum, public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_SCALER_CROPPING_TYPE,
                           {ANDROID_SCALER_CROPPING_TYPE_CENTER_ONLY});

    /*
    // android.sensor.calibrationTransform1 [static, rational[], public]
    ADD_STATIC_ENTRY_RATIONAL(ANDROID_SENSOR_CALIBRATION_TRANSFORM1,
                 {{128, 128, 0, 128, 0, 128, 0, 128, 128, 128, 0, 128, 0, 128,
                   0, 128, 128, 128}});

    // android.sensor.colorTransform1 [static, rational[], public]
    ADD_STATIC_ENTRY_RATIONAL(ANDROID_SENSOR_COLOR_TRANSFORM1,
                 {{3209, 1024, -1655, 1024, -502, 1024, -1002, 1024, 1962, 1024,
                   34, 1024, 73, 1024, -234, 1024, 1438, 1024}});

    // android.sensor.forwardMatrix1 [static, rational[], public]
    ADD_STATIC_ENTRY_RATIONAL(ANDROID_SENSOR_FORWARD_MATRIX1,
                 {{446, 1024, 394, 1024, 146, 1024, 227, 1024, 734, 1024, 62,
                   1024, 14, 1024, 99, 1024, 731, 1024}});
    */

    // android.sensor.availableTestPatternModes [static, int32[], public]
    ADD_STATIC_ENTRY_INT32(ANDROID_SENSOR_AVAILABLE_TEST_PATTERN_MODES,
                           {0, 1, 5});

    // android.sensor.blackLevelPattern [static, int32[], public]
    ADD_STATIC_ENTRY_INT32(ANDROID_SENSOR_BLACK_LEVEL_PATTERN,
                           {64, 64, 64, 64});

    // android.sensor.info.activeArraySize [static, int32[], public]
    ADD_STATIC_ENTRY_INT32(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE,
                           {0, 0, 1856, 1392});

    // android.sensor.info.colorFilterArrangement [static, enum, public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT,
                           {ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGGB});

    // android.sensor.info.exposureTimeRange [static, int64[], public]
    ADD_STATIC_ENTRY_INT64(ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE,
                           {1000, 300000000});

    // android.sensor.info.maxFrameDuration [static, int64, public]
    ADD_STATIC_ENTRY_INT64(ANDROID_SENSOR_INFO_MAX_FRAME_DURATION, {300000000});

    // android.sensor.info.physicalSize [static, float[], public]
    ADD_STATIC_ENTRY_FLOAT(ANDROID_SENSOR_INFO_PHYSICAL_SIZE,
                           {3.20000005, 2.40000010});

    // android.sensor.info.pixelArraySize [static, int32[], public]
    ADD_STATIC_ENTRY_INT32(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE, {1856, 1392});

    // android.sensor.info.preCorrectionActiveArraySize [static, int32[],
    // public]
    ADD_STATIC_ENTRY_INT32(ANDROID_SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE,
                           {0, 0, 1856, 1392});

    // android.sensor.info.sensitivityRange [static, int32[], public]
    ADD_STATIC_ENTRY_INT32(ANDROID_SENSOR_INFO_SENSITIVITY_RANGE, {100, 1600});

    // android.sensor.info.timestampSource [static, enum, public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE,
                           {ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_REALTIME});

    // android.sensor.info.whiteLevel [static, int32, public]
    ADD_STATIC_ENTRY_INT32(ANDROID_SENSOR_INFO_WHITE_LEVEL, {4000});

    // android.sensor.maxAnalogSensitivity [static, int32, public]
    ADD_STATIC_ENTRY_INT32(ANDROID_SENSOR_MAX_ANALOG_SENSITIVITY, {1600});

    // android.sensor.orientation [static, int32, public]
    ADD_STATIC_ENTRY_INT32(ANDROID_SENSOR_ORIENTATION, {0});

    // android.shading.availableModes [static, byte[], public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_SHADING_AVAILABLE_MODES, {0, 1, 2});

    // android.statistics.info.availableFaceDetectModes [static, byte[], public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES,
                           {0, 1, 2});

    // android.statistics.info.availableLensShadingMapModes [static, byte[],
    // public]
    ADD_STATIC_ENTRY_UINT8(
        ANDROID_STATISTICS_INFO_AVAILABLE_LENS_SHADING_MAP_MODES, {0, 1});

    // android.statistics.info.maxFaceCount [static, int32, public]
    ADD_STATIC_ENTRY_INT32(ANDROID_STATISTICS_INFO_MAX_FACE_COUNT, {8});

    // android.sync.maxLatency [static, enum, public]
    ADD_STATIC_ENTRY_INT32(ANDROID_SYNC_MAX_LATENCY,
                           {ANDROID_SYNC_MAX_LATENCY_PER_FRAME_CONTROL});

    // android.tonemap.availableToneMapModes [static, byte[], public]
    ADD_STATIC_ENTRY_UINT8(ANDROID_TONEMAP_AVAILABLE_TONE_MAP_MODES, {0, 1, 2});

    // android.tonemap.maxCurvePoints [static, int32, public]
    ADD_STATIC_ENTRY_INT32(ANDROID_TONEMAP_MAX_CURVE_POINTS, {128});

    // android.lens.focalLength [dynamic, float, public]
    ADD_STATIC_ENTRY_FLOAT(ANDROID_LENS_FOCAL_LENGTH, {5.0f});

    // android.flash.firingPower [dynamic, byte, system]
    ADD_STATIC_ENTRY_UINT8(ANDROID_FLASH_FIRING_POWER, {10});

    // android.flash.firingTime [dynamic, int64, system]
    ADD_STATIC_ENTRY_INT64(ANDROID_FLASH_FIRING_TIME, {0});

    upRequestMetadata(base);
    return res;
}

android::status_t VirtualCamera::upRequestMetadata(CameraMetadata &base) {
    android::status_t res = android::OK;
    // Set current BOOT_TIME timestamp in nanoseconds
    struct timespec ts;
    if (clock_gettime(CLOCK_BOOTTIME, &ts) == 0) {
        static const int64_t kNsPerSec = 1000000000;
        int64_t buffer_timestamp = ts.tv_sec * kNsPerSec + ts.tv_nsec;
        ADD_STATIC_ENTRY_INT64(ANDROID_SENSOR_TIMESTAMP, {buffer_timestamp});
    } else {
        LOGF(ERROR) << " " << mId << " clock_gettime: " << strerror(errno);
    }
    ADD_STATIC_ENTRY_FLOAT(ANDROID_LENS_FOCAL_LENGTH, {5.0f});
    ADD_STATIC_ENTRY_UINT8(ANDROID_STATISTICS_SCENE_FLICKER,
                           {ANDROID_STATISTICS_SCENE_FLICKER_NONE});
    ADD_STATIC_ENTRY_UINT8(ANDROID_FLASH_STATE,
                           {ANDROID_FLASH_STATE_UNAVAILABLE});
    ADD_STATIC_ENTRY_INT64(ANDROID_SENSOR_ROLLING_SHUTTER_SKEW, {0});
    ADD_STATIC_ENTRY_FLOAT(ANDROID_LENS_FOCUS_RANGE, {1.0f / 5.0f, 0});
    return res;
}

int VirtualCamera::openDevice() {
    // TODO: implement usb camera device open sequence: open device nodes etc.

    return 0;
}

int VirtualCamera::closeDevice() {
    // TODO: implement usb camera device close sequence: close device nodes etc.

    return 0;
}

int VirtualCamera::flushDevice() { return 0; }

int VirtualCamera::preprocessCaptureBuffer(camera3_stream_buffer_t *buffer) {
    int res;
    // TODO(b/29334616): This probably should be non-blocking; part
    // of the asynchronous request processing.
    if (buffer->acquire_fence != -1) {
        res = sync_wait(buffer->acquire_fence, CAMERA_SYNC_TIMEOUT_MS);
        if (res == -ETIME) {
            LOGF(ERROR) << " " << mId
                        << " Timeout waiting on buffer acquire fence";
            return res;
        } else if (res) {
            LOGF(ERROR) << " " << mId
                        << " Error waiting on buffer acquire fence: "
                        << strerror(-res) << "(" << res << ")";
            return res;
        }
        ::close(buffer->acquire_fence);
    }

    // Acquire fence has been waited upon.
    buffer->acquire_fence = -1;
    // No release fence waiting unless the device sets it.
    buffer->release_fence = -1;
    buffer->status = CAMERA3_BUFFER_STATUS_OK;
    return 0;
}

int VirtualCamera::enqueueRequest(std::shared_ptr<CaptureRequest> request) {
    // Assume request validated before calling this function.
    // (For now, always exactly 1 output buffer, no inputs).
    {
        std::lock_guard<std::mutex> guard(request_queue_lock_);
        request_queue_.push(request);
        requests_available_.notify_one();
    }
    return 0;
}

std::shared_ptr<CaptureRequest> VirtualCamera::dequeueRequest() {
    std::unique_lock<std::mutex> lock(request_queue_lock_);
    while (request_queue_.empty()) {
        requests_available_.wait(lock);
    }
    std::shared_ptr<CaptureRequest> request = request_queue_.front();
    request_queue_.pop();
    return request;
}

// GetDataPointer(entry, val)
//
// A helper for other methods in this file.
// Gets the data pointer of a given metadata entry into |*val|.

template <typename T>
inline void GetDataPointer(camera_metadata_ro_entry_t &, const T **);

template <>
inline void GetDataPointer<uint8_t>(camera_metadata_ro_entry_t &entry,
                                    const uint8_t **val) {
    *val = entry.data.u8;
}

template <>
inline void GetDataPointer<int32_t>(camera_metadata_ro_entry_t &entry,
                                    const int32_t **val) {
    *val = entry.data.i32;
}

template <>
inline void GetDataPointer<float>(camera_metadata_ro_entry_t &entry,
                                  const float **val) {
    *val = entry.data.f;
}

template <>
inline void GetDataPointer<int64_t>(camera_metadata_ro_entry_t &entry,
                                    const int64_t **val) {
    *val = entry.data.i64;
}

template <>
inline void GetDataPointer<double>(camera_metadata_ro_entry_t &entry,
                                   const double **val) {
    *val = entry.data.d;
}

template <>
inline void GetDataPointer<camera_metadata_rational_t>(
    camera_metadata_ro_entry_t &entry, const camera_metadata_rational_t **val) {
    *val = entry.data.r;
}

// Singleton.
template <typename T>
static int SingleTagValue(const CameraMetadata &metadata, int32_t tag, T *val) {
    if (!val) {
        LOGF(ERROR) << " Null pointer passed to SingleTagValue.";
        return -EINVAL;
    }
    camera_metadata_ro_entry_t entry = metadata.find(tag);
    if (entry.count == 0) {
        LOGF(ERROR) << " Metadata tag " << tag << " is empty.";
        return -ENOENT;
    } else if (entry.count != 1) {
        LOGF(ERROR) << " Error: expected metadata tag " << tag
                    << " to contain exactly 1 value "
                       "(had "
                    << entry.count << ").";
        return -EINVAL;
    }
    const T *data = nullptr;
    GetDataPointer(entry, &data);
    if (data == nullptr) {
        LOGF(ERROR) << " Metadata tag " << tag << " is empty.";
        return -ENODEV;
    }
    *val = *data;
    return 0;
}

// Specialization for std::array.
template <typename T, size_t N>
static int SingleTagValue(const CameraMetadata &metadata, int32_t tag,
                          std::array<T, N> *val) {
    if (!val) {
        LOGF(ERROR) << " Null pointer passed to SingleTagValue.";
        return -EINVAL;
    }
    camera_metadata_ro_entry_t entry = metadata.find(tag);
    if (entry.count == 0) {
        LOGF(ERROR) << " Metadata tag " << tag << " is empty.";
        return -ENOENT;
    } else if (entry.count != N) {
        LOGF(ERROR) << " Error: expected metadata tag " << tag
                    << " to contain a single array of "
                       "exactly "
                    << N << " values (had " << entry.count << ").";
        return -EINVAL;
    }
    const T *data = nullptr;
    GetDataPointer(entry, &data);
    if (data == nullptr) {
        LOGF(ERROR) << " Metadata tag " << tag << " is empty.";
        return -ENODEV;
    }
    // Fill in the array.
    for (size_t i = 0; i < N; ++i) {
        (*val)[i] = data[i];
    }
    return 0;
}

bool VirtualCamera::enqueueRequestBuffers() {
    // Get a request from the queue (blocks this thread until one is available).
    std::shared_ptr<CaptureRequest> request = dequeueRequest();
    // ALOGD("%s:%d: enqueueRequestBuffers", __func__, mId);
    for (auto iter : request->output_buffers) {

        auto camera_buffer = m_VirtualCameraService->getCameraBuffer();
        if (!camera_buffer)
            continue;

        uint32_t fourcc = HalToV4L2PixelFormat(iter.stream->format);
        arc::GrallocFrameBuffer output_frame(
            *iter.buffer, iter.stream->width, iter.stream->height, fourcc,
            iter.stream->format, camera_buffer->GetBufferSize());
        int res = output_frame.Map();
        if (res != 0) {
            LOGF(ERROR) << " " << mId << " output_frame " << res
                        << " is empty.";
            continue;
        }
        if (V4L2_PIX_FMT_YUV420 == fourcc &&
            camera_buffer->GetFourcc() == fourcc) {
            if (camera_buffer->GetWidth() == output_frame.GetWidth() &&
                camera_buffer->GetHeight() == output_frame.GetHeight()) {
                memcpy(output_frame.GetData(), camera_buffer->GetData(),
                       camera_buffer->GetDataSize());
                continue;
            } else {
                res = arc::ImageProcessor::Scale(*camera_buffer.get(),
                                                 &output_frame);

                if (res == 0)
                    continue;
            }
        }
        // Perform the format conversion.
        arc::CachedFrame cached_frame;
        if (V4L2_PIX_FMT_YUV420 == camera_buffer->GetFourcc()) {
            res = cached_frame.SetframeSource(camera_buffer.get());
        } else {
            res = cached_frame.SetSource(camera_buffer.get(), 0);
        }
        if (res != 0) {
            LOGF(ERROR) << " " << mId << " SetSource " << res << " is empty.";
            continue;
        }
        res = cached_frame.Convert(request->settings, &output_frame, true);
    }
    int64_t timestamp = 0;
    CameraMetadata base = request->settings;
    SingleTagValue(base, ANDROID_SENSOR_TIMESTAMP, &timestamp);
    notifyShutter(request->frame_number, timestamp);
    sendResult(request);
    return true;
}

uint32_t VirtualCamera::HalToV4L2PixelFormat(int hal_pixel_format) {
    switch (hal_pixel_format) {
    case HAL_PIXEL_FORMAT_BLOB: {
        return V4L2_PIX_FMT_JPEG;
    }
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED: // Fall-through
    {
        return V4L2_PIX_FMT_BGR32;
    }
    case HAL_PIXEL_FORMAT_RGBA_8888: {
        return V4L2_PIX_FMT_BGR32;
    }
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
        // This is a flexible YUV format that depends on platform. Different
        // platform may have different format. It can be YVU420 or NV12. Now we
        // return YVU420 first.
        // TODO(): call drm_drv.get_fourcc() to get correct format.
        { return V4L2_PIX_FMT_YUV420; }
    case HAL_PIXEL_FORMAT_YCbCr_422_I: {
        return V4L2_PIX_FMT_YUYV;
    }
    case HAL_PIXEL_FORMAT_YCrCb_420_SP: {
        return V4L2_PIX_FMT_NV21;
    }
    case HAL_PIXEL_FORMAT_YV12: {
        return V4L2_PIX_FMT_YVU420;
    }
    default:
        LOGF(ERROR) << " " << mId << " Pixel format " << hal_pixel_format
                    << " is unsupported.";
        break;
    }
    return -1;
}

android::status_t VirtualCamera::initDevice() {
    android::status_t res;
    CameraMetadata base;

    // Create standard settings templates from copies of base metadata
    ADD_STATIC_ENTRY_UINT8(ANDROID_CONTROL_MODE, {ANDROID_CONTROL_MODE_OFF});

    // Use base settings to create all other templates and set them. This is
    // just some samples, More initialization may be needed.
    res = initPreviewTemplate(base);
    if (android::OK != res)
        return res;
    res = initStillTemplate(base);
    if (android::OK != res)
        return res;
    res = initRecordTemplate(base);
    if (android::OK != res)
        return res;
    res = initSnapshotTemplate(base);
    if (android::OK != res)
        return res;
    res = initZslTemplate(base);
    if (android::OK != res)
        return res;
    res = initManualTemplate(base);
    if (android::OK != res)
        return res;

    return android::OK;
}

android::status_t VirtualCamera::initPreviewTemplate(CameraMetadata &base) {
    android::status_t res;
    // Setup default preview controls
    ADD_STATIC_ENTRY_UINT8(ANDROID_CONTROL_CAPTURE_INTENT,
                           {ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW});
    // TODO: set fast auto-focus, auto-whitebalance, auto-exposure, auto flash
    return setTemplate(CAMERA3_TEMPLATE_PREVIEW, base.release());
}

android::status_t VirtualCamera::initStillTemplate(CameraMetadata &base) {
    android::status_t res;
    ADD_STATIC_ENTRY_UINT8(ANDROID_CONTROL_CAPTURE_INTENT,
                           {ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE});
    // TODO: set fast auto-focus, auto-whitebalance, auto-exposure, auto flash
    return setTemplate(CAMERA3_TEMPLATE_STILL_CAPTURE, base.release());
}

android::status_t VirtualCamera::initRecordTemplate(CameraMetadata &base) {
    android::status_t res;
    ADD_STATIC_ENTRY_UINT8(ANDROID_CONTROL_CAPTURE_INTENT,
                           {ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD});
    // TODO: set slow auto-focus, auto-whitebalance, auto-exposure, flash off
    return setTemplate(CAMERA3_TEMPLATE_VIDEO_RECORD, base.release());
}

android::status_t VirtualCamera::initSnapshotTemplate(CameraMetadata &base) {
    android::status_t res;
    ADD_STATIC_ENTRY_UINT8(ANDROID_CONTROL_CAPTURE_INTENT,
                           {ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT});
    // TODO: set slow auto-focus, auto-whitebalance, auto-exposure, flash off
    return setTemplate(CAMERA3_TEMPLATE_VIDEO_SNAPSHOT, base.release());
}

android::status_t VirtualCamera::initZslTemplate(CameraMetadata &base) {
    android::status_t res;
    ADD_STATIC_ENTRY_UINT8(ANDROID_CONTROL_CAPTURE_INTENT,
                           {ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG});
    // TODO: set reprocessing parameters for zsl input queue
    return setTemplate(CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG, base.release());
}

android::status_t VirtualCamera::initManualTemplate(CameraMetadata &base) {
    android::status_t res;
    ADD_STATIC_ENTRY_UINT8(ANDROID_CONTROL_CAPTURE_INTENT,
                           {ANDROID_CONTROL_CAPTURE_INTENT_MANUAL});
    // TODO: set reprocessing parameters for zsl input queue
    return setTemplate(CAMERA3_TEMPLATE_MANUAL, base.release());
}

bool VirtualCamera::isValidCaptureSettings(
    const camera_metadata_t * /*settings*/) {
    // TODO: reject settings that cannot be captured
    return true;
}

} // namespace virtual_camera_hal
