// fake_heartbeater.cpp — Phase 36
//
// 一個假心跳發送器:每 hb_period_ms 發一筆 Empty 到 hb_topic。
// 加 stop_after_sec 參數,模擬「發了 5 秒後死掉」測 watchdog 是否正確報 stale。

#include <chrono>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/empty.hpp"

using namespace std::chrono_literals;

class FakeHeartbeater : public rclcpp::Node
{
public:
  FakeHeartbeater()
  : Node("fake_heartbeater")
  {
    declare_parameter<std::string>("hb_topic", "/lidar_hb");
    declare_parameter<int>("hb_period_ms", 200);
    declare_parameter<double>("stop_after_sec", -1.0);  // -1 = 永遠

    topic_ = get_parameter("hb_topic").as_string();
    period_ms_ = get_parameter("hb_period_ms").as_int();
    stop_after_sec_ = get_parameter("stop_after_sec").as_double();

    pub_ = create_publisher<std_msgs::msg::Empty>(topic_, rclcpp::QoS(10));
    start_ = now();

    timer_ = create_wall_timer(
      std::chrono::milliseconds(period_ms_),
      [this]() {
        if (stop_after_sec_ > 0.0 &&
            (now() - start_).seconds() > stop_after_sec_) {
          if (!stopped_logged_) {
            RCLCPP_WARN(get_logger(),
                        "Stopped publishing after %.1fs (simulating death)",
                        stop_after_sec_);
            stopped_logged_ = true;
          }
          return;
        }
        pub_->publish(std_msgs::msg::Empty{});
      });

    RCLCPP_INFO(get_logger(),
                "Beating %s every %dms%s", topic_.c_str(), period_ms_,
                stop_after_sec_ > 0.0 ? " (will stop)" : "");
  }

private:
  std::string topic_;
  int period_ms_;
  double stop_after_sec_;
  rclcpp::Time start_;
  bool stopped_logged_{false};
  rclcpp::Publisher<std_msgs::msg::Empty>::SharedPtr pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<FakeHeartbeater>());
  rclcpp::shutdown();
  return 0;
}
