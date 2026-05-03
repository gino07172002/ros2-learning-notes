// once_every_decorator.hpp — Phase 30
//
// DecoratorNode 範例:每 N 秒才 tick 一次底下子節點,其他時間直接回 FAILURE
//
// 業界用例:
//   - 限制 action 重執行頻率(避免 spam)
//   - 「每 30 秒巡邏一次」「每 5 分鐘檢查電量」
//
// 對照其他 BT.cpp 內建 decorator:
//   - Inverter:子回 SUCCESS 變 FAILURE,反之亦然
//   - Repeat:子節點重複 N 次
//   - RetryUntilSuccessful:失敗就重試 N 次
//   - 本檔 OnceEvery:時間節流(rate-limit)

#ifndef MY_BT_ADVANCED__ONCE_EVERY_DECORATOR_HPP_
#define MY_BT_ADVANCED__ONCE_EVERY_DECORATOR_HPP_

#include <chrono>
#include "behaviortree_cpp_v3/decorator_node.h"

namespace my_bt_advanced
{

class OnceEveryDecorator : public BT::DecoratorNode
{
public:
  OnceEveryDecorator(const std::string & name, const BT::NodeConfiguration & config);

  static BT::PortsList providedPorts()
  {
    return {
      BT::InputPort<double>("seconds", 5.0,
        "Minimum seconds between two child ticks"),
    };
  }

private:
  // BT.cpp DecoratorNode 用 tick(),不像 StatefulActionNode 拆 onStart/onRunning
  BT::NodeStatus tick() override;

  std::chrono::steady_clock::time_point last_tick_;
  bool first_call_ = true;
};

}  // namespace my_bt_advanced

#endif  // MY_BT_ADVANCED__ONCE_EVERY_DECORATOR_HPP_
