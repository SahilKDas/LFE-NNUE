namespace life {
int NativeSimulation::seedBenchmarkFamily(int stride){
  // A synthetic genealogy stresses family rendering without replacing independent brain families.
  Organism* root=nullptr;for(auto& organism:organisms_)if(organism.living&&organism.isMover&&(!root||organism.cells.size()>root->cells.size()))root=&organism;
  if(!root)return -1;stride=std::max(1,stride);root->parentId=-1;root->generation=0;root->directOffspring=root->totalDescendants=0;
  for(size_t index=0;index<organisms_.size();++index){auto& organism=organisms_[index];if(organism.id!=root->id&&index%size_t(stride)==0){organism.parentId=root->id;organism.generation=1;++root->directOffspring;++root->totalDescendants;}}
  familyRoot_=-2;return root->id;
}
void NativeSimulation::watch(int id){
  if(watchedId_==id)return;watchedId_=organismById(id)?id:-1;decisionCursor_=decisionCount_=0;
  if(watchedId_>=0&&watchedDecisions_.empty())watchedDecisions_.resize(128);
}
void NativeSimulation::captureDecision(const Organism& organism){
  if(organism.id!=watchedId_||watchedDecisions_.empty())return;
  auto& record=watchedDecisions_[decisionCursor_];record.tick=ticks_;record.x=organism.x;record.y=organism.y;record.rotation=organism.rotation;record.radius=organism.perceptionRadius;record.channels=organism.senseChannels;record.action=organism.lastAction;record.outputs=organism.lastOutputs;record.featureCount=uint16_t(std::min(organism.lastFeatures.size(),record.features.size()));std::copy_n(organism.lastFeatures.begin(),record.featureCount,record.features.begin());
  const int row=std::clamp(organism.y,0,height_-1);record.direction=organism.direction;record.temperatureBand=climateBand_[row];record.gradient=climateGradient_[row];record.season=int(std::floor(cyclePhase(ticks_)*4))%4;record.fertilityBand=std::min(4,int(std::floor((climateFertility_[row]-.35)*5)));record.highEnergy=organism.energy>=int(organism.cells.size())*6*EnergyScale;record.damaged=organism.damage>0;
  decisionCursor_=(decisionCursor_+1)%128;decisionCount_=std::min<size_t>(128,decisionCount_+1);
}
WatchedPose NativeSimulation::watchedPose() const {const auto* o=organismById(watchedId_);return o?WatchedPose{o->id,o->x,o->y,o->rotation,o->living,o->isMover}:WatchedPose{};}
void NativeSimulation::refreshFamily(int root) const {
  if(familyRoot_==root&&familyBirths_==births_&&familyDeaths_==deadCount_&&familyRecords_==organisms_.size())return;
  const bool rebuild=familyRoot_!=root||familyRecords_>organisms_.size()||familyBirths_>births_;
  familyRoot_=root;familyBirths_=births_;familyDeaths_=deadCount_;familyRecords_=organisms_.size();
  if(rebuild)familyRoles_.clear();familyRoles_.reserve(organisms_.size());familyRoles_[root]=FamilyRole::Root;familyDescendants_.clear();
  // Memoizing each parent chain makes the initial build linear; later births only resolve new IDs.
  for(const auto& organism:organisms_){
    if(!familyRoles_.contains(organism.id)){familyPath_.clear();int id=organism.id;while(id>=0&&!familyRoles_.contains(id)){const auto* item=organismById(id);if(!item)break;familyPath_.push_back(id);id=item->parentId;if(familyPath_.size()>organisms_.size())break;}
      auto found=familyRoles_.find(id);FamilyRole role=found==familyRoles_.end()?FamilyRole::Unrelated:found->second;
      for(auto position=familyPath_.rbegin();position!=familyPath_.rend();++position){role=role==FamilyRole::Root?FamilyRole::Child:role==FamilyRole::Unrelated?FamilyRole::Unrelated:FamilyRole::Descendant;familyRoles_[*position]=role;}}
    const auto role=familyRoles_.at(organism.id);if(role==FamilyRole::Child||role==FamilyRole::Descendant)familyDescendants_.push_back({organism.id,organism.generation,organism.living});
  }
}
WatchedSnapshot NativeSimulation::watchedSnapshot(size_t stepsBack) const {
  WatchedSnapshot result;result.pose=watchedPose();result.tick=ticks_;const auto* organism=organismById(watchedId_);if(!organism)return result;
  refreshFamily(watchedId_);result.age=organism->lifetime;result.generation=organism->generation;result.energy=organism->energy;result.body=organism->cells;result.deathCause=organism->deathCause;result.directChildren=organism->directOffspring;result.offspringCountComplete=organism->offspringCountComplete;
  constexpr const char* habitats[]={"Surface","Cave","Bridge"};result.habitat=habitats[world.habitat[wrappedIndex(organism->x,organism->y)]];
  const Organism* best=nullptr;for(const auto& entry:familyDescendants_){const auto* candidate=organismById(entry.id);if(!candidate)continue;const bool child=candidate->parentId==watchedId_;if(!candidate->living)continue;result.livingChildren+=child;++result.livingDescendants;
    if(!best||((best->parentId!=watchedId_)&&child)||((best->parentId==watchedId_)==child&&(candidate->birthTick<best->birthTick||(candidate->birthTick==best->birthTick&&candidate->id<best->id))))best=candidate;}
  result.nextDescendant=best?best->id:-1;result.decisionCount=int(decisionCount_);if(decisionCount_){result.hasDecision=true;const auto offset=std::min(stepsBack,decisionCount_-1);result.decision=watchedDecisions_[(decisionCursor_+128-1-offset)%128];}return result;
}
}
