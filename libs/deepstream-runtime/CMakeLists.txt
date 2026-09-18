add_library(deepstream_runtime STATIC
  src/config.cpp
  src/engine_contract.cpp
  src/engine_set.cpp
  src/job_queue.cpp
  src/metadata.cpp
  src/pipeline_description.cpp
  src/plate_worker.cpp
  src/processing.cpp
  src/runtime_control.cpp)
target_sources(deepstream_runtime PRIVATE src/recognition.cpp)
target_include_directories(deepstream_runtime PUBLIC include)
target_link_libraries(deepstream_runtime PUBLIC alpr_core Threads::Threads OpenSSL::Crypto)
set_target_properties(deepstream_runtime PROPERTIES POSITION_INDEPENDENT_CODE ON)
project_warnings(deepstream_runtime)

if(DEEPSTREAM_SDK_ROOT)
  set(CMAKE_CUDA_COMPILER /usr/local/cuda-13.0/bin/nvcc)
  set(CMAKE_CUDA_ARCHITECTURES 86)
  enable_language(CUDA)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET
    gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)
  find_path(DEEPSTREAM_INCLUDE_DIR nvdsmeta.h
    PATHS ${DEEPSTREAM_SDK_ROOT}/sources/includes NO_DEFAULT_PATH REQUIRED)
  find_path(CUDA_INCLUDE_DIR cuda_runtime_api.h
    PATHS /usr/local/cuda/include
          /usr/local/cuda-13.0/targets/x86_64-linux/include
    NO_DEFAULT_PATH REQUIRED)
  find_library(CUDART_LIBRARY cudart
    PATHS /usr/local/cuda/lib64 /usr/local/cuda/targets/x86_64-linux/lib
          /usr/local/cuda-13.0/targets/x86_64-linux/lib
    NO_DEFAULT_PATH REQUIRED)
  find_library(NVDS_META_LIBRARY nvds_meta
    PATHS ${DEEPSTREAM_SDK_ROOT}/lib NO_DEFAULT_PATH REQUIRED)
  find_library(NVDS_GST_META_LIBRARY nvdsgst_meta
    PATHS ${DEEPSTREAM_SDK_ROOT}/lib NO_DEFAULT_PATH REQUIRED)
  find_library(NVDS_INFER_LIBRARY nvds_infer
    PATHS ${DEEPSTREAM_SDK_ROOT}/lib NO_DEFAULT_PATH REQUIRED)
  find_library(NVBUF_SURFACE_LIBRARY nvbufsurface
    PATHS ${DEEPSTREAM_SDK_ROOT}/lib NO_DEFAULT_PATH REQUIRED)
  find_library(NVBUF_SURF_TRANSFORM_LIBRARY nvbufsurftransform
    PATHS ${DEEPSTREAM_SDK_ROOT}/lib NO_DEFAULT_PATH REQUIRED)
  target_sources(deepstream_runtime PRIVATE
    src/deepstream_frame.cpp
    src/deepstream_submission.cpp
    src/gpu_inference_executor.cpp
    src/gpu_preprocess.cu
    src/kafka_publisher.cpp
    src/pipeline_runner.cpp)
  target_include_directories(deepstream_runtime PRIVATE
    ${DEEPSTREAM_INCLUDE_DIR} ${CUDA_INCLUDE_DIR})
  target_compile_definitions(deepstream_runtime PRIVATE
    DEEPSTREAM_RUNTIME_AVAILABLE=1
    DEEPSTREAM_COMPILED_VERSION="9.1")
  target_link_libraries(deepstream_runtime PUBLIC PkgConfig::GSTREAMER PRIVATE
    ${CUDART_LIBRARY}
    ${NVDS_META_LIBRARY}
    ${NVDS_GST_META_LIBRARY}
    ${NVDS_INFER_LIBRARY}
    ${NVBUF_SURFACE_LIBRARY}
    ${NVBUF_SURF_TRANSFORM_LIBRARY})
  set_source_files_properties(src/gpu_preprocess.cu PROPERTIES
    CUDA_STANDARD 17 CUDA_STANDARD_REQUIRED ON)
  set_property(TARGET deepstream_runtime PROPERTY CUDA_ARCHITECTURES 86)
else()
  target_sources(deepstream_runtime PRIVATE src/pipeline_runner_stub.cpp)
endif()
