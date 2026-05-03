// patrol_waypoint_action.cpp

#include "my_bt_advanced/patrol_waypoint_action.hpp"

namespace my_bt_advanced
{

using namespace std::chrono;

PatrolWaypointAction::PatrolWaypointAction(
  const std::string & name,
  const BT::NodeConfiguration & config)
: BT::StatefulActionNode(name, config)
{}

BT::NodeStatus PatrolWaypointAction::onStart()
{
  if (!getInput("travel_time", travel_time_seconds_)) {
    travel_time_seconds_ = 1.0;
  }
  if (!getInput("total_waypoints", total_waypoints_)) {
    total_waypoints_ = 3;
  }
  start_time_ = steady_clock::now();
  return BT::NodeStatus::RUNNING;
}

BT::NodeStatus PatrolWaypointAction::onRunning()
{
  auto elapsed = duration_cast<duration<double>>(
    steady_clock::now() - start_time_).count();

  if (elapsed >= travel_time_seconds_) {
    setOutput<int>("current_waypoint", current_idx_);
    current_idx_ = (current_idx_ + 1) % total_waypoints_;   // wrap-around
    return BT::NodeStatus::SUCCESS;
  }
  return BT::NodeStatus::RUNNING;
}

void PatrolWaypointAction::onHalted()
{
  // Halted 不重置 current_idx_,讓下次再 tick 從同一個 waypoint 繼續
}

}  // namespace my_bt_advanced
