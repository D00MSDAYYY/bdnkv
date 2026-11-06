cmake_minimum_required(VERSION 3.20)
project(CartRobot VERSION 1.0 LANGUAGES CXX)

find_package(Threads REQUIRED)

# Логика робота отделена от примера запуска и имитации оборудования.
add_library(robot_core
    src/camera.cpp
    src/drive_controller.cpp
    src/route.cpp
    src/vision_system.cpp
    src/motion_planner.cpp
    src/robot.cpp
    src/task_scheduler.cpp
)
target_include_directories(robot_core PUBLIC include)
target_compile_features(robot_core PUBLIC cxx_std_20)
set_target_properties(robot_core PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(robot_core PUBLIC Threads::Threads)

add_executable(robot_demo
    src/main.cpp
    src/simulated_camera.cpp
    src/simulated_drive_controller.cpp
)
target_link_libraries(robot_demo PRIVATE robot_core)
set_target_properties(robot_demo PROPERTIES CXX_EXTENSIONS OFF)

include(CTest)
if(BUILD_TESTING)
    add_executable(robot_tests tests/robot_tests.cpp)
    target_link_libraries(robot_tests PRIVATE robot_core)
    add_test(NAME robot_logic COMMAND robot_tests)
endif()
