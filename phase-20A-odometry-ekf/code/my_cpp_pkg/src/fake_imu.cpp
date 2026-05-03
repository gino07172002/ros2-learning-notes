// fake_imu.cpp — Phase 20A
//
// 模擬同一台車的 IMU。角速度報「真值的 95%」(沒有系統性偏差),
// 但加上高頻雜訊(N(0, σ²)),代表 IMU 短期準、長期會 drift。
//
// 發布:
//   /imu/data  (sensor_msgs/Imu)  100 Hz
//
// EKF 應該學到:
//   - 從 wheel/odometry 拿線速度(它對 vx 的 covariance 小)
//   - 從 imu 拿角速度(它對 wz 的 covariance 小,且沒系統性偏差)
//   - 融合後的 yaw / 位置,比單一 source 都更接近真值

#include <chrono>
#include <cmath>
#include <random>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <tf2/LinearMath/Quaternion.h>

using namespace std::chrono_literals;

class FakeImu : public rclcpp::Node
{
public:
  FakeImu()
    : Node("fake_imu"),
      gen_(std::random_device{}()),
      noise_w_(0.0, 0.01),         // 角速度 noise σ=0.01 rad/s
      noise_a_(0.0, 0.05),         // 線加速度 noise σ=0.05 m/s²
      yaw_integrated_(0.0)
  {
    declare_parameter("true_angular_z", 0.3);
    // IMU 角速度沒系統性偏差(實機上 IMU 通常被工廠校準過),
    // 但有高頻 noise(N(0, 0.01²) rad/s)。可改 0.95 模擬未校準 IMU
    declare_parameter("angular_bias", 1.0);
    declare_parameter("true_linear_x", 0.5);

    pub_ = create_publisher<sensor_msgs::msg::Imu>("imu/data", 50);
    timer_ = create_wall_timer(10ms, std::bind(&FakeImu::tick, this));

    last_ = now();
    RCLCPP_INFO(get_logger(), "fake_imu up: 100Hz, angular_bias=%.2f",
                get_parameter("angular_bias").as_double());
  }

private:
  void tick()
  {
    auto t = now();
    double dt = (t - last_).seconds();
    last_ = t;

    const double w_true = get_parameter("true_angular_z").as_double();
    const double bias = get_parameter("angular_bias").as_double();
    const double v_true = get_parameter("true_linear_x").as_double();

    // IMU 報告的角速度 = 真值 × 偏差 + noise
    const double w_reported = w_true * bias + noise_w_(gen_);
    yaw_integrated_ += w_reported * dt;

    sensor_msgs::msg::Imu msg;
    msg.header.stamp = t;
    msg.header.frame_id = "imu_link";

    // orientation:用積分的 yaw(IMU 內 sensor fusion 給的姿態)
    tf2::Quaternion q;
    q.setRPY(0, 0, yaw_integrated_);
    msg.orientation.x = q.x();
    msg.orientation.y = q.y();
    msg.orientation.z = q.z();
    msg.orientation.w = q.w();

    // angular velocity
    msg.angular_velocity.z = w_reported;

    // linear acceleration:圓周運動的向心加速度 = v² / r,r = v/w → a = v*w
    // 在 base_link y 方向(往圓心),這裡為了簡化只放在 x(實際 IMU 朝向會抵銷)
    // 加 noise,EKF 用它配合 yaw 推算速度變化
    msg.linear_acceleration.x = noise_a_(gen_);
    msg.linear_acceleration.y = v_true * w_true + noise_a_(gen_);

    // covariance:只信任 angular_z 跟 orientation yaw,加速度不太信任
    msg.orientation_covariance = {
      1e6, 0, 0,
      0, 1e6, 0,
      0, 0, 1e-2          // yaw covariance 小 → EKF 會吃這個值
    };
    msg.angular_velocity_covariance = {
      1e6, 0, 0,
      0, 1e6, 0,
      0, 0, 1e-4          // wz covariance 很小 → 比 wheel 的 wz 更可信
    };
    msg.linear_acceleration_covariance = {
      1e-1, 0, 0,
      0, 1e-1, 0,
      0, 0, 1e6
    };

    pub_->publish(msg);
  }

  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Time last_;
  std::mt19937 gen_;
  std::normal_distribution<double> noise_w_;
  std::normal_distribution<double> noise_a_;
  double yaw_integrated_;
};

int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<FakeImu>());
  rclcpp::shutdown();
  return 0;
}
