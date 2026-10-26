uint32_t worldCellColor(int x,int y,const life::ClimateSample& climate){
  const int index=y*life::WorldWidth+x;const auto terrain=renderTerrain[index],resource=renderResources[index],habitat=renderHabitat[index],cell=renderCells[index];
  uint32_t base=habitat==uint8_t(life::HabitatType::Cave)?0xff352f46:habitat==uint8_t(life::HabitatType::Bridge)?0xff8b6b3f:terrainColors[terrain];
  if(activeOverlay==Overlay::Habitat){const int elevation=renderElevation[index];base=habitat==uint8_t(life::HabitatType::Cave)?0xff5b4670:habitat==uint8_t(life::HabitatType::Bridge)?0xffd19b52:0xff000000u|uint32_t(25+elevation/3)<<16|uint32_t(45+elevation/2)<<8|uint32_t(35+elevation/4);}
  else if(activeOverlay==Overlay::Resources)base=resource?resourceColors[resource]:0xff202d3b;
  else if(activeOverlay==Overlay::Productivity){const double factor=terrain==uint8_t(life::TerrainType::Fertile)?1.35:terrain==uint8_t(life::TerrainType::Desert)?.3:terrain==uint8_t(life::TerrainType::Plains)?1.:0.;base=0xff202020u|uint32_t(std::clamp(int(45+145*climate.fertility*factor/1.82),35,190))<<8;}
  else if(activeOverlay==Overlay::Climate){base=0xff000000u|uint32_t(int(45+190*climate.temperature))<<16|0x00282800u|uint32_t(int(45+190*(1-climate.temperature)));}
  else if(activeOverlay==Overlay::Nutrients)base=0xff281d16u|uint32_t(std::min(255,30+int(renderNutrients[index])*225/1600))<<8;
  else if(activeOverlay==Overlay::Decomposition){const int carrion=resource==uint8_t(life::ResourceType::Carrion)?200:0,nutrient=std::min(180,int(renderNutrients[index])*180/1600);base=0xff000000u|uint32_t(55+carrion)<<16|uint32_t(35+nutrient)<<8|45;}
  if(activeOverlay==Overlay::Terrain&&!cell){const int west=y*life::WorldWidth+std::max(0,x-1),north=std::max(0,y-1)*life::WorldWidth+x;int shade=std::clamp(100+(int(renderElevation[west])-renderElevation[index]+int(renderElevation[north])-renderElevation[index])/3,65,125);if(habitat==uint8_t(life::HabitatType::Cave))shade=(renderHabitat[west]!=habitat||renderHabitat[north]!=habitat)?145:78;if(habitat==uint8_t(life::HabitatType::Bridge))shade=(x%4==0||renderHabitat[north]!=habitat)?135:95;const auto channel=[&](int shift){return uint32_t(std::min(255,int((base>>shift)&255)*shade/100));};base=0xff000000u|(channel(16)<<16)|(channel(8)<<8)|channel(0);}
  uint32_t color=cell?cellColors[cell]:base;
  if(activeOverlay==Overlay::Species&&cell){const auto found=renderSpeciesByOwner.find(renderOwners[index]);if(found!=renderSpeciesByOwner.end()&&found->second>0){const uint32_t hash=uint32_t(found->second)*2654435761u;color=0xff000000u|((64+(hash&127))<<16)|((64+((hash>>8)&127))<<8)|(64+((hash>>16)&127));}}
  return color;
}
void prepareWorldPalette(){
  static int previousOverlay=-1,previousClimateTick=-1;static uint64_t previousSpecies=UINT64_MAX;const bool climate=activeOverlay==Overlay::Climate||activeOverlay==Overlay::Productivity;
  const bool all=renderPalette.size()!=renderCells.size()||frameCopiedAll||previousOverlay!=int(activeOverlay)||(climate&&previousClimateTick!=renderMetrics.ticks/30)||(activeOverlay==Overlay::Species&&previousSpecies!=renderAtlasRevision);
  renderPalette.resize(renderCells.size());
  if(all)renderWorkers->run([&](int partition){const int begin=life::WorldHeight*partition/10,end=life::WorldHeight*(partition+1)/10;for(int y=begin;y<end;++y){const auto local=life::climateAt(y,life::WorldHeight,renderMetrics.ticks);for(int x=0;x<life::WorldWidth;++x)renderPalette[y*life::WorldWidth+x]=worldCellColor(x,y,local);}});
  else if(!frameDirtyTiles.empty()){
    // Expand tile writes by one cell because terrain lighting reads north/west neighbors.
    // Partition by disjoint output rows so overlapping dirty tiles never race.
    constexpr int tileSize=life::NativeSimulation::dirtyTileSize(),columns=(life::WorldWidth+tileSize-1)/tileSize;
    renderWorkers->run([&](int partition){const int begin=life::WorldHeight*partition/10,end=life::WorldHeight*(partition+1)/10;for(uint16_t tile:frameDirtyTiles){const int tx=(tile%columns)*tileSize,ty=(tile/columns)*tileSize;for(int y=std::max(begin,ty);y<std::min(end,std::min(life::WorldHeight,ty+tileSize+1));++y){const auto local=life::climateAt(y,life::WorldHeight,renderMetrics.ticks);for(int x=tx;x<std::min(life::WorldWidth,tx+tileSize+1);++x)renderPalette[y*life::WorldWidth+x]=worldCellColor(x,y,local);}}});
  }
  previousOverlay=int(activeOverlay);previousClimateTick=renderMetrics.ticks/30;previousSpecies=renderAtlasRevision;frameCopiedAll=false;frameDirtyTiles.clear();
}
