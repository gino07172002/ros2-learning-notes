cmake_minimum_required(VERSION 3.8)
project(my_cpp_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(my_robot_interfaces REQUIRED)

add_executable(approach_controller src/approach_controller.cpp)
ament_target_dependencies(approach_controller
  rclcpp rclcpp_lifecycle rclcpp_action
  geometry_msgs sensor_msgs my_robot_interfaces)

install(TARGETS
  approach_controller
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY
  launch
  DESTINATION share/${PROJECT_NAME}
)

# === Tests ===
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_speed_policy test/test_speed_policy.cpp)
endif()

ament_package()
