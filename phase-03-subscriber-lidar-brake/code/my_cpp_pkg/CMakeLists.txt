cmake_minimum_required(VERSION 3.8)
project(my_cpp_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)

add_executable(auto_brake src/auto_brake.cpp)
ament_target_dependencies(auto_brake rclcpp geometry_msgs sensor_msgs)

install(TARGETS
  auto_brake
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
