#include "motorcycle_violation/deepstream_frame.hpp"

#include <stdexcept>

namespace motorcycle_violation {

GstBufferFrameResource::GstBufferFrameResource(GstBuffer* buffer) : buffer_(buffer) {
  if (buffer_ == nullptr) throw std::invalid_argument("GstBuffer lease cannot be null");
  gst_buffer_ref(buffer_);
}

GstBufferFrameResource::~GstBufferFrameResource() {
  if (buffer_ != nullptr) gst_buffer_unref(buffer_);
}

GstBuffer* GstBufferFrameResource::buffer() const { return buffer_; }

}  // namespace motorcycle_violation
