# bridge_only.launch.py — Phase 35
#
# 最小 Foxglove bridge:單 launch,只啟 foxglove_bridge,讓任何瀏覽器
# 連 ws://<host>:8765 就能看到當下 ROS 2 系統的所有 topic / service / param。
#
# 用法:
#   Terminal 1: ros2 launch my_foxglove_demo bridge_only.launch.py
#   瀏覽器:    https://app.foxglove.dev → Open connection → Foxglove WebSocket
#               → ws://localhost:8765
#
# 適合:Phase 06 / 12 / 17 等「我系統已經跑起來,想看一下」的場景。

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    port = LaunchConfiguration('port')
    address = LaunchConfiguration('address')

    bridge = Node(
        package='foxglove_bridge',
        executable='foxglove_bridge',
        name='foxglove_bridge',
        output='screen',
        parameters=[{
            'port': port,
            'address': address,
            # 預設只給未壓縮 — 簡單但耗頻寬。如果走 wifi,改 True 開壓縮
            'use_compression': False,
            # send_buffer_limit_bytes 預設 10MB,對 PointCloud2 容易爆
            'send_buffer_limit_bytes': 100_000_000,
            # capabilities:預設全開(clientPublish, parameters, services, ...);
            # 上線時建議只留訂閱、別開 service call(避免從外面呼叫 lifecycle 之類)
            'capabilities': [
                'clientPublish',
                'parameters',
                'parametersSubscribe',
                'services',
                'connectionGraph',
            ],
        }],
    )

    return LaunchDescription([
        DeclareLaunchArgument('port', default_value='8765'),
        DeclareLaunchArgument('address', default_value='0.0.0.0'),
        bridge,
    ])
