// watchdog_main.cpp — Phase 36
//
// 把 HeartbeatWatchdog 當一般 Node spin 起來。
// (gtest 那邊是直接 new HeartbeatWatchdog,不走這個 main)

#include "rclcpp/rclcpp.hpp"
#include "my_diag_demo/heartbeat_watchdog.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<my_diag_demo::HeartbeatWatchdog>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
