cmake_minimum_required(VERSION 3.8)
project(my_cpp_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)

add_executable(auto_drive src/auto_drive.cpp)
ament_target_dependencies(auto_drive rclcpp geometry_msgs)

install(TARGETS
  auto_drive
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
