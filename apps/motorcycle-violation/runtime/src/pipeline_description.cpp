#include "motorcycle_violation/pipeline_description.hpp"

#include <sstream>

namespace motorcycle_violation {

std::string pipeline_description(const PipelineConfig& config) {
  std::ostringstream stream;
  stream << source_type_name(config.source.type)
         << " source[" << config.source.id << "]"
         << " -> processing-fps[";
  if (config.processing.max_fps == 0.0) stream << "full";
  else stream << config.processing.max_fps;
  stream << ']'
         << " -> decode -> nvstreammux[1," << config.source.width << 'x'
         << config.source.height << "]"
         << " -> nvinfer[vehicle," << precision_name(config.models.precision) << ']'
         << " -> nvtracker[" << config.tracker.type << ']'
         << " -> zone-filter[bottom-center]"
         << " -> plate-queue[block,capacity=" << config.processing.job_queue_capacity << ']'
         << " -> nvdspreprocess[every-vehicle-roi]"
         << " -> nvinfer[plate,batch=" << config.processing.plate_batch_size << ']'
         << " -> alpr-keypoints -> nvdspreprocess[rectify-156x32-bgr]"
         << " -> nvinfer[lprnet,batch=" << config.processing.lpr_batch_size
         << "] -> alpr-vote -> json";
  if (config.outputs.stdout_enabled) stream << " -> stdout";
  if (config.outputs.kafka.enabled) {
    stream << " -> app-kafka-publisher[nvds_msgapi] -> kafka";
  }
  return stream.str();
}

bool motorcycle_violation_compiled() {
#ifdef DEEPSTREAM_RUNTIME_AVAILABLE
  return true;
#else
  return false;
#endif
}

}  // namespace motorcycle_violation
