/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#pragma once
#ifndef HAL_HANDLE_FRAME_H_
#define HAL_HANDLE_FRAME_H_

#include <aidl/android/hardware/common/NativeHandle.h>
#include <cutils/native_handle.h>

namespace arc {

/**
 * Creates a libcutils native handle from an AIDL native handle, but it does not
 * dup internally, so it will contain the same FDs as the handle itself. The
 * result should be deleted with native_handle_delete.
 */
native_handle_t *
makeFromAidl(const aidl::android::hardware::common::NativeHandle &handle);

/**
 * Creates a libcutils native handle from an AIDL native handle with a dup
 * internally. It's expected the handle is cleaned up with native_handle_close
 * and native_handle_delete.
 */
native_handle_t *
dupFromAidl(const aidl::android::hardware::common::NativeHandle &handle);

/**
 * Creates an AIDL native handle from a libcutils native handle, but does not
 * dup internally, so the result will contain the same FDs as the handle itself.
 *
 * Warning: this passes ownership of the FDs to the ScopedFileDescriptor
 * objects.
 */
aidl::android::hardware::common::NativeHandle
makeToAidl(const native_handle_t *handle);

/**
 * Creates an AIDL native handle from a libcutils native handle with a dup
 * internally.
 */
aidl::android::hardware::common::NativeHandle
dupToAidl(const native_handle_t *handle);

} // namespace arc

#endif // HAL_HANDLE_FRAME_H_
