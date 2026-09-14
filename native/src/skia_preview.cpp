#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include "reference.hpp"
#include "simulation.hpp"
#include "skia_api.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace {
class RenderWorkers {
  static constexpr int WorkerCount=10;
  std::mutex mutex_;std::condition_variable wake_,finished_;std::vector<std::jthread> threads_;std::function<void(int)> task_;uint64_t generation_{};int remaining_{};
public:
  RenderWorkers(){threads_.reserve(WorkerCount);for(int index=0;index<WorkerCount;++index)threads_.emplace_back([this,index](std::stop_token stop){uint64_t observed=0;for(;;){std::unique_lock lock(mutex_);wake_.wait(lock,[&]{return stop.stop_requested()||generation_!=observed;});if(stop.stop_requested())return;observed=generation_;auto task=task_;lock.unlock();task(index);lock.lock();if(--remaining_==0)finished_.notify_one();}});}
  ~RenderWorkers(){for(auto& thread:threads_)thread.request_stop();wake_.notify_all();}
  void run(const std::function<void(int)>& task){{std::lock_guard lock(mutex_);task_=task;remaining_=WorkerCount;++generation_;}wake_.notify_all();task(WorkerCount);std::unique_lock lock(mutex_);finished_.wait(lock,[&]{return remaining_==0;});task_={};}
};

std::unique_ptr<life::skia::Api> api;
std::unique_ptr<RenderWorkers> renderWorkers;
std::vector<uint32_t> pixels;
std::vector<uint8_t> renderCells,renderTerrain,renderResources;
BITMAPINFO bitmap{};
int surfaceWidth{}, surfaceHeight{};
life::skia::Api::FontManager fontManager{};
life::skia::Api::Typeface regularTypeface{},boldTypeface{};
life::NativeSimulation simulation(life::WorldWidth, life::WorldHeight, .2, 500, 1, 1);
float zoom = 1.0f, cameraX = 0.0f, cameraY = 0.0f;
bool dragging = false;
POINT dragOrigin{};
float dragCameraX{}, dragCameraY{};
enum class Tool { Inspect, Pan, Plant, Carrion, Mineral, Fertile, Desert, Water, Mountain };
enum class Overlay { Terrain, Resources, Productivity, Climate };
Tool activeTool = Tool::Inspect;
Overlay activeOverlay = Overlay::Terrain;
uint32_t nextWorldSeed = 2;
std::atomic_bool paused = false;
std::atomic_int requestedTps = 60;
int selectedId = -1;
double measuredTps = 0.0, measuredFps = 0.0;
std::atomic_uint64_t measuredTicks = 0;
uint64_t measuredFrames = 0;
auto measurementStart = std::chrono::steady_clock::now();
auto lastRenderRequest = measurementStart;
std::shared_mutex simulationMutex;
std::jthread simulationThread;
bool uiBenchmark=false;
int uiBenchmarkSeconds=60;
std::atomic_uint64_t uiBenchmarkTicks=0;
uint64_t uiBenchmarkFrames=0,uiBenchmarkInputs=0;
double uiBenchmarkRenderMs=0;
auto uiBenchmarkStart=measurementStart;
auto nextBenchmarkInput=measurementStart;
std::optional<std::chrono::steady_clock::time_point> pendingInput;
std::vector<double> inputLatenciesMs;

constexpr uint32_t terrainColors[] = {0xff2e4330, 0xff2b5b36, 0xff6c5232, 0xff1a3e5f, 0xff4d525a};
constexpr uint32_t resourceColors[] = {0, 0xff18b437, 0xff8b4b32, 0xffd7bd52};
constexpr uint32_t cellColors[] = {0,0xff008000,0xff808080,0xffffa500,0xffffffff,0xff3493eb,0xffff0000,0xff800080,0xffff9f43,0xffd67c4a,0xffd6c36a,0xff59636f};

std::filesystem::path libraryPath() {
  wchar_t executable[MAX_PATH]{};
  GetModuleFileNameW(nullptr, executable, MAX_PATH);
  return std::filesystem::path(executable).parent_path() / "libSkiaSharp.dll";
}

void writeUiBenchmarkReport(){const auto now=std::chrono::steady_clock::now();const double elapsed=std::chrono::duration<double>(now-uiBenchmarkStart).count();std::sort(inputLatenciesMs.begin(),inputLatenciesMs.end());const size_t p95Index=inputLatenciesMs.empty()?0:std::min(inputLatenciesMs.size()-1,size_t(std::ceil(inputLatenciesMs.size()*.95))-1);const double p95=inputLatenciesMs.empty()?0:inputLatenciesMs[p95Index];size_t organisms=0;{std::shared_lock lock(simulationMutex);organisms=simulation.metrics().organisms;}const double tps=uiBenchmarkTicks.load()/elapsed,fps=uiBenchmarkFrames/elapsed,renderMs=uiBenchmarkFrames?uiBenchmarkRenderMs/uiBenchmarkFrames:0;const bool passed=organisms==10'000&&tps>=60&&fps>=45&&p95<100;std::ostringstream report;report<<std::fixed<<std::setprecision(2)<<"threads=12 organisms="<<organisms<<" elapsed_s="<<elapsed<<" measured_tps="<<tps<<" render_fps="<<fps<<" render_ms="<<renderMs<<" input_p95_ms="<<p95<<" interactions="<<inputLatenciesMs.size()<<" result="<<(passed?"PASS":"FAIL")<<"\r\n";const auto path=libraryPath().parent_path()/"ui-benchmark.txt";HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(file!=INVALID_HANDLE_VALUE){const auto value=report.str();DWORD written=0;WriteFile(file,value.data(),DWORD(value.size()),&written,nullptr);CloseHandle(file);}}

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

std::string lineageIds(const std::vector<life::LineageSummary>& records,size_t limit=5){std::ostringstream value;if(records.empty())return "None";for(size_t index=0;index<std::min(limit,records.size());++index){if(index)value<<", ";value<<'#'<<records[index].id<<(records[index].alive?"":"†");}if(records.size()>limit)value<<" +"<<(records.size()-limit);return value.str();}
std::string featureIds(const std::vector<uint16_t>& features,size_t limit=9){std::ostringstream value;if(features.empty())return "None";for(size_t index=0;index<std::min(limit,features.size());++index){if(index)value<<", ";value<<features[index];}if(features.size()>limit)value<<" +"<<(features.size()-limit);return value.str();}

std::pair<int,int> screenToWorld(int screenX, int screenY) {
  return {int((screenX-cameraX)/zoom), int((screenY-82-cameraY)/zoom)};
}

void applyTool(int screenX, int screenY) {
  const auto [x,y] = screenToWorld(screenX, screenY);
  if (x < 0 || y < 0 || x >= life::WorldWidth || y >= life::WorldHeight) return;
  std::unique_lock lock(simulationMutex);
  switch (activeTool) {
    case Tool::Inspect: { const auto* organism=simulation.organismAt(x,y); selectedId=organism?organism->id:-1;simulation.select(selectedId);break; }
    case Tool::Plant: simulation.paintResource(x,y,life::ResourceType::Plant); break;
    case Tool::Carrion: simulation.paintResource(x,y,life::ResourceType::Carrion); break;
    case Tool::Mineral: simulation.paintResource(x,y,life::ResourceType::Mineral); break;
    case Tool::Fertile: simulation.paintTerrain(x,y,life::TerrainType::Fertile); break;
    case Tool::Desert: simulation.paintTerrain(x,y,life::TerrainType::Desert); break;
    case Tool::Water: simulation.paintTerrain(x,y,life::TerrainType::Water); break;
    case Tool::Mountain: simulation.paintTerrain(x,y,life::TerrainType::Mountain); break;
    case Tool::Pan: break;
  }
}

void render(HWND window, HDC dc) {
  const auto renderStarted=std::chrono::steady_clock::now();
  life::NativeMetrics metrics;std::optional<life::OrganismInspection> inspection;
  {std::shared_lock simulationLock(simulationMutex);renderCells=simulation.cells;renderTerrain=simulation.world.terrain;renderResources=simulation.world.resources;metrics=simulation.metrics();inspection=simulation.inspect(selectedId);}
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
  const int renderStride=zoom<=1.25f?2:1;
  constexpr int rasterPartitions=11;const auto rasterRows=[&](int partition){for(int screenY=stageTop+partition*renderStride;screenY<stageBottom;screenY+=renderStride*rasterPartitions){const int worldY=int((screenY-stageTop-cameraY)/zoom);if(worldY<0||worldY>=life::WorldHeight)continue;const auto climate=life::climateAt(worldY,life::WorldHeight,metrics.ticks);for(int screenX=stageLeft;screenX<stageRight;screenX+=renderStride){const int worldX=int((screenX-cameraX)/zoom);if(worldX<0||worldX>=life::WorldWidth)continue;const int index=worldY*life::WorldWidth+worldX;const auto terrain=renderTerrain[index],resource=renderResources[index];uint8_t cell=renderCells[index];if(renderStride>1&&!cell){const int nextX=std::min(worldX+1,life::WorldWidth-1),nextY=std::min(worldY+1,life::WorldHeight-1);cell=std::max({renderCells[worldY*life::WorldWidth+nextX],renderCells[nextY*life::WorldWidth+worldX],renderCells[nextY*life::WorldWidth+nextX]});}uint32_t base=terrainColors[terrain];if(activeOverlay==Overlay::Resources)base=resource?resourceColors[resource]:0xff202d3b;else if(activeOverlay==Overlay::Productivity){const double terrainFactor=terrain==uint8_t(life::TerrainType::Fertile)?1.35:terrain==uint8_t(life::TerrainType::Desert)?.3:terrain==uint8_t(life::TerrainType::Plains)?1.0:0.0;const int green=std::clamp(int(45+145*climate.fertility*terrainFactor/1.82),35,190);base=0xff202020u|uint32_t(green)<<8;}else if(activeOverlay==Overlay::Climate){const int red=int(45+190*climate.temperature),blue=int(45+190*(1-climate.temperature));base=0xff000000u|uint32_t(red)<<16|0x00282800u|uint32_t(blue);}const uint32_t color=cell?cellColors[cell]:(activeOverlay==Overlay::Resources&&resource)?resourceColors[resource]:base;for(int fillY=screenY;fillY<std::min(screenY+renderStride,stageBottom);++fillY){auto* destination=pixels.data()+size_t(fillY)*width;std::fill(destination+screenX,destination+std::min(screenX+renderStride,stageRight),color);}}}};renderWorkers->run(rasterRows);
  text(canvas,paint,"EVOLUTION, ACCELERATED",30,30,10,0xff81d2c7,true);text(canvas,paint,"Life Engine",30,65,28,0xffffffff,true);text(canvas,paint,"NNUE",176,65,28,0xff81d2c7,true);rectangle(canvas,paint,contentRight-96,37,contentRight-88,45,paused.load()?0xff9099c2:0xff81d2c7);text(canvas,paint,paused.load()?"PAUSED":"RUNNING",contentRight-78,47,12,0xffffffff,false);
  const float panelX=contentRight+15,panelRight=width-15,cardGap=8,cardWidth=(aside-38)/2;
  for (int column = 0; column < 2; ++column)
    for (int row = 0; row < 2; ++row) {
      const float left=panelX+column*(cardWidth+cardGap),top=15+row*76;rectangle(canvas,paint,left,top,left+cardWidth,top+68,0xffe1e3ec);
    }
  const char* seasons[]={"Spring","Summer","Autumn","Winter"};const int season=int(std::floor(life::cyclePhase(metrics.ticks)*4))%4;std::ostringstream tpsText,fpsText;tpsText<<std::fixed<<std::setprecision(1)<<measuredTps;fpsText<<std::fixed<<std::setprecision(1)<<measuredFps;const std::string values[]={std::to_string(metrics.organisms),tpsText.str(),fpsText.str(),seasons[season]};const char* labels[]={"ORGANISMS","ACTUAL TPS","RENDER FPS","SEASON"};for(int i=0;i<4;++i){const float left=panelX+(i%2)*(cardWidth+cardGap),top=15+(i/2)*76;text(canvas,paint,labels[i],left+15,top+22,9,0xff416788);text(canvas,paint,values[i],left+15,top+53,26,0xff121d29,true);}
  const bool isPaused=paused.load();const int targetTps=requestedTps.load();float top=179;rectangle(canvas,paint,panelX,top,panelRight,top+220,0xffe1e3ec);text(canvas,paint,"Simulation",panelX+20,top+35,16,0xff121d29,true);rectangle(canvas,paint,panelRight-77,top+15,panelRight-20,top+48,0xff9099c2);text(canvas,paint,isPaused?"Resume":"Pause",panelRight-(isPaused?69:65),top+36,12,0xff000000,true);text(canvas,paint,"Requested simulation TPS",panelX+20,top+76,12,0xff3a4b68);text(canvas,paint,std::to_string(targetTps),panelRight-43,top+76,12,0xff416788);rectangle(canvas,paint,panelX+20,top+95,panelRight-20,top+99,0xff416788);const float thumbX=panelX+20+(panelRight-panelX-40)*(targetTps-1)/119.0f;rectangle(canvas,paint,thumbX-4,top+90,thumbX+4,top+104,0xff81d2c7);constexpr const char* overlayNames[]={"Terrain","Resources","Productivity","Climate"};text(canvas,paint,"Overlay",panelX+20,top+123,11,0xff3a4b68);rectangle(canvas,paint,panelX+82,top+108,panelRight-20,top+136,0xffd5d8e4);text(canvas,paint,overlayNames[int(activeOverlay)],panelX+92,top+127,11,0xff121d29);const float half=(panelRight-panelX-45)/2;rectangle(canvas,paint,panelX+20,top+151,panelX+20+half,top+186,0xff9099c2);rectangle(canvas,paint,panelX+25+half,top+151,panelRight-20,top+186,0xff9099c2);text(canvas,paint,"Reset life",panelX+34,top+173,10,0xff111111,true);text(canvas,paint,"New seeded world",panelX+34+half,top+173,9,0xff111111,true);
  top=411;rectangle(canvas,paint,panelX,top,panelRight,top+162,0xffe1e3ec);text(canvas,paint,"World tools",panelX+20,top+35,16,0xff121d29,true);const char* tools[]={"Inspect","Pan","Plant","Carrion","Mineral","Fertile","Desert","Water","Mountain"};for(int i=0;i<9;++i){const int row=i/5,column=i%5;const float buttonWidth=(aside-70)/5,left=panelX+20+column*(buttonWidth+5),buttonTop=top+52+row*38;rectangle(canvas,paint,left,buttonTop,left+buttonWidth,buttonTop+31,i==int(activeTool)?0xff81d2c7:0xff9099c2);text(canvas,paint,tools[i],left+5,buttonTop+20,9,0xff111111,true);}
  top=585;rectangle(canvas,paint,panelX,top,panelRight,float(height-15),0xffe1e3ec);text(canvas,paint,"Evolution Observatory",panelX+20,top+35,16,0xff121d29,true);rectangle(canvas,paint,panelRight-62,top+13,panelRight-20,top+42,0xff9099c2);text(canvas,paint,"Clear",panelRight-56,top+32,9,0xff111111,true);if(inspection){std::ostringstream identity,energy,traits,brain,place,neural,mutation;identity<<"#"<<inspection->id<<" · "<<inspection->diet<<" · "<<(inspection->alive?"Alive":"Dead");energy<<"Energy "<<std::fixed<<std::setprecision(1)<<inspection->energy/1000.0<<"  Minerals "<<inspection->minerals<<"  Age "<<inspection->age;traits<<"Body "<<inspection->cellCount<<"  Damage "<<inspection->damage<<"  Stress "<<std::setprecision(1)<<inspection->thermalStress*100<<"%";brain<<"Radius "<<inspection->perceptionRadius<<"  Channels 0x"<<std::hex<<int(inspection->senseChannels)<<std::dec<<"  Cost "<<inspection->neuralCost;place<<inspection->terrain<<" · "<<inspection->resource<<"  Temp "<<std::fixed<<std::setprecision(2)<<inspection->temperature<<"  Fertility "<<inspection->fertility;neural<<"Action "<<inspection->action;if(inspection->hasOutputs){neural<<" [";for(int i=0;i<life::OutputSize;++i){if(i)neural<<", ";neural<<std::setprecision(2)<<inspection->outputs[i];}neural<<"]";}mutation<<"Mutations "<<inspection->mutations.size()<<": "<<(inspection->mutations.empty()?"None":inspection->mutations.back());const std::array<std::string,10> lines{identity.str(),energy.str(),traits.str(),brain.str(),place.str(),"Ancestors: "+lineageIds(inspection->ancestors),"Descendants: "+lineageIds(inspection->descendants),"Senses: "+featureIds(inspection->senses),neural.str(),mutation.str()};for(size_t line=0;line<lines.size();++line)text(canvas,paint,lines[line],panelX+20,top+62+float(line)*20,line==0?12:9,line==0?0xff121d29:0xff3a4b68,line==0);}else{text(canvas,paint,"Select a creature.",panelX+20,top+68,12,0xff66738a);}
  std::ostringstream populationSummary,performanceSummary;populationSummary<<"Plant "<<metrics.plantEaters<<" · Carrion "<<metrics.scavengers<<" · Mineral "<<metrics.mineralEaters<<" · Predators "<<metrics.predators;performanceSummary<<"Energy "<<std::fixed<<std::setprecision(1)<<metrics.averageEnergy<<" · Mutation "<<metrics.averageMutation<<"% · Stress "<<metrics.averageStress*100<<"% · "<<std::setprecision(1)<<metrics.memoryEstimate/1048576.0<<" MB · 12 threads";rectangle(canvas,paint,panelX,float(height-61),panelRight,float(height-15),0xff121d29);text(canvas,paint,populationSummary.str(),panelX+10,float(height-42),9,0xffffffff);text(canvas,paint,performanceSummary.str(),panelX+10,float(height-24),9,0xff81d2c7);
  rectangle(canvas,paint,24,float(height-52),320,float(height-20),0xee121d29);text(canvas,paint,"Terrain + resources + evolving organisms",38,float(height-31),11,0xffffffff);
  api->paintDelete(paint);api->surfaceUnref(surface);
  StretchDIBits(dc, 0, 0, width, height, 0, 0, width, height, pixels.data(), &bitmap, DIB_RGB_COLORS, SRCCOPY);
  ++measuredFrames;if(uiBenchmark){++uiBenchmarkFrames;uiBenchmarkRenderMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-renderStarted).count();if(pendingInput){inputLatenciesMs.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-*pendingInput).count());pendingInput.reset();}}
}

LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_PAINT) { PAINTSTRUCT paint{}; HDC dc = BeginPaint(window, &paint); render(window, dc); EndPaint(window, &paint); return 0; }
  if (message == WM_SIZE) { InvalidateRect(window, nullptr, FALSE); return 0; }
  if (message == WM_TIMER) {
    const auto now=std::chrono::steady_clock::now();
    if(uiBenchmark&&now>=nextBenchmarkInput){const auto inputStart=now;{std::unique_lock lock(simulationMutex);const int x=int((uiBenchmarkInputs*37)%life::WorldWidth),y=int((uiBenchmarkInputs*53)%life::WorldHeight);simulation.paintResource(x,y,life::ResourceType::Plant,1000);}++uiBenchmarkInputs;pendingInput=inputStart;nextBenchmarkInput=now+std::chrono::milliseconds(250);InvalidateRect(window,nullptr,FALSE);}
    const double sampleSeconds=std::chrono::duration<double>(now-measurementStart).count();if(sampleSeconds>=1.0){measuredTps=measuredTicks.exchange(0)/sampleSeconds;measuredFps=measuredFrames/sampleSeconds;measuredFrames=0;measurementStart=now;}
    InvalidateRect(window,nullptr,FALSE);lastRenderRequest=now;if(uiBenchmark&&std::chrono::duration<double>(now-uiBenchmarkStart).count()>=uiBenchmarkSeconds){writeUiBenchmarkReport();DestroyWindow(window);}return 0;
  }
  if (message == WM_LBUTTONDOWN || message == WM_MBUTTONDOWN) {
    const int mouseX=GET_X_LPARAM(lParam),mouseY=GET_Y_LPARAM(lParam);RECT client{};GetClientRect(window,&client);const float aside=std::min(390.0f,client.right*.32f),contentRight=client.right-aside,panelX=contentRight+15,panelRight=client.right-15;
    if(message==WM_LBUTTONDOWN&&mouseX>=contentRight){
      if(mouseY>=194&&mouseY<=227&&mouseX>=panelRight-77&&mouseX<=panelRight-20){paused.store(!paused.load());}
      else if(mouseY>=269&&mouseY<=288&&mouseX>=panelX+20&&mouseX<=panelRight-20){requestedTps.store(std::clamp(1+int((mouseX-(panelX+20))*119/std::max(1.0f,panelRight-panelX-40)),1,120));}
      else if(mouseY>=287&&mouseY<=315&&mouseX>=panelX+82&&mouseX<=panelRight-20){activeOverlay=Overlay((int(activeOverlay)+1)%4);}
      else if(mouseY>=330&&mouseY<=365&&mouseX>=panelX+20&&mouseX<=panelRight-20){std::unique_lock lock(simulationMutex);const float half=(panelRight-panelX-45)/2;if(mouseX<=panelX+20+half)simulation.reset();else simulation.regenerate(nextWorldSeed++);selectedId=-1;simulation.select(-1);}
      else if(mouseY>=463&&mouseY<=570){const float buttonWidth=(aside-70)/5;const int row=(mouseY-463)/38,column=int((mouseX-(panelX+20))/(buttonWidth+5));const int tool=row*5+column;if(column>=0&&column<5&&tool>=0&&tool<9)activeTool=Tool(tool);}
      else if(mouseY>=598&&mouseY<=627&&mouseX>=panelRight-62&&mouseX<=panelRight-20){std::unique_lock lock(simulationMutex);selectedId=-1;simulation.select(-1);}
      InvalidateRect(window,nullptr,FALSE);return 0;
    }
    if(mouseY>=82&&mouseX<contentRight){if(message==WM_MBUTTONDOWN||activeTool==Tool::Pan){dragging=true;dragOrigin={mouseX,mouseY};dragCameraX=cameraX;dragCameraY=cameraY;SetCapture(window);}else applyTool(mouseX,mouseY);InvalidateRect(window,nullptr,FALSE);}return 0;
  }
  if (message == WM_MOUSEMOVE && dragging) {
    cameraX = dragCameraX + GET_X_LPARAM(lParam) - dragOrigin.x;
    cameraY = dragCameraY + GET_Y_LPARAM(lParam) - dragOrigin.y;
    InvalidateRect(window, nullptr, FALSE); return 0;
  }
  if(message==WM_MOUSEMOVE&&(wParam&MK_LBUTTON)&&!dragging&&activeTool!=Tool::Inspect&&activeTool!=Tool::Pan){applyTool(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam));InvalidateRect(window,nullptr,FALSE);return 0;}
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

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR commandLine, int show) {
  const std::wstring arguments=commandLine?commandLine:L"";if(const auto marker=arguments.find(L"--ui-benchmark");marker!=std::wstring::npos){uiBenchmark=true;const auto equals=arguments.find(L'=',marker);if(equals!=std::wstring::npos)uiBenchmarkSeconds=std::clamp(_wtoi(arguments.c_str()+equals+1),1,600);simulation.seedBenchmarkMovers(10'000);requestedTps.store(60);}
  try { api = loadSkia(); }
  catch (...) { MessageBoxW(nullptr, L"The embedded Skia renderer could not be loaded.", L"Life Engine", MB_ICONERROR); return 1; }
  fontManager=api->fontManagerCreateDefault();auto normal=api->fontStyleNew(400,5,0),bold=api->fontStyleNew(700,5,0);regularTypeface=api->fontManagerCreateTypeface(fontManager,"Arial",normal);boldTypeface=api->fontManagerCreateTypeface(fontManager,"Arial",bold);api->fontStyleDelete(normal);api->fontStyleDelete(bold);
  WNDCLASSW type{}; type.lpfnWndProc = procedure; type.hInstance = instance;
  type.lpszClassName = L"LifeEngineSkiaPreview"; type.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  RegisterClassW(&type);
  HWND window = CreateWindowExW(0, type.lpszClassName, L"Life Engine NNUE",
    WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1500, 900, nullptr, nullptr, instance, nullptr);
  renderWorkers=std::make_unique<RenderWorkers>();
  ShowWindow(window, show);
  timeBeginPeriod(1);
  if(uiBenchmark){uiBenchmarkStart=nextBenchmarkInput=measurementStart=lastRenderRequest=std::chrono::steady_clock::now();uiBenchmarkTicks.store(0);uiBenchmarkFrames=uiBenchmarkInputs=measuredFrames=0;uiBenchmarkRenderMs=0;inputLatenciesMs.clear();pendingInput.reset();}
  simulationThread=std::jthread([](std::stop_token stop){auto next=std::chrono::steady_clock::now();while(!stop.stop_requested()){if(paused.load()){next=std::chrono::steady_clock::now();std::this_thread::sleep_for(std::chrono::milliseconds(2));continue;}const int target=std::max(1,requestedTps.load());const auto interval=std::chrono::duration<double>(1.0/target);const auto now=std::chrono::steady_clock::now();if(now<next){std::this_thread::sleep_until(next);continue;}{std::unique_lock lock(simulationMutex);simulation.step();}measuredTicks.fetch_add(1);if(uiBenchmark)uiBenchmarkTicks.fetch_add(1);next+=std::chrono::duration_cast<std::chrono::steady_clock::duration>(interval);if(std::chrono::steady_clock::now()-next>std::chrono::milliseconds(250))next=std::chrono::steady_clock::now();}});
  SetTimer(window, 1, 8, nullptr);
  MSG message{}; while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
  simulationThread.request_stop();simulationThread.join();renderWorkers.reset();timeEndPeriod(1);
  if(regularTypeface)api->typefaceUnref(regularTypeface);if(boldTypeface)api->typefaceUnref(boldTypeface);if(fontManager)api->fontManagerUnref(fontManager);api.reset(); return int(message.wParam);
}
