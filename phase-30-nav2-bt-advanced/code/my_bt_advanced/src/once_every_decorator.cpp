// once_every_decorator.cpp

#include "my_bt_advanced/once_every_decorator.hpp"

namespace my_bt_advanced
{

using namespace std::chrono;

OnceEveryDecorator::OnceEveryDecorator(
  const std::string & name,
  const BT::NodeConfiguration & config)
: BT::DecoratorNode(name, config)
{}

BT::NodeStatus OnceEveryDecorator::tick()
{
  double interval = 5.0;
  getInput("seconds", interval);

  auto now = steady_clock::now();

  if (first_call_) {
    // 第一次 tick:讓子節點跑
    first_call_ = false;
    last_tick_ = now;
    return child_node_->executeTick();
  }

  auto elapsed = duration_cast<duration<double>>(now - last_tick_).count();
  if (elapsed < interval) {
    // 還沒到 interval,直接回 FAILURE 不 tick 子節點
    return BT::NodeStatus::FAILURE;
  }

  // 到時間了,讓子跑 + 更新 last_tick_
  last_tick_ = now;
  return child_node_->executeTick();
}

}  // namespace my_bt_advanced
