cmake_minimum_required(VERSION 3.20)
project(deepstream_integration VERSION 0.1.0 LANGUAGES CXX)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build native tests" ON)
option(BUILD_TRAFFIC_ALPR "Build the Traffic ALPR application" ON)
option(BUILD_APP_TEMPLATE "Build the example application template" OFF)
set(DEEPSTREAM_SDK_ROOT "" CACHE PATH "DeepStream SDK root; empty builds the portable local surface")

if(BUILD_TESTING)
  enable_testing()
endif()

function(project_warnings target)
  target_compile_options(${target} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic;-Werror>)
endfunction()

add_subdirectory(libs/service-runtime)
add_subdirectory(libs/messaging)
add_subdirectory(libs/deepstream-runtime)

if(BUILD_TRAFFIC_ALPR)
  add_subdirectory(apps/traffic-alpr)
endif()

if(BUILD_APP_TEMPLATE)
  add_subdirectory(apps/app-template)
endif()
