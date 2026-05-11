# 🤖 Multi-Robot — 多機器人協作

> 從單機(Phase 22A 一台 turtlebot 跑 Nav2)擴到「**多台機器人在同一個世界協作**」。倉儲 / AGV fleet / 群飛無人機都是這個架構。

**狀態**:🟡 進行中 — 01 / 02 已寫文字草稿(⏸,等實際驗證)

---

## 🎯 學完整條支線你會

- 在 Gazebo 同時 spawn 3 台 turtlebot,各自有獨立 namespace
- 跑 multi-robot Nav2(每台一份完整 stack,共用一張地圖)
- 寫「fleet manager」分派任務給多台機器人
- 看穿「兩台機器人會撞 / TF tree 衝突」這類多機特有的雷

---

## 🏭 業界對應

| 場景 | 公司 / 應用 |
|------|-----------|
| 倉儲 AGV fleet | Amazon Kiva / Kiwa、京東無人倉、極智嘉 |
| 餐廳服務機器人 | Pudu Robotics、Bear Robotics |
| 多無人機群飛 | Skydio Dock、軍用偵查群 |
| 工廠物料運輸 | OTTO Motors、MiR(現 AMR fleet) |
| 港口無人車 | Westwell、振華重工 |

**結論**:會 multi-robot orchestration = 能進物流 / 倉儲 / 群機器人公司。

---

## 📋 章節結構與進度

```
multi-robot/
├── README.md                       ← 你正在讀
├── 01-namespace-spawn/             ⏸ 文字草稿(2026-05-05)
├── 02-fleet-coordination/          ⏸ 文字草稿(已寫)
```

> **「⏸ 文字草稿」**:README + launch file 骨架已寫,**沒在實機 / 雲端跑過驗證**。雷區從業界經驗整理。

---

## 🧭 章節預告

### 01. Namespace + Spawn — 多機隔離與部署

**學完你會**:
- 用 Phase 11 的 namespace 機制隔離 3 台機器人 topics(`/tb1/cmd_vel`、`/tb2/cmd_vel`、`/tb3/cmd_vel`)
- 寫 launch file 同時 spawn 3 台 turtlebot 在 Gazebo
- 每台帶獨立 `robot_state_publisher` + `joint_state_publisher`
- 解 TF tree 衝突 — `tf_prefix` 的設計(`tb1/base_link` vs `tb2/base_link`)
- 看穿「多機 odometry 同名打架」這個經典雷

**整合主線**:
- Phase 11 namespace
- Phase 16 TF2(tf_prefix 機制)
- Phase 17 Gazebo(spawn_entity 多次呼叫)

**為什麼這章重要**:**多機系統的入門關卡**。把這層搞定後續章節才有意義。

**預估時長**:1 day
**環境**:☁️ Gazebo + 多 turtlebot 雲端可跑 / 💻 本機 GPU 不足會卡(3 台機器人 + 完整 Nav2 = 高負載)

---

### 02. Fleet Coordination — 任務分派 + 衝突避免

**學完你會**:
- 寫 `FleetManager` Node:訂閱所有機器人位置 + 接受任務佇列
- 任務分派演算法(最簡:離 goal 最近的機器人接手)
- 自訂 `AssignTask.srv` 派任務給某台機器人
- 自訂 `FleetStatus.msg` 廣播全 fleet 狀態
- 衝突避免:檢查兩台機器人路徑會不會交叉,延後其中一台
- 中央式 vs 分散式 fleet 設計取捨

**整合主線**:
- Phase 08 Custom Interfaces(`AssignTask.srv`、`FleetStatus.msg`)
- Phase 22A Nav2(每台一份)
- Phase 13 Action(任務本身用 NavigateToPose action)

**為什麼這章重要**:**從「會跑單機」進化到「會 orchestrate 一群機器人」**,這就是業界 production 的真實場景。

**預估時長**:2 day
**環境**:☁️/💻 同 Phase 22A,雲端推薦

---

## 📦 環境需求(本地)

```bash
# 主線 ROS 2 環境已就緒,額外:

# turtlebot3 multi-robot 範例(內建在 ROS 2 Humble)
sudo apt install ros-humble-turtlebot3-gazebo \
                 ros-humble-nav2-bringup \
                 ros-humble-multirobot-map-merge   # 多機地圖合併工具

# 多機 namespace launch 範例
ls /opt/ros/humble/share/nav2_bringup/launch/multi_tb3_simulation_launch.py
# Nav2 官方有 multi-robot 範例 launch,本支線會基於這個改
```

---

## 🐛 預期會踩的雷

1. **TF tree 全部撞同一個 `base_link`** — 沒設 `tf_prefix`,3 台機器人 TF 互相覆蓋,RViz 看不出有 3 台
2. **`/tf_static` 是全域的** — 即使設了 namespace,TF 仍混在一起(ROS 2 已知設計問題,要用 `tf_prefix` + remap `/tf` `/tf_static`)
3. **Gazebo 同時 spawn 3 台速度慢** — 用 TimerAction 延遲 spawn,避免 spawn race
4. **3 台同時跑 Nav2 → CPU 100%** — Nav2 8 個 lifecycle node × 3 = 24 個 node,WSL 沒 GPU 會卡死
5. **任務分派 race condition** — 兩個 client 同時送 srv 派任務給同一台機器人 → 用 mutex 或單一 fleet manager queue

---

## 🔗 學習資源

- [Nav2 Multi-Robot 範例](https://github.com/ros-navigation/navigation2/tree/main/nav2_bringup/launch)
- [Open-RMF](https://github.com/open-rmf) — 業界級 fleet manager(很重,當參考)
- [CARMA Platform](https://github.com/usdot-fhwa-stol/carma-platform) — 美國交通部開源 fleet 系統

---

## 🚦 開始之前

確認主線進度(至少要做完):
- ✅ Phase 11(Launch 進階,namespace)
- ✅ Phase 17(Gazebo)
- ✅ Phase 22A(Nav2 入門)— 必備
- ✅ Phase 08(Custom Interfaces)— 第 02 章用 srv/msg 做 fleet 通訊
- 推薦 ✅ Phase 20(多機通訊)— 進階場景每台機器人在不同電腦

---

## ⏭️ 從哪開始

主線完成後,**跑 [01-namespace-spawn](01-namespace-spawn/)**(待寫)。

> 這條支線目前只有 README 骨架。實際章節會在 gino 開始做時逐章補。
