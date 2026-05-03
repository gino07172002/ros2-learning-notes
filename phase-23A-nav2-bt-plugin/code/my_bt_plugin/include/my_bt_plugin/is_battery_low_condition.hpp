// is_battery_low_condition.hpp — Phase 23A
//
// 自訂 BT condition node:訂 /battery_state,
// 當 percentage 低於 threshold(預設 20%)回 SUCCESS,否則 FAILURE
//
// BT.cpp 的 condition node 跟 action node 的差別:
//   - Condition:每次 tick 立刻回 SUCCESS / FAILURE,沒中間 RUNNING 狀態
//   - Action:可能會 tick 很多次回 RUNNING 直到完成
//
// 在 Nav2 BT.xml 內用法:
//   <Sequence>
//     <Condition ID="IsBatteryLow" battery_topic="/battery_state" min_battery="0.2"/>
//     <Action ID="ComputePathToPose" goal="${dock_pose}"/>
//     <Action ID="FollowPath"/>
//   </Sequence>

#ifndef MY_BT_PLUGIN__IS_BATTERY_LOW_CONDITION_HPP_
#define MY_BT_PLUGIN__IS_BATTERY_LOW_CONDITION_HPP_

#include <string>

#include "behaviortree_cpp_v3/condition_node.h"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/battery_state.hpp"

namespace my_bt_plugin
{

class IsBatteryLowCondition : public BT::ConditionNode
{
public:
  // BT.cpp 必須要的 ctor:接收 name 跟 ports map
  IsBatteryLowCondition(const std::string & condition_name,
                        const BT::NodeConfiguration & conf);

  IsBatteryLowCondition() = delete;

  // 每次 tick 被呼叫時回傳 SUCCESS / FAILURE
  BT::NodeStatus tick() override;

  // 對外宣告 input ports(讓 BT XML 可以傳參數)
  static BT::PortsList providedPorts()
  {
    return {
      BT::InputPort<std::string>("battery_topic", std::string("/battery_state"),
                                 "Topic name for battery state"),
      BT::InputPort<double>("min_battery", 0.2,
                            "Battery percentage threshold (0–1)"),
    };
  }

private:
  void batteryCallback(sensor_msgs::msg::BatteryState::SharedPtr msg);

  rclcpp::Node::SharedPtr node_;
  rclcpp::Subscription<sensor_msgs::msg::BatteryState>::SharedPtr battery_sub_;
  double battery_percentage_ = 1.0;        // 預設滿電,避免一啟動就觸發
  double min_battery_ = 0.2;
  std::string battery_topic_;
  bool received_first_msg_ = false;
};

}  // namespace my_bt_plugin

#endif  // MY_BT_PLUGIN__IS_BATTERY_LOW_CONDITION_HPP_
