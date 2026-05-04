cmake_minimum_required(VERSION 3.8)
project(my_camera_demo)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)

add_executable(edge_detector src/edge_detector.cpp)

ament_target_dependencies(edge_detector
  rclcpp
  sensor_msgs
  cv_bridge
)

# OpenCV 是 system-level 套件,要用 target_link_libraries
target_include_directories(edge_detector PUBLIC ${OpenCV_INCLUDE_DIRS})
target_link_libraries(edge_detector ${OpenCV_LIBS})

install(TARGETS edge_detector
  DESTINATION lib/${PROJECT_NAME})

ament_package()
