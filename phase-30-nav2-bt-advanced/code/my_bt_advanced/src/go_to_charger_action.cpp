// go_to_charger_action.cpp

#include "my_bt_advanced/go_to_charger_action.hpp"

namespace my_bt_advanced
{

using namespace std::chrono;

GoToChargerAction::GoToChargerAction(
  const std::string & name,
  const BT::NodeConfiguration & config)
: BT::StatefulActionNode(name, config)
{}

BT::NodeStatus GoToChargerAction::onStart()
{
  // 從 input port 讀規劃要花多久
  if (!getInput("travel_time", travel_time_seconds_)) {
    travel_time_seconds_ = 2.0;
  }
  start_time_ = steady_clock::now();
  // 第一個 tick 立刻回 RUNNING,讓父節點知道「我接手了,還沒完」
  return BT::NodeStatus::RUNNING;
}

BT::NodeStatus GoToChargerAction::onRunning()
{
  auto elapsed = duration_cast<duration<double>>(
    steady_clock::now() - start_time_).count();

  if (elapsed >= travel_time_seconds_) {
    // 寫進 output port,讓 BT 後續 node 拿得到
    setOutput<std::string>("result_msg",
      "Reached charger after " + std::to_string(elapsed) + "s");
    return BT::NodeStatus::SUCCESS;
  }
  return BT::NodeStatus::RUNNING;
}

void GoToChargerAction::onHalted()
{
  // 樹被中止 — 例如 Fallback 內前面節點突然 SUCCESS,這個 action 要安全收尾
  // 真實版會 cancel nav2 goal、重置狀態
  setOutput<std::string>("result_msg", "Halted before reaching charger");
}

}  // namespace my_bt_advanced
