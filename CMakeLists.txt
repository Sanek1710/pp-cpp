cmake_minimum_required(VERSION 3.15)
project(pp-cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(third-party)

add_executable(pp-cpp main.cpp)

add_executable(perftest perftest.cpp)
target_link_libraries(perftest unordered_dense)

add_executable(pp-cursored PPCursored.cpp)
target_link_libraries(pp-cursored unordered_dense)

add_compile_definitions(pp-cursored PRIVATE ROOT="${PROJECT_SOURCE_DIR}")
