// patrol_waypoint_action.hpp — Phase 30
//
// 第二個 StatefulActionNode:巡邏到下個 waypoint
// 跟 GoToCharger 不同:它**內部維護 waypoint index**,每次 SUCCESS 後 +1
//
// 業界用例:
//   - 配送機器人巡迴
//   - 倉儲 picking robot 走過多個收貨點
//
// 為了 unit test 簡單,不真接 nav2 action,只用「等 N 秒」+ 維護 index

#ifndef MY_BT_ADVANCED__PATROL_WAYPOINT_ACTION_HPP_
#define MY_BT_ADVANCED__PATROL_WAYPOINT_ACTION_HPP_

#include <chrono>
#include "behaviortree_cpp_v3/action_node.h"

namespace my_bt_advanced
{

class PatrolWaypointAction : public BT::StatefulActionNode
{
public:
  PatrolWaypointAction(const std::string & name, const BT::NodeConfiguration & config);

  BT::NodeStatus onStart() override;
  BT::NodeStatus onRunning() override;
  void onHalted() override;

  static BT::PortsList providedPorts()
  {
    return {
      BT::InputPort<int>("total_waypoints", 3,
        "Total number of waypoints in the patrol route"),
      BT::InputPort<double>("travel_time", 1.0,
        "Simulated time per waypoint in seconds"),
      BT::OutputPort<int>("current_waypoint",
        "Index of the waypoint just visited (0-based)"),
    };
  }

private:
  std::chrono::steady_clock::time_point start_time_;
  double travel_time_seconds_ = 1.0;
  int current_idx_ = 0;
  int total_waypoints_ = 3;
};

}  // namespace my_bt_advanced

#endif  // MY_BT_ADVANCED__PATROL_WAYPOINT_ACTION_HPP_
