add_library(deepstream_runtime STATIC
  src/config.cpp
  src/engine_contract.cpp
  src/job_queue.cpp
  src/metadata.cpp
  src/pipeline_description.cpp
  src/plate_worker.cpp
  src/processing.cpp)
target_include_directories(deepstream_runtime PUBLIC include)
target_link_libraries(deepstream_runtime PUBLIC alpr_core Threads::Threads)
set_target_properties(deepstream_runtime PROPERTIES POSITION_INDEPENDENT_CODE ON)
project_warnings(deepstream_runtime)

if(DEEPSTREAM_SDK_ROOT)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET gstreamer-1.0)
  target_sources(deepstream_runtime PRIVATE src/deepstream_frame.cpp)
  target_link_libraries(deepstream_runtime PUBLIC PkgConfig::GSTREAMER)
endif()
