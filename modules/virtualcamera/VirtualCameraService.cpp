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
#include "VirtualCameraService.h"
#include "arc/common.h"
#include <android/binder_manager.h>

namespace virtual_camera_hal {

static void LambdaOnDeath(void *cookie) {
    auto deathMonitor = static_cast<VirtualCameraService *>(cookie);
    deathMonitor->OnDeath();
};
static void LambdaOnUnlink(void *cookie) {
    auto deathMonitor = static_cast<VirtualCameraService *>(cookie);
    deathMonitor->OnUnlink();
};

static void LambdaOnDeathNuPlayer(void *cookie) {
    auto deathMonitor = static_cast<VirtualCameraService *>(cookie);
    deathMonitor->OnDeathNuPlayer();
};
static void LambdaOnUnlinkNuPlayer(void *cookie) {
    auto deathMonitor = static_cast<VirtualCameraService *>(cookie);
    deathMonitor->OnUnlinkNuPlayer();
};

VirtualCameraService::VirtualCameraService()
    : m_IVirtualMedia(nullptr), m_INuPlayerService(nullptr) {}

VirtualCameraService::~VirtualCameraService() {}

void VirtualCameraService::OnDeath() {
    m_IVirtualMedia = nullptr;
    LOGF(INFO) << "OnDeath()";
}

void VirtualCameraService::OnUnlink() { LOGF(INFO) << "OnUnlink()"; }

void VirtualCameraService::OnDeathNuPlayer() {
    m_INuPlayerService = nullptr;
    LOGF(INFO) << "OnDeathNuPlayer()";
}

void VirtualCameraService::OnUnlinkNuPlayer() {
    LOGF(INFO) << "OnUnlinkNuPlayer()";
}

std::shared_ptr<arc::FrameBuffer> VirtualCameraService::getCameraBuffer() {
    auto service = getNuPlayerService();
    if (nullptr != service) {
        ::aidl::android::hardware::virtualmedia::CameraBufferHandle
            bufferHandle;
        auto status = service->getCameraBuffer(&bufferHandle);
        if (!status.isOk()) {
            LOG(ERROR) << "getCameraBuffer fail: " << status.getMessage();
            return NULL;
        }
        return std::make_shared<arc::CameraFrameBuffer>(
            bufferHandle.file, bufferHandle.width, bufferHandle.height,
            bufferHandle.fourcc);
    }
    return NULL;
}

std::shared_ptr<::aidl::android::hardware::virtualmedia::IVirtualMedia> &
VirtualCameraService::getVirtualMedia() {
    if (nullptr == m_IVirtualMedia) {
        const std::string statsServiceName =
            std::string() +
            ::aidl::android::hardware::virtualmedia::BnVirtualMedia::
                descriptor +
            "/default";
        auto binder = AServiceManager_waitForService(statsServiceName.c_str());
        m_IVirtualMedia =
            ::aidl::android::hardware::virtualmedia::BnVirtualMedia::fromBinder(
                ::ndk::SpAIBinder(binder));
        if (!m_IVirtualMedia) {
            LOGF(ERROR) << "m_IVirtualMedia fail";
            return m_IVirtualMedia;
        }
        LOGF(INFO) << "m_IVirtualMedia: " << m_IVirtualMedia.get();
        AIBinder_DeathRecipient *recipient =
            AIBinder_DeathRecipient_new(LambdaOnDeath);
        AIBinder_DeathRecipient_setOnUnlinked(recipient, LambdaOnUnlink);
        auto status = ndk::ScopedAStatus::fromStatus(
            AIBinder_linkToDeath(binder, recipient, static_cast<void *>(this)));
        if (!status.isOk()) {
            LOG(ERROR) << "Failed to linkToDeath: " << status.getMessage();
        }
    }
    return m_IVirtualMedia;
}

std::shared_ptr<::aidl::android::hardware::virtualmedia::INuPlayerService> &
VirtualCameraService::getNuPlayerService() {
    if (nullptr == m_INuPlayerService) {
        auto service = getVirtualMedia();
        if (nullptr == service) {
            return m_INuPlayerService;
        }
        auto status = service->getNuPlayerService(&m_INuPlayerService);
        if (!status.isOk()) {
            LOG(ERROR) << "Failed to linkToDeath: " << status.getMessage();
            return m_INuPlayerService;
        }
        LOGF(INFO) << "m_INuPlayerService: " << m_INuPlayerService.get();
        auto binder = m_INuPlayerService->asBinder().get();
        AIBinder_DeathRecipient *recipient =
            AIBinder_DeathRecipient_new(LambdaOnDeathNuPlayer);
        AIBinder_DeathRecipient_setOnUnlinked(recipient,
                                              LambdaOnUnlinkNuPlayer);
        status = ndk::ScopedAStatus::fromStatus(
            AIBinder_linkToDeath(binder, recipient, static_cast<void *>(this)));
        if (!status.isOk()) {
            LOG(ERROR) << "Failed to linkToDeath: " << status.getMessage();
        }
    }
    return m_INuPlayerService;
}

} // namespace virtual_camera_hal
