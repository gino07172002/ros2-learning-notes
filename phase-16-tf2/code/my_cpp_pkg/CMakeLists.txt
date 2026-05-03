cmake_minimum_required(VERSION 3.8)
project(my_cpp_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2 REQUIRED)
find_package(geometry_msgs REQUIRED)

add_executable(static_broadcaster src/static_broadcaster.cpp)
ament_target_dependencies(static_broadcaster
  rclcpp tf2_ros tf2 geometry_msgs)

add_executable(dynamic_broadcaster src/dynamic_broadcaster.cpp)
ament_target_dependencies(dynamic_broadcaster
  rclcpp tf2_ros tf2 geometry_msgs)

add_executable(tf_listener src/tf_listener.cpp)
ament_target_dependencies(tf_listener
  rclcpp tf2_ros tf2 geometry_msgs)

install(TARGETS
  static_broadcaster
  dynamic_broadcaster
  tf_listener
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
