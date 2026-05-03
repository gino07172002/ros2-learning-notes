cmake_minimum_required(VERSION 3.8)
project(my_cpp_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

# === Tests ===
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  # 1. 純邏輯測試（不需要 rclcpp runtime）
  ament_add_gtest(test_brake_calculator
    test/test_brake_calculator.cpp
  )

  # 2. 含 rclcpp 的測試
  ament_add_gtest(test_with_rclcpp
    test/test_with_rclcpp.cpp
  )
  ament_target_dependencies(test_with_rclcpp rclcpp std_msgs)
endif()

ament_package()
