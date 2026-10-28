#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>

namespace life {
struct PerformancePowerState {
  bool known{},saver{},noBattery{};unsigned percent{255};
  constexpr bool eligible()const{return known&&!saver&&(noBattery||(percent>=30&&percent<=100));}
  std::string reason()const{
    if(!known)return "Power status is unavailable";
    if(saver)return "Battery Saver is enabled";
    if(!noBattery&&percent<30)return "Battery is below 30%";
    if(!noBattery&&percent>100)return "Battery charge is unavailable";
    return {};
  }
};
inline PerformancePowerState performancePowerState(){
  SYSTEM_POWER_STATUS status{};if(!GetSystemPowerStatus(&status)||status.SystemStatusFlag>1)return {};
  return {true,status.SystemStatusFlag==1,status.BatteryFlag!=255&&(status.BatteryFlag&128)!=0,status.BatteryLifePercent};
}
}
