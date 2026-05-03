cmake_minimum_required(VERSION 3.8)
project(my_cpp_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(my_robot_interfaces REQUIRED)

add_executable(countdown_server src/countdown_server.cpp)
ament_target_dependencies(countdown_server
  rclcpp rclcpp_action my_robot_interfaces)

add_executable(countdown_client src/countdown_client.cpp)
ament_target_dependencies(countdown_client
  rclcpp rclcpp_action my_robot_interfaces)

install(TARGETS
  countdown_server
  countdown_client
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
