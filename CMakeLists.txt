cmake_minimum_required(VERSION 3.15)
project(pp-cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(preprocessor
    preprocessor.cpp
    preprocessor.hpp
)
target_include_directories(preprocessor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(pp-cpp main.cpp)
target_link_libraries(pp-cpp PRIVATE preprocessor)

