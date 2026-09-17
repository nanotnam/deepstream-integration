add_library(deepstream_runtime STATIC
  src/config.cpp
  src/metadata.cpp
  src/pipeline_description.cpp)
target_include_directories(deepstream_runtime PUBLIC include)
target_link_libraries(deepstream_runtime PUBLIC alpr_core)
set_target_properties(deepstream_runtime PROPERTIES POSITION_INDEPENDENT_CODE ON)
project_warnings(deepstream_runtime)
