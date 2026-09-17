add_library(nvdsinfer_custom_alpr SHARED src/parsers.cpp)
target_include_directories(nvdsinfer_custom_alpr PRIVATE
  include
  ${DEEPSTREAM_SDK_ROOT}/sources/includes)
target_link_libraries(nvdsinfer_custom_alpr PRIVATE alpr_core)
project_warnings(nvdsinfer_custom_alpr)
install(TARGETS nvdsinfer_custom_alpr LIBRARY DESTINATION lib)
