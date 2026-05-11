# Phase 06：Parameters（參數系統）

**學完你會**：把寫死在 code 裡的「魔法數字」拔掉，通通變成可以從外部動態調整的 Parameter。透過 CLI、YAML 設定檔、甚至超直覺的 `rqt_reconfigure` GUI 來調參數——再也不用經歷「改一個小數字就得重新編譯」的痛苦循環啦！

**前置**：[Phase 03](../phase-03-subscriber-lidar-brake/) 的 `auto_brake.cpp`（我們要在它身上開刀，加上 param 功能）。

**產出**：[`code/my_cpp_pkg/`](code/my_cpp_pkg/) — 含 `auto_brake_param` 執行檔 + 給它吃的 `config/auto_brake_params.yaml`。

---

## 🤔 為什麼我們需要 Parameters？

還記得 Phase 03 我們寫的 `auto_brake.cpp` 嗎？來看看這段程式碼：

```cpp
if (min_forward_distance > 1.0f) {     // ⚠️ 這裡的 1.0 是寫死的！
    twist_msg.linear.x = 0.2;          // ⚠️ 這裡的 0.2 也是寫死的！
}
```

這就是傳說中的 **Hardcode（寫死）**。這會帶來什麼悲慘世界？
- 想把煞車距離改成 0.5m 試試看？ → 改 code → 重新 colcon build → 重跑（恭喜，30 秒過去了）
- 老闆說：「車子給我開慢一點」？ → 改 code → 重編 → 重跑...
- 如果要在不同大小的場地測試？ → 難道要複製十份不同設定的 code？

**Parameters（參數系統）就是來拯救你的！** 它的核心理念很簡單：把這些常數宣告成 Parameter，讓你在**程式執行中隨時動態修改**！

如果你熟悉其他技術，ROS 2 Parameter 就相當於：
| 你熟悉的玩意 | ROS 2 Parameter 對應的招式 |
|---------|----------------|
| 環境變數 (`$DEBUG=1`) | `ros2 param set` |
| `.env` / `config.yaml` | `params.yaml` + `--params-file` 參數 |
| Spring Boot `@Value` | `node->get_parameter("name")` |
| Docker `ENV` | Launch file 裡的 `parameters=[...]` |

---

## 🕵️ 終端機偵探課：看透 Node 的內建參數

別急著寫 code，其實**所有 ROS 2 Node 一出生就自帶一些內建 parameter**（例如：`use_sim_time`）。我們寫自己的 param 之前，先來看看預設有什麼：

```bash
# 先隨便跑一個 Node (這裡拿 Phase 03 的 auto_brake 來當白老鼠)
ros2 run my_cpp_pkg auto_brake &

# 看看這傢伙身上掛了哪些 param
ros2 param list /auto_brake_node
```

你應該會看到：
```
/auto_brake_node:
  use_sim_time         ← 內建 param，決定要用現實時間還是模擬時間
```

💡 **最重要的內建 param 是 `use_sim_time`**：
- 預設是 `false`（使用真實世界的時鐘）。
- 如果你跑 Gazebo 或是重播 Bag 檔，必須設為 `true`（使用模擬器提供的時間）。
- 未來在 Phase 17 跑 Gazebo、Phase 22A 跑 Nav2 時，這傢伙會如影隨形。

來玩玩看查詢和修改：
```bash
# 查查看
ros2 param get /auto_brake_node use_sim_time
# 預期輸出: Boolean value is: False

# 霸王硬上弓，改掉它！
ros2 param set /auto_brake_node use_sim_time true
# 預期輸出: Set parameter successful

# 看看這個 param 的身家調查 (型別、預設值、限制)
ros2 param describe /auto_brake_node use_sim_time
```

**我們這章要幹嘛？** 就是要**自訂我們自己的 param**（例如 `safe_distance`、`max_speed`），來取代那些寫死的數字。學完之後，你的 Node 執行 `ros2 param list` 時，就會看到你的寶貝參數跟 `use_sim_time` 並列啦！

---

## 💻 動刀改造 Phase 03

完整程式碼見 [`code/my_cpp_pkg/src/auto_brake_param.cpp`](code/my_cpp_pkg/src/auto_brake_param.cpp)。

我們來宣告三個 parameter：

| 名稱 | 預設值 | 用途 |
|------|--------|------|
| `safe_distance` | 1.0 m | 觸發煞車的危險距離 |
| `max_speed` | 0.2 m/s | 正常前進的速度 |
| `corridor_width` | 0.4 m | 偵測走廊的寬度 |

最關鍵的程式碼在這裡：

```cpp
// 1. 在建構子裡霸氣宣告它們的存在
this->declare_parameter<double>("safe_distance", 1.0);
this->declare_parameter<double>("max_speed", 0.2);
this->declare_parameter<double>("corridor_width", 0.4);

// 2. 第一次讀取，並把它們存到成員變數裡（快取起來，才不用每次都查表）
safe_distance_ = this->get_parameter("safe_distance").as_double();
max_speed_     = this->get_parameter("max_speed").as_double();
corridor_width_ = this->get_parameter("corridor_width").as_double();

// 3. 註冊一個「變更攔截器」（有人亂改數值時，先經過這裡驗證）
param_callback_handle_ = this->add_on_set_parameters_callback(
    std::bind(&AutoBrakeParamNode::on_param_change, this, _1));
```

攔截器 `on_param_change` 就像門神，負責驗證新來的數值：

```cpp
rcl_interfaces::msg::SetParametersResult on_param_change(
    const std::vector<rclcpp::Parameter> & params)
{
    rcl_interfaces::msg::SetParametersResult result;
    result.successful = true;

    for (const auto & p : params) {
        if (p.get_name() == "safe_distance") {
            double v = p.as_double();
            if (v < 0.0) {                              // ← 驗證：煞車距離不能是負的吧？
                result.successful = false;
                result.reason = "safe_distance must be >= 0";
                return result;
            }
            safe_distance_ = v;                          // ← 驗證過關，套用新數值
            RCLCPP_INFO(this->get_logger(), "safe_distance 被改成 %.2f 啦", v);
        }
        // max_speed, corridor_width 也是同樣的套路...
    }
    return result;
}
```

> **🤔 為什麼要這麼麻煩弄個「快取」？** 
> 想像一下，光達每秒送 10 次資料，Callback 就會被叫 10 次。如果每次都去 `get_parameter` 查表，效能會很慘。所以**「先存進變數 (快取) + 透過 on_set_callback 更新」才是真正的業界慣例**！
> 
> **⚠️ 還有，`param_callback_handle_` 一定要存起來**：如果不存進成員變數，這個攔截器一註冊完就立刻被系統回收（析構），等於沒註冊一樣！

---

## 🚀 五個 Demo：見證 Parameters 的魔法

### Demo 1：預設值啟動 + CLI 查詢

```bash
# Terminal 1：模擬 2 公尺前有個障礙物
python3 /tmp/fake_lidar.py 2.0

# Terminal 2：把我們的 Node 跑起來
ros2 run phase06_pkg auto_brake_param
```

觀察啟動 log：
```
[INFO] Started with: safe_distance=1.00, max_speed=0.20, corridor_width=0.40
[INFO] Clear (closest=2.00m, threshold=1.00m). Speed=0.20
```

用 CLI 查看看：
```bash
ros2 param list /auto_brake_param_node
ros2 param get /auto_brake_param_node safe_distance
# 系統回答: Double value is: 1.0
```

### Demo 2：CLI 即時修改，機器人行為一秒切換

不用關程式、不用重編譯，我們直接改：

```bash
ros2 param set /auto_brake_param_node safe_distance 2.5
# 系統回答: Set parameter successful
```

這時候回頭看 Node 的 log，馬上變臉：
```
[INFO] Clear (closest=2.00m, threshold=1.00m). Speed=0.20  ← 這是改之前
[INFO] safe_distance 被改成 2.50                            ← 攔截器收到了！
[WARN] BRAKING (obstacle=2.00m < threshold=2.50m)           ← 同樣是 2m 的障礙物，因為安全距離提高了，現在觸發煞車了！
[WARN] BRAKING (obstacle=2.00m < threshold=2.50m)
```

🎯 **這就是 Parameters 最迷人的地方**：同樣的程式碼、同樣的環境，動動手指改個數值，系統行為瞬間改變。

### Demo 3：門神發威（驗證攔截器擋下惡搞數值）

我們來試試看塞一些不合理的數字：
```bash
ros2 param set /auto_brake_param_node max_speed 5.0
# 系統回答: Setting parameter failed: max_speed must be in [0, 2.0]

ros2 param set /auto_brake_param_node safe_distance -1.0
# 系統回答: Setting parameter failed: safe_distance must be >= 0

ros2 param get /auto_brake_param_node safe_distance
# 系統回答: Double value is: 2.5    ← 沒被改成 -1.0，還維持在 Demo 2 的 2.5
```

🎯 **Production Code 必備的防呆機制**——再也不怕手滑打錯字讓機器人暴走啦。

### Demo 4：用 YAML 批次灌入設定

當你的參數多到不行，總不能一個一個 set。寫個 [`config/auto_brake_params.yaml`](code/my_cpp_pkg/config/auto_brake_params.yaml)：

```yaml
auto_brake_param_node:           # ⚠️ 這裡的名稱必須跟 Node 名字一模一樣！
  ros__parameters:
    safe_distance: 0.8
    max_speed: 0.15
    corridor_width: 0.5
```

啟動時，帶著這個 YAML 檔一起上路：

```bash
ros2 run phase06_pkg auto_brake_param --ros-args \
  --params-file ~/ros2_ws/install/phase06_pkg/share/phase06_pkg/config/auto_brake_params.yaml
```

啟動 log：
```
[INFO] Started with: safe_distance=0.80, max_speed=0.15, corridor_width=0.50
                              ↑              ↑                ↑
                             現在這些值都是從 YAML 來的，不是預設值囉！
```

🎯 **業界標準做法**：程式碼裡放預設值當保底，真正的設定檔（Source of truth）放在 YAML 裡。

> ⚠️ **新手常見雷：YAML 裡的 Node 名稱拼錯**。`auto_brake_param_node:` 必須跟 code 裡的 `Node("auto_brake_param_node")` 一字不差。打錯的話，系統不會報錯，它只會默默忽略你的 YAML，繼續用預設值（這也是很靜默失敗的...）。

### Demo 5：GUI 神器 `rqt_reconfigure`，拖拉點拽改參數

開個 GUI 來玩玩：

```bash
rqt --standalone rqt_reconfigure.param_plugin.ParamPlugin
```

> ⚠️ **注意**：Humble 版本不能用以前舊的 `rqt_reconfigure.rqt_reconfigure.RqtReconfigure`，會找不到 plugin 的。

介面開起來後，左邊會列出所有活著的 Node：

![rqt_reconfigure 主視窗，左邊列出所有節點](images/rqt_reconfigure_overview.png)

點擊 `auto_brake_param_node`，右邊就會列出所有你可以調的參數：

![選中 auto_brake_param_node 後，右側顯示三個 param 與 QoS 隱藏 param](images/rqt_reconfigure_panel.png)

> 中間的 `qos_overrides.*` 跟 `use_sim_time` 是 ROS 2 的系統隱藏屬性，可以不用理它們。
> 📝 *小抱怨：Humble 版本的 double 參數沒有做滑桿，只給輸入框。雖然稍微少了點拖曳的快感，但輸入數字反而比較精準啦。*

我們把 `safe_distance` 改成 `5.0` 然後按 Enter：

![safe_distance 輸入框已改成 5.0](images/rqt_reconfigure_safe_5.png)

去看終端機的 log，馬上就改了：
```
[WARN] BRAKING (obstacle=2.00m < threshold=5.00m)    ← 從 INFO 變 WARN 啦！
```

🎯 **不用重編譯、不用重啟，甚至不用敲指令，打開介面改個數字就能即時改變機器人行為**——這招學起來，下次要 Demo 給老闆看的時候，保證讓他覺得你很神！

---

## 🐛 開發者血淚史：常見踩雷指南

### 雷 1：rqt_reconfigure 找不到 Plugin
Humble 已經換路徑了！請愛用 `rqt_reconfigure.param_plugin.ParamPlugin`。

### 雷 2：YAML 吃了沒反應
通常是 Node 的名字打錯了。記得用 `ros2 node list` 對照一下，大小寫、底線一字不差才行。

### 雷 3：Callback Handle 憑空消失
```cpp
this->add_on_set_parameters_callback(...);  // ❌ 沒接回傳值，瞬間被析構
auto h = this->add_on_set_parameters_callback(...);  // ❌ 放在區域變數，函式跑完就被回收
self->param_callback_handle_ = ...;  // ✅ 乖乖存成類別的成員變數
```

### 雷 4：在 Callback 裡瘋狂查表
千萬不要在像 PID 控制器這種高頻率執行的 Callback 裡直接呼叫 `get_parameter`。效能會拖垮！**請永遠愛用「快取 + on_set 攔截器更新」的招式**。

### 雷 5：硬塞錯的型別
```cpp
this->declare_parameter<double>("safe_distance", 1.0);
ros2 param set /node safe_distance 2  // ❌ 宣告 double，你給 int (2) 系統會生氣
ros2 param set /node safe_distance 2.0  // ✅ 加上小數點保平安
```

---

## 🎯 本章精華總結

- **Parameters = Node 內建的超強設定系統**：從宣告、讀取到變更監聽，ROS 2 全包了。
- **改 Param 的三種姿勢**：CLI（自己測試時用）、YAML（正式上線用）、GUI（Demo 耍帥用）。
- **變更攔截器（on_set_callback）**：在數值被改掉之前先做驗證，不爽可以拒絕。
- **效能王道**：高頻場景一定要用快取，不要傻傻地每次都去 `get_parameter`。
- **YAML 的標準寫法**：`<node_name>: ros__parameters: <param>: <value>`。

---

## 👣 下一步去哪？

恭喜你學會了怎麼完美控制「單一」節點！但在真實世界裡，一個系統常常要同時跑好幾個節點。難道我們每次啟動都要手動開好幾個終端機慢慢敲指令嗎？
- Phase 07 — Mini Capstone 1：先來個小驗收，把 Param + Service + LiDAR 串起來做個酷東西！
- Phase 09 — Launch Files：教你用一個 `ros2 launch` 指令，一鍵喚醒整個系統，還能自動把 YAML 參數塞進去！

---

<sub>🐍 Python 玩家注意：`rclpy` 的 Parameter API 概念完全一模一樣，就是換成 `self.declare_parameter('name', default)` 這種寫法而已啦。</sub>
