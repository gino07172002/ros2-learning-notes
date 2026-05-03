// count_successes_decorator.hpp — Phase 30
//
// DecoratorNode:累計子節點 SUCCESS 次數,達到 target 才回 SUCCESS
//
// 業界用例:
//   - 連續 N 次成功才認定為「真的穩定了」(去 noise)
//   - 巡邏走完 N 個 waypoint 才結束任務
//
// 跟內建 RepeatNode 不一樣:RepeatNode 是「強制重複 N 次」,
// 本 decorator 是「等子節點累積 N 次成功」(失敗就 reset)

#ifndef MY_BT_ADVANCED__COUNT_SUCCESSES_DECORATOR_HPP_
#define MY_BT_ADVANCED__COUNT_SUCCESSES_DECORATOR_HPP_

#include "behaviortree_cpp_v3/decorator_node.h"

namespace my_bt_advanced
{

class CountSuccessesDecorator : public BT::DecoratorNode
{
public:
  CountSuccessesDecorator(const std::string & name, const BT::NodeConfiguration & config);

  static BT::PortsList providedPorts()
  {
    return {
      BT::InputPort<int>("target_count", 3,
        "How many SUCCESS results before this decorator returns SUCCESS"),
    };
  }

private:
  BT::NodeStatus tick() override;
  int success_count_ = 0;
};

}  // namespace my_bt_advanced

#endif  // MY_BT_ADVANCED__COUNT_SUCCESSES_DECORATOR_HPP_
