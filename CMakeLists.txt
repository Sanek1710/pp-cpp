cmake_minimum_required(VERSION 3.15)
project(pp-cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wattributes)

add_subdirectory(third-party)

add_executable(pp-cpp main.cpp)

add_executable(perftest perftest.cpp)
target_link_libraries(perftest unordered_dense)

add_executable(pp-cursored PPCursored.cpp)
target_link_libraries(pp-cursored unordered_dense)

add_compile_definitions(pp-cursored PRIVATE ROOT="${PROJECT_SOURCE_DIR}")

add_executable(pp-iterator PPIterator.cpp)
target_link_libraries(pp-iterator unordered_dense)

add_executable(pp-raw-iterator PPRawIterator.cpp)
target_link_libraries(pp-raw-iterator unordered_dense)

add_compile_definitions(pp-raw-iterator PRIVATE ROOT="${PROJECT_SOURCE_DIR}")

add_executable(testin testin.cpp)
add_compile_definitions(testin PRIVATE ROOT="${PROJECT_SOURCE_DIR}")

