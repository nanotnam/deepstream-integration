#include "deepstream_runtime/pipeline_description.hpp"

#include <sstream>

namespace deepstream_runtime {

std::string pipeline_description(const PipelineConfig& config) {
  std::ostringstream stream;
  stream << source_type_name(config.source.type)
         << " source[" << config.source.id << "]"
         << " -> decode -> nvstreammux[1," << config.source.width << 'x'
         << config.source.height << "]"
         << " -> nvinfer[vehicle," << precision_name(config.models.precision) << ']'
         << " -> nvtracker[" << config.tracker.type << ']'
         << " -> nvdspreprocess[vehicle-roi] -> nvinfer[plate]"
         << " -> alpr-keypoints -> nvdspreprocess[rectify-156x32-bgr]"
         << " -> nvinfer[lprnet] -> alpr-vote -> json";
  if (config.outputs.stdout_enabled) stream << " -> stdout";
  if (config.outputs.kafka.enabled) stream << " -> nvmsgconv -> kafka";
  return stream.str();
}

bool deepstream_runtime_compiled() {
#ifdef DEEPSTREAM_RUNTIME_AVAILABLE
  return true;
#else
  return false;
#endif
}

}  // namespace deepstream_runtime
