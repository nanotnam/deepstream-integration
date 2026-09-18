add_subdirectory(core)
add_subdirectory(runtime)

add_executable(motorcycle-violation src/main.cpp)
target_link_libraries(motorcycle-violation PRIVATE motorcycle_violation_runtime)
project_warnings(motorcycle-violation)

if(DEEPSTREAM_SDK_ROOT)
  add_subdirectory(plugins/infer)
  add_subdirectory(plugins/preprocess)
  add_subdirectory(plugins/msgconv)
endif()

if(BUILD_TESTING)
  add_subdirectory(tests/native)
endif()

install(TARGETS motorcycle-violation RUNTIME DESTINATION bin)
install(DIRECTORY configs/ DESTINATION share/motorcycle-violation/configs)
install(DIRECTORY models/bundles/ DESTINATION share/motorcycle-violation/models/bundles)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/contracts/alpr/
        DESTINATION share/motorcycle-violation/schemas)
install(FILES docs/operations.md DESTINATION share/motorcycle-violation/docs)
