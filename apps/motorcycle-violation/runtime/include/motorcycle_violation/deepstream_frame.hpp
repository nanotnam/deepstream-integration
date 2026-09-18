#pragma once

#include "motorcycle_violation/job_queue.hpp"

#include <gst/gst.h>

namespace motorcycle_violation {

// Holds a GStreamer buffer reference while asynchronous ROI work is queued. The
// NvBufSurface and NvDsBatchMeta owned by the buffer must only be accessed while this
// lease is alive.
class GstBufferFrameResource final : public FrameResource {
 public:
  explicit GstBufferFrameResource(GstBuffer* buffer);
  ~GstBufferFrameResource() override;
  GstBufferFrameResource(const GstBufferFrameResource&) = delete;
  GstBufferFrameResource& operator=(const GstBufferFrameResource&) = delete;

  GstBuffer* buffer() const;

 private:
  GstBuffer* buffer_{nullptr};
};

}  // namespace motorcycle_violation
