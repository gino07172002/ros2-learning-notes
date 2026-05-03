cmake_minimum_required(VERSION 3.8)
project(capstone_a)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav2_msgs REQUIRED)

add_executable(auto_navigator src/auto_navigator.cpp)
ament_target_dependencies(auto_navigator
  rclcpp rclcpp_action geometry_msgs nav2_msgs)

install(TARGETS auto_navigator
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME})

ament_package()
