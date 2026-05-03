cmake_minimum_required(VERSION 3.8)
project(my_bt_advanced)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(behaviortree_cpp_v3 REQUIRED)
find_package(nav2_behavior_tree REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)

# === 4 個 BT node 包成一個 SHARED library ===
add_library(my_bt_advanced_nodes SHARED
  src/go_to_charger_action.cpp
  src/once_every_decorator.cpp
  src/count_successes_decorator.cpp
  src/patrol_waypoint_action.cpp
  src/plugin_registration.cpp)

target_include_directories(my_bt_advanced_nodes
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

ament_target_dependencies(my_bt_advanced_nodes
  rclcpp behaviortree_cpp_v3 nav2_behavior_tree
  nav2_msgs geometry_msgs sensor_msgs)

install(TARGETS my_bt_advanced_nodes
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

install(DIRECTORY include/
  DESTINATION include)

install(DIRECTORY trees
  DESTINATION share/${PROJECT_NAME})

# === Tests ===
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_advanced_nodes test/test_advanced_nodes.cpp)
  target_link_libraries(test_advanced_nodes my_bt_advanced_nodes)
  ament_target_dependencies(test_advanced_nodes
    rclcpp behaviortree_cpp_v3)
endif()

ament_export_include_directories(include)
ament_export_libraries(my_bt_advanced_nodes)
ament_export_dependencies(rclcpp behaviortree_cpp_v3 nav2_behavior_tree)

ament_package()
