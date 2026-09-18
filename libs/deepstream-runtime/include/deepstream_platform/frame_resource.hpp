#pragma once

#include <memory>

namespace deepstream_platform {

// Type-erased lifetime token retained by asynchronous application work. Concrete
// SDK adapters can use it to keep a frame buffer and its attached metadata alive.
class FrameResource {
 public:
  virtual ~FrameResource() = default;
};

using FrameLease = std::shared_ptr<FrameResource>;

}  // namespace deepstream_platform
