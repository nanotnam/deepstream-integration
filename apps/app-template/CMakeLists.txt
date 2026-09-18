add_executable(deepstream-app-template src/main.cpp)
target_link_libraries(deepstream-app-template PRIVATE
  deepstream_platform messaging_runtime service_runtime)
project_warnings(deepstream-app-template)
