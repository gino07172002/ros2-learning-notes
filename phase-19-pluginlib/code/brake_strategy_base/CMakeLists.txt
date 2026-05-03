cmake_minimum_required(VERSION 3.8)
project(brake_strategy_base)

find_package(ament_cmake REQUIRED)

# 純 header-only，只裝 include
install(DIRECTORY include/
  DESTINATION include
)

ament_export_include_directories(include)

ament_package()
