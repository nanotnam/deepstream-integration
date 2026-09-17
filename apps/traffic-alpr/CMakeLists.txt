add_executable(traffic-alpr src/main.cpp)
target_link_libraries(traffic-alpr PRIVATE deepstream_runtime)
project_warnings(traffic-alpr)
install(TARGETS traffic-alpr RUNTIME DESTINATION bin)

