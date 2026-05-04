cmake_minimum_required(VERSION 3.8)
project(my_pcl_demo)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(vision_msgs REQUIRED)
find_package(pcl_conversions REQUIRED)
find_package(PCL REQUIRED COMPONENTS common filters segmentation search)

add_executable(cluster_extractor src/cluster_extractor.cpp)

ament_target_dependencies(cluster_extractor
  rclcpp
  sensor_msgs
  vision_msgs
  pcl_conversions
)

target_include_directories(cluster_extractor PUBLIC ${PCL_INCLUDE_DIRS})
target_link_libraries(cluster_extractor ${PCL_LIBRARIES})

install(TARGETS cluster_extractor
  DESTINATION lib/${PROJECT_NAME})

ament_package()
