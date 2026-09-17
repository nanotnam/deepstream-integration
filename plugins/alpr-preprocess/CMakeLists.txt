add_library(nvds_preprocess_alpr SHARED
  src/job_collector.cpp
  src/rectify_adapter.cpp)
target_include_directories(nvds_preprocess_alpr PRIVATE
  include
  ${DEEPSTREAM_SDK_ROOT}/sources/includes)
target_link_libraries(nvds_preprocess_alpr PRIVATE alpr_core deepstream_runtime)
project_warnings(nvds_preprocess_alpr)
install(TARGETS nvds_preprocess_alpr LIBRARY DESTINATION lib)
