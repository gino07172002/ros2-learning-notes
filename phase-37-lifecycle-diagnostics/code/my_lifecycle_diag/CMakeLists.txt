cmake_minimum_required(VERSION 3.8)
project(my_lifecycle_diag)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(lifecycle_msgs REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(diagnostic_updater REQUIRED)

# === 邏輯 library(給 main + gtest 共用)===
add_library(healthy_lifecycle SHARED src/healthy_lifecycle_node.cpp)
target_include_directories(healthy_lifecycle PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
ament_target_dependencies(healthy_lifecycle
  rclcpp rclcpp_lifecycle lifecycle_msgs diagnostic_msgs diagnostic_updater)

# === executable ===
add_executable(healthy_lifecycle_node src/healthy_main.cpp)
target_link_libraries(healthy_lifecycle_node healthy_lifecycle)
ament_target_dependencies(healthy_lifecycle_node rclcpp rclcpp_lifecycle)

install(TARGETS healthy_lifecycle
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

install(TARGETS healthy_lifecycle_node
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_healthy_lifecycle test/test_healthy_lifecycle.cpp)
  target_link_libraries(test_healthy_lifecycle healthy_lifecycle)
  ament_target_dependencies(test_healthy_lifecycle
    rclcpp rclcpp_lifecycle diagnostic_msgs diagnostic_updater)
endif()

ament_export_include_directories(include)
ament_export_libraries(healthy_lifecycle)
ament_export_dependencies(rclcpp rclcpp_lifecycle diagnostic_msgs diagnostic_updater)

ament_package()
