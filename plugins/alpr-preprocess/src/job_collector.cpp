#include "alpr_deepstream/job_collector.hpp"

#include "deepstream_runtime/deepstream_frame.hpp"

#include <gstnvdsmeta.h>
#include <nvdsmeta.h>

#include <memory>
#include <string>
#include <vector>

extern "C" bool MbfsAlprSubmitEveryVehicleFrame(
    deepstream_runtime::ProcessingCoordinator* coordinator, GstBuffer* buffer,
    uint32_t vehicle_component_id) {
  if (coordinator == nullptr || buffer == nullptr) return false;
  try {
    NvDsBatchMeta* batch_meta = gst_buffer_get_nvds_batch_meta(buffer);
    if (batch_meta == nullptr) return false;
    auto lease = std::make_shared<deepstream_runtime::GstBufferFrameResource>(buffer);
    for (NvDsMetaList* frame_node = batch_meta->frame_meta_list;
         frame_node != nullptr; frame_node = frame_node->next) {
      auto* frame = static_cast<NvDsFrameMeta*>(frame_node->data);
      if (frame == nullptr || frame->source_frame_width == 0U ||
          frame->source_frame_height == 0U) return false;
      std::vector<alpr::TrackedVehicle> tracks;
      for (NvDsMetaList* object_node = frame->obj_meta_list;
           object_node != nullptr; object_node = object_node->next) {
        auto* object = static_cast<NvDsObjectMeta*>(object_node->data);
        if (object == nullptr || object->unique_component_id != vehicle_component_id) {
          continue;
        }
        const float frame_width = static_cast<float>(frame->source_frame_width);
        const float frame_height = static_cast<float>(frame->source_frame_height);
        alpr::TrackedVehicle track;
        track.track_id = object->object_id;
        track.detection.class_id = object->class_id;
        track.detection.label = std::string(object->obj_label);
        track.detection.score = object->confidence;
        track.detection.box = {
            object->rect_params.left / frame_width,
            object->rect_params.top / frame_height,
            object->rect_params.width / frame_width,
            object->rect_params.height / frame_height};
        tracks.push_back(std::move(track));
      }
      const auto submission = coordinator->submit_frame(
          frame->frame_num, frame->buf_pts, tracks, lease);
      if (submission.queue_closed) return false;
    }
    return true;
  } catch (...) {
    return false;
  }
}
