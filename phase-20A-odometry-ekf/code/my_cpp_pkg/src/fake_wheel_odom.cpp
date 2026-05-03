// fake_wheel_odom.cpp — Phase 20A
//
// 模擬一台車以等速圓周運動(線速度 v、角速度 w),其輪式里程計
// 把線速度高估 5%(輪徑磨耗、打滑等系統性偏差)。
//
// 發布:
//   /wheel/odometry  (nav_msgs/Odometry)  20 Hz
//
// 為什麼不發 TF:
//   讓 EKF 根據自己的融合結果發 odom→base_link,wheel-only 只當 sensor input
//
// 真實情境:
//   實機上這個 node 會由電機驅動板 / can bus driver 提供。我們在這
//   裡用程式生成,讓教學能 100% 重現,不依賴硬體。

#include <chrono>
#include <cmath>
#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <tf2/LinearMath/Quaternion.h>

using namespace std::chrono_literals;

class FakeWheelOdom : public rclcpp::Node
{
public:
  FakeWheelOdom() : Node("fake_wheel_odom"), x_(0.0), y_(0.0), yaw_(0.0)
  {
    // 真值:0.5 m/s 線速度,0.3 rad/s 角速度 → 半徑 ~1.67 m
    declare_parameter("true_linear_x", 0.5);
    declare_parameter("true_angular_z", 0.3);
    // 系統性偏差(模擬輪徑磨耗 / 打滑):
    //   linear_bias  — wheel 把 vx 高估 5%
    //   angular_bias — wheel 把 wz 低估 15%(模擬左右輪打滑造成 yaw 累積偏差)
    declare_parameter("linear_bias", 1.05);
    declare_parameter("angular_bias", 0.85);

    pub_ = create_publisher<nav_msgs::msg::Odometry>("wheel/odometry", 10);
    timer_ = create_wall_timer(50ms, std::bind(&FakeWheelOdom::tick, this));

    last_ = now();
    last_print_ = last_;
    RCLCPP_INFO(get_logger(), "fake_wheel_odom up: 20Hz, linear_bias=%.2f",
                get_parameter("linear_bias").as_double());
  }

private:
  void tick()
  {
    auto t = now();
    double dt = (t - last_).seconds();
    last_ = t;

    const double v_true = get_parameter("true_linear_x").as_double();
    const double w_true = get_parameter("true_angular_z").as_double();
    const double lbias = get_parameter("linear_bias").as_double();
    const double abias = get_parameter("angular_bias").as_double();

    // 「報告給 EKF 看的」速度都帶系統性偏差,內部位姿積分用報告值
    const double v_reported = v_true * lbias;
    const double w_reported = w_true * abias;
    yaw_ += w_reported * dt;
    x_ += v_reported * std::cos(yaw_) * dt;
    y_ += v_reported * std::sin(yaw_) * dt;

    nav_msgs::msg::Odometry msg;
    msg.header.stamp = t;
    msg.header.frame_id = "odom";
    msg.child_frame_id = "base_link";
    msg.pose.pose.position.x = x_;
    msg.pose.pose.position.y = y_;
    tf2::Quaternion q;
    q.setRPY(0, 0, yaw_);
    msg.pose.pose.orientation.x = q.x();
    msg.pose.pose.orientation.y = q.y();
    msg.pose.pose.orientation.z = q.z();
    msg.pose.pose.orientation.w = q.w();

    msg.twist.twist.linear.x = v_reported;
    msg.twist.twist.angular.z = w_reported;

    // 共變異數:只信任 vx 跟 wz,其他軸極大(EKF 看到大值會自動忽略)
    auto &pc = msg.pose.covariance;
    pc.fill(0.0);
    pc[0]  = 1e-3; pc[7]  = 1e-3; pc[14] = 1e6;
    pc[21] = 1e6;  pc[28] = 1e6;  pc[35] = 1e-2;

    auto &tc = msg.twist.covariance;
    tc.fill(0.0);
    tc[0]  = 1e-3; tc[7]  = 1e6; tc[14] = 1e6;
    tc[21] = 1e6;  tc[28] = 1e6; tc[35] = 1e-3;

    pub_->publish(msg);

    // 每 1 秒打印一次當前 wheel-only 累積位姿(教學用)
    if ((t - last_print_).seconds() >= 1.0) {
      RCLCPP_INFO(get_logger(),
        "[wheel-only] x=%.3f y=%.3f yaw=%.3f", x_, y_, yaw_);
      last_print_ = t;
    }
  }

  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Time last_;
  rclcpp::Time last_print_;
  double x_, y_, yaw_;
};

int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<FakeWheelOdom>());
  rclcpp::shutdown();
  return 0;
}
