cmake_minimum_required(VERSION 3.16)

project(LogisticsRoutePlanner VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(logistics_core STATIC
    src/model/Node.cpp
    src/model/Edge.cpp
    src/graph/Graph.cpp
    src/config/ConfigLoader.cpp
    src/planning/Dijkstra.cpp
    src/planning/Planner.cpp
    src/visualization/Visualizer.cpp
)
target_include_directories(logistics_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_include_directories(logistics_core SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
target_compile_features(logistics_core PUBLIC cxx_std_17)

add_executable(logistics_app src/main.cpp)
target_link_libraries(logistics_app PRIVATE logistics_core)

function(logistics_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()
logistics_warnings(logistics_core)
logistics_warnings(logistics_app)

# Works from the build directory and beside the executable, without source-tree paths.
configure_file(config/network.json config/network.json COPYONLY)
# Run even when only JSON changed and the executable does not need relinking.
add_custom_target(copy_network_config ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:logistics_app>/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/network.json"
        "$<TARGET_FILE_DIR:logistics_app>/config/network.json"
    DEPENDS logistics_app "${CMAKE_CURRENT_SOURCE_DIR}/config/network.json"
)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
