# slam_from_bag.launch.py — Phase 32
#
# 離線 SLAM:slam_toolbox 用 sim_time,bag play 帶 --clock 餵時間。
#
# Usage:
#   Terminal 1:ros2 launch my_bag_demo slam_from_bag.launch.py
#   Terminal 2:ros2 bag play <bag_dir> --clock 100 \
#               --qos-profile-overrides-path config/qos_override.yaml
#
# 為什麼分兩 terminal:bag play 是 blocking 命令,且使用者通常會想
# 控制 rate / pause / 跳過。launch 內 ExecuteProcess 帶 bag play 也行
# (注解掉的範例放在底下),但 debug 比較難。

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, TimerAction
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_share = get_package_share_directory('my_slam_demo')
    slam_yaml = os.path.join(pkg_share, 'config', 'slam_async.yaml')

    use_sim_time = LaunchConfiguration('use_sim_time')

    # SLAM 必須 use_sim_time:=true,才會吃 bag 的 /clock
    slam = Node(
        package='slam_toolbox',
        executable='async_slam_toolbox_node',
        name='slam_toolbox',
        output='screen',
        parameters=[slam_yaml, {'use_sim_time': use_sim_time}],
    )

    # base_footprint → base_link → laser 的 static TF 通常 bag 內有 /tf_static,
    # 所以這裡不另外發。如果 bag 沒錄 tf_static,要在這裡補 static_transform_publisher。

    return LaunchDescription([
        DeclareLaunchArgument(
            'use_sim_time', default_value='true',
            description='Always true when SLAMing from bag — bag time is sim time'
        ),
        # 等 1 秒,讓 use_sim_time / parameters 設好再啟 SLAM
        TimerAction(period=1.0, actions=[slam]),
    ])
