// brake_strategy.hpp
// pluginlib base interface — 純抽象 class 定義
//
// 所有具體 plugin 都繼承這個 class 並實作 compute_speed()。
// 這是 pluginlib 的核心設計：
//   - 主程式只依賴這個 base，不知道具體實作
//   - runtime 用設定檔指定要載入哪個 plugin
//   - 業界 Nav2 / MoveIt / ros2_control 都這個架構

#pragma once

namespace brake_strategy_base
{

class BrakeStrategy
{
public:
    // pluginlib 載入時會 default-construct，不能在 constructor 做事
    // 改用 initialize() 接收參數
    virtual void initialize(double safe_distance, double max_speed) = 0;

    // 主邏輯：給障礙物距離，回傳該送的速度
    virtual double compute_speed(double obstacle_distance) const = 0;

    // 給 logger 用的策略名
    virtual const char * name() const = 0;

    virtual ~BrakeStrategy() = default;
};

}  // namespace brake_strategy_base
