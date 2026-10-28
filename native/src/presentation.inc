std::shared_ptr<const life::FrameDiagnostics> producerDiagnostics;
auto producerNextDiagnostics=std::chrono::steady_clock::time_point{};
int producerLastTick=-1;
std::shared_ptr<const life::AtlasSnapshot> producerAtlas;std::shared_ptr<const std::unordered_map<int,int>> producerSpecies;
uint64_t producerAtlasRevision=UINT64_MAX;
uint64_t producerEpoch=0;bool producerOwners=false;

void invalidatePresentation(){frameExchange.invalidate();producerDiagnostics.reset();producerAtlas.reset();producerSpecies.reset();producerAtlasRevision=UINT64_MAX;producerLastTick=-1;producerNextDiagnostics={};}
void publishSimulationFrame(bool force){
  if(!presentationEnabled.load(std::memory_order_relaxed))return;
  const auto pose=simulation.watchedPose();const int requestedRoot=presentationRoot.load(std::memory_order_relaxed),root=requestedRoot==pose.id?requestedRoot:-1;const size_t history=presentationHistory.load(std::memory_order_relaxed);
  const bool includeOwners=root>=0||presentationOwners.load();const auto request=presentationContext.load(std::memory_order_relaxed);const auto now=std::chrono::steady_clock::now();const bool context=!producerDiagnostics||producerDiagnostics->watched.pose.id!=pose.id||producerDiagnostics->watched.pose.alive!=pose.alive||producerDiagnostics->highlightRoot!=root||producerDiagnostics->history!=history||producerDiagnostics->context!=request||producerEpoch!=frameExchange.epoch()||producerOwners!=includeOwners||(presentationAtlas.load()&&!producerDiagnostics->atlas);
  if(!force&&!context&&producerLastTick==simulation.tickCount())return;
  if(context||now>=producerNextDiagnostics){
    if(presentationAtlas.load()&&(!producerAtlas||producerAtlasRevision!=simulation.atlasRevision())){producerAtlas=std::make_shared<const life::AtlasSnapshot>(simulation.atlasSnapshot());auto members=std::make_shared<std::unordered_map<int,int>>();members->reserve(simulation.organisms().size());for(const auto& organism:simulation.organisms())if(organism.living)members->emplace(organism.id,organism.speciesId);producerSpecies=members;producerAtlasRevision=simulation.atlasRevision();}
    auto diagnostics=std::make_shared<life::FrameDiagnostics>();if(presentationAtlas.load()){diagnostics->atlas=producerAtlas;diagnostics->speciesByOwner=producerSpecies;}diagnostics->metrics=simulation.metrics();diagnostics->inspection=simulation.inspect(pose.id);diagnostics->watched=simulation.watchedSnapshot(history);diagnostics->highlightRoot=root;diagnostics->history=history;diagnostics->context=request;
    if(root>=0){for(const auto& [id,role]:simulation.familyRoles(root))if(role!=life::FamilyRole::Unrelated)diagnostics->family.emplace(id,role);diagnostics->familyTiles.reserve(std::min(diagnostics->family.size()*4,simulation.cells.size()));constexpr int tile=life::NativeSimulation::dirtyTileSize(),columns=(life::WorldWidth+tile-1)/tile;
      for(const auto& organism:simulation.organisms())if(organism.living&&diagnostics->family.contains(organism.id))for(const auto& cell:organism.cells){int x=cell.x,y=cell.y;if(organism.rotation==1){x=-cell.x;y=-cell.y;}else if(organism.rotation==2){x=cell.y;y=-cell.x;}else if(organism.rotation==3){x=-cell.y;y=cell.x;}x=life::wrapCoordinate(organism.x+x,life::WorldWidth);y=life::wrapCoordinate(organism.y+y,life::WorldHeight);diagnostics->familyTiles.emplace_back(organism.id,uint16_t((y/tile)*columns+x/tile));}}
    producerDiagnostics=std::move(diagnostics);producerNextDiagnostics=now+std::chrono::milliseconds(100);
  }
  frameExchange.publish(simulation,producerDiagnostics,includeOwners,simulationUiCommand);producerOwners=includeOwners;producerLastTick=simulation.tickCount();producerEpoch=frameExchange.epoch();
}

bool consumeSimulationFrame(){
  const bool buffered=!paused.load()&&requestedFps.load()==requestedTps.load()&&(!headlessMode||headlessFrameSeconds<0);auto lease=buffered?frameExchange.acquireBuffered():frameExchange.acquire();if(!lease)return false;auto& incoming=lease.frame();const bool oldOwnersReady=renderOwnersReady,ownersChanged=renderOwnersReady!=incoming.ownersCurrent;renderOwnersReady=incoming.ownersCurrent;incoming.ownersCurrent=oldOwnersReady;++watchPublishedRevision;
  constexpr int tile=life::NativeSimulation::dirtyTileSize(),columns=(life::WorldWidth+tile-1)/tile,count=columns*((life::WorldHeight+tile-1)/tile);
  const bool initial=renderOwners.size()!=incoming.owners.size(),epoch=renderFrameEpoch!=incoming.epoch,root=renderFamilyRoot!=followCamera.target;
  std::array<uint8_t,count> familyDirty{};frameDirtyTiles.clear();familyOwnerDirtyTiles.clear();
  backgroundCopiedAll=backgroundCopiedAll||initial||epoch;
  constexpr std::array<uint8_t,8> paletteLayers{9,3,9,1,9,5,7,9};const uint8_t relevant=paletteLayers[size_t(activeOverlay)];int changedCount=0;
  for(int index=0;index<count;++index){if(!epoch&&renderDirtyVersions.size()==incoming.versions.size()&&renderDirtyVersions[index]==incoming.versions[index])continue;++changedCount;bool palette=initial||epoch;for(size_t layer=0;layer<4&&!palette;++layer)if((relevant&(1u<<layer))&&(renderLayerVersions[layer].size()!=incoming.layerVersions[layer].size()||renderLayerVersions[layer][index]!=incoming.layerVersions[layer][index]))palette=true;if(palette)frameDirtyTiles.push_back(uint16_t(index));bool changed=initial||epoch;
    if(initial||epoch||renderLayerVersions[3].size()!=incoming.layerVersions[3].size()||renderLayerVersions[3][index]!=incoming.layerVersions[3][index])backgroundDirtyTiles.push_back(uint16_t(index));
    const bool body=initial||epoch||renderLayerVersions[0].size()!=incoming.layerVersions[0].size()||renderLayerVersions[0][index]!=incoming.layerVersions[0][index];if(!body||followCamera.target<0||!renderOwnersReady)continue;
    const int x=(index%columns)*tile,y=(index/columns)*tile;for(int row=y;row<std::min(y+tile,life::WorldHeight)&&!changed;++row){const size_t begin=size_t(row)*life::WorldWidth+x;changed=!std::equal(incoming.owners.begin()+begin,incoming.owners.begin()+begin+std::min(tile,life::WorldWidth-x),renderOwners.begin()+begin);}if(changed)familyDirty[index]=1;}
  if(incoming.diagnostics&&incoming.diagnostics!=renderFrameDiagnostics&&incoming.diagnostics->context==presentationContext.load(std::memory_order_relaxed)){const auto& diagnostics=*incoming.diagnostics;if(diagnostics.atlas){renderAtlas=diagnostics.atlas;renderAtlasRevision=renderAtlas->revision;renderSpeciesByOwner=diagnostics.speciesByOwner;}renderMetrics=diagnostics.metrics;renderInspection=diagnostics.inspection;renderWatched=diagnostics.watched;
    if(diagnostics.highlightRoot==followCamera.target){for(const auto& [id,index]:diagnostics.familyTiles){const auto previous=renderFamily.find(id);const auto next=diagnostics.family.find(id);if(root||previous==renderFamily.end()||next==diagnostics.family.end()||previous->second!=next->second)familyDirty[index]=1;}renderFamily=diagnostics.family;}
    renderFrameDiagnostics=incoming.diagnostics;}
  renderUiCommand=incoming.commandRevision;renderWatchedPose=incoming.pose;renderMetrics.ticks=incoming.tick;renderMetrics.dirtyTiles=changedCount;
  renderCells.swap(incoming.cells);renderOwners.swap(incoming.owners);renderTerrain.swap(incoming.world.terrain);renderResources.swap(incoming.world.resources);renderNutrients.swap(incoming.world.nutrients);renderResourceAmounts.swap(incoming.world.resourceAmount);renderDirtyVersions.swap(incoming.versions);renderLayerVersions.swap(incoming.layerVersions);std::swap(renderFrameEpoch,incoming.epoch);
  renderFamilyCells.resize(renderCells.size());renderFamilyEdges.resize(renderCells.size());for(int index=0;index<count;++index)if(familyDirty[index])familyOwnerDirtyTiles.push_back(uint16_t(index));frameCopiedAll=frameCopiedAll||initial||epoch||int(frameDirtyTiles.size())>count*3/4;
  if(renderOwnersReady)updateDerivedFamily(initial||epoch||root||ownersChanged);return true;
}
