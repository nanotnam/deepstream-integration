#include "alpr_deepstream/job_collector.hpp"

#include "deepstream_runtime/deepstream_submission.hpp"

extern "C" bool MbfsAlprSubmitEveryVehicleFrame(
    deepstream_runtime::ProcessingCoordinator* coordinator, GstBuffer* buffer,
    uint32_t vehicle_component_id) {
  return deepstream_runtime::submit_deepstream_buffer(
      coordinator, buffer, vehicle_component_id);
}
