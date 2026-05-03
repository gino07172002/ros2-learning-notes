cmake_minimum_required(VERSION 3.8)
project(my_robot_interfaces)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
# 兩個關鍵依賴：rosidl 生成器 + Header 用到的 std_msgs
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)

# 列出所有 .msg / .srv / .action 檔案
# ⚠️ 注意：路徑是相對於 package 根目錄，且必須包含子資料夾名（msg/、srv/、action/）
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/BrakeStatus.msg"
  "srv/SetBrakeMode.srv"
  "action/Approach.action"
  "action/Countdown.action"
  DEPENDENCIES std_msgs
  # ↑ 如果 .msg 內有引用其他套件的型別（這裡用了 std_msgs/Header），
  # 必須在這裡再宣告一次。漏寫會在 build 時報錯：
  #   'std_msgs/msg/Header' 找不到
)

# 讓下游套件可以 find_package(my_robot_interfaces) 並用我們生成的 type
ament_export_dependencies(rosidl_default_runtime)

ament_package()
