cmake_minimum_required(VERSION 3.8)
project(my_cpp_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(rcl_interfaces REQUIRED)

add_executable(auto_brake_param src/auto_brake_param.cpp)
ament_target_dependencies(auto_brake_param
  rclcpp geometry_msgs sensor_msgs rcl_interfaces)

install(TARGETS
  auto_brake_param
  DESTINATION lib/${PROJECT_NAME}
)

# 把 config/ 內的 YAML 一起安裝，這樣 launch file 才找得到
install(DIRECTORY
  config
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
