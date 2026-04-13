# **🚀 零成本上手 ROS 2 (C++)：從雲端環境到第一支自駕程式**

**目標讀者**：已經熟悉 C++ 與 Linux 終端機操作，但對 ROS 2 (Robot Operating System) 完全零基礎的開發者。

**學習目標**：不花一毛錢、不在本地端安裝任何東西，直接在雲端寫 C++ 控制 3D 模擬器中的機器人。

## **📍 課前觀念建立**

如果你懂 Linux 和 C++，請這樣理解 ROS 2：

1. **它不是「作業系統」**：它是一個建構在 Linux (通常是 Ubuntu) 之上的中介軟體 (Middleware) 和通訊框架。  
2. **它的核心是「通訊」**：你可以把機器人想像成一個大型系統，裡面有很多獨立的 C++ 程式（稱為 **Node 節點**）。有些負責看影像、有些負責控制輪子。ROS 2 提供了一個機制，讓這些程式可以透過發布/訂閱（Publish/Subscribe）互相傳遞資料（稱為 **Topic 主題**）。  
3. **colcon 就是進階版的 make**：ROS 2 有專屬的編譯管理工具叫 colcon，它會在底層呼叫 CMake 來幫你編譯 C++ 程式。

## ---

**🛠️ 第一階段：架設免安裝的雲端實驗室**

我們將使用 The Construct 平台的 **ROSjects**，它本質上是一台已經幫你安裝好 ROS 2 (Ubuntu) 和 3D 模擬器 (Gazebo) 的免費雲端虛擬機。

1. **註冊與建立**：進入 [The Construct 平台](https://app.theconstructsim.com/)，註冊後點擊 **ROSjects** \-\> **Create New ROSject**。  
2. **選擇版本**：ROS Distro 請務必選擇 **ROS 2 Humble**（目前主流的長期支援版）。  
3. **啟動環境**：建立完成後，在專案卡片右上角找到一個藍色按鈕 \</\> Open（中文版介面可能被奇葩地翻譯為 \</\> 开放式 Rosjects），點擊它進入開發虛擬機。  
4. **熟悉介面**：畫面下方工具列有三個核心：  
   * **Terminal**：標準的 Linux 終端機。  
   * **Code Editor**：VS Code 風格的程式碼編輯器。  
   * **Gazebo**：顯示實體機器人的 3D 模擬視窗。

## ---

**🕵️‍♂️ 第二階段：ROS 2 終端機偵探課**

打開終端機與 Gazebo 視窗。假設你的環境中已經有一台載入好的機器人（例如 OriginBot 賽車）。我們來調查它：

**1\. 找出通訊頻道 (Topics)**

機器人所有的感測器與控制指令都在這些頻道裡流動。

Bash

ros2 topic list

*💡 你會看到一長串列表，其中負責控制移動的頻道通常叫做 /cmd\_vel。但請注意，如果場地內有多台車，名稱可能會加上前綴，例如 /originbot\_1/cmd\_vel。*

**2\. 鍵盤遙控與「重新導向」(Remapping)**

我們可以用內建的鍵盤工具來測試車子。但如果鍵盤工具預設發送的頻道 (/cmd\_vel) 跟車子實際接收的頻道 (/originbot\_1/cmd\_vel) 對不上怎麼辦？

在 ROS 2，你不需要改程式碼，可以在執行時直接動態替換頻道：

Bash

ros2 run teleop\_twist\_keyboard teleop\_twist\_keyboard \--ros-args \-r cmd\_vel:=/originbot\_1/cmd\_vel

*💡 執行後，滑鼠點擊終端機保持游標閃爍，按下 I, J, K, L 就能看到車子移動了！*

## ---

**💻 第三階段：動手寫 C++ 程式 (Publisher)**

現在，我們要寫一支 C++ 程式來取代鍵盤，自動發送速度指令給車子。

### **⚠️ 新手避坑指南：絕對不要手動建資料夾！**

在寫一般 C++ 專案時，你可能習慣右鍵新增資料夾。**在 ROS 2 中絕對不行！**

一個合法的 ROS 2 套件必須包含 package.xml 和 CMakeLists.txt。請務必使用標準 CLI 指令來生成套件。

### **步驟 1：建立標準 C++ 套件**

在終端機輸入：

Bash

cd \~/ros2\_ws/src  
ros2 pkg create \--build-type ament\_cmake my\_cpp\_pkg \--dependencies rclcpp

*💡 這會建立一個名為 my\_cpp\_pkg 的資料夾，並自動設定好 C++ 編譯環境，同時將 rclcpp (ROS 2 C++ 核心函式庫) 加入依賴。*

### **步驟 2：撰寫「自動駕駛」節點**

打開 **Code Editor**，進入 ros2\_ws/src/my\_cpp\_pkg/src/，新增 auto\_drive.cpp：

C++

\#**include** "rclcpp/rclcpp.hpp"  
\#**include** "geometry\_msgs/msg/twist.hpp" // 速度指令的標準格式  
\#**include** \<chrono\>

using namespace std::chrono\_literals;

class AutoDriveNode : public rclcpp::Node {  
public:  
    AutoDriveNode() : Node("auto\_drive\_node") {  
        // 建立發布者 (Publisher)，對準目標頻道  
        publisher\_ \= this\-\>create\_publisher\<geometry\_msgs::msg::Twist\>("/originbot\_1/cmd\_vel", 10);  
          
        // 建立計時器，每 0.5 秒執行一次發送迴圈  
        timer\_ \= this\-\>create\_wall\_timer(500ms, std::bind(\&AutoDriveNode::timer\_callback, this));  
        start\_time\_ \= this\-\>now();  
    }

private:  
    void timer\_callback() {  
        auto msg \= geometry\_msgs::msg::Twist();  
        auto elapsed\_time \= this\-\>now() \- start\_time\_;

        // 前 3 秒往前開，之後停車  
        if (elapsed\_time.seconds() \< 3.0) {  
            msg.linear.x \= 0.2; // 線速度 0.2 m/s  
            msg.angular.z \= 0.0; // 角速度 0 (不轉彎)  
        } else {  
            msg.linear.x \= 0.0;  
            msg.angular.z \= 0.0;  
        }  
        publisher\_-\>publish(msg); // 發送指令  
    }

    rclcpp::Publisher\<geometry\_msgs::msg::Twist\>::SharedPtr publisher\_;  
    rclcpp::TimerBase::SharedPtr timer\_;  
    rclcpp::Time start\_time\_;  
};

int main(int argc, char \* argv\[\]) {  
    rclcpp::init(argc, argv);  
    rclcpp::spin(std::make\_shared\<AutoDriveNode\>());  
    rclcpp::shutdown();  
    return 0;  
}

### **步驟 3：設定 CMakeLists.txt**

打開 my\_cpp\_pkg 根目錄下的 CMakeLists.txt。

因為我們用到了新的速度訊息格式 (geometry\_msgs)，需要告訴編譯器：

1. 在 find\_package(rclcpp REQUIRED) 下方加入：  
   CMake  
   find\_package(geometry\_msgs REQUIRED)

2. 在檔案中下方的空白處加入編譯規則：  
   CMake  
   add\_executable(auto\_drive src/auto\_drive.cpp)  
   ament\_target\_dependencies(auto\_drive rclcpp geometry\_msgs)

   install(TARGETS  
     auto\_drive  
     DESTINATION lib/${PROJECT\_NAME}  
   )

### **步驟 4：編譯與執行 (The Colcon Way)**

回到終端機，回到工作區的根目錄執行編譯：

Bash

cd \~/ros2\_ws  
colcon build \--packages-select my\_cpp\_pkg

編譯成功後，一定要「刷新」環境變數，讓終端機載入剛做好的執行檔：

Bash

source install/setup.bash

最後，確保模擬器畫面開著，執行你的程式：

Bash

ros2 run my\_cpp\_pkg auto\_drive

**恭喜！你應該會看到機器人順利地往前開了 3 秒鐘然後停下。你的第一個 ROS 2 自動駕駛專案大功告成！**

---

如果你準備好進入「第二階段」，我們可以開始挑戰讓車子不僅會「盲走」，還能透過雷達 (LiDAR) **「看見」** 牆壁並自動避障！隨時跟我說你準備好了沒！