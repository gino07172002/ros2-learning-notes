cmake_minimum_required(VERSION 3.8)
project(my_robot_description)

find_package(ament_cmake REQUIRED)

# 純資源套件 — 沒 .cpp，只裝 urdf + launch
install(DIRECTORY
  urdf
  launch
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
