cmake_minimum_required(VERSION 3.15)
project(pp-cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wattributes)
add_compile_definitions(PRIVATE ROOT="${PROJECT_SOURCE_DIR}/")

add_subdirectory(third-party)

add_executable(perftest perftest.cpp)
target_link_libraries(perftest unordered_dense)

add_subdirectory(tkz)
add_library(preprocessor Printable.cpp Preprocessor.cpp)
target_link_libraries(preprocessor tkz unordered_dense)

add_executable(main main.cpp)
target_link_libraries(main preprocessor)

enable_testing()
add_subdirectory(tests)

add_executable(storage Storage.cpp)
target_link_libraries(storage unordered_dense)
