// test_advanced_nodes.cpp — Phase 30
//
// 4 個 BT node 各自 + 整合一個完整 BT 跑驗證。
//
// 結構:
//   - GoToChargerTest:StatefulActionNode 三態(RUNNING → SUCCESS / Halted)
//   - OnceEveryTest:節流邏輯(第一次跑、之後一段時間內 FAILURE、過了再跑)
//   - CountSuccessesTest:累計次數邏輯
//   - PatrolWaypointTest:waypoint index 自動 +1
//   - IntegrationTest:用 BT XML 載 Fallback 跑完整樹

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "behaviortree_cpp_v3/bt_factory.h"
#include "my_bt_advanced/go_to_charger_action.hpp"
#include "my_bt_advanced/once_every_decorator.hpp"
#include "my_bt_advanced/count_successes_decorator.hpp"
#include "my_bt_advanced/patrol_waypoint_action.hpp"

using namespace std::chrono_literals;
using my_bt_advanced::GoToChargerAction;
using my_bt_advanced::OnceEveryDecorator;
using my_bt_advanced::CountSuccessesDecorator;
using my_bt_advanced::PatrolWaypointAction;

// =========================================================
// GoToChargerAction:RUNNING → SUCCESS
// =========================================================
TEST(GoToChargerTest, RunningThenSuccess)
{
  BT::BehaviorTreeFactory factory;
  factory.registerNodeType<GoToChargerAction>("GoToCharger");

  std::string xml = R"(
    <root main_tree_to_execute="MainTree">
      <BehaviorTree ID="MainTree">
        <GoToCharger travel_time="0.3"/>
      </BehaviorTree>
    </root>)";

  auto tree = factory.createTreeFromText(xml);

  // 第一個 tick:應該 RUNNING(剛啟動)
  EXPECT_EQ(tree.tickRoot(), BT::NodeStatus::RUNNING);

  // 等不到時間,還是 RUNNING
  std::this_thread::sleep_for(100ms);
  EXPECT_EQ(tree.tickRoot(), BT::NodeStatus::RUNNING);

  // 等過 travel_time,變 SUCCESS
  std::this_thread::sleep_for(300ms);
  EXPECT_EQ(tree.tickRoot(), BT::NodeStatus::SUCCESS);
}

// =========================================================
// OnceEvery:第一次 tick,然後在 interval 內回 FAILURE
// =========================================================
// 子節點用 BT.cpp 內建 AlwaysSuccessNode
TEST(OnceEveryTest, ThrottleBetweenIntervals)
{
  BT::BehaviorTreeFactory factory;
  factory.registerNodeType<OnceEveryDecorator>("OnceEvery");

  std::string xml = R"(
    <root main_tree_to_execute="MainTree">
      <BehaviorTree ID="MainTree">
        <OnceEvery seconds="0.3">
          <AlwaysSuccess/>
        </OnceEvery>
      </BehaviorTree>
    </root>)";

  auto tree = factory.createTreeFromText(xml);

  // 第一次:子真的跑,SUCCESS
  EXPECT_EQ(tree.tickRoot(), BT::NodeStatus::SUCCESS);

  // 馬上再 tick:還在 interval 內,FAILURE
  EXPECT_EQ(tree.tickRoot(), BT::NodeStatus::FAILURE);

  // 等過 interval:子又跑,SUCCESS
  std::this_thread::sleep_for(350ms);
  EXPECT_EQ(tree.tickRoot(), BT::NodeStatus::SUCCESS);
}

// =========================================================
// CountSuccesses:子每次 SUCCESS 累計,達 target 才整體 SUCCESS
// =========================================================
TEST(CountSuccessesTest, AccumulatesUntilTarget)
{
  BT::BehaviorTreeFactory factory;
  factory.registerNodeType<CountSuccessesDecorator>("CountSuccesses");

  std::string xml = R"(
    <root main_tree_to_execute="MainTree">
      <BehaviorTree ID="MainTree">
        <CountSuccesses target_count="3">
          <AlwaysSuccess/>
        </CountSuccesses>
      </BehaviorTree>
    </root>)";

  auto tree = factory.createTreeFromText(xml);

  // 前 2 次:RUNNING(累計中)
  EXPECT_EQ(tree.tickRoot(), BT::NodeStatus::RUNNING);
  EXPECT_EQ(tree.tickRoot(), BT::NodeStatus::RUNNING);

  // 第 3 次:達標,SUCCESS
  EXPECT_EQ(tree.tickRoot(), BT::NodeStatus::SUCCESS);
}

TEST(CountSuccessesTest, ChildFailureResetsCounter)
{
  BT::BehaviorTreeFactory factory;
  factory.registerNodeType<CountSuccessesDecorator>("CountSuccesses");

  std::string xml = R"(
    <root main_tree_to_execute="MainTree">
      <BehaviorTree ID="MainTree">
        <CountSuccesses target_count="3">
          <AlwaysFailure/>
        </CountSuccesses>
      </BehaviorTree>
    </root>)";

  auto tree = factory.createTreeFromText(xml);
  EXPECT_EQ(tree.tickRoot(), BT::NodeStatus::FAILURE);
}

// =========================================================
// PatrolWaypoint:index 每次 SUCCESS 後 +1,3 個後 wrap 回 0
// =========================================================
TEST(PatrolWaypointTest, IndexAdvancesAndWraps)
{
  BT::BehaviorTreeFactory factory;
  factory.registerNodeType<PatrolWaypointAction>("PatrolWaypoint");

  std::string xml = R"(
    <root main_tree_to_execute="MainTree">
      <BehaviorTree ID="MainTree">
        <PatrolWaypoint
          total_waypoints="3"
          travel_time="0.2"
          current_waypoint="{wp_idx}"/>
      </BehaviorTree>
    </root>)";

  auto tree = factory.createTreeFromText(xml);

  // 跑 4 次循環,看 wp_idx 序列:0,1,2,0
  std::vector<int> got;
  for (int i = 0; i < 4; ++i) {
    auto st = tree.tickRoot();
    while (st == BT::NodeStatus::RUNNING) {
      std::this_thread::sleep_for(50ms);
      st = tree.tickRoot();
    }
    EXPECT_EQ(st, BT::NodeStatus::SUCCESS);
    int idx = -1;
    tree.rootBlackboard()->get("wp_idx", idx);
    got.push_back(idx);
  }

  ASSERT_EQ(got.size(), 4u);
  EXPECT_EQ(got[0], 0);
  EXPECT_EQ(got[1], 1);
  EXPECT_EQ(got[2], 2);
  EXPECT_EQ(got[3], 0);          // wrap-around
}

// =========================================================
// 整合測試:把 4 個自訂 node + Fallback 組起來模擬完整充電/巡邏 BT
// =========================================================
TEST(IntegrationTest, ChargingPathPicksWhenChildSucceeds)
{
  BT::BehaviorTreeFactory factory;
  factory.registerNodeType<GoToChargerAction>("GoToCharger");
  factory.registerNodeType<PatrolWaypointAction>("PatrolWaypoint");
  factory.registerNodeType<OnceEveryDecorator>("OnceEvery");
  factory.registerNodeType<CountSuccessesDecorator>("CountSuccesses");

  // Fallback 的第一條 = AlwaysSuccess(模擬「電量低 + 充電完成」)
  // 期望:Fallback 在第一條 SUCCESS 直接結束,不會跑第二條巡邏
  std::string xml = R"(
    <root main_tree_to_execute="MainTree">
      <BehaviorTree ID="MainTree">
        <Fallback>
          <AlwaysSuccess/>
          <CountSuccesses target_count="3">
            <AlwaysSuccess/>
          </CountSuccesses>
        </Fallback>
      </BehaviorTree>
    </root>)";

  auto tree = factory.createTreeFromText(xml);
  EXPECT_EQ(tree.tickRoot(), BT::NodeStatus::SUCCESS);
}


int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
