#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace life::skia {
struct ImageInfo { void* colorSpace; int32_t width, height, colorType, alphaType; };
struct Rect { float left, top, right, bottom; };

class Api {
  HMODULE library{};
  template<class T> T symbol(const char* name) {
    auto address = GetProcAddress(library, name);
    if (!address) throw std::runtime_error(name);
    return reinterpret_cast<T>(address);
  }
public:
  using Surface = void*; using Canvas = void*; using Paint = void*;
  Surface (__cdecl *surfaceNewRasterDirect)(const ImageInfo*, void*, size_t, void*, void*, void*);
  Canvas (__cdecl *surfaceGetCanvas)(Surface);
  void (__cdecl *surfaceFlush)(Surface);
  void (__cdecl *surfaceUnref)(Surface);
  void (__cdecl *canvasClear)(Canvas, uint32_t);
  void (__cdecl *canvasDrawRect)(Canvas, const Rect*, Paint);
  Paint (__cdecl *paintNew)();
  void (__cdecl *paintDelete)(Paint);
  void (__cdecl *paintSetColor)(Paint, uint32_t);

  explicit Api(const wchar_t* path) {
    library = LoadLibraryW(path);
    if (!library) throw std::runtime_error("libSkiaSharp.dll");
    surfaceNewRasterDirect = symbol<decltype(surfaceNewRasterDirect)>("sk_surface_new_raster_direct");
    surfaceGetCanvas = symbol<decltype(surfaceGetCanvas)>("sk_surface_get_canvas");
    surfaceFlush = symbol<decltype(surfaceFlush)>("sk_surface_flush");
    surfaceUnref = symbol<decltype(surfaceUnref)>("sk_surface_unref");
    canvasClear = symbol<decltype(canvasClear)>("sk_canvas_clear");
    canvasDrawRect = symbol<decltype(canvasDrawRect)>("sk_canvas_draw_rect");
    paintNew = symbol<decltype(paintNew)>("sk_paint_new");
    paintDelete = symbol<decltype(paintDelete)>("sk_paint_delete");
    paintSetColor = symbol<decltype(paintSetColor)>("sk_paint_set_color");
  }
  ~Api() { if (library) FreeLibrary(library); }
  Api(const Api&) = delete; Api& operator=(const Api&) = delete;
};
}
