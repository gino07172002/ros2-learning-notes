# ekf_demo.launch.py — Phase 20A
#
# 啟動全套 EKF demo:
#   1. fake_wheel_odom — 發 /wheel/odometry(線速度高估 5%)
#   2. fake_imu        — 發 /imu/data(角速度低估 5% + noise)
#   3. ekf_node        — 融合兩者,發 /odometry/filtered + odom→base_link TF
#
# 驗證流程在 README 的 Demo 段。

from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    # 部署到 WSL workspace 後套件叫 phase20a_pkg(部署腳本會 sed 改名)
    pkg_share = get_package_share_directory('phase20a_pkg')
    ekf_yaml = os.path.join(pkg_share, 'config', 'ekf.yaml')

    return LaunchDescription([
        # Static TF: imu_link 跟 base_link 同位置(教學簡化;實機要設真實安裝偏移)
        # ⚠️ 沒這條 EKF 會把 IMU 訊息全部丟掉(找不到 imu_link → base_link 變換)
        Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='imu_static_tf',
            arguments=['0', '0', '0', '0', '0', '0', 'base_link', 'imu_link'],
        ),
        Node(
            package='phase20a_pkg',
            executable='fake_wheel_odom',
            name='fake_wheel_odom',
            output='screen',
        ),
        Node(
            package='phase20a_pkg',
            executable='fake_imu',
            name='fake_imu',
            output='screen',
        ),
        Node(
            package='robot_localization',
            executable='ekf_node',
            name='ekf_filter_node',
            output='screen',
            parameters=[ekf_yaml],
        ),
        Node(
            package='phase20a_pkg',
            executable='comparator',
            name='ekf_comparator',
            output='screen',
        ),
    ])
