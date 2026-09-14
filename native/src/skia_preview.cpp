#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include "reference.hpp"
#include "simulation.hpp"
#include "skia_api.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

namespace {
std::unique_ptr<life::skia::Api> api;
std::vector<uint32_t> pixels;
BITMAPINFO bitmap{};
int surfaceWidth{}, surfaceHeight{};
life::skia::Api::FontManager fontManager{};
life::skia::Api::Typeface regularTypeface{},boldTypeface{};
life::NativeSimulation simulation(life::WorldWidth, life::WorldHeight, .2, 500, 1, 1);
float zoom = 1.0f, cameraX = 0.0f, cameraY = 0.0f;
bool dragging = false;
POINT dragOrigin{};
float dragCameraX{}, dragCameraY{};

constexpr uint32_t terrainColors[] = {0xff2e4330, 0xff2b5b36, 0xff6c5232, 0xff1a3e5f, 0xff4d525a};
constexpr uint32_t resourceColors[] = {0, 0xff18b437, 0xff8b4b32, 0xffd7bd52};
constexpr uint32_t cellColors[] = {0,0xff008000,0xff808080,0xffffa500,0xffffffff,0xff3493eb,0xffff0000,0xff800080,0xffff9f43,0xffd67c4a,0xffd6c36a,0xff59636f};

std::filesystem::path libraryPath() {
  wchar_t executable[MAX_PATH]{};
  GetModuleFileNameW(nullptr, executable, MAX_PATH);
  return std::filesystem::path(executable).parent_path() / "libSkiaSharp.dll";
}

constexpr int SkiaResourceId = 101;

std::filesystem::path embeddedLibraryPath() {
  wchar_t temporaryDirectory[MAX_PATH]{};
  const DWORD length = GetTempPathW(MAX_PATH, temporaryDirectory);
  if (length == 0 || length >= MAX_PATH) throw std::runtime_error("temporary directory");
  const auto destination = std::filesystem::path(temporaryDirectory) / "LifeEngine-SkiaSharp-4.152.0-x64.dll";
  HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(SkiaResourceId), MAKEINTRESOURCEW(10));
  if (!resource) throw std::runtime_error("embedded Skia resource");
  HGLOBAL loaded = LoadResource(nullptr, resource);
  const DWORD resourceSize = SizeofResource(nullptr, resource);
  const void* bytes = loaded ? LockResource(loaded) : nullptr;
  if (!bytes || resourceSize == 0) throw std::runtime_error("embedded Skia data");
  if (std::filesystem::exists(destination) && std::filesystem::file_size(destination) == resourceSize)
    return destination;
  const auto staging = destination.wstring() + L".tmp";
  HANDLE file = CreateFileW(staging.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
    FILE_ATTRIBUTE_TEMPORARY, nullptr);
  if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("Skia extraction");
  DWORD written = 0;
  const BOOL ok = WriteFile(file, bytes, resourceSize, &written, nullptr);
  CloseHandle(file);
  if (!ok || written != resourceSize) {
    DeleteFileW(staging.c_str());
    throw std::runtime_error("Skia extraction");
  }
  if (!MoveFileExW(staging.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    DeleteFileW(staging.c_str());
    if (!std::filesystem::exists(destination) || std::filesystem::file_size(destination) != resourceSize)
      throw std::runtime_error("Skia installation");
  }
  return destination;
}

std::unique_ptr<life::skia::Api> loadSkia() {
  try { return std::make_unique<life::skia::Api>(libraryPath().c_str()); }
  catch (...) { return std::make_unique<life::skia::Api>(embeddedLibraryPath().c_str()); }
}

void rectangle(life::skia::Api::Canvas canvas, life::skia::Api::Paint paint,
  float left, float top, float right, float bottom, uint32_t color) {
  api->paintSetColor(paint, color);
  const life::skia::Rect rect{left, top, right, bottom};
  api->canvasDrawRect(canvas, &rect, paint);
}

void text(life::skia::Api::Canvas canvas,life::skia::Api::Paint paint,const std::string& value,float x,float y,float size,uint32_t color,bool bold=false){auto font=api->fontNew(bold?boldTypeface:regularTypeface,size,1,0);const int count=api->fontTextToGlyphs(font,value.data(),value.size(),0,nullptr,0);if(count>0){std::vector<uint16_t> glyphs(count);api->fontTextToGlyphs(font,value.data(),value.size(),0,glyphs.data(),count);auto builder=api->textBlobBuilderNew();life::skia::RunBuffer run{};api->textBlobBuilderAllocRun(builder,font,count,0,0,nullptr,&run);std::memcpy(run.glyphs,glyphs.data(),glyphs.size()*sizeof(uint16_t));auto blob=api->textBlobBuilderMake(builder);api->paintSetColor(paint,color);api->paintSetAntialias(paint,true);api->canvasDrawTextBlob(canvas,blob,x,y,paint);api->textBlobUnref(blob);api->textBlobBuilderDelete(builder);}api->fontDelete(font);}

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
  api->canvasClear(canvas, 0xff3a4b68);
  const float aside = std::min(390.0f, width * .32f),contentRight=width-aside,headerHeight=82;
  rectangle(canvas,paint,0,0,contentRight,headerHeight,0xff416788);rectangle(canvas,paint,0,headerHeight,contentRight,float(height),0xff121d29);rectangle(canvas,paint,contentRight,0,float(width),float(height),0xff3a4b68);
  const int stageLeft=0,stageTop=int(headerHeight),stageRight=std::max(0,int(contentRight)),stageBottom=height;
  for(int screenY=stageTop;screenY<stageBottom;++screenY){const int worldY=int((screenY-stageTop-cameraY)/zoom);if(worldY<0||worldY>=life::WorldHeight)continue;auto* destination=pixels.data()+size_t(screenY)*width;for(int screenX=stageLeft;screenX<stageRight;++screenX){const int worldX=int((screenX-cameraX)/zoom);if(worldX<0||worldX>=life::WorldWidth)continue;const int index=worldY*life::WorldWidth+worldX;const auto resource=simulation.world.resources[index],cell=simulation.cells[index];destination[screenX]=cell?cellColors[cell]:resource?resourceColors[resource]:terrainColors[simulation.world.terrain[index]];}}
  text(canvas,paint,"EVOLUTION, ACCELERATED",30,30,10,0xff81d2c7,true);text(canvas,paint,"Life Engine",30,65,28,0xffffffff,true);text(canvas,paint,"NNUE",176,65,28,0xff81d2c7,true);rectangle(canvas,paint,contentRight-96,37,contentRight-88,45,0xff81d2c7);text(canvas,paint,"RUNNING",contentRight-78,47,12,0xffffffff,false);
  const float panelX=contentRight+15,panelRight=width-15,cardGap=8,cardWidth=(aside-38)/2;
  for (int column = 0; column < 2; ++column)
    for (int row = 0; row < 2; ++row) {
      const float left=panelX+column*(cardWidth+cardGap),top=15+row*76;rectangle(canvas,paint,left,top,left+cardWidth,top+68,0xffe1e3ec);
    }
  const auto metrics=simulation.metrics();const char* seasons[]={"Spring","Summer","Autumn","Winter"};const int season=int(std::floor(life::cyclePhase(metrics.ticks)*4))%4;const std::string values[]={std::to_string(metrics.organisms),"60","60",seasons[season]};const char* labels[]={"ORGANISMS","ACTUAL TPS","RENDER FPS","SEASON"};for(int i=0;i<4;++i){const float left=panelX+(i%2)*(cardWidth+cardGap),top=15+(i/2)*76;text(canvas,paint,labels[i],left+15,top+22,9,0xff416788);text(canvas,paint,values[i],left+15,top+53,26,0xff121d29,true);}
  float top=179;rectangle(canvas,paint,panelX,top,panelRight,top+220,0xffe1e3ec);text(canvas,paint,"Simulation",panelX+20,top+35,16,0xff121d29,true);rectangle(canvas,paint,panelRight-77,top+15,panelRight-20,top+48,0xff9099c2);text(canvas,paint,"Pause",panelRight-65,top+36,12,0xff000000,true);text(canvas,paint,"Requested simulation TPS",panelX+20,top+76,12,0xff3a4b68);text(canvas,paint,"60",panelRight-40,top+76,12,0xff416788);rectangle(canvas,paint,panelX+20,top+95,panelRight-20,top+99,0xff416788);text(canvas,paint,"Overlay",panelX+20,top+130,12,0xff3a4b68);rectangle(canvas,paint,panelX+20,top+145,panelRight-20,top+180,0xffd5d8e4);text(canvas,paint,"Terrain",panelX+30,top+168,12,0xff121d29);
  top=411;rectangle(canvas,paint,panelX,top,panelRight,top+162,0xffe1e3ec);text(canvas,paint,"World tools",panelX+20,top+35,16,0xff121d29,true);const char* tools[]={"Inspect","Pan","Plant","Carrion","Mineral","Fertile","Desert","Water","Mountain"};for(int i=0;i<9;++i){const int row=i/5,column=i%5;const float buttonWidth=(aside-70)/5,left=panelX+20+column*(buttonWidth+5),buttonTop=top+52+row*38;rectangle(canvas,paint,left,buttonTop,left+buttonWidth,buttonTop+31,i==0?0xff81d2c7:0xff9099c2);text(canvas,paint,tools[i],left+5,buttonTop+20,9,0xff111111,true);}
  top=585;rectangle(canvas,paint,panelX,top,panelRight,float(height-15),0xffe1e3ec);text(canvas,paint,"Evolution Observatory",panelX+20,top+35,16,0xff121d29,true);text(canvas,paint,"Select a creature.",panelX+20,top+68,12,0xff66738a);
  rectangle(canvas,paint,24,float(height-52),320,float(height-20),0xee121d29);text(canvas,paint,"Terrain + resources + evolving organisms",38,float(height-31),11,0xffffffff);
  api->paintDelete(paint);api->surfaceUnref(surface);
  StretchDIBits(dc, 0, 0, width, height, 0, 0, width, height, pixels.data(), &bitmap, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_PAINT) { PAINTSTRUCT paint{}; HDC dc = BeginPaint(window, &paint); render(window, dc); EndPaint(window, &paint); return 0; }
  if (message == WM_SIZE) { InvalidateRect(window, nullptr, FALSE); return 0; }
  if (message == WM_TIMER) { simulation.step(); InvalidateRect(window, nullptr, FALSE); return 0; }
  if (message == WM_LBUTTONDOWN || message == WM_MBUTTONDOWN) {
    dragging = true; dragOrigin = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    dragCameraX = cameraX; dragCameraY = cameraY; SetCapture(window); return 0;
  }
  if (message == WM_MOUSEMOVE && dragging) {
    cameraX = dragCameraX + GET_X_LPARAM(lParam) - dragOrigin.x;
    cameraY = dragCameraY + GET_Y_LPARAM(lParam) - dragOrigin.y;
    InvalidateRect(window, nullptr, FALSE); return 0;
  }
  if (message == WM_LBUTTONUP || message == WM_MBUTTONUP) { dragging = false; ReleaseCapture(); return 0; }
  if (message == WM_MOUSEWHEEL) {
    POINT cursor{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}; ScreenToClient(window, &cursor);
    const float previous = zoom; zoom = std::clamp(zoom * (GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 1.2f : 1.0f / 1.2f), .2f, 12.0f);
    cameraX = cursor.x - (cursor.x - cameraX) * zoom / previous;
    cameraY = cursor.y-82 - (cursor.y-82-cameraY) * zoom / previous;
    InvalidateRect(window, nullptr, FALSE); return 0;
  }
  if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
  return DefWindowProcW(window, message, wParam, lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show) {
  try { api = loadSkia(); }
  catch (...) { MessageBoxW(nullptr, L"The embedded Skia renderer could not be loaded.", L"Life Engine", MB_ICONERROR); return 1; }
  fontManager=api->fontManagerCreateDefault();auto normal=api->fontStyleNew(400,5,0),bold=api->fontStyleNew(700,5,0);regularTypeface=api->fontManagerCreateTypeface(fontManager,"Arial",normal);boldTypeface=api->fontManagerCreateTypeface(fontManager,"Arial",bold);api->fontStyleDelete(normal);api->fontStyleDelete(bold);
  WNDCLASSW type{}; type.lpfnWndProc = procedure; type.hInstance = instance;
  type.lpszClassName = L"LifeEngineSkiaPreview"; type.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  RegisterClassW(&type);
  HWND window = CreateWindowExW(0, type.lpszClassName, L"Life Engine NNUE — Native Skia Preview",
    WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1500, 900, nullptr, nullptr, instance, nullptr);
  ShowWindow(window, show);
  SetTimer(window, 1, 16, nullptr);
  MSG message{}; while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
  if(regularTypeface)api->typefaceUnref(regularTypeface);if(boldTypeface)api->typefaceUnref(boldTypeface);if(fontManager)api->fontManagerUnref(fontManager);api.reset(); return int(message.wParam);
}
