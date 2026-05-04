"""
Multi-robot/01: 同時 spawn 3 台 turtlebot3 在 Gazebo,各自用 namespace 隔離。

預期結果:
  /tb1/cmd_vel  /tb2/cmd_vel  /tb3/cmd_vel  ← 3 套 cmd_vel
  /tb1/odom     /tb2/odom     /tb3/odom     ← 3 套 odom
  TF: world → tb1/odom → tb1/base_link
              tb2/odom → tb2/base_link
              tb3/odom → tb3/base_link

跑法:
  export TURTLEBOT3_MODEL=burger
  ros2 launch multi_robot_demo spawn_three.launch.py
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, TimerAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node


def _read_urdf_with_prefix(prefix: str) -> str:
    """讀 turtlebot3 URDF,把所有 link/joint name 加 prefix。

    實務上更乾淨的做法是用 xacro:macro,本檔從簡用字串替換。
    """
    urdf_path = os.path.join(
        get_package_share_directory('turtlebot3_description'),
        'urdf', 'turtlebot3_burger.urdf')
    with open(urdf_path, 'r') as f:
        urdf = f.read()
    # 簡易替換 — 實務上請用 xacro
    # link/joint 屬性加 prefix
    for tag in ['link name=', 'joint name=', 'parent link=', 'child link=']:
        urdf = urdf.replace(f'{tag}"', f'{tag}"{prefix}/')
    return urdf


def _spawn_robot(name: str, x: float, y: float):
    """給一台 turtlebot 生 robot_state_publisher + spawn_entity 兩個 Node"""
    urdf = _read_urdf_with_prefix(name)

    rsp = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        namespace=name,
        name='robot_state_publisher',
        parameters=[{
            'robot_description': urdf,
            'frame_prefix': f'{name}/',
            'use_sim_time': True,
        }],
        output='screen',
    )

    spawn = Node(
        package='gazebo_ros',
        executable='spawn_entity.py',
        name=f'spawn_{name}',
        arguments=[
            '-topic', f'/{name}/robot_description',
            '-entity', name,
            '-x', str(x),
            '-y', str(y),
            '-z', '0.01',
            '-robot_namespace', name,
        ],
        output='screen',
    )

    return [rsp, spawn]


def generate_launch_description():
    # 1. 啟動 Gazebo(empty world)
    gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(
                get_package_share_directory('gazebo_ros'),
                'launch', 'gazebo.launch.py')))

    # 2. 3 台機器人,擺三個對角位置
    tb1 = _spawn_robot('tb1', -2.0, -2.0)
    tb2 = _spawn_robot('tb2',  0.0,  0.0)
    tb3 = _spawn_robot('tb3',  2.0,  2.0)

    # 3. 用 TimerAction 延遲 spawn,讓 Gazebo 第一台 spawn 完再下一台
    return LaunchDescription([
        gazebo,
        TimerAction(period=2.0, actions=tb1),
        TimerAction(period=4.0, actions=tb2),
        TimerAction(period=6.0, actions=tb3),
    ])
