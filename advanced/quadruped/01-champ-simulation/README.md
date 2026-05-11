# 01. CHAMP Simulation — 通用四足模擬

> 不用花幾十萬買一隻機械狗，我們用開源的 CHAMP 框架在 Gazebo 裡面跑一隻擁有 12 個自由度 (DOF) 的四足機器人。這是進入波士頓動力 (Spot) 或宇樹 (Unitree) 開發領域的第一步。

**學完你會**:
- 從原始碼編譯並安裝 `champ` (適用於 ROS 2 的通用四足模擬框架)
- 在 Gazebo 中召喚一隻四足機器人
- 使用標準的 `teleop_twist_keyboard`，透過鍵盤控制四足機器人前進、後退與轉向
- 觀察 `/joint_states` 瞭解四足機器人底層的馬達運作方式
- 體會「開源專案版本維護」的殘酷現實 (Humble 相容性除錯)

**前置**:
- [Phase 17 Gazebo](../../../phase-17-gazebo/) — 模擬器基礎
- [Phase 18 ros2_control](../../../phase-18-ros2-control/) — 硬體抽象層概念

**產出**:
- 透過指令啟動的 CHAMP 四足模擬器環境

**環境**:💻 本機 WSL2 / Ubuntu (Gazebo 在四足物理運算上非常耗 CPU，不建議在資源受限的雲端環境跑)

---

## 📍 為什麼選擇 CHAMP？

業界的四足機器人百花齊放，但每一家都有自己的 SDK (例如 Spot SDK, Unitree SDK)。如果你只是想學「四足的運動控制學」或是「如何在四足上跑 Nav2」，直接買實機的成本太高了。

CHAMP 是一個開源的通用四足機器人控制框架。它把四足機器人的複雜數學（反向運動學 IK、步態產生器 Gait Generator）包裝了起來，只對外暴露標準的 `/cmd_vel` 介面。
這意味著：**你可以把四足機器人當作一台輪式自走車 (Turtlebot) 來控制！**

學會 CHAMP，你就掌握了四足機器人在 ROS 2 生態系中最通用的串接方式。

---

## 💻 步驟 1: 編譯與安裝 CHAMP (ROS 2 Humble 版)

由於 CHAMP 原本是為 ROS 1 設計的，其 ROS 2 分支維護較不活躍。我們必須小心選擇正確的分支。

打開終端機，進入你的 ROS 2 工作區：

```bash
cd ~/ros2_ws/src

# 1. 抓取 CHAMP 核心框架 (ros2 分支)
git clone --recursive https://github.com/chvmp/champ.git -b ros2

# 2. 抓取 CHAMP 的遙控工具
git clone https://github.com/chvmp/champ_teleop.git -b ros2

# 3. 抓取一個現成的機器人描述檔 (以 CHAMP 預設的機器人為例)
# 如果你需要其他機器人 (如 a1, spot)，可以找對應的 description 套件
```

解決依賴並編譯：

```bash
cd ~/ros2_ws
# 用 rosdep 自動安裝缺少的系統套件
rosdep install --from-paths src --ignore-src -r -y

# 編譯所有 champ 相關套件
colcon build --packages-select champ_description champ_navigation champ_msgs champ_teleop champ_config
source install/setup.bash
```

> ⚠️ **新手雷區**：在編譯過程中可能會遇到一些警告 (Warnings)，只要沒有 `[ERROR]` 導致編譯中斷，都可以先忽略。

---

## 🚀 步驟 2: 在 Gazebo 中啟動四足機器人

CHAMP 提供了一個方便的 launch 檔，能一次幫我們把 Gazebo、RViz、以及核心的步態產生器 (Gait Generator) 開起來。

```bash
# 啟動模擬環境
ros2 launch champ_config gazebo.launch.py
```

這時候你應該會看到：
1. **Gazebo** 中出現了一隻四條腿的機器人，穩穩地站在地面上。
2. **RViz** 也會啟動，顯示出機器人的 TF 樹與雷射掃描範圍。

---

## 🎮 步驟 3: 用鍵盤讓它走起來

打開另一個終端機，啟動鍵盤控制節點：

```bash
source /opt/ros/humble/setup.bash
source ~/ros2_ws/install/setup.bash

# 啟動鍵盤遙控
ros2 run teleop_twist_keyboard teleop_twist_keyboard
```

- 按 `i` 前進
- 按 `,` 後退
- 按 `j` / `l` 左右平移 (螃蟹步)
- 按 `u` / `o` 原地旋轉

切換到 Gazebo 視窗，你會看到機器人不是用輪子滑行，而是**踏著步伐 (Trotting Gait)** 往前走！這就是 CHAMP 底層幫我們把 `cmd_vel` (速度向量) 即時轉換成 12 個馬達角度的魔力。

---

## 🔍 步驟 4: 偷窺底層運作

到底 CHAMP 是怎麼讓機器人走路的？我們可以觀察 Topic：

```bash
ros2 topic list
```
你會看到：
- `/cmd_vel`：我們鍵盤發出的指令
- `/joint_states`：12 個馬達的當前角度
- `/joint_group_effort_controller/command` (或類似名稱)：這是發給 Gazebo 底層 `ros2_control` 的馬達控制訊號。

觀察馬達角度的瘋狂更新：
```bash
ros2 topic echo /joint_states
```
四條腿、每條腿 3 個關節，這 12 個數值在走路時會以非常高的頻率（通常 >= 50Hz）不斷變動，這就是四足機器人運算量遠高於輪式機器人的原因。

---

## 🐛 常見雷

### ⚠️ 雷 1：Gazebo 裡面機器人一出生就抽搐或飛走
**症狀**：機器人在 Gazebo 裡不斷鬼畜抖動，甚至飛上天。
**原因**：四足的物理模擬非常吃重即時運算，如果你的電腦 CPU 效能不足，Gazebo 的物理引擎 (Physics Step) 會跟不上控制器發送指令的頻率，導致物理碰撞爆炸。
**解**：在 WSL2 中確保沒有其他佔用大量 CPU 的程式。或者修改 Gazebo 的 `<real_time_update_rate>` 降低物理模擬速度（讓模擬時間變慢，換取運算穩定）。

### ⚠️ 雷 2：`colcon build` 時報錯找不到套件
**症狀**：編譯失敗，說缺少 `nav2_xxx` 等相依套件。
**解**：確保你在編譯前有確實執行 `rosdep install` 指令，它會幫你把 `ros-humble-nav2-bringup` 那些 APT 套件補齊。

---

## 🎯 學到的關鍵概念

- **抽象化的威力**：不管機器人有幾個輪子、幾條腿，對上層演算法 (Navigation) 來說，它永遠只是一個接收 `/cmd_vel` 的載具。這是 ROS 系統設計最精妙的地方。
- **Gait Generator (步態產生器)**：這是四足機器人專屬的心臟。它負責把抽象的速度指令，翻譯成 12 顆馬達的連續軌跡。

---

## 🌟 進階挑戰

1. **觀察 TF Tree**：打開 `rqt_tf_tree`，看看四足機器人的 TF 結構跟 Turtlebot 有什麼不同（提示：注意每條腿的 link 命名方式）。
2. **切換視角**：在 RViz 中，把 Fixed Frame 從 `odom` 改成 `base_link`，看看在「機器人本體視角」下，世界是怎麼移動的。

---

## 🔗 下一步

現在機器人會走路了，但它的走路姿勢是可以改變的！進入 [02. Gait Control](../02-gait-control/)，我們將深入了解如何調整步態參數，讓它用不同的姿勢行走。
