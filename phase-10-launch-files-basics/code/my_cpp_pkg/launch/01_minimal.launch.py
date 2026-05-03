# 01_minimal.launch.py
# 最小範例：啟動單一 Node
#
# Launch File 是 Python 程式（不是 YAML），所以可以用 if/for 動態組合。
# 但它必須提供一個 generate_launch_description() 函數回傳 LaunchDescription 物件。

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        Node(
            package='turtlesim',           # 從哪個套件啟動
            executable='turtlesim_node',   # 對應 ros2 run xxx yyy 的 yyy
            name='my_turtle',              # 重新命名節點（取代 code 內的 default name）
        ),
    ])
