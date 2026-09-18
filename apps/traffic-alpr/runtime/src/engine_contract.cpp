#include "traffic_alpr/engine_contract.hpp"

namespace traffic_alpr {
namespace {

bool valid_profile(const BatchProfile& profile) {
  return profile.minimum > 0U && profile.minimum <= profile.optimal &&
         profile.optimal <= profile.maximum &&
         (profile.dynamic || (profile.minimum == profile.maximum));
}

}  // namespace

bool validate_engine_batch_profiles(const PipelineConfig& config,
                                    const EngineBatchProfiles& profiles,
                                    std::string* error) {
  if (!valid_profile(profiles.vehicle) || !valid_profile(profiles.plate) ||
      !valid_profile(profiles.lprnet)) {
    *error = "engine batch profile is invalid";
    return false;
  }
  if (profiles.vehicle.minimum > 1U || profiles.vehicle.maximum < 1U) {
    *error = "vehicle engine profile must support batch 1";
    return false;
  }
  if (config.processing.plate_batch_size < profiles.plate.minimum ||
      config.processing.plate_batch_size > profiles.plate.maximum) {
    *error = "processing.plate_batch_size is outside the plate engine profile";
    return false;
  }
  if (config.processing.lpr_batch_size < profiles.lprnet.minimum ||
      config.processing.lpr_batch_size > profiles.lprnet.maximum) {
    *error = "processing.lpr_batch_size is outside the LPRNet engine profile";
    return false;
  }
  return true;
}

}  // namespace traffic_alpr
