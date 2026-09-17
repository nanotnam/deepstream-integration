cmake_minimum_required(VERSION 3.20)
project(deepstream_integration VERSION 0.1.0 LANGUAGES CXX)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build native tests" ON)
set(DEEPSTREAM_SDK_ROOT "" CACHE PATH "DeepStream SDK root; empty builds the portable local surface")

function(project_warnings target)
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endfunction()

add_subdirectory(libs/alpr-core)
add_subdirectory(libs/deepstream-runtime)
add_subdirectory(apps/traffic-alpr)

if(DEEPSTREAM_SDK_ROOT)
  add_subdirectory(plugins/alpr-infer)
  add_subdirectory(plugins/alpr-preprocess)
  add_subdirectory(plugins/alpr-msgconv)
endif()

if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests/native)
endif()
