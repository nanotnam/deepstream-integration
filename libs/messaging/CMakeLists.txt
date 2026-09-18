add_library(messaging_runtime STATIC src/runtime.cpp)
target_include_directories(messaging_runtime PUBLIC include)
target_link_libraries(messaging_runtime PUBLIC Threads::Threads)
set_target_properties(messaging_runtime PROPERTIES POSITION_INDEPENDENT_CODE ON)
project_warnings(messaging_runtime)

if(BUILD_TESTING)
  add_executable(messaging_runtime_test tests/runtime_test.cpp)
  target_link_libraries(messaging_runtime_test PRIVATE messaging_runtime)
  target_compile_options(messaging_runtime_test PRIVATE
    -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
  add_test(NAME messaging_runtime_test COMMAND messaging_runtime_test)
endif()
