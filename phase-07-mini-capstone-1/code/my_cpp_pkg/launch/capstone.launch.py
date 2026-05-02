# Mini Capstone 1 一鍵啟動 launch file
#
# 一行 ros2 launch 同時啟動：
#   - turtlesim_node       (烏龜模擬器)
#   - smart_brake_node     (你的本章主程式)
#   - 自動 remap cmd_vel -> /turtle1/cmd_vel
#
# 不啟動 fake_lidar——讓你之後手動跑 fake_lidar 來「製造障礙物」效果。

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        Node(
            package='turtlesim',
            executable='turtlesim_node',
            name='turtlesim',
        ),
        Node(
            package='phase07_pkg',
            executable='smart_brake',
            name='smart_brake_node',
            output='screen',
            remappings=[
                ('cmd_vel', '/turtle1/cmd_vel'),
            ],
            parameters=[{
                'max_speed': 0.5,
                'safe_distance': 1.0,
                'corridor_width': 0.4,
            }],
        ),
    ])
