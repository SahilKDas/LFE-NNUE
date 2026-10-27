void updateFamilyPixels(size_t begin,size_t count){
  if(renderFamily.empty()){std::fill_n(renderFamilyCells.begin()+begin,count,uint8_t(0));return;}
  for(size_t index=begin;index<begin+count;++index){if(renderOwners[index]<0){renderFamilyCells[index]=0;continue;}const auto found=renderFamily.find(renderOwners[index]);renderFamilyCells[index]=found==renderFamily.end()?0:uint8_t(found->second);}
}
void updateFamilyEdges(int x,int y,int width,int height){
  for(int row=y;row<y+height;++row)for(int column=x;column<x+width;++column){const int wx=life::wrapCoordinate(column,life::WorldWidth),wy=life::wrapCoordinate(row,life::WorldHeight),index=wy*life::WorldWidth+wx;uint8_t edges=0;const int neighbors[]={wy*life::WorldWidth+life::wrapCoordinate(wx-1,life::WorldWidth),wy*life::WorldWidth+life::wrapCoordinate(wx+1,life::WorldWidth),life::wrapCoordinate(wy-1,life::WorldHeight)*life::WorldWidth+wx,life::wrapCoordinate(wy+1,life::WorldHeight)*life::WorldWidth+wx};
    for(int side=0;side<4;++side){uint8_t role=renderCells[index]?renderFamilyCells[index]:renderFamilyCells[neighbors[side]];if(renderCells[index]&&renderOwners[index]==renderOwners[neighbors[side]])role=0;edges|=uint8_t(role<<(side*2));}renderFamilyEdges[index]=edges;
  }
}
uint32_t familyOutline(uint32_t color,int index,int screenX,int screenY,double sx,double sy){
  const uint8_t edges=renderFamilyEdges[index];if(!edges||(renderCells[index]&&zoom<3))return color;
  const double fx=sx-std::floor(sx),fy=sy-std::floor(sy),edge=std::min(.5,(renderCells[index]?1.:1.4)/zoom);uint8_t role=0;
  const auto include=[&](uint8_t candidate){if(candidate&&(role==0||candidate<role))role=candidate;};if(fx<edge)include(edges&3);if(fx>1-edge)include((edges>>2)&3);if(fy<edge)include((edges>>4)&3);if(fy>1-edge)include((edges>>6)&3);
  if(role==1)return 0xffe5c56f;if(role==2&&(screenX+screenY)%8<5)return 0xff81d2c7;if(role==3&&(screenX+screenY)%5==0)return 0xffb88cde;return color;
}
void updateDerivedFamily(bool refreshAll){
  if(followCamera.target<0&&renderFamily.empty()){renderFamilyRoot=-1;familyOwnerDirtyTiles.clear();return;}
  constexpr int tileSize=life::NativeSimulation::dirtyTileSize(),columns=(life::WorldWidth+tileSize-1)/tileSize;
  if(refreshAll){renderWorkers->run([&](int partition){const size_t begin=renderOwners.size()*partition/10,end=renderOwners.size()*(partition+1)/10;updateFamilyPixels(begin,end-begin);});}
  else if(!familyOwnerDirtyTiles.empty())renderWorkers->run([&](int partition){for(size_t offset=partition;offset<familyOwnerDirtyTiles.size();offset+=10){const uint16_t tile=familyOwnerDirtyTiles[offset];const int x=(tile%columns)*tileSize,y=(tile/columns)*tileSize;for(int row=y;row<std::min(y+tileSize,life::WorldHeight);++row)updateFamilyPixels(size_t(row)*life::WorldWidth+x,std::min(tileSize,life::WorldWidth-x));}});

  static bool edgesEnabled=false;const bool enabled=followCamera.target>=0&&familyHighlights&&!renderFamily.empty();
  if(enabled){if(refreshAll||!edgesEnabled)renderWorkers->run([&](int partition){const int begin=life::WorldHeight*partition/10,end=life::WorldHeight*(partition+1)/10;updateFamilyEdges(0,begin,life::WorldWidth,end-begin);});
    else if(!familyOwnerDirtyTiles.empty())renderWorkers->run([&](int partition){const int begin=life::WorldHeight*partition/10,end=life::WorldHeight*(partition+1)/10;for(uint16_t tile:familyOwnerDirtyTiles){const int x=(tile%columns)*tileSize,y=(tile/columns)*tileSize;for(int row=y-1;row<y+std::min(tileSize,life::WorldHeight-y)+1;++row){const int wy=life::wrapCoordinate(row,life::WorldHeight);if(wy>=begin&&wy<end)updateFamilyEdges(x-1,wy,std::min(tileSize,life::WorldWidth-x)+2,1);}}});}
  edgesEnabled=enabled;renderFamilyRoot=followCamera.target;
  familyOwnerDirtyTiles.clear();
}

#include "presentation.inc"

void publishWatchFrame(){
  presentationRoot=followCamera.target;presentationHistory=watchedHistoryOffset;
  if(presentationEnabled.load(std::memory_order_relaxed)){if(!consumeSimulationFrame())updateDerivedFamily(false);return;}
  std::shared_lock lock(simulationMutex,std::defer_lock);if(renderCells.empty())lock.lock();else if(!lock.try_lock())return;
  ++watchPublishedRevision;
  constexpr int familyTileSize=life::NativeSimulation::dirtyTileSize(),familyColumns=(life::WorldWidth+familyTileSize-1)/familyTileSize;
  std::array<uint8_t,familyColumns*((life::WorldHeight+familyTileSize-1)/familyTileSize)> familyTileDirty{};
  const bool initial=renderCells.empty();if(initial){const size_t size=simulation.cells.size();renderCells.resize(size);renderTerrain.resize(size);renderResources.resize(size);renderNutrients.resize(size);renderElevation.resize(size);renderHabitat.resize(size);renderOwners.resize(size);renderFamilyCells.resize(size);renderFamilyEdges.resize(size);}
  renderWatchedPose=simulation.watchedPose();
  renderMetrics.ticks=simulation.tickCount();
  const auto now=std::chrono::steady_clock::now();const bool familyRootChanged=renderFamilyRoot!=followCamera.target;
  if(now>=nextWatchDiagnostics){nextWatchDiagnostics=now+std::chrono::milliseconds(100);renderMetrics=simulation.metrics();renderInspection=simulation.inspect(selectedId);renderWatched=simulation.watchedSnapshot(watchedHistoryOffset);
    if(followCamera.target>=0){std::unordered_map<int,life::FamilyRole> family;bool added=false;for(const auto& [id,role]:simulation.familyRoles(followCamera.target))if(role!=life::FamilyRole::Unrelated){family.emplace(id,role);const auto old=renderFamily.find(id);added=added||old==renderFamily.end()||old->second!=role;}
      if(added&&!familyRootChanged)for(const auto& organism:simulation.organisms()){const auto current=family.find(organism.id),old=renderFamily.find(organism.id);if(!organism.living||current==family.end()||(old!=renderFamily.end()&&old->second==current->second))continue;for(const auto& cell:organism.cells){int dx=cell.x,dy=cell.y;if(organism.rotation==1){dx=-cell.x;dy=-cell.y;}else if(organism.rotation==2){dx=cell.y;dy=-cell.x;}else if(organism.rotation==3){dx=-cell.y;dy=cell.x;}const int x=life::wrapCoordinate(organism.x+dx,life::WorldWidth),y=life::wrapCoordinate(organism.y+dy,life::WorldHeight);familyTileDirty[(y/familyTileSize)*familyColumns+x/familyTileSize]=1;}}
      renderFamily=std::move(family);}
  }
  const auto dirty=simulation.changedTiles(renderDirtyVersions);renderMetrics.dirtyTiles=int(dirty.size());frameDirtyTiles=dirty;
  constexpr int tileSize=life::NativeSimulation::dirtyTileSize();const int columns=(life::WorldWidth+tileSize-1)/tileSize,count=columns*((life::WorldHeight+tileSize-1)/tileSize);
  familyOwnerDirtyTiles.clear();for(uint16_t tile:dirty){const int x=(tile%columns)*tileSize,y=(tile/columns)*tileSize;bool changed=false;for(int row=y;row<std::min(y+tileSize,life::WorldHeight)&&!changed;++row){const size_t begin=size_t(row)*life::WorldWidth+x;changed=!std::equal(simulation.owners.begin()+begin,simulation.owners.begin()+begin+std::min(tileSize,life::WorldWidth-x),renderOwners.begin()+begin);}if(changed)familyTileDirty[tile]=1;}for(size_t tile=0;tile<familyTileDirty.size();++tile)if(familyTileDirty[tile])familyOwnerDirtyTiles.push_back(uint16_t(tile));
  const bool full=initial||int(dirty.size())>count/4;frameCopiedAll=frameCopiedAll||initial||int(dirty.size())>count*3/4;
  backgroundCopiedAll=true; // Legacy synchronous path has no independent static-layer versions.
  if(full){renderCells=simulation.cells;renderTerrain=simulation.world.terrain;renderResources=simulation.world.resources;renderNutrients=simulation.world.nutrients;renderElevation=simulation.world.elevation;renderHabitat=simulation.world.habitat;renderOwners=simulation.owners;}
  else for(const uint16_t tile:dirty){const int tx=(tile%columns)*tileSize,ty=(tile/columns)*tileSize;for(int y=ty;y<std::min(ty+tileSize,life::WorldHeight);++y){const int begin=y*life::WorldWidth+tx,length=std::min(tileSize,life::WorldWidth-tx);std::copy_n(simulation.cells.begin()+begin,length,renderCells.begin()+begin);std::copy_n(simulation.world.terrain.begin()+begin,length,renderTerrain.begin()+begin);std::copy_n(simulation.world.resources.begin()+begin,length,renderResources.begin()+begin);std::copy_n(simulation.world.nutrients.begin()+begin,length,renderNutrients.begin()+begin);std::copy_n(simulation.world.elevation.begin()+begin,length,renderElevation.begin()+begin);std::copy_n(simulation.world.habitat.begin()+begin,length,renderHabitat.begin()+begin);std::copy_n(simulation.owners.begin()+begin,length,renderOwners.begin()+begin);}}
  lock.unlock();
  updateDerivedFamily(initial||familyRootChanged);
}

void drawFollowCard(life::skia::Api::Canvas canvas,life::skia::Api::Paint paint,float viewportRight){
  if(followCamera.target<0||renderWatched.pose.id!=followCamera.target)return;
  const life::FollowHudLayout layout(viewportRight,compactFollow);if(!layout.visible())return;const float left=layout.left,top=layout.top,right=layout.right,bottom=layout.bottom;
  rectangle(canvas,paint,left,top,right,bottom,0xee121d29);
  const auto& watched=renderWatched;const bool alive=renderWatchedPose.alive;
  text(canvas,paint,"#"+std::to_string(watched.pose.id)+" · Gen "+std::to_string(watched.generation)+" · Age "+std::to_string(watched.age)+(alive?(followCamera.following?" · Following":" · Released"):" · Life ended"),left+12,top+22,12,0xffe5c56f,true);
  text(canvas,paint,alive?"Energy "+std::to_string(watched.energy/1000.0).substr(0,5)+" · "+watched.habitat:"Cause: "+watched.deathCause,left+12,top+41,10,0xffffffff);
  text(canvas,paint,"Offspring "+std::string(watched.offspringCountComplete?"":"at least ")+std::to_string(watched.directChildren)+" ("+std::to_string(watched.livingChildren)+" living) · Surviving descendants "+std::to_string(watched.livingDescendants),left+12,top+59,10,0xffb8c3d1);
  const float button=layout.buttonWidth;const std::array<std::string,4> labels{followCamera.following?"Release (F)":"Follow (F)",familyHighlights?"Family on (G)":"Family off (G)",layout.compact?"Details (C)":"Compact (C)","Descendant (D)"};
  for(int index=0;index<4;++index){const float x=left+6+index*(button+6);const bool disabled=(index==0&&!alive)||(index==3&&(alive||watched.nextDescendant<0));rectangle(canvas,paint,x,top+69,x+button,top+92,disabled?0xff293644:0xff416788);text(canvas,paint,labels[index],x+5,top+84,9,disabled?0xff66738a:0xffffffff);}
  if(layout.compact){text(canvas,paint,alive?(viewportRight<950?"Compact at this size · Enlarge the window for sensory details":"Gold: watched · Cyan dashes: children · Violet dots: descendants"):"Camera held here · Choose a descendant to continue",left+12,top+107,9,0xff81d2c7);return;}
  if(!watched.pose.mover){text(canvas,paint,"No movement network",left+12,top+124,12,0xff81d2c7);return;}
  if(!watched.hasDecision){text(canvas,paint,alive?"Waiting for the next neural decision...":"No decision history in this session; life summary retained.",left+12,top+124,11,0xffb8c3d1);return;}
  const auto& decision=watched.decision;
  text(canvas,paint,"Inputs at tick "+std::to_string(decision.tick)+" ("+std::to_string(std::max(0,renderMetrics.ticks-decision.tick))+" ticks old) · [ ] history "+std::to_string(std::min<size_t>(watchedHistoryOffset+1,watched.decisionCount))+"/"+std::to_string(watched.decisionCount),left+12,top+119,10,0xffffffff);
  constexpr float tile=18;const float gx=left+12,gy=top+136;const auto senses=life::decodeSenses(decision);std::string hovered;
  for(const auto& cell:senses){const float x=gx+(cell.dx+4)*tile,y=gy+(cell.dy+4)*tile;uint32_t color=0xff293644;if(cell.categories&(1u<<5))color=0xffb64a4a;else if(cell.categories&(1u<<4)||cell.categories&0x780)color=0xff416788;else if(cell.categories&(1u<<1))color=0xff367b46;else if(cell.categories&(1u<<12))color=0xff88543b;else if(cell.categories&(1u<<13))color=0xffb49c45;else if(cell.nutrients)color=0xff62864f;else if(cell.categories)color=0xff455568;rectangle(canvas,paint,x,y,x+tile-2,y+tile-2,color);if(senseHover.x>=x&&senseHover.x<x+tile&&senseHover.y>=y&&senseHover.y<y+tile)hovered="("+std::to_string(cell.dx)+", "+std::to_string(cell.dy)+") "+life::senseLabel(cell);}
  rectangle(canvas,paint,gx+4*tile,gy+4*tile,gx+5*tile-2,gy+5*tile-2,0xffe5c56f);
  constexpr const char* actions[]={"Up","Down","Left","Right","Wait"};const float chartX=left+196;
  float magnitude=.001f;for(float score:decision.outputs)magnitude=std::max(magnitude,std::abs(score));
  text(canvas,paint,"Neural outputs",chartX,gy+4,11,0xff81d2c7,true);
  for(int action=0;action<5;++action){const float y=gy+26+action*21;const uint32_t color=action==decision.action?0xffe5c56f:0xff9099c2;std::ostringstream label;label<<actions[action]<<" "<<std::fixed<<std::setprecision(3)<<decision.outputs[action]<<(action==decision.action?"  <":"");text(canvas,paint,label.str(),chartX,y,10,color,action==decision.action);rectangle(canvas,paint,chartX,y+4,chartX+std::min(120.f,right-chartX-12)*std::abs(decision.outputs[action])/magnitude,y+7,color);}
  text(canvas,paint,"Decision pose "+std::to_string(decision.x)+", "+std::to_string(decision.y)+" · rotation "+std::to_string(decision.rotation),chartX,gy+150,9,0xffb8c3d1);
  constexpr const char* bands[]={"Cold","Cool","Mild","Warm","Hot"};constexpr const char* seasons[]={"Spring","Summer","Autumn","Winter"};
  text(canvas,paint,(decision.channels&life::Temperature)?std::string("Temperature: ")+bands[std::clamp(decision.temperatureBand,0,4)]+" · N/S gradient "+std::to_string(decision.gradient):"Temperature: disabled",left+12,top+315,9,0xffb8c3d1);
  text(canvas,paint,(decision.channels&life::Fertility)?std::string("Season: ")+seasons[decision.season]+" · Fertility band "+std::to_string(decision.fertilityBand):"Season / fertility: disabled",left+12,top+329,9,0xffb8c3d1);
  text(canvas,paint,(decision.channels&life::Internal)?std::string("Internal: ")+(decision.highEnergy?"high energy":"low energy")+(decision.damaged?" · injured":" · healthy")+" · heading "+actions[std::clamp(decision.direction,0,4)]:"Internal state: disabled",left+12,top+343,9,0xffb8c3d1);
  constexpr const char* channels[]={"Organism","Heading","Danger","Resources","Terrain","Temperature","Fertility","Internal","Carrion density","Soil nutrients"};
  for(int row=0;row<5;++row){std::string value;for(int column=0;column<2;++column){const int bit=row*2+column;if(column)value+=" · ";value+=channels[bit];value+=(decision.channels&(1<<bit))?": on":": disabled";}text(canvas,paint,value,left+12,top+361+row*12,9,0xffb8c3d1);}
  if(hovered.empty())hovered="Hover a sense tile · Gold self · Cyan children · Violet descendants";
  text(canvas,paint,hovered.substr(0,76),left+12,bottom-8,9,0xff81d2c7);
}
