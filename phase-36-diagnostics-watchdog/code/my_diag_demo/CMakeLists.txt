cmake_minimum_required(VERSION 3.8)
project(my_diag_demo)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(diagnostic_updater REQUIRED)

# === watchdog 邏輯包成 library,給 main 跟 gtest 共用 ===
add_library(heartbeat_watchdog SHARED src/heartbeat_watchdog.cpp)
target_include_directories(heartbeat_watchdog PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
ament_target_dependencies(heartbeat_watchdog
  rclcpp std_msgs diagnostic_msgs diagnostic_updater)

# === executables ===
add_executable(watchdog_node src/watchdog_main.cpp)
target_link_libraries(watchdog_node heartbeat_watchdog)
ament_target_dependencies(watchdog_node rclcpp)

add_executable(fake_heartbeater src/fake_heartbeater.cpp)
ament_target_dependencies(fake_heartbeater rclcpp std_msgs)

install(TARGETS heartbeat_watchdog
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

install(TARGETS watchdog_node fake_heartbeater
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

# === Tests ===
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_heartbeat_watchdog test/test_heartbeat_watchdog.cpp)
  target_link_libraries(test_heartbeat_watchdog heartbeat_watchdog)
  ament_target_dependencies(test_heartbeat_watchdog
    rclcpp std_msgs diagnostic_msgs diagnostic_updater)
endif()

ament_export_include_directories(include)
ament_export_libraries(heartbeat_watchdog)
ament_export_dependencies(rclcpp std_msgs diagnostic_msgs diagnostic_updater)

ament_package()
