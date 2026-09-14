#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "skia_api.hpp"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace {
std::unique_ptr<life::skia::Api> api;
std::vector<uint32_t> pixels;
BITMAPINFO bitmap{};
int surfaceWidth{}, surfaceHeight{};

std::filesystem::path libraryPath() {
  wchar_t executable[MAX_PATH]{};
  GetModuleFileNameW(nullptr, executable, MAX_PATH);
  return std::filesystem::path(executable).parent_path() / "libSkiaSharp.dll";
}

void rectangle(life::skia::Api::Canvas canvas, life::skia::Api::Paint paint,
  float left, float top, float right, float bottom, uint32_t color) {
  api->paintSetColor(paint, color);
  const life::skia::Rect rect{left, top, right, bottom};
  api->canvasDrawRect(canvas, &rect, paint);
}

void render(HWND window, HDC dc) {
  RECT client{}; GetClientRect(window, &client);
  const int width = std::max(1L, client.right), height = std::max(1L, client.bottom);
  if (width != surfaceWidth || height != surfaceHeight) {
    surfaceWidth = width; surfaceHeight = height; pixels.resize(size_t(width) * height);
    bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap.bmiHeader.biWidth = width; bitmap.bmiHeader.biHeight = -height;
    bitmap.bmiHeader.biPlanes = 1; bitmap.bmiHeader.biBitCount = 32; bitmap.bmiHeader.biCompression = BI_RGB;
  }
  const life::skia::ImageInfo info{nullptr, width, height, 6, 2};
  auto surface = api->surfaceNewRasterDirect(&info, pixels.data(), size_t(width) * 4, nullptr, nullptr, nullptr);
  if (!surface) return;
  auto canvas = api->surfaceGetCanvas(surface);
  auto paint = api->paintNew();
  api->canvasClear(canvas, 0xff121d29);
  const float aside = std::min(390.0f, width * .32f);
  rectangle(canvas, paint, 14, 14, width - aside - 12, height - 14, 0xff19243a);
  rectangle(canvas, paint, width - aside, 0, float(width), float(height), 0xffdfe3ee);
  for (int column = 0; column < 2; ++column)
    for (int row = 0; row < 2; ++row) {
      const float cardWidth = (aside - 42) / 2;
      const float left = width - aside + 14 + column * (cardWidth + 10);
      rectangle(canvas, paint, left, 16 + row * 82, left + cardWidth, 88 + row * 82, 0xfff6f7fb);
    }
  rectangle(canvas, paint, width - aside + 14, 190, width - 14, 455, 0xfff6f7fb);
  rectangle(canvas, paint, width - aside + 14, 470, width - 14, height - 14, 0xfff6f7fb);
  api->paintDelete(paint); api->surfaceUnref(surface);
  StretchDIBits(dc, 0, 0, width, height, 0, 0, width, height, pixels.data(), &bitmap, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_PAINT) { PAINTSTRUCT paint{}; HDC dc = BeginPaint(window, &paint); render(window, dc); EndPaint(window, &paint); return 0; }
  if (message == WM_SIZE) { InvalidateRect(window, nullptr, FALSE); return 0; }
  if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
  return DefWindowProcW(window, message, wParam, lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show) {
  try { api = std::make_unique<life::skia::Api>(libraryPath().c_str()); }
  catch (...) { MessageBoxW(nullptr, L"libSkiaSharp.dll is missing or incompatible.", L"Life Engine", MB_ICONERROR); return 1; }
  WNDCLASSW type{}; type.lpfnWndProc = procedure; type.hInstance = instance;
  type.lpszClassName = L"LifeEngineSkiaPreview"; type.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  RegisterClassW(&type);
  HWND window = CreateWindowExW(0, type.lpszClassName, L"Life Engine NNUE — Native Skia Preview",
    WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1500, 900, nullptr, nullptr, instance, nullptr);
  ShowWindow(window, show);
  MSG message{}; while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
  api.reset(); return int(message.wParam);
}
