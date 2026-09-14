#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace life::skia {
struct ImageInfo { void* colorSpace; int32_t width, height, colorType, alphaType; };
struct Rect { float left, top, right, bottom; };
struct RunBuffer { void* glyphs; void* positions; void* utf8Text; void* clusters; };

class Api {
  HMODULE library{};
  template<class T> T symbol(const char* name) {
    auto address = GetProcAddress(library, name);
    if (!address) throw std::runtime_error(name);
    return reinterpret_cast<T>(address);
  }
public:
  using Surface = void*; using Canvas = void*; using Paint = void*; using FontManager=void*;using FontStyle=void*;using Typeface=void*;using Font=void*;using TextBlobBuilder=void*;using TextBlob=void*;
  Surface (__cdecl *surfaceNewRasterDirect)(const ImageInfo*, void*, size_t, void*, void*, void*);
  Canvas (__cdecl *surfaceGetCanvas)(Surface);
  void (__cdecl *surfaceUnref)(Surface);
  void (__cdecl *canvasClear)(Canvas, uint32_t);
  void (__cdecl *canvasDrawRect)(Canvas, const Rect*, Paint);
  Paint (__cdecl *paintNew)();
  void (__cdecl *paintDelete)(Paint);
  void (__cdecl *paintSetColor)(Paint, uint32_t);
  void (__cdecl *paintSetAntialias)(Paint, bool);
  FontManager (__cdecl *fontManagerCreateDefault)();
  void (__cdecl *fontManagerUnref)(FontManager);
  FontStyle (__cdecl *fontStyleNew)(int,int,int);
  void (__cdecl *fontStyleDelete)(FontStyle);
  Typeface (__cdecl *fontManagerCreateTypeface)(FontManager,const char*,FontStyle);
  void (__cdecl *typefaceUnref)(Typeface);
  Font (__cdecl *fontNew)(Typeface,float,float,float);
  void (__cdecl *fontDelete)(Font);
  int (__cdecl *fontTextToGlyphs)(Font,const void*,size_t,int,uint16_t*,int);
  TextBlobBuilder (__cdecl *textBlobBuilderNew)();
  void (__cdecl *textBlobBuilderDelete)(TextBlobBuilder);
  void (__cdecl *textBlobBuilderAllocRun)(TextBlobBuilder,Font,int,float,float,const Rect*,RunBuffer*);
  TextBlob (__cdecl *textBlobBuilderMake)(TextBlobBuilder);
  void (__cdecl *textBlobUnref)(TextBlob);
  void (__cdecl *canvasDrawTextBlob)(Canvas,TextBlob,float,float,Paint);

  explicit Api(const wchar_t* path) {
    library = LoadLibraryW(path);
    if (!library) throw std::runtime_error("libSkiaSharp.dll");
    surfaceNewRasterDirect = symbol<decltype(surfaceNewRasterDirect)>("sk_surface_new_raster_direct");
    surfaceGetCanvas = symbol<decltype(surfaceGetCanvas)>("sk_surface_get_canvas");
    surfaceUnref = symbol<decltype(surfaceUnref)>("sk_surface_unref");
    canvasClear = symbol<decltype(canvasClear)>("sk_canvas_clear");
    canvasDrawRect = symbol<decltype(canvasDrawRect)>("sk_canvas_draw_rect");
    paintNew = symbol<decltype(paintNew)>("sk_paint_new");
    paintDelete = symbol<decltype(paintDelete)>("sk_paint_delete");
    paintSetColor = symbol<decltype(paintSetColor)>("sk_paint_set_color");
    paintSetAntialias=symbol<decltype(paintSetAntialias)>("sk_paint_set_antialias");
    fontManagerCreateDefault=symbol<decltype(fontManagerCreateDefault)>("sk_fontmgr_create_default");fontManagerUnref=symbol<decltype(fontManagerUnref)>("sk_fontmgr_unref");fontStyleNew=symbol<decltype(fontStyleNew)>("sk_fontstyle_new");fontStyleDelete=symbol<decltype(fontStyleDelete)>("sk_fontstyle_delete");fontManagerCreateTypeface=symbol<decltype(fontManagerCreateTypeface)>("sk_fontmgr_legacy_create_typeface");typefaceUnref=symbol<decltype(typefaceUnref)>("sk_typeface_unref");fontNew=symbol<decltype(fontNew)>("sk_font_new_with_values");fontDelete=symbol<decltype(fontDelete)>("sk_font_delete");fontTextToGlyphs=symbol<decltype(fontTextToGlyphs)>("sk_font_text_to_glyphs");textBlobBuilderNew=symbol<decltype(textBlobBuilderNew)>("sk_textblob_builder_new");textBlobBuilderDelete=symbol<decltype(textBlobBuilderDelete)>("sk_textblob_builder_delete");textBlobBuilderAllocRun=symbol<decltype(textBlobBuilderAllocRun)>("sk_textblob_builder_alloc_run");textBlobBuilderMake=symbol<decltype(textBlobBuilderMake)>("sk_textblob_builder_make");textBlobUnref=symbol<decltype(textBlobUnref)>("sk_textblob_unref");canvasDrawTextBlob=symbol<decltype(canvasDrawTextBlob)>("sk_canvas_draw_text_blob");
  }
  ~Api() { if (library) FreeLibrary(library); }
  Api(const Api&) = delete; Api& operator=(const Api&) = delete;
};
}
