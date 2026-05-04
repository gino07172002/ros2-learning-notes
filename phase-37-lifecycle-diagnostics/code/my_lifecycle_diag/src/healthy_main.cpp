// healthy_main.cpp — Phase 37

#include "rclcpp/rclcpp.hpp"
#include "my_lifecycle_diag/healthy_lifecycle_node.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // 用 MultiThreadedExecutor 因為 lifecycle service callback 跟 work timer
  // 在不同 callback group,單執行緒會卡(實機常踩到的雷)
  rclcpp::executors::MultiThreadedExecutor exec;
  auto node = std::make_shared<my_lifecycle_diag::HealthyLifecycleNode>();
  exec.add_node(node->get_node_base_interface());
  exec.spin();
  rclcpp::shutdown();
  return 0;
}
