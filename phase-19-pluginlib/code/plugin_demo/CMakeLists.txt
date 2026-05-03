cmake_minimum_required(VERSION 3.8)
project(plugin_demo)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(pluginlib REQUIRED)
find_package(brake_strategy_base REQUIRED)

add_executable(strategy_loader src/strategy_loader.cpp)
ament_target_dependencies(strategy_loader
  rclcpp pluginlib brake_strategy_base
)

install(TARGETS strategy_loader
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
