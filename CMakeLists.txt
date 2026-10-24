cmake_minimum_required(VERSION 3.16)

project(LogisticsRoutePlanner VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(logistics_core STATIC
    src/model/Node.cpp
    src/model/Edge.cpp
    src/model/Vehicle.cpp
    src/model/TimeWindow.cpp
    src/model/Order.cpp
    src/graph/Graph.cpp
    src/config/ConfigLoader.cpp
    src/config/OrderConfigLoader.cpp
    src/planning/Dijkstra.cpp
    src/planning/ShortestPathCache.cpp
    src/planning/Planner.cpp
    src/planning/DynamicOrder.cpp
    src/planning/Replanner.cpp
    src/traffic/TrafficSimulator.cpp
    src/visualization/Visualizer.cpp
    src/simulation/SimClock.cpp
    src/simulation/Simulation.cpp
    src/simulation/SimulationOrders.cpp
    src/simulation/FleetSimulation.cpp
    src/simulation/SimulationTraffic.cpp
)
target_include_directories(logistics_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_include_directories(logistics_core SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
target_compile_features(logistics_core PUBLIC cxx_std_17)

add_executable(logistics_app src/main.cpp)
target_link_libraries(logistics_app PRIVATE logistics_core)
target_compile_definitions(logistics_app PRIVATE LOGISTICS_VERSION="${PROJECT_VERSION}")

function(logistics_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()
logistics_warnings(logistics_core)
logistics_warnings(logistics_app)

option(LOGISTICS_BUILD_UI "Build the local web UI bridge" ON)
if(LOGISTICS_BUILD_UI)
    add_executable(logistics_ui_bridge ui/bridge.cpp)
    target_include_directories(logistics_ui_bridge SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
    target_link_libraries(logistics_ui_bridge PRIVATE logistics_core)
    logistics_warnings(logistics_ui_bridge)
endif()

# Works from the build directory and beside the executable, without source-tree paths.
configure_file(config/network.json config/network.json COPYONLY)
configure_file(config/orders.json config/orders.json COPYONLY)
# Run even when only JSON changed and the executable does not need relinking.
add_custom_target(copy_network_config ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:logistics_app>/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/network.json"
        "$<TARGET_FILE_DIR:logistics_app>/config/network.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/orders.json"
        "$<TARGET_FILE_DIR:logistics_app>/config/orders.json"
    DEPENDS logistics_app "${CMAKE_CURRENT_SOURCE_DIR}/config/network.json" "${CMAKE_CURRENT_SOURCE_DIR}/config/orders.json"
)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
    if(LOGISTICS_BUILD_UI)
        find_package(Python3 3.9 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            foreach(suite bridge http)
                add_test(NAME ui.${suite}
                    COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s tests -p "test_ui_${suite}.py" -v)
                set_tests_properties(ui.${suite} PROPERTIES
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                    ENVIRONMENT "LOGISTICS_UI_BRIDGE=$<TARGET_FILE:logistics_ui_bridge>")
            endforeach()
        endif()
    endif()
endif()
