#pragma once

#define LFE_VERSION_MAJOR 1
#define LFE_VERSION_MINOR 0
#define LFE_VERSION_PATCH 0
#define LFE_VERSION_BUILD 0
#define LFE_VERSION_STRING "1.0.0"
#define LFE_VERSION_WSTRING L"1.0.0"

#ifndef RC_INVOKED
namespace life {
inline constexpr auto AppName = "LFE-NNUE";
inline constexpr auto AppVersion = LFE_VERSION_STRING;
inline constexpr auto AppDisplayName = L"LFE-NNUE v" LFE_VERSION_WSTRING;
}
#endif
