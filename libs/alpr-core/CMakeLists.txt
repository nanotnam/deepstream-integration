add_library(alpr_core STATIC
  src/detection.cpp
  src/event.cpp
  src/geometry.cpp
  src/ocr.cpp
  src/scheduler.cpp
  src/tensor.cpp)
target_include_directories(alpr_core PUBLIC include)
set_target_properties(alpr_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
project_warnings(alpr_core)
