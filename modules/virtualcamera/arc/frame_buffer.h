/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_FRAME_BUFFER_H_
#define HAL_FRAME_BUFFER_H_

#include <HandleImporter.h>
#include <android/binder_auto_utils.h>
#include <cstdint>
#include <memory>

namespace arc {
using ::android::hardware::camera::common::V1_0::helper::HandleImporter;
using ::android::hardware::graphics::mapper::V2_0::YCbCrLayout;

class FrameBuffer : virtual public android::RefBase {
  public:
    FrameBuffer();
    virtual ~FrameBuffer();

    // If mapped successfully, the address will be assigned to |data_| and
    // return 0. Otherwise, returns -EINVAL.
    virtual int Map() = 0;

    // Unmaps the mapped address. Returns 0 for success.
    virtual int Unmap() = 0;

    uint8_t *GetData() const { return data_; }
    size_t GetDataSize() const { return data_size_; }
    size_t GetBufferSize() const { return buffer_size_; }
    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }
    uint32_t GetFourcc() const { return fourcc_; }

    void SetFourcc(uint32_t fourcc) { fourcc_ = fourcc; }
    virtual int SetDataSize(size_t data_size);

  protected:
    uint8_t *data_;

    // The number of bytes used in the buffer.
    size_t data_size_;

    // The number of bytes allocated in the buffer.
    size_t buffer_size_;

    // Frame resolution.
    uint32_t width_;
    uint32_t height_;

    // This is V4L2_PIX_FMT_* in linux/videodev2.h.
    uint32_t fourcc_;
};

// AllocatedFrameBuffer is used for the buffer from hal malloc-ed. User should
// be aware to manage the memory.
class AllocatedFrameBuffer : public FrameBuffer {
  public:
    explicit AllocatedFrameBuffer(int buffer_size);
    explicit AllocatedFrameBuffer(uint8_t *buffer, int buffer_size);
    ~AllocatedFrameBuffer() override;

    // No-op for the two functions.
    int Map() override { return 0; }
    int Unmap() override { return 0; }

    void SetWidth(uint32_t width) { width_ = width; }
    void SetHeight(uint32_t height) { height_ = height; }
    int SetDataSize(size_t data_size) override;
    void Reset();

  private:
    std::unique_ptr<uint8_t[]> buffer_;
};

// GrallocFrameBuffer is used for the buffer from Android framework. Uses
// CameraBufferMapper to lock and unlock the buffer.
class GrallocFrameBuffer : public FrameBuffer {
  public:
    GrallocFrameBuffer(buffer_handle_t buffer, uint32_t width, uint32_t height,
                       uint32_t fourcc, uint32_t device_buffer_length,
                       uint32_t stream_usage);
    ~GrallocFrameBuffer();

    int Map() override;
    int Unmap() override;
    int SetDataSize(size_t data_size) override;

  private:
    // The currently used buffer for |buffer_mapper_| operations.
    buffer_handle_t buffer_;

    // Used to import gralloc buffer.
    std::shared_ptr<HandleImporter> importer_;

    bool is_mapped_;

    // Camera stream and device buffer context.
    uint32_t device_buffer_length_;
    uint32_t stream_usage_;
};

class CameraFrameBuffer : public FrameBuffer {
  public:
    CameraFrameBuffer(::ndk::ScopedFileDescriptor &fd, uint32_t width,
                      uint32_t height, uint32_t fourcc);
    ~CameraFrameBuffer();

    int Map() override;
    int Unmap() override;

  private:
    std::unique_ptr<uint8_t[]> buffer_;
};

} // namespace arc

#endif // HAL_USB_FRAME_BUFFER_H_
