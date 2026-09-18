add_subdirectory(core)
add_subdirectory(runtime)

add_executable(traffic-alpr src/main.cpp)
target_link_libraries(traffic-alpr PRIVATE traffic_alpr_runtime)
project_warnings(traffic-alpr)

if(DEEPSTREAM_SDK_ROOT)
  add_subdirectory(plugins/infer)
  add_subdirectory(plugins/preprocess)
  add_subdirectory(plugins/msgconv)
endif()

if(BUILD_TESTING)
  add_subdirectory(tests/native)
endif()

install(TARGETS traffic-alpr RUNTIME DESTINATION bin)
install(DIRECTORY configs/ DESTINATION share/traffic-alpr/configs)
install(DIRECTORY models/bundles/ DESTINATION share/traffic-alpr/models/bundles)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/contracts/alpr/
        DESTINATION share/traffic-alpr/schemas)
install(FILES docs/operations.md DESTINATION share/traffic-alpr/docs)
