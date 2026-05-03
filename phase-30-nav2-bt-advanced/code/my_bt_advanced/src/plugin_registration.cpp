// plugin_registration.cpp — Phase 30
//
// 把 4 個 BT node 註冊成一個 plugin library。
// nav2_bt_navigator 用 dlopen 載這個 .so 後,4 個 node 都會出現在 BT factory。
//
// 對照 Phase 23A:那裡只有 1 個 node,所以註冊 + 實作放同檔。
// 多 node 時拆開比較乾淨。

#include "behaviortree_cpp_v3/bt_factory.h"

#include "my_bt_advanced/go_to_charger_action.hpp"
#include "my_bt_advanced/once_every_decorator.hpp"
#include "my_bt_advanced/count_successes_decorator.hpp"
#include "my_bt_advanced/patrol_waypoint_action.hpp"

BT_REGISTER_NODES(factory)
{
  factory.registerNodeType<my_bt_advanced::GoToChargerAction>("GoToCharger");
  factory.registerNodeType<my_bt_advanced::OnceEveryDecorator>("OnceEvery");
  factory.registerNodeType<my_bt_advanced::CountSuccessesDecorator>("CountSuccesses");
  factory.registerNodeType<my_bt_advanced::PatrolWaypointAction>("PatrolWaypoint");
}
