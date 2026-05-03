# display.launch.py — Phase 20B
# 啟動 robot_state_publisher 把 URDF 發 /robot_description + 持續發 TF
#
# 真正的 RViz 顯示要使用者本機跑(這章在 WSL 純驗證 TF / TF tree)

from launch import LaunchDescription
from launch.substitutions import Command
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

    return LaunchDescription([
        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            parameters=[{'robot_description': robot_description}],
            output='screen',
        ),
        # joint_state_publisher 沒 GUI 也要,否則 robot_state_publisher 不知道 joint angle
        Node(
            package='joint_state_publisher',
            executable='joint_state_publisher',
            output='screen',
        ),
    ])
