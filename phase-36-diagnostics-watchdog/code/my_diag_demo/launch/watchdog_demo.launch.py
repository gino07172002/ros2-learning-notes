# watchdog_demo.launch.py — Phase 36
#
# 啟動:
#   1. heartbeat_watchdog (監控 /lidar_hb /imu_hb)
#   2. fake_heartbeater for /lidar_hb (永遠跳)
#   3. fake_heartbeater for /imu_hb (跑 5 秒就死,模擬 sensor 掛了)
#   4. diagnostic_aggregator (聚合 /diagnostics → /diagnostics_agg)
#
# 跑起來後 5 秒前 OK,5 秒後 imu_hb 變 STALE → 整體 WARN。

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    pkg_share = get_package_share_directory('my_diag_demo')
    aggregator_yaml = os.path.join(pkg_share, 'config', 'aggregator.yaml')

    watchdog = Node(
        package='my_diag_demo',
        executable='watchdog_node',
        name='watchdog_node',
        output='screen',
        parameters=[{
            'watched_topics': ['/lidar_hb', '/imu_hb'],
            'timeout_sec': 1.0,
        }],
    )

    # /lidar_hb 永遠跳
    lidar_hb = Node(
        package='my_diag_demo',
        executable='fake_heartbeater',
        name='lidar_heartbeater',
        output='screen',
        parameters=[{
            'hb_topic': '/lidar_hb',
            'hb_period_ms': 200,
            'stop_after_sec': -1.0,
        }],
    )

    # /imu_hb 5 秒後死掉
    imu_hb = Node(
        package='my_diag_demo',
        executable='fake_heartbeater',
        name='imu_heartbeater',
        output='screen',
        parameters=[{
            'hb_topic': '/imu_hb',
            'hb_period_ms': 200,
            'stop_after_sec': 5.0,
        }],
    )

    aggregator = Node(
        package='diagnostic_aggregator',
        executable='aggregator_node',
        name='diagnostic_aggregator',
        output='screen',
        parameters=[aggregator_yaml],
    )

    return LaunchDescription([watchdog, lidar_hb, imu_hb, aggregator])
