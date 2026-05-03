cmake_minimum_required(VERSION 3.8)
project(my_arm_description)

find_package(ament_cmake REQUIRED)

# 安裝 urdf / srdf / launch / rviz 給 robot_state_publisher / MoveIt 取用
install(DIRECTORY urdf srdf launch rviz
  DESTINATION share/${PROJECT_NAME})

ament_package()
