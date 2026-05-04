#!/usr/bin/env python3
"""parse_odom_csv.py — Phase 32

讀 rosbag2 (mcap 或 sqlite3) 內某個 topic,轉成 CSV(stdout)。
不需要啟 ros2 node — 純 rosbag2_py + rclpy serialization。

Usage:
    python3 parse_odom_csv.py <bag_dir> <topic>            > out.csv
    python3 parse_odom_csv.py my_slam_bag /odom            > odom.csv
    python3 parse_odom_csv.py my_slam_bag /scan            > scan.csv

支援的 message 型別:
    nav_msgs/msg/Odometry  — 輸出 t, x, y, z, qx, qy, qz, qw, vx, wz
    sensor_msgs/msg/LaserScan — 輸出 t, range_min, range_max, num_pts, mean_range
    其他 — 通用模式,輸出 t, repr(msg) 第一段
"""
import sys
import csv
import math

import rosbag2_py
from rclpy.serialization import deserialize_message
from rosidl_runtime_py.utilities import get_message


def open_reader(bag_dir: str) -> rosbag2_py.SequentialReader:
    """auto-detect mcap 或 sqlite3 backend。"""
    storage_id = "mcap"
    # 偷看 metadata 副檔名 — .mcap → mcap,否則 sqlite3
    import os
    has_mcap = any(f.endswith(".mcap") for f in os.listdir(bag_dir))
    storage_id = "mcap" if has_mcap else "sqlite3"

    storage_opts = rosbag2_py.StorageOptions(uri=bag_dir, storage_id=storage_id)
    converter_opts = rosbag2_py.ConverterOptions(
        input_serialization_format="cdr",
        output_serialization_format="cdr",
    )
    reader = rosbag2_py.SequentialReader()
    reader.open(storage_opts, converter_opts)
    return reader


def topic_type_map(reader) -> dict:
    return {t.name: t.type for t in reader.get_all_topics_and_types()}


def write_odometry(writer, t_ns: int, msg) -> None:
    p = msg.pose.pose.position
    q = msg.pose.pose.orientation
    v = msg.twist.twist.linear
    w = msg.twist.twist.angular
    writer.writerow([t_ns / 1e9, p.x, p.y, p.z, q.x, q.y, q.z, q.w, v.x, w.z])


def write_laserscan(writer, t_ns: int, msg) -> None:
    valid = [r for r in msg.ranges if math.isfinite(r) and r > 0.0]
    mean_r = sum(valid) / len(valid) if valid else 0.0
    writer.writerow(
        [t_ns / 1e9, msg.range_min, msg.range_max, len(msg.ranges), mean_r]
    )


def write_generic(writer, t_ns: int, msg) -> None:
    writer.writerow([t_ns / 1e9, repr(msg)[:200]])


HANDLERS = {
    "nav_msgs/msg/Odometry": (
        ["t", "x", "y", "z", "qx", "qy", "qz", "qw", "vx", "wz"],
        write_odometry,
    ),
    "sensor_msgs/msg/LaserScan": (
        ["t", "range_min", "range_max", "num_pts", "mean_range"],
        write_laserscan,
    ),
}


def main(argv: list) -> int:
    if len(argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    bag_dir, target_topic = argv[1], argv[2]

    reader = open_reader(bag_dir)
    type_map = topic_type_map(reader)

    if target_topic not in type_map:
        print(
            f"[error] topic {target_topic} not in bag. "
            f"Available: {list(type_map.keys())}",
            file=sys.stderr,
        )
        return 1

    msg_type_name = type_map[target_topic]
    msg_type = get_message(msg_type_name)

    header, handler = HANDLERS.get(
        msg_type_name, (["t", "msg_repr"], write_generic)
    )

    writer = csv.writer(sys.stdout)
    writer.writerow(header)

    count = 0
    while reader.has_next():
        topic, raw, t_ns = reader.read_next()
        if topic != target_topic:
            continue
        msg = deserialize_message(raw, msg_type)
        handler(writer, t_ns, msg)
        count += 1

    print(f"[info] wrote {count} rows for {target_topic}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
