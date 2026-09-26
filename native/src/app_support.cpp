#include "app_support.hpp"
#include "version.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace life {
namespace {
std::mutex logMutex;
int number(std::string_view value, int fallback) {
  try { size_t used{}; const int parsed=std::stoi(std::string(value),&used); return used==value.size()?parsed:fallback; }
  catch (...) { return fallback; }
}
void normalize(AppSettings& value) {
  value.schema=1;
  value.requestedTps=std::clamp(value.requestedTps,1,120);
  if(value.requestedFps<30||value.requestedFps>360||value.requestedFps%30) value.requestedFps=30;
  value.overlay=std::clamp(value.overlay,0,6);
  value.windowWidth=std::clamp(value.windowWidth,960,7680);
  value.windowHeight=std::clamp(value.windowHeight,640,4320);
  value.windowX=std::clamp(value.windowX,-32000,32000);
  value.windowY=std::clamp(value.windowY,-32000,32000);
}
}

AppSettings parseSettings(std::string_view text) {
  AppSettings result; std::istringstream input{std::string(text)}; std::string line;
  while(std::getline(input,line)) { const auto split=line.find('='); if(split==std::string::npos) continue; const auto key=line.substr(0,split),value=line.substr(split+1);
    if(key=="requested_tps") result.requestedTps=number(value,result.requestedTps);
    else if(key=="requested_fps") result.requestedFps=number(value,result.requestedFps);
    else if(key=="overlay") result.overlay=number(value,result.overlay);
    else if(key=="window_x") result.windowX=number(value,result.windowX);
    else if(key=="window_y") result.windowY=number(value,result.windowY);
    else if(key=="window_width") result.windowWidth=number(value,result.windowWidth);
    else if(key=="window_height") result.windowHeight=number(value,result.windowHeight);
    else if(key=="maximized") result.maximized=number(value,0)!=0;
  }
  normalize(result); return result;
}

std::string serializeSettings(const AppSettings& raw) {
  auto value=raw; normalize(value); std::ostringstream out;
  out<<"schema=1\nrequested_tps="<<value.requestedTps<<"\nrequested_fps="<<value.requestedFps<<"\noverlay="<<value.overlay
     <<"\nwindow_x="<<value.windowX<<"\nwindow_y="<<value.windowY<<"\nwindow_width="<<value.windowWidth
     <<"\nwindow_height="<<value.windowHeight<<"\nmaximized="<<(value.maximized?1:0)<<'\n'; return out.str();
}

std::filesystem::path appDataDirectory() {
#ifdef _WIN32
  if(const wchar_t* local=_wgetenv(L"LOCALAPPDATA")) return std::filesystem::path(local)/L"LFE-NNUE";
#endif
  return std::filesystem::temp_directory_path()/"LFE-NNUE";
}
std::filesystem::path settingsFilePath(){return appDataDirectory()/"settings.ini";}
std::filesystem::path logFilePath(){return appDataDirectory()/"logs"/"life-engine.log";}

AppSettings loadSettings() { std::ifstream input(settingsFilePath(),std::ios::binary); if(!input) return {}; std::ostringstream text; text<<input.rdbuf(); return parseSettings(text.str()); }
bool saveSettings(const AppSettings& settings) { try { std::filesystem::create_directories(appDataDirectory()); const auto target=settingsFilePath();const std::filesystem::path temporary=target.wstring()+L".tmp"; {std::ofstream output(temporary,std::ios::binary|std::ios::trunc); if(!output)return false; output<<serializeSettings(settings); output.flush(); if(!output)return false;} std::error_code error; std::filesystem::rename(temporary,target,error); if(error){std::filesystem::remove(target,error);error.clear();std::filesystem::rename(temporary,target,error);} return !error; } catch(...){return false;} }

void initializeLog() { try { const auto path=logFilePath(); std::filesystem::create_directories(path.parent_path()); std::error_code error; if(std::filesystem::exists(path,error)&&std::filesystem::file_size(path,error)>1024*1024){const auto previous=path.parent_path()/"life-engine.previous.log";std::filesystem::remove(previous,error);error.clear();std::filesystem::rename(path,previous,error);} logMessage(std::string("starting ")+AppName+" v"+AppVersion); } catch(...){} }
void logMessage(std::string_view message) { try { std::lock_guard lock(logMutex); const auto now=std::chrono::system_clock::now(); const auto time=std::chrono::system_clock::to_time_t(now); std::tm local{};
#ifdef _WIN32
  localtime_s(&local,&time);
#else
  localtime_r(&time,&local);
#endif
  std::ofstream output(logFilePath(),std::ios::app); output<<std::put_time(&local,"%Y-%m-%d %H:%M:%S")<<" | "<<message<<'\n'; } catch(...){} }
}
