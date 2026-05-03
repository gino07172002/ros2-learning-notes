# 02_remap_and_params.launch.py
# 進階：多個 Node + remap + 參數注入
#
# 這個 launch file 一次啟動兩個 Node：
#   1. turtlesim - 提供烏龜模擬
#   2. smart_brake_v2 (Phase 08) - 控制烏龜，從 launch 注入參數

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        # ── Node 1: turtlesim ──
        Node(
            package='turtlesim',
            executable='turtlesim_node',
            name='turtlesim',
        ),

        # ── Node 2: smart_brake_v2 ──
        Node(
            package='phase08_pkg',
            executable='smart_brake_v2',
            name='smart_brake_v2',
            output='screen',  # log 印到 terminal（不加會被吃掉）
            # 把 program 內的 'cmd_vel' 重新映射到 turtlesim 的 '/turtle1/cmd_vel'
            # ⚠️ 注意 source/destination 順序——左邊是程式裡寫的、右邊是真實 topic
            remappings=[
                ('cmd_vel', '/turtle1/cmd_vel'),
            ],
            # parameters=[...] 會在 Node 啟動時呼叫 declare_parameter 之前注入。
            # 沒有它，code 裡的 declare_parameter(default) 會生效。
            # 有它，這裡的值會覆蓋掉 default。
            parameters=[{
                'use_sim_time': False,
            }],
        ),
    ])
