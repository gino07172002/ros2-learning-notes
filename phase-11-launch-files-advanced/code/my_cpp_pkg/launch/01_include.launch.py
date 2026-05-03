# 01_include.launch.py
# IncludeLaunchDescription：把另一個 launch file 嵌套進來
#
# 業界場景：
#   - bringup.launch 包含 perception.launch + control.launch + visualization.launch
#   - 每個子 launch 由不同團隊維護，bringup 只負責「把它們組起來」
#   - Nav2 / MoveIt 都是這個架構

import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node


def generate_launch_description():
    # 找到 phase10_pkg 的 launch file 路徑
    phase10_share = get_package_share_directory('phase10_pkg')
    other_launch = os.path.join(phase10_share, 'launch', '02_remap_and_params.launch.py')

    return LaunchDescription([
        # 嵌套：把整個 phase10 的 02 launch 拉進來
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(other_launch),
        ),
        # 額外加一個 rqt_graph，讓使用者開機就看到通訊圖
        Node(
            package='rqt_graph',
            executable='rqt_graph',
            name='rqt_graph_viewer',
        ),
    ])
