cmake_minimum_required(VERSION 3.8)
project(my_cpp_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)

# 假輪式里程計:車繞圓圈,每秒打滑一點(systematic drift)
add_executable(fake_wheel_odom src/fake_wheel_odom.cpp)
ament_target_dependencies(fake_wheel_odom
  rclcpp nav_msgs geometry_msgs tf2)

# 假 IMU:角速度、線加速度都對齊真值 + 高頻 noise(短期準長期 drift)
add_executable(fake_imu src/fake_imu.cpp)
ament_target_dependencies(fake_imu
  rclcpp sensor_msgs geometry_msgs tf2)

# Comparator:訂 wheel/EKF,跟「真值」(知道是定速圓周運動)比較
find_package(tf2_geometry_msgs REQUIRED)
add_executable(comparator src/comparator.cpp)
ament_target_dependencies(comparator
  rclcpp nav_msgs tf2 tf2_geometry_msgs)

install(TARGETS
  fake_wheel_odom fake_imu comparator
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME})

ament_package()
