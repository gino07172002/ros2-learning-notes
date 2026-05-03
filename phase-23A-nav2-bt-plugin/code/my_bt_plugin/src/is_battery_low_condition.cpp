// is_battery_low_condition.cpp — Phase 23A

#include "my_bt_plugin/is_battery_low_condition.hpp"

namespace my_bt_plugin
{

IsBatteryLowCondition::IsBatteryLowCondition(
  const std::string & condition_name,
  const BT::NodeConfiguration & conf)
: BT::ConditionNode(condition_name, conf)
{
  // BT.cpp 把 nav2 的 rclcpp::Node 透過 blackboard 傳進來
  // (Nav2 的 bt_navigator 會塞 "node" key 進 blackboard)
  node_ = config().blackboard->get<rclcpp::Node::SharedPtr>("node");

  getInput("min_battery", min_battery_);
  getInput("battery_topic", battery_topic_);

  battery_sub_ = node_->create_subscription<sensor_msgs::msg::BatteryState>(
    battery_topic_, rclcpp::SystemDefaultsQoS(),
    std::bind(&IsBatteryLowCondition::batteryCallback, this, std::placeholders::_1));

  RCLCPP_INFO(node_->get_logger(),
    "[IsBatteryLow] subscribed to %s, threshold=%.2f",
    battery_topic_.c_str(), min_battery_);
}

void IsBatteryLowCondition::batteryCallback(
  sensor_msgs::msg::BatteryState::SharedPtr msg)
{
  battery_percentage_ = msg->percentage;
  received_first_msg_ = true;
}

BT::NodeStatus IsBatteryLowCondition::tick()
{
  if (!received_first_msg_) {
    // 沒收到任何 battery 訊息 → 不要 trigger 充電
    return BT::NodeStatus::FAILURE;
  }

  if (battery_percentage_ < min_battery_) {
    RCLCPP_WARN_ONCE(node_->get_logger(),
      "[IsBatteryLow] battery %.2f < threshold %.2f → SUCCESS (trigger recovery)",
      battery_percentage_, min_battery_);
    return BT::NodeStatus::SUCCESS;
  }
  return BT::NodeStatus::FAILURE;
}

}  // namespace my_bt_plugin

// === BT.cpp plugin 註冊 ===
// 必須用這個巨集,nav2_bt_navigator 載 plugin 才認得
#include "behaviortree_cpp_v3/bt_factory.h"
BT_REGISTER_NODES(factory)
{
  factory.registerNodeType<my_bt_plugin::IsBatteryLowCondition>("IsBatteryLow");
}
