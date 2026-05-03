# plan_demo.launch.py — Phase 22B
#
# 啟動完整 MoveIt + 我們的 plan_demo:
#   1. Include my_arm_moveit_config 的 move_group.launch.py(rsp/jsp/move_group)
#   2. (delay 8s)plan_demo Node 跑 4 段 plan demo

from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, TimerAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
from ament_index_python.packages import get_package_share_directory
import os
import yaml


def load_yaml(package_name, file_path):
    package_path = get_package_share_directory(package_name)
    absolute_file_path = os.path.join(package_path, file_path)
    with open(absolute_file_path, 'r') as f:
        return yaml.safe_load(f)


def generate_launch_description():
    moveit_pkg = get_package_share_directory('my_arm_moveit_config')
    arm_desc_pkg = get_package_share_directory('my_arm_description')

    move_group = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(moveit_pkg, 'launch', 'move_group.launch.py')),
    )

    # ⚠️ 雷:MoveGroupInterface 需要看到 robot_description / robot_description_semantic
    #     /robot_description_kinematics 跟 move_group 同一份。
    # plan_demo 是獨立 Node,沒這些參數會炸 "Unable to parse SRDF"。
    xacro_file = os.path.join(arm_desc_pkg, 'urdf', 'arm.urdf.xacro')
    srdf_file = os.path.join(arm_desc_pkg, 'srdf', 'arm.srdf')

    robot_description = {
        'robot_description': ParameterValue(
            Command(['xacro ', xacro_file]),
            value_type=str)
    }

    with open(srdf_file, 'r') as f:
        srdf_content = f.read()
    robot_description_semantic = {'robot_description_semantic': srdf_content}

    kinematics = {'robot_description_kinematics':
                  load_yaml('my_arm_moveit_config', 'config/kinematics.yaml')}

    plan_demo = TimerAction(
        period=8.0,
        actions=[
            Node(
                package='my_arm_moveit_demo',
                executable='plan_demo',
                name='plan_demo',
                output='screen',
                parameters=[
                    robot_description,
                    robot_description_semantic,
                    kinematics,
                ],
            ),
        ],
    )

    return LaunchDescription([move_group, plan_demo])
