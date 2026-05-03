cmake_minimum_required(VERSION 3.8)
project(my_cpp_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(std_msgs REQUIRED)

# === 一般 executable: executors_demo ===
add_executable(executors_demo src/executors_demo.cpp)
ament_target_dependencies(executors_demo rclcpp)

# === LifecycleNode 是一般 executable，依賴 rclcpp_lifecycle ===
add_executable(lifecycle_demo src/lifecycle_demo.cpp)
ament_target_dependencies(lifecycle_demo rclcpp rclcpp_lifecycle std_msgs)

# === Composable Nodes 編成 shared library，由 component_container 載入 ===
add_library(composable_pub_node SHARED src/composable_publisher.cpp)
ament_target_dependencies(composable_pub_node rclcpp rclcpp_components std_msgs)
rclcpp_components_register_nodes(composable_pub_node "phase09_components::ComposablePublisher")

add_library(composable_sub_node SHARED src/composable_subscriber.cpp)
ament_target_dependencies(composable_sub_node rclcpp rclcpp_components std_msgs)
rclcpp_components_register_nodes(composable_sub_node "phase09_components::ComposableSubscriber")

# === Install ===
install(TARGETS
  executors_demo
  lifecycle_demo
  DESTINATION lib/${PROJECT_NAME})

# Composable libs 安裝到 lib (不是 lib/${PROJECT_NAME})
install(TARGETS
  composable_pub_node
  composable_sub_node
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

install(DIRECTORY
  launch
  DESTINATION share/${PROJECT_NAME})

ament_package()
