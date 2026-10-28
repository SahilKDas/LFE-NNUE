#pragma once
#include <windows.h>
#include <chrono>
#include <thread>
#include <vector>

namespace life {
// Sleep without the coarse/occlusion-sensitive Sleep timer used by std::sleep_until.
// Each pacing thread owns its timer; no spin loop, process priority or global timer changes.
class FramePacer {
  HANDLE timer_{CreateWaitableTimerExW(nullptr,nullptr,0x00000002,TIMER_MODIFY_STATE|SYNCHRONIZE)};
public:
  static void deadlineWork(){
    THREAD_POWER_THROTTLING_STATE qos{};qos.Version=THREAD_POWER_THROTTLING_CURRENT_VERSION;qos.ControlMask=THREAD_POWER_THROTTLING_EXECUTION_SPEED;
    SetThreadInformation(GetCurrentThread(),ThreadPowerThrottling,&qos,sizeof(qos));
  }
  static void preferFastCore(unsigned role){
    static const auto cores=[] {
      std::vector<PROCESSOR_NUMBER> result;DWORD bytes=0;GetLogicalProcessorInformationEx(RelationProcessorCore,nullptr,&bytes);std::vector<unsigned char> buffer(bytes);
      if(!bytes||!GetLogicalProcessorInformationEx(RelationProcessorCore,reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()),&bytes))return result;
      BYTE fastest=0,slowest=255;for(size_t offset=0;offset<bytes;){const auto* info=reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data()+offset);fastest=std::max(fastest,info->Processor.EfficiencyClass);slowest=std::min(slowest,info->Processor.EfficiencyClass);offset+=info->Size;}
      if(fastest==slowest)return result;
      for(size_t offset=0;offset<bytes;){const auto* info=reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data()+offset);if(info->Processor.EfficiencyClass==fastest)for(WORD group=0;group<info->Processor.GroupCount;++group){const auto& mask=info->Processor.GroupMask[group];for(BYTE number=0;number<sizeof(KAFFINITY)*8;++number)if(mask.Mask&(KAFFINITY(1)<<number)){result.push_back({mask.Group,number,0});break;}}offset+=info->Size;}return result;
    }();
    if(!cores.empty()){auto ideal=cores[role%cores.size()];SetThreadIdealProcessorEx(GetCurrentThread(),&ideal,nullptr);}
  }
  explicit FramePacer(unsigned role=0){
    if(!timer_)timer_=CreateWaitableTimerW(nullptr,FALSE,nullptr);
    // Deadline-driven work must not be mistaken for background EcoQoS work on
    // hybrid CPUs. This is a scheduling hint, not affinity or raised priority.
    deadlineWork();
    preferFastCore(role);
  }
  FramePacer(const FramePacer&)=delete;
  FramePacer& operator=(const FramePacer&)=delete;
  ~FramePacer(){if(timer_)CloseHandle(timer_);}
  void until(std::chrono::steady_clock::time_point deadline){
    const auto remaining=deadline-std::chrono::steady_clock::now();
    if(remaining<=std::chrono::steady_clock::duration::zero())return;
    if(!timer_){std::this_thread::sleep_until(deadline);return;}
    LARGE_INTEGER due{};due.QuadPart=-std::max<long long>(1,std::chrono::duration_cast<std::chrono::nanoseconds>(remaining).count()/100);
    if(SetWaitableTimer(timer_,&due,0,nullptr,nullptr,FALSE))WaitForSingleObject(timer_,INFINITE);
    else std::this_thread::sleep_until(deadline);
  }
};
}
