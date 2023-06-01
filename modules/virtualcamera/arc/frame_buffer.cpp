/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "frame_buffer.h"
#include "common.h"
#include "image_processor.h"
#include <cutils/ashmem.h>
#include <fcntl.h>
#include <hardware/gralloc.h>
#include <libyuv.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utility>

namespace arc {

using ::android::hardware::graphics::mapper::V2_0::YCbCrLayout;

FrameBuffer::FrameBuffer()
    : data_(nullptr), data_size_(0), buffer_size_(0), width_(0), height_(0),
      fourcc_(0) {}

FrameBuffer::~FrameBuffer() {}

int FrameBuffer::SetDataSize(size_t data_size) {
    if (data_size > buffer_size_) {
        LOGF(ERROR) << " Buffer overflow: Buffer only has " << buffer_size_
                    << ", but data needs " << data_size;
        return -EINVAL;
    }
    data_size_ = data_size;
    return 0;
}

AllocatedFrameBuffer::AllocatedFrameBuffer(int buffer_size) {
    buffer_.reset(new uint8_t[buffer_size]);
    buffer_size_ = buffer_size;
    data_ = buffer_.get();
}

AllocatedFrameBuffer::AllocatedFrameBuffer(uint8_t *buffer, int buffer_size) {
    buffer_.reset(buffer);
    buffer_size_ = buffer_size;
    data_ = buffer_.get();
}

AllocatedFrameBuffer::~AllocatedFrameBuffer() {}

int AllocatedFrameBuffer::SetDataSize(size_t size) {
    if (size > buffer_size_) {
        buffer_.reset(new uint8_t[size]);
        buffer_size_ = size;
        data_ = buffer_.get();
    }
    data_size_ = size;
    return 0;
}

void AllocatedFrameBuffer::Reset() { memset(data_, 0, buffer_size_); }

GrallocFrameBuffer::GrallocFrameBuffer(buffer_handle_t buffer, uint32_t width,
                                       uint32_t height, uint32_t fourcc,
                                       uint32_t device_buffer_length,
                                       uint32_t stream_usage)
    : buffer_(buffer), importer_(std::make_shared<HandleImporter>()),
      is_mapped_(false), device_buffer_length_(device_buffer_length),
      stream_usage_(stream_usage) {
    width_ = width;
    height_ = height;
    fourcc_ = fourcc;
}

GrallocFrameBuffer::~GrallocFrameBuffer() {
    if (Unmap() != 0) {
        LOGF(ERROR) << " Unmap failed";
    }
}

int GrallocFrameBuffer::Map() {
    // base::AutoLock l(lock_);
    if (is_mapped_) {
        LOGF(ERROR) << "The buffer is already mapped";
        return -EINVAL;
    }

    IMapper::Rect region{0, 0, static_cast<int32_t>(width_),
                         static_cast<int32_t>(height_)};
    void *addr;
    YCbCrLayout yuv_data;
    switch (fourcc_) {
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YVU420:
    case V4L2_PIX_FMT_YUYV: {
        yuv_data = importer_->lockYCbCr(
            buffer_, GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN,
            region);
        addr = yuv_data.y;
    } break;
    case V4L2_PIX_FMT_JPEG: {
        addr = importer_->lock(
            buffer_, GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN,
            device_buffer_length_);
        buffer_size_ = device_buffer_length_;
    } break;
    case V4L2_PIX_FMT_BGR32:
    case V4L2_PIX_FMT_RGB32: {
        addr = importer_->lock(
            buffer_, GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN,
            region);
    } break;
    default:
        return -EINVAL;
    }
    if (stream_usage_) {
    }
    if (!addr) {
        LOGF(ERROR) << " Failed to gralloc lock buffer";
        return -EINVAL;
    }

    data_ = static_cast<uint8_t *>(addr);
    if (fourcc_ == V4L2_PIX_FMT_YVU420 || fourcc_ == V4L2_PIX_FMT_YUV420 ||
        fourcc_ == V4L2_PIX_FMT_NV21 || fourcc_ == V4L2_PIX_FMT_RGB32 ||
        fourcc_ == V4L2_PIX_FMT_BGR32) {
        buffer_size_ =
            ImageProcessor::GetConvertedSize(fourcc_, width_, height_);
    }
    is_mapped_ = true;
    return 0;
}

int GrallocFrameBuffer::Unmap() {
    // base::AutoLock l(lock_);
    if (is_mapped_)
        importer_->unlock(buffer_);

    is_mapped_ = false;
    return 0;
}

int GrallocFrameBuffer::SetDataSize(size_t data_size) {
    data_size_ = data_size;
    buffer_size_ = data_size;
    return 0;
}

CameraFrameBuffer::CameraFrameBuffer(::ndk::ScopedFileDescriptor &fd,
                                     uint32_t width, uint32_t height,
                                     uint32_t fourcc) {
    width_ = width;
    height_ = height;
    buffer_size_ = ashmem_get_size_region(fd.get());

    if (fourcc == 1) {
        fourcc_ = V4L2_PIX_FMT_YUV420;
    } else {
        fourcc_ = V4L2_PIX_FMT_YUV420;
    }
    data_size_ = buffer_size_;
    buffer_.reset(new uint8_t[data_size_]);
    read(fd.get(), buffer_.get(), data_size_);
    data_ = buffer_.get();
}

CameraFrameBuffer::~CameraFrameBuffer() {}

int CameraFrameBuffer::Map() { return 0; }

int CameraFrameBuffer::Unmap() { return 0; }

} // namespace arc
