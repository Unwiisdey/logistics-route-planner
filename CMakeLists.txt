cmake_minimum_required(VERSION 3.16)

project(LogisticsRoutePlanner VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Stage 1 exposes data declarations only. Add implementation targets as needed.
add_library(logistics_core INTERFACE)
target_include_directories(logistics_core INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_features(logistics_core INTERFACE cxx_std_17)

add_executable(logistics_app src/main.cpp)
target_link_libraries(logistics_app PRIVATE logistics_core)

if(MSVC)
    target_compile_options(logistics_app PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(logistics_app PRIVATE -Wall -Wextra -Wpedantic)
endif()
