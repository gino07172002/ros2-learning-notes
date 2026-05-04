cmake_minimum_required(VERSION 3.8)
project(my_bag_demo)

find_package(ament_cmake REQUIRED)

install(DIRECTORY launch scripts config
  DESTINATION share/${PROJECT_NAME})

install(PROGRAMS
  scripts/parse_odom_csv.py
  DESTINATION lib/${PROJECT_NAME})

ament_package()
