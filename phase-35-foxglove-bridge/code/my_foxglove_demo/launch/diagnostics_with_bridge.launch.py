# diagnostics_with_bridge.launch.py — Phase 35
#
# 串 Phase 36 的 Diagnostics + Watchdog 一起跑,瀏覽器直接看儀表板。
#
# 啟動順序:
#   1. heartbeat_watchdog + 兩個 fake_heartbeater(其中 imu_hb 5 秒後死)
#   2. diagnostic_aggregator
#   3. foxglove_bridge(port 8765)
#
# 連到 https://app.foxglove.dev 用 ws://localhost:8765,加 Diagnostics panel
# 訂 /diagnostics_agg → 5 秒前綠燈,5 秒後 imu 變黃。

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node


def generate_launch_description():
    diag_share = get_package_share_directory('my_diag_demo')
    diag_launch = os.path.join(diag_share, 'launch', 'watchdog_demo.launch.py')

    fox_share = get_package_share_directory('my_foxglove_demo')
    bridge_launch = os.path.join(fox_share, 'launch', 'bridge_only.launch.py')

    return LaunchDescription([
        IncludeLaunchDescription(PythonLaunchDescriptionSource(diag_launch)),
        IncludeLaunchDescription(PythonLaunchDescriptionSource(bridge_launch)),
    ])
