// test_load_plugin.cpp — Phase 23A
//
// gtest 整合測試:
//   1. 用 BT.cpp BehaviorTreeFactory 註冊我們的 plugin
//   2. 從 inline XML 創 tree
//   3. 模擬發 BatteryState(percentage=0.5)→ 期 FAILURE(電量足)
//   4. 模擬發 BatteryState(percentage=0.1)→ 期 SUCCESS(該充電)
//
// 不需要起 nav2 / 不需要 Gazebo,純單元 + 整合測試,WSL 100% 可跑

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "behaviortree_cpp_v3/bt_factory.h"
#include "sensor_msgs/msg/battery_state.hpp"
#include "my_bt_plugin/is_battery_low_condition.hpp"

using namespace std::chrono_literals;

class IsBatteryLowFixture : public ::testing::Test
{
protected:
  void SetUp() override
  {
    rclcpp::init(0, nullptr);
    node_ = std::make_shared<rclcpp::Node>("bt_test_node");
    pub_ = node_->create_publisher<sensor_msgs::msg::BatteryState>(
      "/battery_state", 10);

    factory_.registerNodeType<my_bt_plugin::IsBatteryLowCondition>("IsBatteryLow");
  }

  void TearDown() override
  {
    rclcpp::shutdown();
  }

  void publishBattery(float pct)
  {
    sensor_msgs::msg::BatteryState msg;
    msg.percentage = pct;
    pub_->publish(msg);
    // spin 一下讓 sub 收到
    auto end = std::chrono::steady_clock::now() + 200ms;
    while (std::chrono::steady_clock::now() < end) {
      rclcpp::spin_some(node_);
      std::this_thread::sleep_for(20ms);
    }
  }

  BT::Tree createTree(double min_battery)
  {
    std::string xml_text = R"(
      <root main_tree_to_execute="MainTree">
        <BehaviorTree ID="MainTree">
          <Sequence>
            <IsBatteryLow battery_topic="/battery_state" min_battery=")" +
            std::to_string(min_battery) + R"("/>
          </Sequence>
        </BehaviorTree>
      </root>
    )";

    auto blackboard = BT::Blackboard::create();
    blackboard->set<rclcpp::Node::SharedPtr>("node", node_);
    return factory_.createTreeFromText(xml_text, blackboard);
  }

  BT::BehaviorTreeFactory factory_;
  rclcpp::Node::SharedPtr node_;
  rclcpp::Publisher<sensor_msgs::msg::BatteryState>::SharedPtr pub_;
};

// === Test 1:沒收到 battery message → FAILURE(預設行為) ===
TEST_F(IsBatteryLowFixture, NoMessageReturnsFailure)
{
  auto tree = createTree(0.2);
  auto status = tree.tickRoot();
  EXPECT_EQ(status, BT::NodeStatus::FAILURE);
}

// === Test 2:電量充足(50%)→ FAILURE(不需充電) ===
TEST_F(IsBatteryLowFixture, FullBatteryReturnsFailure)
{
  auto tree = createTree(0.2);
  publishBattery(0.5f);
  auto status = tree.tickRoot();
  EXPECT_EQ(status, BT::NodeStatus::FAILURE);
}

// === Test 3:電量低(10%)< threshold(20%)→ SUCCESS(觸發充電 BT 分支) ===
TEST_F(IsBatteryLowFixture, LowBatteryReturnsSuccess)
{
  auto tree = createTree(0.2);
  publishBattery(0.1f);
  auto status = tree.tickRoot();
  EXPECT_EQ(status, BT::NodeStatus::SUCCESS);
}

// === Test 4:threshold 邊界(剛好相等不觸發,< 才觸發) ===
TEST_F(IsBatteryLowFixture, ExactThresholdReturnsFailure)
{
  auto tree = createTree(0.2);
  publishBattery(0.2f);
  auto status = tree.tickRoot();
  EXPECT_EQ(status, BT::NodeStatus::FAILURE);  // 0.2 不 < 0.2
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
