# display.launch.py — Phase 20B
# 啟動 robot_state_publisher 把 URDF 發 /robot_description + 持續發 TF
# + joint_state_publisher (CLI 版) 或 joint_state_publisher_gui (GUI 版,可拉 slider)
#
# 用法:
#   ros2 launch my_arm_description display.launch.py            # 預設無 GUI,joint 全 0
#   ros2 launch my_arm_description display.launch.py gui:=true  # 開 GUI 6 slider
#
# ⚠️ 兩個 jsp 互斥:同時啟動會雙發 /joint_states,RViz 內手臂在 0 跟拉動值間跳動

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition, UnlessCondition
from launch.substitutions import Command, LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    pkg_share = get_package_share_directory('my_arm_description')
    xacro_file = os.path.join(pkg_share, 'urdf', 'arm.urdf.xacro')

    # xacro 在 launch 內展開:Command 把 xacro 當 process 跑,結果 string
    # ⚠️ 雷:必須用 ParameterValue(..., value_type=str) 包,否則 launch 會
    # 試圖把 URDF 當 YAML parse → "Unable to parse... as yaml"
    robot_description = ParameterValue(
        Command(['xacro ', xacro_file]),
        value_type=str
    )

    gui_arg = DeclareLaunchArgument(
        'gui', default_value='false',
        description='Use joint_state_publisher_gui (true) or headless joint_state_publisher (false)')

    use_gui = LaunchConfiguration('gui')

    return LaunchDescription([
        gui_arg,

        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            parameters=[{'robot_description': robot_description}],
            output='screen',
        ),

        # CLI 版 jsp:default,所有 joint 維持 0 給 rsp 用,RViz 看靜態手臂
        Node(
            package='joint_state_publisher',
            executable='joint_state_publisher',
            output='screen',
            condition=UnlessCondition(use_gui),
        ),

        # GUI 版 jsp:傳 gui:=true 才啟,跟 CLI 版互斥(雷:雙發會跳動)
        Node(
            package='joint_state_publisher_gui',
            executable='joint_state_publisher_gui',
            output='screen',
            condition=IfCondition(use_gui),
        ),
    ])
