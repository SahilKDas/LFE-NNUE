#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace life {
struct AppSettings {
  int schema = 1;
  int requestedTps = 60;
  int requestedFps = 30;
  int overlay = 0;
  int windowX = -1;
  int windowY = -1;
  int windowWidth = 1500;
  int windowHeight = 900;
  bool maximized = false;
};

AppSettings parseSettings(std::string_view text);
std::string serializeSettings(const AppSettings& settings);
std::filesystem::path appDataDirectory();
std::filesystem::path settingsFilePath();
std::filesystem::path logFilePath();
AppSettings loadSettings();
bool saveSettings(const AppSettings& settings);
void initializeLog();
void logMessage(std::string_view message);
}
