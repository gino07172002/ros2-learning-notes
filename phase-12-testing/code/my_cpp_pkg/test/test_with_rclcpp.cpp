// test_with_rclcpp.cpp
// gtest + rclcpp — 測試 Node 之間 publish/subscribe 真實流動
//
// 比純單元測試強：能驗證
//   - Publisher 真的有發出訊息
//   - Subscriber 真的收得到
//   - QoS 設定對齊
//   - 訊息內容正確
//
// 比 launch_testing 簡單：只起 1 個 process（測試 process 內含被測 Node + 測試 Node）

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

// === Test Fixture: 每個 test 都有一個 ROS context ===
class RclcppTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        rclcpp::init(0, nullptr);
    }

    void TearDown() override {
        rclcpp::shutdown();
    }
};

// 1. 測試訊息真的有從 publisher 流到 subscriber
TEST_F(RclcppTestFixture, PubSubRoundtrip) {
    auto node = std::make_shared<rclcpp::Node>("test_node");
    auto pub = node->create_publisher<std_msgs::msg::String>("test_topic", 10);

    std::string received;
    auto sub = node->create_subscription<std_msgs::msg::String>(
        "test_topic", 10,
        [&received](const std_msgs::msg::String::SharedPtr msg) {
            received = msg->data;
        });

    // 等 discovery 完成
    std::this_thread::sleep_for(500ms);

    // 發送
    auto msg = std_msgs::msg::String();
    msg.data = "hello test";
    pub->publish(msg);

    // 給時間讓訊息傳遞
    rclcpp::executors::SingleThreadedExecutor exec;
    exec.add_node(node);
    auto deadline = std::chrono::steady_clock::now() + 2s;
    while (received.empty() && std::chrono::steady_clock::now() < deadline) {
        exec.spin_some(100ms);
    }

    EXPECT_EQ(received, "hello test");
}

// 2. 測試 Node 名稱
TEST_F(RclcppTestFixture, NodeNameCorrect) {
    auto node = std::make_shared<rclcpp::Node>("my_robot");
    EXPECT_EQ(std::string(node->get_name()), "my_robot");
    EXPECT_EQ(std::string(node->get_namespace()), "/");
}

int main(int argc, char ** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
