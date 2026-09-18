add_library(nvds_preprocess_alpr SHARED
  src/job_collector.cpp
  src/rectify_adapter.cpp)
target_include_directories(nvds_preprocess_alpr PRIVATE
  include
  ${DEEPSTREAM_SDK_ROOT}/sources/includes)
target_link_libraries(nvds_preprocess_alpr PRIVATE alpr_core motorcycle_violation_runtime)
find_library(NVDS_META_LIBRARY nvds_meta
  PATHS ${DEEPSTREAM_SDK_ROOT}/lib NO_DEFAULT_PATH REQUIRED)
find_library(NVDS_GST_META_LIBRARY nvdsgst_meta
  PATHS ${DEEPSTREAM_SDK_ROOT}/lib NO_DEFAULT_PATH REQUIRED)
target_link_libraries(nvds_preprocess_alpr PRIVATE
  ${NVDS_META_LIBRARY} ${NVDS_GST_META_LIBRARY})
project_warnings(nvds_preprocess_alpr)
install(TARGETS nvds_preprocess_alpr LIBRARY DESTINATION lib)
