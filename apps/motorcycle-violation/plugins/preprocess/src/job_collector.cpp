#include "alpr_deepstream/job_collector.hpp"

#include "motorcycle_violation/deepstream_submission.hpp"

extern "C" bool MbfsAlprSubmitEveryVehicleFrame(
    motorcycle_violation::ProcessingCoordinator* coordinator, GstBuffer* buffer,
    uint32_t vehicle_component_id) {
  return motorcycle_violation::submit_deepstream_buffer(
      coordinator, buffer, vehicle_component_id);
}
