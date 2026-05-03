cmake_minimum_required(VERSION 3.8)
project(brake_strategy_plugins)

find_package(ament_cmake REQUIRED)
find_package(pluginlib REQUIRED)
find_package(brake_strategy_base REQUIRED)

# 把兩個 plugin 編成同一個 shared library
add_library(${PROJECT_NAME} SHARED
  src/conservative_strategy.cpp
  src/aggressive_strategy.cpp
)
ament_target_dependencies(${PROJECT_NAME}
  pluginlib
  brake_strategy_base
)

# ⚠️ 把 plugins.xml 註冊給 pluginlib（讓外部找得到）
pluginlib_export_plugin_description_file(brake_strategy_base plugins.xml)

install(TARGETS ${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(FILES plugins.xml
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
