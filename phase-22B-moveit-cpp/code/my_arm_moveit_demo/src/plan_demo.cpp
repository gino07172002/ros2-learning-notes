// plan_demo.cpp — Phase 22B
//
// MoveIt 2 C++ MoveGroupInterface 的三段 demo:
//   1. 規劃到 SRDF named pose("home" → "ready")
//   2. 規劃到指定 joint values 向量
//   3. 規劃到 cartesian Pose(IK 解 → 規劃)
//
// 純文字驗證:plan succeeded / trajectory point count / planning time
// 不需要 Gazebo / RViz,只需 move_group + rsp + jsp 三個 node 在背景跑

#include <chrono>
#include <memory>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "moveit/move_group_interface/move_group_interface.h"
#include "geometry_msgs/msg/pose.hpp"

using namespace std::chrono_literals;
using moveit::planning_interface::MoveGroupInterface;

static const char * GROUP_NAME = "arm";

void log_plan_summary(const rclcpp::Logger & log,
                      const std::string & label,
                      bool ok,
                      const MoveGroupInterface::Plan & plan)
{
  if (!ok) {
    RCLCPP_ERROR(log, "[%s] plan FAILED", label.c_str());
    return;
  }
  const auto & traj = plan.trajectory_.joint_trajectory;
  RCLCPP_INFO(log,
    "[%s] ✅ plan OK | points=%zu | duration=%.3fs | planning_time=%.3fs",
    label.c_str(),
    traj.points.size(),
    traj.points.empty() ? 0.0 :
      rclcpp::Duration(traj.points.back().time_from_start).seconds(),
    plan.planning_time_);
}


int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  // MoveGroupInterface 需要一個 spinning Node
  auto node = std::make_shared<rclcpp::Node>(
    "plan_demo",
    rclcpp::NodeOptions().automatically_declare_parameters_from_overrides(true));

  // 用獨立 thread spin executor,讓 MoveIt API 內部能呼叫 service / action
  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);
  std::thread spinner([&executor]() { executor.spin(); });

  // 給 move_group 一點時間 ready
  std::this_thread::sleep_for(2s);

  MoveGroupInterface arm(node, GROUP_NAME);
  arm.setPlanningTime(5.0);
  arm.setNumPlanningAttempts(5);

  RCLCPP_INFO(node->get_logger(),
    "[plan_demo] connected to group '%s'\n"
    "  planning_frame=%s\n"
    "  end_effector_link=%s\n"
    "  joint_count=%zu",
    GROUP_NAME,
    arm.getPlanningFrame().c_str(),
    arm.getEndEffectorLink().c_str(),
    arm.getJointNames().size());

  // ─── Demo 1: SRDF named pose "ready" ───────────────────────────────
  {
    arm.setNamedTarget("ready");
    MoveGroupInterface::Plan plan;
    auto code = arm.plan(plan);
    bool ok = (code == moveit::core::MoveItErrorCode::SUCCESS);
    log_plan_summary(node->get_logger(), "named pose 'ready'", ok, plan);
  }

  // ─── Demo 2: joint values target ───────────────────────────────────
  {
    std::vector<double> joints = {0.5, -0.3, 0.6, 0.0, 0.8, 0.0};
    arm.setJointValueTarget(joints);
    MoveGroupInterface::Plan plan;
    auto code = arm.plan(plan);
    bool ok = (code == moveit::core::MoveItErrorCode::SUCCESS);
    log_plan_summary(node->get_logger(), "joint values target", ok, plan);
  }

  // ─── Demo 3: cartesian pose target ─────────────────────────────────
  {
    geometry_msgs::msg::Pose target;
    // 從目前位置出發稍微 offset(用 setPoseReferenceFrame 跟 setPoseTarget 的 IK)
    // 這個 6R 手臂直立時 tool0 在 (0, 0, 1.10),所以靠近這個位置 IK 才解得到
    target.position.x = 0.2;
    target.position.y = 0.0;
    target.position.z = 0.9;
    target.orientation.w = 1.0;
    arm.setPoseTarget(target);
    MoveGroupInterface::Plan plan;
    auto code = arm.plan(plan);
    bool ok = (code == moveit::core::MoveItErrorCode::SUCCESS);
    log_plan_summary(node->get_logger(), "cartesian pose (0.2, 0.0, 0.9)", ok, plan);
  }

  // ─── Demo 4: 跑回 named pose "home"(收尾) ─────────────────────────
  {
    arm.setNamedTarget("home");
    MoveGroupInterface::Plan plan;
    auto code = arm.plan(plan);
    bool ok = (code == moveit::core::MoveItErrorCode::SUCCESS);
    log_plan_summary(node->get_logger(), "named pose 'home' (return)", ok, plan);
  }

  RCLCPP_INFO(node->get_logger(), "[plan_demo] all demos done");
  executor.cancel();
  spinner.join();
  rclcpp::shutdown();
  return 0;
}
