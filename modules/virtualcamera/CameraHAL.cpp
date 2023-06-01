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
#include "CameraHAL.h"

#include <hardware/camera_common.h>
#include <hardware/hardware.h>
#include <utils/Mutex.h>

#include <cstdlib>

#include "VendorTags.h"
#include "VirtualCamera.h"
#include "arc/common.h"
#include <android/binder_process.h>

/*
 * This file serves as the entry point to the HAL.  It contains the module
 * structure and functions used by the framework to load and interface to this
 * HAL, as well as the handles to the individual camera devices.
 */

namespace virtual_camera_hal {

static CameraHAL gCameraHAL;
// Handle containing vendor tag functionality
static VendorTags gVendorTags;

CameraHAL::CameraHAL()
    : mCallbacks(NULL), m_VirtualCameraService(new VirtualCameraService) {
    // Should not allocate the camera devices for now, as it is unclear if the
    // device is plugged.

    mCameras.add(new VirtualCamera(m_VirtualCameraService, 0));
    mCameras.add(new VirtualCamera(m_VirtualCameraService, 1));
    ABinderProcess_startThreadPool();
}

CameraHAL::~CameraHAL() {}

int CameraHAL::getNumberOfCameras() {
    android::Mutex::Autolock al(mModuleLock);
    LOGF(INFO) << " " << mCameras.size();
    return static_cast<int>(mCameras.size());
}

int CameraHAL::getCameraInfo(int id, struct camera_info *info) {
    android::Mutex::Autolock al(mModuleLock);
    LOGF(INFO) << " camera id " << id << " info= " << info;
    if (id < 0 || id >= static_cast<int>(mCameras.size())) {
        LOGF(ERROR) << " Invalid camera id " << id;
        return -ENODEV;
    }

    return mCameras[id]->getInfo(info);
}

int CameraHAL::setCallbacks(const camera_module_callbacks_t *callbacks) {
    LOGF(INFO) << " callbacks=" << callbacks;
    mCallbacks = callbacks;
    return 0;
}

int CameraHAL::open(const hw_module_t *mod, const char *name,
                    hw_device_t **dev) {
    int id;
    char *nameEnd;

    android::Mutex::Autolock al(mModuleLock);
    LOGF(INFO) << " module=" << mod << ", name=" << name << ", device=" << dev;
    if (*name == '\0') {
        LOGF(ERROR) << " Invalid camera id name is NULL";
        return -EINVAL;
    }
    id = strtol(name, &nameEnd, 10);
    if (*nameEnd != '\0') {
        LOGF(ERROR) << "%s: Invalid camera id name " << name;
        return -EINVAL;
    } else if (id < 0 || id >= static_cast<int>(mCameras.size())) {
        LOGF(ERROR) << " Invalid camera id " << id;
        return -ENODEV;
    }
    return mCameras[id]->open(mod, dev);
}

extern "C" {

static int get_number_of_cameras() { return gCameraHAL.getNumberOfCameras(); }

static int get_camera_info(int id, struct camera_info *info) {
    return gCameraHAL.getCameraInfo(id, info);
}

static int set_callbacks(const camera_module_callbacks_t *callbacks) {
    return gCameraHAL.setCallbacks(callbacks);
}

static int open_dev(const hw_module_t *mod, const char *name,
                    hw_device_t **dev) {
    return gCameraHAL.open(mod, name, dev);
}

static hw_module_methods_t gCameraModuleMethods = {.open = open_dev};

static int get_tag_count(const vendor_tag_ops_t *ops) {
    return gVendorTags.getTagCount(ops);
}

static void get_all_tags(const vendor_tag_ops_t *ops, uint32_t *tag_array) {
    gVendorTags.getAllTags(ops, tag_array);
}

static const char *get_section_name(const vendor_tag_ops_t *ops, uint32_t tag) {
    return gVendorTags.getSectionName(ops, tag);
}

static const char *get_tag_name(const vendor_tag_ops_t *ops, uint32_t tag) {
    return gVendorTags.getTagName(ops, tag);
}

static int get_tag_type(const vendor_tag_ops_t *ops, uint32_t tag) {
    return gVendorTags.getTagType(ops, tag);
}

static void get_vendor_tag_ops(vendor_tag_ops_t *ops) {
    ops->get_tag_count = get_tag_count;
    ops->get_all_tags = get_all_tags;
    ops->get_section_name = get_section_name;
    ops->get_tag_name = get_tag_name;
    ops->get_tag_type = get_tag_type;
}

static int set_torch_mode(const char *, bool) { return -ENOSYS; }

camera_module_t HAL_MODULE_INFO_SYM __attribute__((visibility("default"))) = {
    .common =
        {
            .tag = HARDWARE_MODULE_TAG,
            .module_api_version = CAMERA_MODULE_API_VERSION_2_2,
            .hal_api_version = HARDWARE_HAL_API_VERSION,
            .id = CAMERA_HARDWARE_MODULE_ID,
            .name = "Default Virtual Camera HAL",
            .author = "The Android Open Source Project",
            .methods = &gCameraModuleMethods,
            .dso = NULL,
            .reserved = {0},
        },
    .get_number_of_cameras = get_number_of_cameras,
    .get_camera_info = get_camera_info,
    .set_callbacks = set_callbacks,
    .get_vendor_tag_ops = get_vendor_tag_ops,
    .open_legacy = NULL,
    .set_torch_mode = set_torch_mode,
    .init = NULL,
    .reserved = {0},
};
} // extern "C"

} // namespace virtual_camera_hal
