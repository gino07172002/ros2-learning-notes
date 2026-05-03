# composition_demo.launch.py
# 把兩個可組合 Node 載入同一個 process（同一個 Container）
#
# 跟「兩個獨立 ros2 run」的差別：
#   - 一個 process（看 ps -ef 會看到）
#   - intra-process communication：同 process 內 publish 直接記憶體共享，不經過序列化
#   - 啟動快、共用 Executor

from launch import LaunchDescription
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode


def generate_launch_description():
    return LaunchDescription([
        ComposableNodeContainer(
            name='phase09_container',
            namespace='',
            package='rclcpp_components',
            executable='component_container',
            composable_node_descriptions=[
                ComposableNode(
                    package='phase09_pkg',
                    plugin='phase09_components::ComposablePublisher',
                    name='publisher_node',
                ),
                ComposableNode(
                    package='phase09_pkg',
                    plugin='phase09_components::ComposableSubscriber',
                    name='subscriber_node',
                ),
            ],
            output='screen',
        ),
    ])
