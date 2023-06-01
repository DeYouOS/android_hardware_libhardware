#include "native_handle.h"
#include <fcntl.h>

namespace arc {

using aidl::android::hardware::common::NativeHandle;

static native_handle_t *fromAidl(const NativeHandle &handle, bool doDup) {
    native_handle_t *to =
        native_handle_create(handle.fds.size(), handle.ints.size());
    if (!to)
        return nullptr;

    for (size_t i = 0; i < handle.fds.size(); i++) {
        int fd = handle.fds[i].get();
        to->data[i] = doDup ? fcntl(fd, F_DUPFD_CLOEXEC, 0) : fd;
    }
    memcpy(to->data + handle.fds.size(), handle.ints.data(),
           handle.ints.size() * sizeof(int));
    return to;
}

native_handle_t *makeFromAidl(const NativeHandle &handle) {
    return fromAidl(handle, false /* doDup */);
}
native_handle_t *dupFromAidl(const NativeHandle &handle) {
    return fromAidl(handle, true /* doDup */);
}

static NativeHandle toAidl(const native_handle_t *handle, bool doDup) {
    NativeHandle to;

    to.fds = std::vector<ndk::ScopedFileDescriptor>(handle->numFds);
    for (int i = 0; i < handle->numFds; i++) {
        int fd = handle->data[i];
        to.fds.at(i).set(doDup ? fcntl(fd, F_DUPFD_CLOEXEC, 0) : fd);
    }

    to.ints =
        std::vector<int32_t>(handle->data + handle->numFds,
                             handle->data + handle->numFds + handle->numInts);
    return to;
}

NativeHandle makeToAidl(const native_handle_t *handle) {
    return toAidl(handle, false /* doDup */);
}

NativeHandle dupToAidl(const native_handle_t *handle) {
    return toAidl(handle, true /* doDup */);
}

} // namespace arc
