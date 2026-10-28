uint32_t worldCellColor(int x,int y,const life::ClimateSample& climate,bool background=false){
  const int index=y*life::WorldWidth+x;const auto terrain=renderTerrain[index],resource=renderResources[index],cell=background?uint8_t(0):renderCells[index];const auto overlay=background?Overlay::Terrain:activeOverlay;
  uint32_t base=terrainColors[terrain];
  if(overlay==Overlay::Resources)base=resource?resourceColors[resource]:0xff202d3b;
  else if(overlay==Overlay::Productivity){const double factor=terrain==uint8_t(life::TerrainType::Fertile)?1.35:terrain==uint8_t(life::TerrainType::Desert)?.3:terrain==uint8_t(life::TerrainType::Plains)?1.:0.;base=0xff202020u|uint32_t(std::clamp(int(45+145*climate.fertility*factor/1.82),35,190))<<8;}
  else if(overlay==Overlay::Climate){base=0xff000000u|uint32_t(int(45+190*climate.temperature))<<16|0x00282800u|uint32_t(int(45+190*(1-climate.temperature)));}
  else if(overlay==Overlay::Nutrients)base=0xff281d16u|uint32_t(std::min(255,30+int(renderNutrients[index])*225/1600))<<8;
  else if(overlay==Overlay::Decomposition){const int carrion=resource==uint8_t(life::ResourceType::Carrion)?200:0,nutrient=std::min(180,int(renderNutrients[index])*180/1600);base=0xff000000u|uint32_t(55+carrion)<<16|uint32_t(35+nutrient)<<8|45;}
  uint32_t color=cell?cellColors[cell]:base;
  if(overlay==Overlay::Species&&cell&&renderOwnersReady){const auto found=renderSpeciesByOwner->find(renderOwners[index]);if(found!=renderSpeciesByOwner->end()&&found->second>0){const uint32_t hash=uint32_t(found->second)*2654435761u;color=0xff000000u|((64+(hash&127))<<16)|((64+((hash>>8)&127))<<8)|(64+((hash>>16)&127));}}
  return color;
}
uint32_t cachedWorldCellColor(int x,int y,const life::ClimateSample& climate){
  const int index=y*life::WorldWidth+x;const auto cell=renderCells[index];if(cell&&activeOverlay!=Overlay::Species)return cellColors[cell];
  if(activeOverlay==Overlay::Terrain)return cell?cellColors[cell]:terrainBackground[index];return worldCellColor(x,y,climate);
}
void prepareTerrainBackground(){
  constexpr int tileSize=life::NativeSimulation::dirtyTileSize(),columns=(life::WorldWidth+tileSize-1)/tileSize;
  const bool all=backgroundCopiedAll||terrainBackground.size()!=renderCells.size();terrainBackground.resize(renderCells.size());
  if(all)renderWorkers->run([&](int partition){for(int y=life::WorldHeight*partition/10;y<life::WorldHeight*(partition+1)/10;++y)for(int x=0;x<life::WorldWidth;++x)terrainBackground[y*life::WorldWidth+x]=worldCellColor(x,y,{},true);});
  else if(!backgroundDirtyTiles.empty())renderWorkers->run([&](int partition){const int begin=life::WorldHeight*partition/10,end=life::WorldHeight*(partition+1)/10;for(const uint16_t tile:backgroundDirtyTiles){const int tx=(tile%columns)*tileSize,ty=(tile/columns)*tileSize;for(int y=std::max(begin,ty);y<std::min(end,std::min(life::WorldHeight,ty+tileSize+1));++y)for(int x=tx;x<std::min(life::WorldWidth,tx+tileSize+1);++x)terrainBackground[y*life::WorldWidth+x]=worldCellColor(x,y,{},true);}});
  backgroundCopiedAll=false;backgroundDirtyTiles.clear();
}
void prepareWorldPalette(){
  prepareTerrainBackground();
  static int previousOverlay=-1,previousClimateTick=-1;static uint64_t previousSpecies=UINT64_MAX;static bool previousOwnersReady=false;const bool climate=activeOverlay==Overlay::Climate||activeOverlay==Overlay::Productivity;
  const bool all=renderPalette.size()!=renderCells.size()||frameCopiedAll||previousOverlay!=int(activeOverlay)||(climate&&previousClimateTick!=renderMetrics.ticks/30)||(activeOverlay==Overlay::Species&&(previousSpecies!=renderAtlasRevision||previousOwnersReady!=renderOwnersReady));
  renderPalette.resize(renderCells.size());std::array<life::ClimateSample,life::WorldHeight> climateRows;if(climate)for(int y=0;y<life::WorldHeight;++y)climateRows[y]=life::climateAt(y,life::WorldHeight,renderMetrics.ticks);
  if(all)renderWorkers->run([&](int partition){const int begin=life::WorldHeight*partition/10,end=life::WorldHeight*(partition+1)/10;for(int y=begin;y<end;++y){const auto local=climate?climateRows[y]:life::ClimateSample{};for(int x=0;x<life::WorldWidth;++x)renderPalette[y*life::WorldWidth+x]=cachedWorldCellColor(x,y,local);}});
  else if(!frameDirtyTiles.empty()){
    // Keep adjacent tile edges coherent when rebuilding the clipped palette.
    // Partition by disjoint output rows so overlapping dirty tiles never race.
    constexpr int tileSize=life::NativeSimulation::dirtyTileSize(),columns=(life::WorldWidth+tileSize-1)/tileSize;
    renderWorkers->run([&](int partition){const int begin=life::WorldHeight*partition/10,end=life::WorldHeight*(partition+1)/10;for(uint16_t tile:frameDirtyTiles){const int tx=(tile%columns)*tileSize,ty=(tile/columns)*tileSize;for(int y=std::max(begin,ty);y<std::min(end,std::min(life::WorldHeight,ty+tileSize+1));++y){const auto local=climate?climateRows[y]:life::ClimateSample{};for(int x=tx;x<std::min(life::WorldWidth,tx+tileSize+1);++x)renderPalette[y*life::WorldWidth+x]=cachedWorldCellColor(x,y,local);}}});
  }
  previousOverlay=int(activeOverlay);previousClimateTick=renderMetrics.ticks/30;previousSpecies=renderAtlasRevision;previousOwnersReady=renderOwnersReady;frameCopiedAll=false;frameDirtyTiles.clear();
}
