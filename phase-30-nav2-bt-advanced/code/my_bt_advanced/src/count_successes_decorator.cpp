// count_successes_decorator.cpp

#include "my_bt_advanced/count_successes_decorator.hpp"

namespace my_bt_advanced
{

CountSuccessesDecorator::CountSuccessesDecorator(
  const std::string & name,
  const BT::NodeConfiguration & config)
: BT::DecoratorNode(name, config)
{}

BT::NodeStatus CountSuccessesDecorator::tick()
{
  int target = 3;
  getInput("target_count", target);

  auto child_status = child_node_->executeTick();

  if (child_status == BT::NodeStatus::RUNNING) {
    return BT::NodeStatus::RUNNING;
  }

  if (child_status == BT::NodeStatus::SUCCESS) {
    success_count_++;
    if (success_count_ >= target) {
      success_count_ = 0;          // reset 給下次再用
      return BT::NodeStatus::SUCCESS;
    }
    // 還沒達到目標,繼續(回 RUNNING 讓父節點下次再 tick)
    return BT::NodeStatus::RUNNING;
  }

  // child FAILURE → 整個 decorator 也 FAILURE,且 reset
  success_count_ = 0;
  return BT::NodeStatus::FAILURE;
}

}  // namespace my_bt_advanced
