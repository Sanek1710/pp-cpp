enable_testing()

add_compile_definitions(PRIVATE TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/")

add_executable(tests tests.cpp)
target_link_libraries(tests preprocessor)
target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR})

add_test(NAME sqlite COMMAND tests sqlite)
add_test(NAME perf COMMAND tests perf)
add_test(NAME user COMMAND tests user)
add_test(NAME small COMMAND tests small) 
