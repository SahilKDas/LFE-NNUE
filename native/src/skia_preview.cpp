#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <commdlg.h>
#include "app_support.hpp"
#include "reference.hpp"
#include "simulation.hpp"
#include "skia_api.hpp"
#include "version.hpp"
#include "raster_export.hpp"
#include <fstream>
#include <stdexcept>
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
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
class RenderWorkers {
  static constexpr int WorkerCount=4;
  std::mutex mutex_;std::condition_variable wake_,finished_;std::vector<std::jthread> threads_;std::function<void(int)> task_;uint64_t generation_{};int remaining_{};
public:
  RenderWorkers(){threads_.reserve(WorkerCount);for(int index=0;index<WorkerCount;++index)threads_.emplace_back([this,index](std::stop_token stop){uint64_t observed=0;for(;;){std::unique_lock lock(mutex_);wake_.wait(lock,[&]{return stop.stop_requested()||generation_!=observed;});if(stop.stop_requested())return;observed=generation_;auto task=task_;lock.unlock();task(index);lock.lock();if(--remaining_==0)finished_.notify_one();}});}
  ~RenderWorkers(){for(auto& thread:threads_)thread.request_stop();wake_.notify_all();}
  void run(const std::function<void(int)>& task){std::atomic_int next{0};const auto batch=[&](int){for(int partition=next.fetch_add(1);partition<10;partition=next.fetch_add(1))task(partition);};{std::lock_guard lock(mutex_);task_=batch;remaining_=WorkerCount;++generation_;}wake_.notify_all();batch(-1);std::unique_lock lock(mutex_);finished_.wait(lock,[&]{return remaining_==0;});task_={};}
};

std::unique_ptr<life::skia::Api> api;
std::unique_ptr<RenderWorkers> renderWorkers;
std::vector<uint32_t> pixels;
std::vector<uint8_t> renderCells,renderTerrain,renderResources,renderElevation,renderHabitat;std::vector<uint16_t> renderNutrients;
std::vector<int32_t> renderOwners;
std::unordered_map<int,int> renderSpeciesByOwner;
std::vector<uint32_t> renderDirtyVersions;
life::NativeMetrics renderMetrics;
std::optional<life::OrganismInspection> renderInspection;
life::AtlasSnapshot renderAtlas;
uint64_t renderAtlasRevision=UINT64_MAX;
BITMAPINFO bitmap{};
int surfaceWidth{}, surfaceHeight{};
life::skia::Api::FontManager fontManager{};
life::skia::Api::Typeface regularTypeface{},boldTypeface{};
life::NativeSimulation simulation(life::WorldWidth, life::WorldHeight, .2, 500, 1, 1);
float zoom = 1.0f, cameraX = 0.0f, cameraY = 0.0f;
life::FollowCamera followCamera;
life::WatchedPose renderWatchedPose;
life::WatchedSnapshot renderWatched;
bool familyHighlights=true,compactFollow=false;
size_t watchedHistoryOffset=0;
std::unordered_map<int,life::FamilyRole> renderFamily;
std::vector<uint8_t> renderFamilyCells;
std::vector<uint8_t> renderFamilyEdges;
std::vector<uint32_t> renderPalette;
std::vector<int> rasterColumns,rasterRows;
std::vector<uint16_t> frameDirtyTiles;
bool frameCopiedAll=false;
auto nextWatchDiagnostics=std::chrono::steady_clock::time_point{};
auto previousCameraFrame=std::chrono::steady_clock::now();
POINT senseHover{-1,-1};
bool dragging = false;
POINT dragOrigin{};
float dragCameraX{}, dragCameraY{};
enum class Tool { Inspect, Pan, Plant, Carrion, Mineral, Fertile, Desert, Water, Mountain, Nutrients, Cave, Bridge };
enum class Overlay { Terrain, Resources, Productivity, Climate, Species, Nutrients, Decomposition, Habitat };
Tool activeTool = Tool::Inspect;
Overlay activeOverlay = Overlay::Terrain;
bool atlasOpen=false;
bool weatherVisible=true;
bool habitatHistory=false;
std::string saveStatus;
int atlasTab=0,atlasTimelineWindow=600;
uint32_t nextWorldSeed = 2;
std::atomic_bool paused = false;
std::atomic_int requestedTps = 60;
std::atomic_int requestedFps = 30;
int selectedId = -1;
double measuredTps = 0.0, measuredFps = 0.0;
std::atomic_uint64_t measuredTicks = 0;
uint64_t measuredFrames = 0;
auto measurementStart = std::chrono::steady_clock::now();
auto lastRenderRequest = measurementStart;
auto lastPresentedFrame = measurementStart-std::chrono::seconds(1);
std::shared_mutex simulationMutex;
std::jthread simulationThread;
std::jthread presentationThread;
std::jthread persistenceThread;
std::atomic_bool persistenceBusy=false;
constexpr UINT PersistenceMessage=WM_APP+2;
struct PersistenceResult {bool loaded{},success{};std::string message;};
bool scheduledPaint=false;
std::atomic_bool presentationPending=false;
std::atomic_uint64_t presentationDrops=0;
bool uiBenchmark=false;
bool followBenchmark=false;
int benchmarkFamilyPeak=0;
int uiBenchmarkSeconds=60;
std::atomic_uint64_t uiBenchmarkTicks=0;
uint64_t uiBenchmarkFrames=0,uiBenchmarkInputs=0;
double uiBenchmarkRenderMs=0;
auto uiBenchmarkStart=measurementStart;
auto nextBenchmarkInput=measurementStart;
std::optional<std::chrono::steady_clock::time_point> pendingInput;
std::vector<double> inputLatenciesMs;
std::vector<double> frameIntervalsMs;
auto previousBenchmarkFrame=measurementStart;
life::AppSettings appSettings;
bool headlessMode=false;
int headlessWidth=1920,headlessHeight=1080;
double headlessFrameSeconds=-1;
uint64_t watchPublishedRevision=0;
struct FrameWork {double snapshot{},raster{},ui{};} lastFrameWork;
RECT viewportBounds(HWND window){RECT bounds{};if(headlessMode){bounds.right=headlessWidth;bounds.bottom=headlessHeight;}else GetClientRect(window,&bounds);return bounds;}

constexpr uint32_t terrainColors[] = {0xff2e4330, 0xff2b5b36, 0xff6c5232, 0xff1a3e5f, 0xff4d525a};
constexpr uint32_t resourceColors[] = {0, 0xff18b437, 0xff8b4b32, 0xffd7bd52};
constexpr uint32_t cellColors[] = {0,0xff008000,0xff808080,0xffffa500,0xffffffff,0xff3493eb,0xffff0000,0xff800080,0xffff9f43,0xffd67c4a,0xffd6c36a,0xff59636f,0xff72b59b};
constexpr UINT PresentMessage=WM_APP+1;

std::filesystem::path libraryPath() {
  wchar_t executable[MAX_PATH]{};
  GetModuleFileNameW(nullptr, executable, MAX_PATH);
  return std::filesystem::path(executable).parent_path() / "libSkiaSharp.dll";
}

void writeUiBenchmarkReport(){const auto now=std::chrono::steady_clock::now();const double elapsed=std::chrono::duration<double>(now-uiBenchmarkStart).count();std::sort(inputLatenciesMs.begin(),inputLatenciesMs.end());const size_t p95Index=inputLatenciesMs.empty()?0:std::min(inputLatenciesMs.size()-1,size_t(std::ceil(inputLatenciesMs.size()*.95))-1);const double p95=inputLatenciesMs.empty()?0:inputLatenciesMs[p95Index];size_t organisms=0;{std::shared_lock lock(simulationMutex);organisms=simulation.metrics().organisms;}const double tps=uiBenchmarkTicks.load()/elapsed,fps=uiBenchmarkFrames/elapsed,renderMs=uiBenchmarkFrames?uiBenchmarkRenderMs/uiBenchmarkFrames:0;const int fpsCap=requestedFps.load();const bool passed=organisms==10'000&&tps>=59.5&&fps>=fpsCap*.85&&fps<=fpsCap*1.05&&p95<100;std::ostringstream report;report<<std::fixed<<std::setprecision(2)<<"threads=12 organisms="<<organisms<<" fps_cap="<<fpsCap<<" elapsed_s="<<elapsed<<" measured_tps="<<tps<<" render_fps="<<fps<<" render_ms="<<renderMs<<" input_p95_ms="<<p95<<" interactions="<<inputLatenciesMs.size()<<" result="<<(passed?"PASS":"FAIL")<<"\r\n";const auto path=libraryPath().parent_path()/"ui-benchmark.txt";HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(file!=INVALID_HANDLE_VALUE){const auto value=report.str();DWORD written=0;WriteFile(file,value.data(),DWORD(value.size()),&written,nullptr);CloseHandle(file);}}

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
  int x=int(std::floor((screenX-cameraX)/zoom)),y=int(std::floor((screenY-82-cameraY)/zoom));
  if(followCamera.wrapped){x=life::wrapCoordinate(x,life::WorldWidth);y=life::wrapCoordinate(y,life::WorldHeight);}return {x,y};
}

void clearFollowing(bool clearWrap=false){const bool wrapped=followCamera.wrapped;followCamera.clear();followCamera.wrapped=clearWrap?false:wrapped;renderWatched={};renderWatchedPose={};renderFamily.clear();std::fill(renderFamilyCells.begin(),renderFamilyCells.end(),0);std::fill(renderFamilyEdges.begin(),renderFamilyEdges.end(),0);watchedHistoryOffset=0;nextWatchDiagnostics={};}
void toggleFollowing(HWND window){
  if(persistenceBusy.load()||selectedId<0)return;
  if(followCamera.following&&followCamera.target==selectedId){followCamera.release();return;}
  std::unique_lock lock(simulationMutex);simulation.watch(selectedId);const auto pose=simulation.watchedPose();if(pose.id<0||!pose.alive)return;
  const RECT client=viewportBounds(window);const double aside=std::min(390.0,client.right*.32);followCamera.begin(selectedId,pose.x,pose.y,cameraX,cameraY,zoom,client.right-aside,client.bottom-82);watchedHistoryOffset=0;nextWatchDiagnostics={};
}
void followDescendant(HWND window){
  if(persistenceBusy.load()||renderWatched.nextDescendant<0)return;
  {std::unique_lock lock(simulationMutex);selectedId=renderWatched.nextDescendant;simulation.select(selectedId);simulation.watch(selectedId);}clearFollowing();toggleFollowing(window);
}

void applyTool(int screenX, int screenY) {
  if(persistenceBusy.load())return;
  const auto [x,y] = screenToWorld(screenX, screenY);
  if (x < 0 || y < 0 || x >= life::WorldWidth || y >= life::WorldHeight) return;
  std::unique_lock lock(simulationMutex);
  switch (activeTool) {
    case Tool::Inspect: { const auto* organism=simulation.organismAt(x,y); selectedId=organism?organism->id:-1;simulation.select(selectedId);simulation.watch(selectedId);clearFollowing();break; }
    case Tool::Plant: simulation.paintResource(x,y,life::ResourceType::Plant); break;
    case Tool::Carrion: simulation.paintResource(x,y,life::ResourceType::Carrion); break;
    case Tool::Mineral: simulation.paintResource(x,y,life::ResourceType::Mineral); break;
    case Tool::Fertile: simulation.paintTerrain(x,y,life::TerrainType::Fertile); break;
    case Tool::Desert: simulation.paintTerrain(x,y,life::TerrainType::Desert); break;
    case Tool::Water: simulation.paintTerrain(x,y,life::TerrainType::Water); break;
    case Tool::Mountain: simulation.paintTerrain(x,y,life::TerrainType::Mountain); break;
    case Tool::Nutrients: simulation.paintNutrients(x,y); break;
    case Tool::Cave: simulation.paintHabitat(x,y,life::HabitatType::Cave); break;
    case Tool::Bridge: simulation.paintHabitat(x,y,life::HabitatType::Bridge); break;
    case Tool::Pan: break;
  }
}

void renderAtlasView(life::skia::Api::Canvas canvas,life::skia::Api::Paint paint,float left,float top,float right,float bottom){
  rectangle(canvas,paint,left,top,right,bottom,0xff121d29);text(canvas,paint,"EVOLUTION ATLAS",left+24,top+34,20,0xffffffff,true);text(canvas,paint,std::to_string(renderAtlas.timeline.size())+" samples · "+std::to_string(renderAtlas.species.size())+" species · "+std::to_string(renderAtlas.events.size())+" events",left+24,top+55,10,0xff81d2c7);
  constexpr const char* tabs[]={"Timeline","Species","Tree","Ecology"};const float tabTop=top+68,tabWidth=(right-left-48)/4;for(int index=0;index<4;++index){const float x=left+24+index*tabWidth;rectangle(canvas,paint,x,tabTop,x+tabWidth-6,tabTop+32,index==atlasTab?0xff81d2c7:0xff3a4b68);text(canvas,paint,tabs[index],x+12,tabTop+21,11,index==atlasTab?0xff121d29:0xffffffff,true);}
  const float bodyTop=tabTop+48;if(atlasTab==0){rectangle(canvas,paint,left+24,bodyTop,right-24,bottom-30,0xff1b2938);if(renderAtlas.timeline.empty()){text(canvas,paint,"Timeline begins after 60 simulation ticks.",left+42,bodyTop+42,12,0xff9099c2);return;}const size_t count=std::min<size_t>(renderAtlas.timeline.size(),std::max(10,atlasTimelineWindow)),begin=renderAtlas.timeline.size()-count;int maximum=1;for(size_t index=begin;index<renderAtlas.timeline.size();++index)maximum=std::max(maximum,renderAtlas.timeline[index].population);const float graphLeft=left+52,graphRight=right-42,graphTop=bodyTop+34,graphBottom=bottom-78;float previousX=graphLeft,previousY=graphBottom;for(size_t offset=0;offset<count;++offset){const auto& sample=renderAtlas.timeline[begin+offset];const float x=graphLeft+(graphRight-graphLeft)*offset/std::max<size_t>(1,count-1),y=graphBottom-(graphBottom-graphTop)*(habitatHistory?sample.cavePopulation+sample.bridgePopulation:sample.population)/maximum;if(offset){const int steps=std::max(1,int(std::abs(x-previousX)));for(int step=0;step<=steps;++step){const float amount=float(step)/steps,px=previousX+(x-previousX)*amount,py=previousY+(y-previousY)*amount;rectangle(canvas,paint,px,py,px+2,py+2,0xff81d2c7);}}previousX=x;previousY=y;}const auto& latest=renderAtlas.timeline.back();text(canvas,paint,(habitatHistory?"Cave + bridge population ":"Population ")+std::to_string(habitatHistory?latest.cavePopulation+latest.bridgePopulation:latest.population)+"  Peak "+std::to_string(maximum)+"  O2 "+std::to_string(int(latest.oxygen*100))+"%  Species "+std::to_string(latest.species),graphLeft,bodyTop+20,11,0xffffffff);text(canvas,paint,"H: population / habitat history · Wheel zoom · recent "+std::to_string(count)+" samples",graphLeft,bottom-48,10,0xff9099c2);int eventLine=0;for(auto event=renderAtlas.events.rbegin();event!=renderAtlas.events.rend()&&eventLine<3;++event,++eventLine)text(canvas,paint,std::to_string(event->tick)+" · "+event->label,graphLeft,bottom-30-eventLine*16,9,0xffd7bd52);}
  else if(atlasTab==1){text(canvas,paint,"ID    Population   Peak   Parent   Diet   Habitat   State",left+32,bodyTop+18,10,0xff81d2c7,true);int row=0;for(const auto& species:renderAtlas.species){if(row>=int((bottom-bodyTop-35)/22))break;const float y=bodyTop+32+row*22;rectangle(canvas,paint,left+24,y-15,right-24,y+5,species.id==renderAtlas.selectedSpecies?0xff416788:(row%2?0xff1b2938:0xff202d3b));std::ostringstream value;value<<"#"<<species.id<<"      "<<species.population<<"          "<<species.peakPopulation<<"      "<<(species.parentId<0?0:species.parentId)<<"       "<<int(species.dietMask)<<"  "<<(species.dominantHabitat==1?"Cave":species.dominantHabitat==2?"Bridge":"Surface")<<"  "<<(species.extinct?"Extinct":"Living");text(canvas,paint,value.str(),left+34,y,10,species.extinct?0xff9099c2:0xffffffff);++row;}}
  else if(atlasTab==2){text(canvas,paint,"Species ancestry",left+32,bodyTop+18,12,0xff81d2c7,true);int row=0;for(const auto& species:renderAtlas.species){if(row>=int((bottom-bodyTop-30)/20))break;const float y=bodyTop+42+row*20;const int depth=species.parentId<0?0:1;text(canvas,paint,std::string(depth*4,' ')+(species.parentId>=0?"↳ ":"")+"Species #"+std::to_string(species.id)+"  from #"+std::to_string(std::max(0,species.parentId))+(species.extinct?" †":""),left+40,y,10,species.extinct?0xff9099c2:0xffffffff);++row;}}
  else {if(renderAtlas.timeline.empty()){text(canvas,paint,"Ecology sampling begins after 60 ticks.",left+40,bodyTop+38,12,0xff9099c2);return;}const auto& sample=renderAtlas.timeline.back();const std::array<std::string,10> lines{"Population "+std::to_string(sample.population)+" · Living species "+std::to_string(sample.species),"Resources  Plant "+std::to_string(sample.plant)+" · Carrion "+std::to_string(sample.carrion)+" · Mineral "+std::to_string(sample.mineral),"Diets  Plant "+std::to_string(sample.plantEaters)+" · Scavenger "+std::to_string(sample.scavengers)+" · Mineral "+std::to_string(sample.mineralEaters)+" · Decomposer "+std::to_string(sample.decomposers),"Predators "+std::to_string(sample.predators)+" · Critical respiration "+std::to_string(sample.criticalRespiratory),"Average energy "+std::to_string(int(sample.averageEnergy))+" · Body "+std::to_string(sample.averageBodySize)+" · Perception "+std::to_string(sample.averagePerceptionCost),"Soil nutrients "+std::to_string(sample.soilNutrients)+" · Poor tiles "+std::to_string(sample.nutrientPoorTiles)+" · Carrion processed "+std::to_string(sample.carrionProcessed)+" · Plants produced "+std::to_string(sample.plantProduced),"Atmosphere  O2 "+std::to_string(sample.oxygen*100)+"% · CO2 "+std::to_string(sample.carbonDioxide*100)+"%","Climate  Temperature "+std::to_string(sample.temperature)+" · Fertility "+std::to_string(sample.fertility),"Habitats  Cave "+std::to_string(sample.cavePopulation)+" · Bridge "+std::to_string(sample.bridgePopulation)+" · Elevation "+std::to_string(int(sample.averageElevation))+" · Rain "+std::to_string(sample.rainfall)+" · Moisture "+std::to_string(int(sample.averageMoisture)),"Latest event  "+(renderAtlas.events.empty()?std::string("None"):renderAtlas.events.back().label)};for(size_t index=0;index<lines.size();++index)text(canvas,paint,lines[index],left+40,bodyTop+40+index*30,index==0?13:11,index==0?0xffffffff:0xffb8c3d1,index==0);}
}

#include "watch_ui.inc"
#include "world_raster.inc"

void render(HWND window, HDC dc) {
  const auto renderStarted=std::chrono::steady_clock::now();
  publishWatchFrame();const auto metrics=renderMetrics;const auto inspection=renderInspection;lastFrameWork.snapshot=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-renderStarted).count();
  if(atlasOpen||activeOverlay==Overlay::Species){std::shared_lock atlasLock(simulationMutex,std::try_to_lock);if(atlasLock.owns_lock()){if(renderAtlasRevision!=simulation.atlasRevision()){renderAtlas=simulation.atlasSnapshot();renderAtlasRevision=renderAtlas.revision;renderSpeciesByOwner.clear();renderSpeciesByOwner.reserve(simulation.organisms().size());for(const auto& organism:simulation.organisms())if(organism.living)renderSpeciesByOwner[organism.id]=organism.speciesId;}}}
  const RECT client=viewportBounds(window);
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
  const auto cameraNow=std::chrono::steady_clock::now();const double cameraSeconds=headlessFrameSeconds>=0?headlessFrameSeconds:std::chrono::duration<double>(cameraNow-previousCameraFrame).count();previousCameraFrame=cameraNow;
  if(followCamera.target==renderWatchedPose.id){followCamera.offsetX=cameraX;followCamera.offsetY=cameraY;followCamera.advance(renderWatchedPose.x,renderWatchedPose.y,renderWatchedPose.alive,zoom,contentRight,height-headerHeight,cameraSeconds,atlasOpen);if(followCamera.following){cameraX=float(followCamera.offsetX);cameraY=float(followCamera.offsetY);}}
  rectangle(canvas,paint,0,0,contentRight,headerHeight,0xff416788);rectangle(canvas,paint,0,headerHeight,contentRight,float(height),0xff121d29);rectangle(canvas,paint,contentRight,0,float(width),float(height),0xff3a4b68);
  const bool wrappedView=followCamera.wrapped;const int worldStageTop=int(headerHeight);const float viewportCameraY=cameraY;const int stageLeft=wrappedView?0:std::max(0,int(std::floor(cameraX))),stageRight=wrappedView?int(contentRight):std::min(std::max(0,int(contentRight)),int(std::ceil(cameraX+life::WorldWidth*zoom))),stageTop=wrappedView?worldStageTop:std::max(worldStageTop,int(std::floor(worldStageTop+viewportCameraY))),stageBottom=wrappedView?height:std::min(height,int(std::ceil(worldStageTop+viewportCameraY+life::WorldHeight*zoom)));

  const auto rasterStart=std::chrono::steady_clock::now();
  prepareWorldPalette();rasterColumns.resize(width);rasterRows.resize(height);
  for(int x=stageLeft;x<stageRight;++x){int sample=int(std::floor((x-cameraX)/zoom));if(wrappedView)sample=life::wrapCoordinate(sample,life::WorldWidth);rasterColumns[x]=sample>=0&&sample<life::WorldWidth?sample:-1;}
  for(int y=stageTop;y<stageBottom;++y){int sample=int(std::floor((y-worldStageTop-viewportCameraY)/zoom));if(wrappedView)sample=life::wrapCoordinate(sample,life::WorldHeight);rasterRows[y]=sample>=0&&sample<life::WorldHeight?sample*life::WorldWidth:-1;}
  constexpr int rasterPartitions=10;renderWorkers->run([&](int partition){for(int y=stageTop+partition;y<stageBottom;y+=rasterPartitions){const int row=rasterRows[y];if(row<0)continue;auto* destination=pixels.data()+size_t(y)*width;for(int x=stageLeft;x<stageRight;++x){const int column=rasterColumns[x];if(column<0)continue;const int index=row+column;uint32_t color=renderPalette[index];if(followCamera.target>=0&&familyHighlights&&renderFamilyEdges[index])color=familyOutline(color,index,x,y,(x-cameraX)/zoom,(y-worldStageTop-viewportCameraY)/zoom);destination[x]=color;}}});lastFrameWork.raster=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-rasterStart).count();const auto uiStart=std::chrono::steady_clock::now();
  if(weatherVisible&&stageRight>stageLeft&&stageBottom>stageTop){
    // Distant clouds drift slowly; their shadows sit below the rain planes.
    for(int cloud=0;cloud<5;++cloud){const int span=stageRight-stageLeft+240,cx=stageLeft-120+(cloud*271+metrics.ticks/3)%span,cy=stageTop+40+(cloud*83)%std::max(1,stageBottom-stageTop);for(int band=-4;band<=4;++band){const int half=95-band*band*4;const float left=float(std::clamp(cx-half,stageLeft,stageRight)),right=float(std::clamp(cx+half,stageLeft,stageRight)),top=float(std::clamp(cy+band*5,stageTop,stageBottom)),bottom=float(std::clamp(cy+band*5+5,stageTop,stageBottom));if(right>left&&bottom>top)rectangle(canvas,paint,left,top,right,bottom,band<0?0x0c06121e:0x107f98b0);}}
    // Three projected depths give weather independent speed, size, and parallax.
    for(int depth=0;depth<3;++depth)for(int drop=0;drop<35+int(50*(.5+.5*std::sin(life::cyclePhase(metrics.ticks)*std::numbers::pi*2)));++drop){const uint32_t seed=uint32_t(drop*1664525u+depth*1013904223u);const int spanX=stageRight-stageLeft,spanY=stageBottom-stageTop;const int x=stageLeft+int((seed+uint32_t(metrics.ticks*(depth+1)))%uint32_t(spanX)),y=stageTop+int(((seed>>8)+uint32_t(metrics.ticks*(depth+2)*3))%uint32_t(spanY));int wx=int(std::floor((x-cameraX)/zoom)),wy=int(std::floor((y-worldStageTop-viewportCameraY)/zoom));if(wrappedView){wx=life::wrapCoordinate(wx,life::WorldWidth);wy=life::wrapCoordinate(wy,life::WorldHeight);}if(wx<0||wy<0||wx>=life::WorldWidth||wy>=life::WorldHeight||renderHabitat[wy*life::WorldWidth+wx]==uint8_t(life::HabitatType::Cave))continue;const uint32_t alpha=uint32_t(28+depth*18);rectangle(canvas,paint,float(x),float(y),float(std::min(x+1+depth,stageRight)),float(std::min(y+3+depth*3,stageBottom)),(alpha<<24)|0x009abde2);if(y>stageBottom-8)rectangle(canvas,paint,float(std::max(stageLeft,x-2)),float(y),float(std::min(stageRight,x+3)),float(y+1),0x5076a7d2);}
  }
  if(!atlasOpen&&followCamera.target>=0&&renderWatchedPose.id==followCamera.target){const double cx=(contentRight*.5-cameraX)/zoom,cy=((height-headerHeight)*.5-viewportCameraY)/zoom;const double wx=wrappedView?life::nearestWrapped(renderWatchedPose.x,cx,life::WorldWidth):renderWatchedPose.x,wy=wrappedView?life::nearestWrapped(renderWatchedPose.y,cy,life::WorldHeight):renderWatchedPose.y;const float x=float(cameraX+(wx+.5)*zoom),y=float(worldStageTop+viewportCameraY+(wy+.5)*zoom),radius=std::max(7.f,zoom*.8f);if(x-radius>=0&&x+radius<contentRight&&y-radius>=headerHeight&&y+radius<height-28){for(int sx:{-1,1})for(int sy:{-1,1}){const float px=x+sx*radius,py=y+sy*radius;rectangle(canvas,paint,px-(sx>0?5:0),py,px+(sx<0?5:1),py+1,0xffe5c56f);rectangle(canvas,paint,px,py-(sy>0?5:0),px+1,py+(sy<0?5:1),0xffe5c56f);}}}
  if(!atlasOpen)drawFollowCard(canvas,paint,contentRight);
  // Save/load controls are drawn after the Atlas so they stay accessible.
  text(canvas,paint,"EVOLUTION, ACCELERATED",30,30,10,0xff81d2c7,true);text(canvas,paint,"Life Engine",30,65,28,0xffffffff,true);text(canvas,paint,"NNUE",176,65,28,0xff81d2c7,true);text(canvas,paint,"v" LFE_VERSION_STRING,249,64,10,0xffb8c3d1);rectangle(canvas,paint,contentRight-210,25,contentRight-116,58,atlasOpen?0xff81d2c7:0xff3a4b68);text(canvas,paint,"ATLAS",contentRight-190,47,11,atlasOpen?0xff121d29:0xffffffff,true);rectangle(canvas,paint,contentRight-96,37,contentRight-88,45,paused.load()?0xff9099c2:0xff81d2c7);text(canvas,paint,paused.load()?"PAUSED":"RUNNING",contentRight-78,47,12,0xffffffff,false);
  const float panelX=contentRight+15,panelRight=width-15,cardGap=8,cardWidth=(aside-38)/2;
  for (int column = 0; column < 2; ++column)
    for (int row = 0; row < 2; ++row) {
      const float left=panelX+column*(cardWidth+cardGap),top=15+row*76;rectangle(canvas,paint,left,top,left+cardWidth,top+68,0xffe1e3ec);
    }
  const char* seasons[]={"Spring","Summer","Autumn","Winter"};const int season=int(std::floor(life::cyclePhase(metrics.ticks)*4))%4;std::ostringstream tpsText,fpsText;tpsText<<std::fixed<<std::setprecision(1)<<measuredTps;fpsText<<std::fixed<<std::setprecision(1)<<measuredFps;const std::string values[]={std::to_string(metrics.organisms),tpsText.str(),fpsText.str(),seasons[season]};const char* labels[]={"ORGANISMS","ACTUAL TPS","RENDER FPS","SEASON"};for(int i=0;i<4;++i){const float left=panelX+(i%2)*(cardWidth+cardGap),top=15+(i/2)*76;text(canvas,paint,labels[i],left+15,top+22,9,0xff416788);text(canvas,paint,values[i],left+15,top+53,26,0xff121d29,true);}
  const bool isPaused=paused.load();const int targetTps=requestedTps.load(),targetFps=requestedFps.load();float top=179;rectangle(canvas,paint,panelX,top,panelRight,top+240,0xffe1e3ec);text(canvas,paint,"Simulation",panelX+20,top+35,16,0xff121d29,true);rectangle(canvas,paint,panelRight-77,top+15,panelRight-20,top+48,0xff9099c2);text(canvas,paint,isPaused?"Resume":"Pause",panelRight-(isPaused?69:65),top+36,12,0xff000000,true);text(canvas,paint,"Requested simulation TPS",panelX+20,top+76,12,0xff3a4b68);text(canvas,paint,std::to_string(targetTps),panelRight-43,top+76,12,0xff416788);rectangle(canvas,paint,panelX+20,top+95,panelRight-20,top+99,0xff416788);const float thumbX=panelX+20+(panelRight-panelX-40)*(targetTps-1)/119.0f;rectangle(canvas,paint,thumbX-4,top+90,thumbX+4,top+104,0xff81d2c7);
  text(canvas,paint,"FPS",panelX+20,top+131,10,0xff3a4b68,true);const float fpsLeft=panelX+53,fpsGap=3,fpsWidth=(panelRight-fpsLeft-20-fpsGap*5)/6;for(int i=0;i<12;++i){const int row=i/6,column=i%6,value=(i+1)*30;const float left=fpsLeft+column*(fpsWidth+fpsGap),buttonTop=top+108+row*24;rectangle(canvas,paint,left,buttonTop,left+fpsWidth,buttonTop+20,value==targetFps?0xff81d2c7:0xff9099c2);text(canvas,paint,std::to_string(value),left+5,buttonTop+14,8,0xff111111,true);}
  constexpr const char* overlayNames[]={"Terrain","Resources","Productivity","Climate","Species","Nutrients","Decomposition","Habitat depth"};text(canvas,paint,"Overlay",panelX+20,top+177,11,0xff3a4b68);rectangle(canvas,paint,panelX+82,top+158,panelRight-20,top+184,0xffd5d8e4);text(canvas,paint,overlayNames[int(activeOverlay)],panelX+92,top+176,11,0xff121d29);const float half=(panelRight-panelX-45)/2;rectangle(canvas,paint,panelX+20,top+193,panelX+20+half,top+228,0xff9099c2);rectangle(canvas,paint,panelX+25+half,top+193,panelRight-20,top+228,0xff9099c2);text(canvas,paint,"Reset life",panelX+34,top+215,10,0xff111111,true);text(canvas,paint,"New seeded world",panelX+34+half,top+215,9,0xff111111,true);
  top=411;rectangle(canvas,paint,panelX,top,panelRight,top+162,0xffe1e3ec);text(canvas,paint,"World tools",panelX+20,top+35,16,0xff121d29,true);const char* tools[]={"Inspect","Pan","Plant","Carrion","Mineral","Fertile","Desert","Water","Mountain","Nutrients","Cave","Bridge"};for(int i=0;i<12;++i){const int row=i/6,column=i%6;const float buttonWidth=(aside-75)/6,left=panelX+20+column*(buttonWidth+5),buttonTop=top+52+row*38;rectangle(canvas,paint,left,buttonTop,left+buttonWidth,buttonTop+31,i==int(activeTool)?0xff81d2c7:0xff9099c2);text(canvas,paint,tools[i],left+4,buttonTop+20,8,0xff111111,true);}
  top=585;rectangle(canvas,paint,panelX,top,panelRight,float(height-15),0xffe1e3ec);text(canvas,paint,"Evolution Observatory",panelX+20,top+35,16,0xff121d29,true);rectangle(canvas,paint,panelRight-62,top+13,panelRight-20,top+42,0xff9099c2);text(canvas,paint,"Clear",panelRight-56,top+32,9,0xff111111,true);if(inspection){std::ostringstream identity,energy,health,brain,place,decomposition,neural,mutation;identity<<"#"<<inspection->id<<" · "<<inspection->diet<<" · "<<(inspection->alive?"Alive":"Dead");if(!inspection->alive&&!inspection->deathCause.empty())identity<<" ("<<inspection->deathCause<<")";energy<<"Energy "<<std::fixed<<std::setprecision(1)<<inspection->energy/1000.0<<"  Minerals "<<inspection->minerals<<"  Age "<<inspection->age<<"  Body "<<inspection->cellCount<<"  Damage "<<inspection->damage;health<<"Resp "<<std::setprecision(1)<<inspection->respiratoryStress*100<<"%  Risk "<<inspection->respiratoryRisk*100<<"%  O2 tol "<<inspection->oxygenTolerance*100<<"%  Recovery "<<inspection->stressRecovery*100<<"%  Frailty "<<inspection->frailty;brain<<"Radius "<<inspection->perceptionRadius<<"  Channels 0x"<<std::hex<<int(inspection->senseChannels)<<std::dec<<"  Cost "<<inspection->neuralCost;decomposition<<"Decomposition  Carrion "<<inspection->carrionConsumed<<"  Nutrients "<<inspection->nutrientsReleased<<"  Energy "<<inspection->decompositionEnergy<<"  Local soil "<<inspection->localNutrients;place<<inspection->terrain<<" · "<<inspection->resource<<"  Temp "<<std::fixed<<std::setprecision(2)<<inspection->temperature<<"  Fertility "<<inspection->fertility<<"  Thermal "<<inspection->thermalStress*100<<"%";neural<<"Action "<<inspection->action;if(inspection->hasOutputs){neural<<" [";for(int i=0;i<life::OutputSize;++i){if(i)neural<<", ";neural<<std::setprecision(2)<<inspection->outputs[i];}neural<<"]";}mutation<<"Mutations "<<inspection->mutations.size()<<": "<<(inspection->mutations.empty()?"None":inspection->mutations.back());const std::array<std::string,11> lines{identity.str(),energy.str(),health.str(),brain.str(),place.str(),decomposition.str(),"Ancestors: "+lineageIds(inspection->ancestors),"Descendants: "+lineageIds(inspection->descendants),"Senses: "+featureIds(inspection->senses),neural.str(),mutation.str()};for(size_t line=0;line<lines.size();++line)text(canvas,paint,line==0?lines[line].substr(0,29):lines[line],panelX+20,top+62+float(line)*20,line==0?12:9,line==0?0xff121d29:0xff3a4b68,line==0);}else{text(canvas,paint,"Select a creature.",panelX+20,top+68,12,0xff66738a);}
  if(inspection){rectangle(canvas,paint,panelRight-86,632,panelRight-20,656,followCamera.following?0xff81d2c7:0xff9099c2);text(canvas,paint,followCamera.following?"Release":"Follow (F)",panelRight-80,648,10,0xff121d29,true);}
  std::ostringstream populationSummary,performanceSummary;populationSummary<<"Plant "<<metrics.plantEaters<<" · Carrion "<<metrics.scavengers<<" · Mineral "<<metrics.mineralEaters<<" · Decomp "<<metrics.decomposers<<" · Predators "<<metrics.predators;performanceSummary<<"Caves "<<metrics.caveOrganisms<<" · Bridges "<<metrics.bridgeOrganisms<<" · Elev "<<std::fixed<<std::setprecision(0)<<metrics.averageElevation<<" · O2 "<<std::setprecision(1)<<metrics.oxygen*100<<"%";rectangle(canvas,paint,panelX,float(height-61),panelRight,float(height-15),0xff121d29);text(canvas,paint,populationSummary.str(),panelX+10,float(height-42),9,0xffffffff);text(canvas,paint,performanceSummary.str(),panelX+10,float(height-24),9,0xff81d2c7);
  rectangle(canvas,paint,24,float(height-52),320,float(height-20),0xee121d29);text(canvas,paint,"Terrain + resources + evolving organisms",38,float(height-31),11,0xffffffff);if(atlasOpen)renderAtlasView(canvas,paint,0,headerHeight,contentRight,float(height));
  rectangle(canvas,paint,20,float(height-28),103,float(height-4),0xff416788);text(canvas,paint,"Save (F5)",30,float(height-12),10,0xffffffff);rectangle(canvas,paint,109,float(height-28),192,float(height-4),0xff416788);text(canvas,paint,"Load (F9)",119,float(height-12),10,0xffffffff);text(canvas,paint,"W Weather · "+saveStatus,205,float(height-12),10,0xffb8c3d1);
  api->paintDelete(paint);api->surfaceUnref(surface);
  if(!headlessMode)StretchDIBits(dc, 0, 0, width, height, 0, 0, width, height, pixels.data(), &bitmap, DIB_RGB_COLORS, SRCCOPY);
  lastFrameWork.ui=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-uiStart).count();
  ++measuredFrames;if(uiBenchmark){const auto frameNow=std::chrono::steady_clock::now();if(uiBenchmarkFrames)frameIntervalsMs.push_back(std::chrono::duration<double,std::milli>(frameNow-previousBenchmarkFrame).count());previousBenchmarkFrame=frameNow;++uiBenchmarkFrames;uiBenchmarkRenderMs+=std::chrono::duration<double,std::milli>(frameNow-renderStarted).count();if(pendingInput){inputLatenciesMs.push_back(std::chrono::duration<double,std::milli>(frameNow-*pendingInput).count());pendingInput.reset();}}
}

#include "headless.inc"

void writeDetailedBenchmarkReport(){const auto now=std::chrono::steady_clock::now();const double elapsed=std::chrono::duration<double>(now-uiBenchmarkStart).count();std::sort(inputLatenciesMs.begin(),inputLatenciesMs.end());std::sort(frameIntervalsMs.begin(),frameIntervalsMs.end());const auto percentile=[](const std::vector<double>& values,double fraction){return values.empty()?0.0:values[std::min(values.size()-1,size_t(std::ceil(values.size()*fraction))-1)];};const double inputP95=percentile(inputLatenciesMs,.95),frameP50=percentile(frameIntervalsMs,.50),frameP95=percentile(frameIntervalsMs,.95),frameP99=percentile(frameIntervalsMs,.99),onePercentLow=frameP99>0?1000.0/frameP99:0;size_t organisms=0,brainFamilies=0;int births=0,deaths=0;{std::shared_lock lock(simulationMutex);organisms=simulation.metrics().organisms;births=simulation.birthCount();deaths=simulation.deadLineageRecordCount();std::unordered_set<const life::Nnue*> families;for(const auto& organism:simulation.organisms())if(organism.living&&organism.brain)families.insert(organism.brain.get());brainFamilies=families.size();}const double tps=uiBenchmarkTicks.load()/elapsed,fps=uiBenchmarkFrames/elapsed,renderMs=uiBenchmarkFrames?uiBenchmarkRenderMs/uiBenchmarkFrames:0;const bool passed=organisms>=8'500&&organisms<=10'000&&brainFamilies>=128&&births>0&&deaths>0&&tps>=58&&fps>=55&&onePercentLow>=45&&inputP95<100;std::ostringstream report;report<<std::fixed<<std::setprecision(2)<<"threads=12 organisms="<<organisms<<" brain_families="<<brainFamilies<<" births="<<births<<" deaths="<<deaths<<" elapsed_s="<<elapsed<<" measured_tps="<<tps<<" render_fps="<<fps<<" one_percent_low_fps="<<onePercentLow<<" frame_p50_ms="<<frameP50<<" frame_p95_ms="<<frameP95<<" frame_p99_ms="<<frameP99<<" render_ms="<<renderMs<<" input_p95_ms="<<inputP95<<" interactions="<<inputLatenciesMs.size()<<" follow="<<followBenchmark<<" family_peak="<<benchmarkFamilyPeak<<" coalesced_presentations="<<presentationDrops.load()<<" result="<<(passed?"PASS":"FAIL")<<"\r\n";const auto path=libraryPath().parent_path()/"ui-benchmark.txt";HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(file!=INVALID_HANDLE_VALUE){const auto value=report.str();DWORD written=0;WriteFile(file,value.data(),DWORD(value.size()),&written,nullptr);CloseHandle(file);}}

LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_PAINT) { PAINTSTRUCT paint{};HDC dc=BeginPaint(window,&paint);if(scheduledPaint){scheduledPaint=false;lastPresentedFrame=std::chrono::steady_clock::now();render(window,dc);}EndPaint(window,&paint);return 0; }
  if (message == PresentMessage) {presentationPending.store(false);if(uiBenchmark&&std::chrono::duration<double>(std::chrono::steady_clock::now()-uiBenchmarkStart).count()>=uiBenchmarkSeconds){writeDetailedBenchmarkReport();DestroyWindow(window);return 0;}scheduledPaint=true;InvalidateRect(window,nullptr,FALSE);UpdateWindow(window);return 0; }
  if (message == WM_SIZE) { InvalidateRect(window, nullptr, FALSE); return 0; }
  if(message==WM_KEYDOWN&&wParam=='F'){toggleFollowing(window);return 0;}
  if(message==WM_KEYDOWN&&wParam=='G'){familyHighlights=!familyHighlights;return 0;}
  if(message==WM_KEYDOWN&&wParam=='C'){compactFollow=!compactFollow;return 0;}
  if(message==WM_KEYDOWN&&wParam=='D'){if(!renderWatchedPose.alive)followDescendant(window);return 0;}
  if(message==WM_KEYDOWN&&(wParam==VK_OEM_4||wParam==VK_OEM_6)){if(wParam==VK_OEM_4)watchedHistoryOffset=std::min<size_t>(127,watchedHistoryOffset+1);else if(watchedHistoryOffset)--watchedHistoryOffset;nextWatchDiagnostics={};return 0;}
  if(message==WM_KEYDOWN&&wParam=='H'){habitatHistory=!habitatHistory;return 0;}
  if(message==WM_KEYDOWN&&wParam=='W'){weatherVisible=!weatherVisible;return 0;}
  if(message==PersistenceMessage){std::unique_ptr<PersistenceResult> result(reinterpret_cast<PersistenceResult*>(lParam));saveStatus=result->message;if(result->loaded&&result->success){{std::shared_lock lock(simulationMutex);const auto view=simulation.savedView;zoom=std::clamp(view.zoom,.2f,12.f);cameraX=view.cameraX;cameraY=view.cameraY;activeOverlay=Overlay(std::clamp(view.overlay,0,7));selectedId=view.selectedOrganism;weatherVisible=view.weather;atlasOpen=view.atlasOpen;atlasTab=std::clamp(view.atlasTab,0,3);atlasTimelineWindow=std::clamp(view.timelineWindow,120,24000);requestedFps=std::clamp(view.fps/30,1,12)*30;requestedTps=std::clamp(view.tps,1,120);paused=view.paused;activeTool=Tool(std::clamp(view.tool,0,11));nextWorldSeed=view.nextWorldSeed;habitatHistory=view.habitatHistory;clearFollowing(true);const auto preferences=simulation.followPreferences;familyHighlights=preferences.family;compactFollow=preferences.compact;followCamera.target=preferences.target;followCamera.wrapped=preferences.wrapped;followCamera.following=preferences.enabled;const auto pose=simulation.watchedPose();RECT bounds{};GetClientRect(window,&bounds);followCamera.anchorX=life::nearestWrapped(pose.x,(bounds.right-std::min(390.,bounds.right*.32))*.5/zoom-cameraX/zoom,life::WorldWidth);followCamera.anchorY=life::nearestWrapped(pose.y,(bounds.bottom-82)*.5/zoom-cameraY/zoom,life::WorldHeight);}renderDirtyVersions.clear();renderAtlasRevision=UINT64_MAX;measuredTicks=0;}persistenceBusy=false;InvalidateRect(window,nullptr,FALSE);return 0;}
  if(message==WM_KEYDOWN&&(wParam==VK_F5||wParam==VK_F9)){if(persistenceBusy.exchange(true))return 0;const bool loading=wParam==VK_F9;auto savePath=life::appDataDirectory()/"world.lfesave";if(GetKeyState(VK_SHIFT)<0){wchar_t filename[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window;dialog.lpstrFile=filename;dialog.nMaxFile=32768;dialog.lpstrFilter=L"Life Engine world (*.lfesave)\0*.lfesave\0\0";dialog.lpstrDefExt=L"lfesave";dialog.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(loading?OFN_FILEMUSTEXIST:OFN_OVERWRITEPROMPT);if(!(loading?GetOpenFileNameW(&dialog):GetSaveFileNameW(&dialog))){persistenceBusy=false;return 0;}savePath=filename;}const life::NativeSimulation::SavedView view{zoom,cameraX,cameraY,int(activeOverlay),selectedId,requestedFps.load(),requestedTps.load(),atlasTab,atlasTimelineWindow,weatherVisible,atlasOpen,paused.load(),int(activeTool),nextWorldSeed,habitatHistory};const life::FollowPreferences preferences{followCamera.target,followCamera.following,familyHighlights,compactFollow,followCamera.wrapped};saveStatus=loading?"Loading world...":"Saving world...";persistenceThread=std::jthread([window,loading,view,savePath,preferences]{auto result=std::make_unique<PersistenceResult>();result->loaded=loading;try{std::unique_lock lock(simulationMutex);const auto path=savePath;std::filesystem::create_directories(path.parent_path());if(loading)simulation.load(path);else{simulation.savedView=view;simulation.followPreferences=preferences;simulation.save(path);}result->success=true;result->message=loading?"World loaded":"World saved";}catch(const std::exception& error){result->message=error.what();life::logMessage(result->message);}if(PostMessageW(window,PersistenceMessage,0,reinterpret_cast<LPARAM>(result.get())))result.release();});return 0;}
  if(message==WM_KEYDOWN&&(wParam=='A'||wParam=='a')){atlasOpen=!atlasOpen;InvalidateRect(window,nullptr,FALSE);return 0;}
  if (message == WM_TIMER) {
    const auto now=std::chrono::steady_clock::now();
    if(uiBenchmark&&now>=nextBenchmarkInput){std::unique_lock lock(simulationMutex,std::try_to_lock);if(lock.owns_lock()){const int x=int((uiBenchmarkInputs*37)%life::WorldWidth),y=int((uiBenchmarkInputs*53)%life::WorldHeight);simulation.paintResource(x,y,life::ResourceType::Plant,1000);++uiBenchmarkInputs;if(followBenchmark){benchmarkFamilyPeak=std::max(benchmarkFamilyPeak,renderWatched.livingDescendants);if(!renderWatchedPose.alive&&renderWatched.nextDescendant>=0){selectedId=renderWatched.nextDescendant;simulation.select(selectedId);simulation.watch(selectedId);followCamera.clear();}}pendingInput=now;nextBenchmarkInput=now+std::chrono::milliseconds(250);InvalidateRect(window,nullptr,FALSE);}}if(followBenchmark&&!followCamera.following&&renderWatchedPose.alive&&followCamera.target<0)toggleFollowing(window);
    const double sampleSeconds=std::chrono::duration<double>(now-measurementStart).count();if(sampleSeconds>=1.0){measuredTps=measuredTicks.exchange(0)/sampleSeconds;measuredFps=measuredFrames/sampleSeconds;measuredFrames=0;measurementStart=now;}
    lastRenderRequest=now;if(uiBenchmark&&std::chrono::duration<double>(now-uiBenchmarkStart).count()>=uiBenchmarkSeconds){writeDetailedBenchmarkReport();DestroyWindow(window);}return 0;
  }
  if(message==WM_LBUTTONDOWN){RECT client{};GetClientRect(window,&client);if(GET_Y_LPARAM(lParam)>=client.bottom-28&&GET_X_LPARAM(lParam)>=20&&GET_X_LPARAM(lParam)<=192){SendMessageW(window,WM_KEYDOWN,GET_X_LPARAM(lParam)<=103?VK_F5:VK_F9,0);return 0;}}
  if(message==WM_LBUTTONDOWN&&!atlasOpen&&followCamera.target>=0){const RECT bounds=viewportBounds(window);const life::FollowHudLayout layout(bounds.right-std::min(390.f,bounds.right*.32f),compactFollow);const int which=layout.buttonAt(float(GET_X_LPARAM(lParam)),float(GET_Y_LPARAM(lParam)));if(which>=0){if(which==0)toggleFollowing(window);else if(which==1)familyHighlights=!familyHighlights;else if(which==2)compactFollow=!compactFollow;else if(which==3&&!renderWatchedPose.alive)followDescendant(window);return 0;}}
  if (message == WM_LBUTTONDOWN || message == WM_MBUTTONDOWN) {
    const int mouseX=GET_X_LPARAM(lParam),mouseY=GET_Y_LPARAM(lParam);RECT client{};GetClientRect(window,&client);const float aside=std::min(390.0f,client.right*.32f),contentRight=client.right-aside,panelX=contentRight+15,panelRight=client.right-15;
    if(message==WM_LBUTTONDOWN&&mouseY>=632&&mouseY<=656&&mouseX>=panelRight-86&&mouseX<=panelRight-20){toggleFollowing(window);return 0;}
    if(message==WM_LBUTTONDOWN&&!atlasOpen&&followCamera.target>=0&&life::FollowHudLayout(contentRight,compactFollow).contains(float(mouseX),float(mouseY)))return 0;
    if(message==WM_LBUTTONDOWN&&atlasOpen&&mouseX<contentRight&&mouseY>=150){const float tabWidth=(contentRight-48)/4.0f;if(mouseY<=182){atlasTab=std::clamp(int((mouseX-24)/tabWidth),0,3);}else if(atlasTab==1&&mouseY>=212&&!persistenceBusy.load()){const int row=(mouseY-212)/22;if(row>=0&&row<int(renderAtlas.species.size())){const auto species=renderAtlas.species[size_t(row)];std::unique_lock lock(simulationMutex);simulation.selectSpecies(species.id);selectedId=species.representativeId;simulation.select(selectedId);simulation.watch(selectedId);clearFollowing();}}InvalidateRect(window,nullptr,FALSE);return 0;}
    if(message==WM_LBUTTONDOWN&&mouseY>=25&&mouseY<=58&&mouseX>=contentRight-210&&mouseX<=contentRight-116){atlasOpen=!atlasOpen;InvalidateRect(window,nullptr,FALSE);return 0;}
    if(message==WM_LBUTTONDOWN&&mouseX>=contentRight){
      if(mouseY>=194&&mouseY<=227&&mouseX>=panelRight-77&&mouseX<=panelRight-20){paused.store(!paused.load());}
      else if(mouseY>=269&&mouseY<=288&&mouseX>=panelX+20&&mouseX<=panelRight-20){requestedTps.store(std::clamp(1+int((mouseX-(panelX+20))*119/std::max(1.0f,panelRight-panelX-40)),1,120));}
      else if(mouseY>=287&&mouseY<331){const float fpsLeft=panelX+53,fpsGap=3,fpsWidth=(panelRight-fpsLeft-20-fpsGap*5)/6;const int row=(mouseY-287)/24,column=int((mouseX-fpsLeft)/(fpsWidth+fpsGap));if(mouseX>=fpsLeft&&column>=0&&column<6)requestedFps.store((row*6+column+1)*30);}
      else if(mouseY>=337&&mouseY<=363&&mouseX>=panelX+82&&mouseX<=panelRight-20){activeOverlay=Overlay((int(activeOverlay)+1)%8);}
      else if(mouseY>=372&&mouseY<=407&&mouseX>=panelX+20&&mouseX<=panelRight-20){if(persistenceBusy.load())return 0;const float half=(panelRight-panelX-45)/2;if(mouseX<=panelX+20+half&&MessageBoxW(window,L"Reset life and clear the Evolution Atlas history?",L"Life Engine",MB_ICONWARNING|MB_OKCANCEL)!=IDOK)return 0;std::unique_lock lock(simulationMutex);if(mouseX<=panelX+20+half)simulation.reset();else simulation.regenerate(nextWorldSeed++);selectedId=-1;simulation.select(-1);simulation.watch(-1);clearFollowing(true);cameraX=0;cameraY=0;}
      else if(mouseY>=463&&mouseY<=570){const float buttonWidth=(aside-75)/6;const int row=(mouseY-463)/38,column=int((mouseX-(panelX+20))/(buttonWidth+5));const int tool=row*6+column;if(column>=0&&column<6&&tool>=0&&tool<12)activeTool=Tool(tool);}
      else if(mouseY>=598&&mouseY<=627&&mouseX>=panelRight-62&&mouseX<=panelRight-20){if(persistenceBusy.load())return 0;std::unique_lock lock(simulationMutex);selectedId=-1;simulation.select(-1);simulation.watch(-1);clearFollowing();}
      InvalidateRect(window,nullptr,FALSE);return 0;
    }
    if(mouseY>=82&&mouseX<contentRight){if(message==WM_MBUTTONDOWN||activeTool==Tool::Pan){followCamera.release();dragging=true;dragOrigin={mouseX,mouseY};dragCameraX=cameraX;dragCameraY=cameraY;SetCapture(window);}else applyTool(mouseX,mouseY);InvalidateRect(window,nullptr,FALSE);}return 0;
  }
  if(message==WM_MOUSEMOVE)senseHover={GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)};
  if (message == WM_MOUSEMOVE && dragging) {
    cameraX = dragCameraX + GET_X_LPARAM(lParam) - dragOrigin.x;
    cameraY = dragCameraY + GET_Y_LPARAM(lParam) - dragOrigin.y;
    InvalidateRect(window, nullptr, FALSE); return 0;
  }
  if(message==WM_MOUSEMOVE&&(wParam&MK_LBUTTON)&&!dragging&&activeTool!=Tool::Inspect&&activeTool!=Tool::Pan){applyTool(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam));InvalidateRect(window,nullptr,FALSE);return 0;}
  if (message == WM_LBUTTONUP || message == WM_MBUTTONUP) { dragging = false; ReleaseCapture(); return 0; }
  if (message == WM_MOUSEWHEEL) {
    POINT cursor{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}; ScreenToClient(window, &cursor);
    RECT client{};GetClientRect(window,&client);const float aside=std::min(390.0f,client.right*.32f),contentRight=client.right-aside;if(atlasOpen&&cursor.x<contentRight){atlasTimelineWindow=std::clamp(atlasTimelineWindow+(GET_WHEEL_DELTA_WPARAM(wParam)>0?-120:120),120,24000);InvalidateRect(window,nullptr,FALSE);return 0;}
    const float previous = zoom; zoom = std::clamp(zoom * (GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 1.2f : 1.0f / 1.2f), .2f, 12.0f);
    cameraX = cursor.x - (cursor.x - cameraX) * zoom / previous;
    cameraY = cursor.y-82 - (cursor.y-82-cameraY) * zoom / previous;
    InvalidateRect(window, nullptr, FALSE); return 0;
  }
  if (message == WM_DESTROY) { if(uiBenchmark){PostQuitMessage(0);return 0;}RECT bounds{};if(GetWindowRect(window,&bounds)){appSettings.windowX=bounds.left;appSettings.windowY=bounds.top;appSettings.windowWidth=bounds.right-bounds.left;appSettings.windowHeight=bounds.bottom-bounds.top;}appSettings.maximized=IsZoomed(window)!=FALSE;appSettings.requestedTps=requestedTps.load();appSettings.requestedFps=requestedFps.load();appSettings.overlay=int(activeOverlay);if(!life::saveSettings(appSettings))life::logMessage("warning: settings could not be saved");life::logMessage("clean shutdown");PostQuitMessage(0); return 0; }
  return DefWindowProcW(window, message, wParam, lParam);
}
}

LONG WINAPI reportCrash(EXCEPTION_POINTERS* details){std::ostringstream message;message<<"unhandled exception code=0x"<<std::hex<<(details&&details->ExceptionRecord?details->ExceptionRecord->ExceptionCode:0)<<" address="<<(details&&details->ExceptionRecord?details->ExceptionRecord->ExceptionAddress:nullptr);life::logMessage(message.str());return EXCEPTION_EXECUTE_HANDLER;}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR commandLine, int show) {
  const std::wstring arguments=commandLine?commandLine:L"";headlessMode=arguments.find(L"--headless-script")!=std::wstring::npos;
  if(headlessMode)SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
  if(!headlessMode){life::initializeLog();SetUnhandledExceptionFilter(reportCrash);appSettings=life::loadSettings();requestedTps.store(appSettings.requestedTps);requestedFps.store(appSettings.requestedFps);activeOverlay=Overlay(appSettings.overlay);}
  if(const auto marker=arguments.find(L"--ui-benchmark");marker!=std::wstring::npos){uiBenchmark=true;const auto equals=arguments.find(L'=',marker);if(equals!=std::wstring::npos)uiBenchmarkSeconds=std::clamp(_wtoi(arguments.c_str()+equals+1),1,600);simulation.seedBenchmarkMovers(10'000);requestedTps.store(60);requestedFps.store(60);}if(const auto marker=arguments.find(L"--fps=");marker!=std::wstring::npos){const int value=_wtoi(arguments.c_str()+marker+6);if(value>=30&&value<=360&&value%30==0)requestedFps.store(value);}
  if(const auto marker=arguments.find(L"--follow-benchmark");marker!=std::wstring::npos){uiBenchmark=followBenchmark=true;const auto equals=arguments.find(L'=',marker);if(equals!=std::wstring::npos)uiBenchmarkSeconds=std::clamp(_wtoi(arguments.c_str()+equals+1),1,600);simulation.seedBenchmarkMovers(10'000);selectedId=simulation.seedBenchmarkFamily();simulation.select(selectedId);simulation.watch(selectedId);requestedTps=60;requestedFps=60;}
  try { api = loadSkia();if(!headlessMode)life::logMessage("Skia renderer loaded"); }
  catch (const std::exception& error) {if(headlessMode)return 3;life::logMessage(std::string("Skia initialization failed: ")+error.what());MessageBoxW(nullptr,L"The embedded Skia renderer could not be loaded. See the local diagnostic log for details.",life::AppDisplayName,MB_ICONERROR);return 1;}
  catch (...) {if(headlessMode)return 3;life::logMessage("Skia initialization failed with an unknown error");MessageBoxW(nullptr,L"The embedded Skia renderer could not be loaded. See the local diagnostic log for details.",life::AppDisplayName,MB_ICONERROR);return 1;}
  if(arguments.find(L"--self-test")!=std::wstring::npos){try{life::NativeSimulation smoke(96,60,.2,500,53,17);for(int tick=0;tick<120;++tick)smoke.step();const auto metrics=smoke.metrics();if(metrics.ticks!=120||metrics.organisms<1||!api)throw std::runtime_error("simulation invariant failed");const auto report=libraryPath().parent_path()/"self-test.txt";std::ofstream output(report);output<<"LFE-NNUE v"<<life::AppVersion<<" self-test PASS\n"<<"ticks="<<metrics.ticks<<" organisms="<<metrics.organisms<<"\n";life::logMessage("self-test passed");api.reset();return 0;}catch(const std::exception& error){life::logMessage(std::string("self-test failed: ")+error.what());api.reset();return 2;}}
  fontManager=api->fontManagerCreateDefault();auto normal=api->fontStyleNew(400,5,0),bold=api->fontStyleNew(700,5,0);regularTypeface=api->fontManagerCreateTypeface(fontManager,"Arial",normal);boldTypeface=api->fontManagerCreateTypeface(fontManager,"Arial",bold);api->fontStyleDelete(normal);api->fontStyleDelete(bold);
  if(headlessMode){renderWorkers=std::make_unique<RenderWorkers>();timeBeginPeriod(1);const int result=runHeadlessScript(headlessScriptPath(arguments));timeEndPeriod(1);renderWorkers.reset();if(regularTypeface)api->typefaceUnref(regularTypeface);if(boldTypeface)api->typefaceUnref(boldTypeface);if(fontManager)api->fontManagerUnref(fontManager);api.reset();return result;}
  WNDCLASSW type{}; type.lpfnWndProc = procedure; type.hInstance = instance;
  type.lpszClassName = L"LifeEngineSkiaPreview"; type.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  RegisterClassW(&type);
  const int initialX=appSettings.windowX<0?CW_USEDEFAULT:appSettings.windowX,initialY=appSettings.windowY<0?CW_USEDEFAULT:appSettings.windowY;
  HWND window = CreateWindowExW(0,type.lpszClassName,life::AppDisplayName,WS_OVERLAPPEDWINDOW,initialX,initialY,appSettings.windowWidth,appSettings.windowHeight,nullptr,nullptr,instance,nullptr);
  if(!window){life::logMessage("window creation failed");MessageBoxW(nullptr,L"The application window could not be created.",life::AppDisplayName,MB_ICONERROR);api.reset();return 1;}
  renderWorkers=std::make_unique<RenderWorkers>();
  ShowWindow(window,appSettings.maximized?SW_SHOWMAXIMIZED:show);life::logMessage("main window ready");
  if(followBenchmark)toggleFollowing(window);
  timeBeginPeriod(1);
  if(uiBenchmark){uiBenchmarkStart=nextBenchmarkInput=measurementStart=lastRenderRequest=previousBenchmarkFrame=std::chrono::steady_clock::now();uiBenchmarkTicks.store(0);uiBenchmarkFrames=uiBenchmarkInputs=measuredFrames=0;uiBenchmarkRenderMs=0;presentationDrops.store(0);inputLatenciesMs.clear();frameIntervalsMs.clear();pendingInput.reset();}
  simulationThread=std::jthread([](std::stop_token stop){auto next=std::chrono::steady_clock::now();while(!stop.stop_requested()){if(paused.load()){next=std::chrono::steady_clock::now();std::this_thread::sleep_for(std::chrono::milliseconds(2));continue;}const int target=std::max(1,requestedTps.load());const auto interval=std::chrono::duration<double>(1.0/target);const auto now=std::chrono::steady_clock::now();if(now<next){std::this_thread::sleep_until(next);continue;}{std::unique_lock lock(simulationMutex);simulation.step();}measuredTicks.fetch_add(1);if(uiBenchmark)uiBenchmarkTicks.fetch_add(1);next+=std::chrono::duration_cast<std::chrono::steady_clock::duration>(interval);if(std::chrono::steady_clock::now()-next>std::chrono::milliseconds(250))next=std::chrono::steady_clock::now();}});
  presentationThread=std::jthread([window](std::stop_token stop){auto next=std::chrono::steady_clock::now();while(!stop.stop_requested()){const int target=std::clamp(requestedFps.load(),30,360);next+=std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(1.0/target));if(!presentationPending.exchange(true))PostMessageW(window,PresentMessage,0,0);else presentationDrops.fetch_add(1);std::this_thread::sleep_until(next);if(std::chrono::steady_clock::now()-next>std::chrono::milliseconds(100))next=std::chrono::steady_clock::now();}});
  SetTimer(window, 1, 50, nullptr);
  MSG message{}; while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
  simulationThread.request_stop();presentationThread.request_stop();simulationThread.join();presentationThread.join();renderWorkers.reset();timeEndPeriod(1);
  if(regularTypeface)api->typefaceUnref(regularTypeface);if(boldTypeface)api->typefaceUnref(boldTypeface);if(fontManager)api->fontManagerUnref(fontManager);api.reset(); return int(message.wParam);
}
