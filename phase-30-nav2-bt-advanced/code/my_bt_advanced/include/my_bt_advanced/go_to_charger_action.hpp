// go_to_charger_action.hpp — Phase 30
//
// StatefulActionNode 範例:模擬「往充電站走一段時間」的長任務
//
// BT.cpp 4 種 base class 對照:
//   - ConditionNode      → 立刻回 SUCCESS / FAILURE(Phase 23A 寫過)
//   - SimpleActionNode   → 立刻完成的動作
//   - StatefulActionNode → 可以跨多次 tick 的長任務(本檔)
//                          onStart() → 第 1 次 tick 呼叫
//                          onRunning() → 後續每次 tick 呼叫,回 RUNNING / SUCCESS / FAILURE
//                          onHalted() → 樹被中止時呼叫,清乾淨
//
// 業界用例:Action client 包裝(送 nav2 NavigateToPose goal,等到完成)
// 這裡為了 unit test 簡單,只用「等 N 秒」模擬長任務,不真接 nav2

#ifndef MY_BT_ADVANCED__GO_TO_CHARGER_ACTION_HPP_
#define MY_BT_ADVANCED__GO_TO_CHARGER_ACTION_HPP_

#include <chrono>
#include <string>
#include "behaviortree_cpp_v3/action_node.h"

namespace my_bt_advanced
{

class GoToChargerAction : public BT::StatefulActionNode
{
public:
  GoToChargerAction(const std::string & name, const BT::NodeConfiguration & config);

  // BT.cpp 4 個 lifecycle method:
  BT::NodeStatus onStart() override;        // 第一次 tick 呼叫(初始化)
  BT::NodeStatus onRunning() override;      // 後續 tick(回 RUNNING / SUCCESS / FAILURE)
  void onHalted() override;                 // 被父節點中止

  static BT::PortsList providedPorts()
  {
    return {
      BT::InputPort<double>("travel_time", 2.0,
        "Simulated travel time to charger in seconds"),
      BT::OutputPort<std::string>("result_msg",
        "Last result text (debug)"),
    };
  }

private:
  std::chrono::steady_clock::time_point start_time_;
  double travel_time_seconds_ = 2.0;
};

}  // namespace my_bt_advanced

#endif  // MY_BT_ADVANCED__GO_TO_CHARGER_ACTION_HPP_
