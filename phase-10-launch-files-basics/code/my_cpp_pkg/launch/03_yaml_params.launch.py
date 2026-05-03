# 03_yaml_params.launch.py
# 從 YAML 檔案載入參數——business code 慣例
#
# 為什麼用 YAML 不直接寫進 launch：
#   - 同一份 launch 給不同場景用（dev / staging / prod 三份 YAML）
#   - 給非開發者改參數（YAML 比 Python 友善）
#   - launch 改動少 → CI/CD 不會頻繁重 build

import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    # 找到本套件的 share 路徑（colcon 安裝後的位置）
    pkg_share = get_package_share_directory('phase10_pkg')
    config_path = os.path.join(pkg_share, 'config', 'smart_brake.yaml')

    return LaunchDescription([
        Node(
            package='turtlesim',
            executable='turtlesim_node',
            name='turtlesim',
        ),
        Node(
            package='phase08_pkg',
            executable='smart_brake_v2',
            name='smart_brake_v2',
            output='screen',
            remappings=[('cmd_vel', '/turtle1/cmd_vel')],
            # 直接傳檔案路徑——launch 會自動讀 YAML 並注入
            parameters=[config_path],
        ),
    ])
