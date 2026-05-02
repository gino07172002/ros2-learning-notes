cmake_minimum_required(VERSION 3.8)
project(my_cpp_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(rcl_interfaces REQUIRED)

add_executable(smart_brake src/smart_brake.cpp)
ament_target_dependencies(smart_brake
  rclcpp geometry_msgs sensor_msgs std_srvs rcl_interfaces)

install(TARGETS
  smart_brake
  DESTINATION lib/${PROJECT_NAME}
)

# 安裝 launch file 才能用 ros2 launch 找到
install(DIRECTORY
  launch
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
