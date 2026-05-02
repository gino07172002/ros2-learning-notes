cmake_minimum_required(VERSION 3.8)
project(my_cpp_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
# ⚠️ 關鍵：必須宣告依賴自訂 interface 套件
find_package(my_robot_interfaces REQUIRED)

add_executable(smart_brake_v2 src/smart_brake_v2.cpp)
ament_target_dependencies(smart_brake_v2
  rclcpp rclcpp_action geometry_msgs sensor_msgs my_robot_interfaces)

add_executable(approach_client src/approach_client.cpp)
ament_target_dependencies(approach_client
  rclcpp rclcpp_action my_robot_interfaces)

install(TARGETS
  smart_brake_v2
  approach_client
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
