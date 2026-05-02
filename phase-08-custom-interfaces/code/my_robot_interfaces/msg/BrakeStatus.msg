# BrakeStatus.msg
# 廣播煞車系統的當前狀態。每 1Hz 由 smart_brake_v2 發送。
#
# IDL 語法重點：
#   <type> <field_name>
#   constants 用 大寫 + 等號定義
#
# 不能放：
#   - 註解寫在欄位同一行的後面（會錯）→ 必須單獨一行
#   - tab 縮排（用空格）

# === Mode 常數 (enum 慣例) ===
uint8 MODE_DISABLED=0
uint8 MODE_ENABLED=1
uint8 MODE_EMERGENCY=2

# === 欄位 ===
# 用標準 std_msgs/Header 帶時間戳與 frame_id（業界慣例）
std_msgs/Header header

# 當前模式 (對應上面三個常數)
uint8 mode

# 當前 cmd_vel.linear.x 實際送出的值
float32 current_speed

# 光達看到正前方最近障礙物距離；無障礙物時 = 100.0（哨兵值）
float32 closest_obstacle_distance

# 系統運轉幾秒了
float32 uptime_seconds

# 自由文字訊息，給人類看的
string status_text
