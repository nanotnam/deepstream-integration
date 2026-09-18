#include "alpr_deepstream/job_collector.hpp"

#include "traffic_alpr/deepstream_submission.hpp"

extern "C" bool MbfsAlprSubmitEveryVehicleFrame(
    traffic_alpr::ProcessingCoordinator* coordinator, GstBuffer* buffer,
    uint32_t vehicle_component_id) {
  return traffic_alpr::submit_deepstream_buffer(
      coordinator, buffer, vehicle_component_id);
}
