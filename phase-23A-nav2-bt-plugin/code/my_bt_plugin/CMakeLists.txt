cmake_minimum_required(VERSION 3.8)
project(my_bt_plugin)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(behaviortree_cpp_v3 REQUIRED)
find_package(nav2_behavior_tree REQUIRED)
find_package(sensor_msgs REQUIRED)

# === BT plugin 是 shared library ===
add_library(is_battery_low_condition_bt_node SHARED
  src/is_battery_low_condition.cpp)

target_include_directories(is_battery_low_condition_bt_node
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

ament_target_dependencies(is_battery_low_condition_bt_node
  rclcpp
  behaviortree_cpp_v3
  nav2_behavior_tree
  sensor_msgs)

# 編成 SHARED lib,nav2_bt_navigator runtime dlopen
target_compile_definitions(is_battery_low_condition_bt_node
  PRIVATE BT_PLUGIN_EXPORT)

install(TARGETS is_battery_low_condition_bt_node
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

install(DIRECTORY include/
  DESTINATION include)

# === Tests:用 BT.cpp 自己 load 我們的 plugin 並 tick 一遍 ===
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_load_plugin test/test_load_plugin.cpp)
  target_link_libraries(test_load_plugin
    is_battery_low_condition_bt_node)
  ament_target_dependencies(test_load_plugin
    rclcpp behaviortree_cpp_v3 sensor_msgs)
endif()

ament_export_include_directories(include)
ament_export_libraries(is_battery_low_condition_bt_node)
ament_export_dependencies(rclcpp behaviortree_cpp_v3 nav2_behavior_tree sensor_msgs)

ament_package()
