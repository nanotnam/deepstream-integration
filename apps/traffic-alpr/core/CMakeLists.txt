add_library(traffic_alpr_core STATIC
  src/detection.cpp
  src/event.cpp
  src/geometry.cpp
  src/ocr.cpp
  src/scheduler.cpp
  src/tensor.cpp)
target_include_directories(traffic_alpr_core PUBLIC include)
set_target_properties(traffic_alpr_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
project_warnings(traffic_alpr_core)
