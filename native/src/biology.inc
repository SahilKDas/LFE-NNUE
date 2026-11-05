namespace life {
ResearchGenes NativeSimulation::mutateResearchGenes(ResearchGenes g){
  if(developmentRandom_()<.04){const int next=int(g.incubation)+(developmentRandom_()<.5?-10:10);g.incubation=uint16_t(std::clamp(next,0,240));}
  if(developmentRandom_()<.12)g.investment=uint16_t(std::clamp(int(g.investment)+(developmentRandom_()<.5?-25:25),100,800));
  if(developmentRandom_()<.12)g.shell=uint16_t(std::clamp(int(g.shell)+(developmentRandom_()<.5?-1:1),0,32));
  if(developmentRandom_()<.12)g.growthInterval=uint16_t(std::clamp(int(g.growthInterval)+(developmentRandom_()<.5?-3:3),1,240));
  for(auto& rate:g.emission)if(developmentRandom_()<.12)rate=uint8_t(std::clamp(int(rate)+(developmentRandom_()<.5?-1:1),0,16));
  if(developmentRandom_()<.12)g.resistance=std::clamp(g.resistance+(developmentRandom_()-.5)*.08,0.,1.);
  if(developmentRandom_()<.12)g.infectionRecovery=std::clamp(g.infectionRecovery+(developmentRandom_()-.5)*.004,.001,.1);
  return g;
}
bool NativeSimulation::updateDevelopment(Organism& o){
  if(std::as_const(o).research->development.stage==LifeStage::Adult)return true;
  auto& state=*o.research.operator->();auto& development=state.development;
  if(development.stage==LifeStage::Egg){const double temperature=climateTemperature_[wrapY(o.y)];development.ticks+=temperature>=.35&&temperature<=.65?2:1;if(development.ticks<int(state.genes.incubation)*2)return false;
    if(development.matureBody.empty())throw std::runtime_error("Egg has no inherited body");
    std::stable_sort(development.matureBody.begin(),development.matureBody.end(),[](const BodyCell& a,const BodyCell& b){const int da=std::abs(a.x)+std::abs(a.y),db=std::abs(b.x)+std::abs(b.y);return da!=db?da<db:a.x!=b.x?a.x<b.x:a.y<b.y;});
    registerAtmosphere(o,-1);clearBody(o);o.cells={development.matureBody.front()};development.stage=LifeStage::Juvenile;development.growthTicks=0;++o.bodyRevision;refreshCapabilities(o);placeBody(o);registerAtmosphere(o,1);++hatches_;addJournal(JournalKind::Development,o.id,"Hatched","Juvenile begins expressing its inherited body");
  }
  if(++development.growthTicks<int(state.genes.growthInterval))return true;
  if(o.cells.size()<development.matureBody.size()){const auto next=development.matureBody[o.cells.size()];const int minerals=(o.cells.size()>=3?100:0)+(next.type==CellType::Killer?250:next.type==CellType::Armor?180:0);if(o.energy<250||o.minerals<minerals||!isClearBody(std::span<const BodyCell>(&next,1),o.id,o.x,o.y,o.rotation))return true;
    registerAtmosphere(o,-1);clearBody(o);o.energy-=250;o.minerals-=minerals;o.cells.push_back(next);development.growthTicks=0;++o.bodyRevision;refreshCapabilities(o);placeBody(o);registerAtmosphere(o,1);largest_=std::max(largest_,int(o.cells.size()));
  }
  if(o.cells.size()==development.matureBody.size()){development.stage=LifeStage::Adult;development.matureBody.clear();++maturations_;addJournal(JournalKind::Development,o.id,"Matured","Inherited body fully expressed");}
  return true;
}
}

namespace life {
void NativeSimulation::updateSignals(){
  const int columns=(width_+7)/8,rows=(height_+7)/8;const size_t count=size_t(columns)*rows;
  for(auto& layer:signals_)if(layer.size()!=count)layer.assign(count,0);
  for(size_t slot:activeSlots_){if(slot==SIZE_MAX||slot>=organisms_.size())continue;auto& o=organisms_[slot];if(!o.living)continue;const auto rates=std::as_const(o).research->genes.emission;
    for(int channel=0;channel<2;++channel)if(rates[channel]&&o.energy>=rates[channel]*2){o.energy-=rates[channel]*2;auto& value=signals_[channel][size_t(wrapY(o.y)/8)*columns+wrapX(o.x)/8];value=uint16_t(std::min(4095,int(value)+rates[channel]*100));}
  }
  for(int channel=0;channel<2;++channel){auto& layer=signals_[channel];auto& next=signalScratch_[channel];next.resize(count);for(int y=0;y<rows;++y)for(int x=0;x<columns;++x){const size_t i=size_t(y)*columns+x;const int sum=layer[i]*11+layer[size_t((y+rows-1)%rows)*columns+x]+layer[size_t((y+1)%rows)*columns+x]+layer[size_t(y)*columns+(x+columns-1)%columns]+layer[size_t(y)*columns+(x+1)%columns];next[i]=uint16_t(sum/16);}layer.swap(next);}
}
void NativeSimulation::updateDisease(){
  infectionPrepared_.resize(organisms_.size());for(size_t slot=0;slot<organisms_.size();++slot)infectionPrepared_[slot]=std::as_const(organisms_[slot]).research->infection.load;
  static constexpr int dx[]={0,0,-1,1},dy[]={-1,1,0,0};
  // Stable ID order makes contact draws independent of storage compaction.
  std::vector<int>& order=diseaseOrder_;order.clear();for(auto slot:activeSlots_)if(slot!=SIZE_MAX&&organisms_[slot].living)order.push_back(organisms_[slot].id);std::sort(order.begin(),order.end());
  for(int id:order){auto* o=organismById(id);if(!o||!o->living)continue;const auto& previous=*std::as_const(*o).research.operator->();const auto genes=previous.genes;auto infection=previous.infection;
    if(infection.load>0){infection.load=std::clamp(infection.load+.022*(1-genes.resistance)-genes.infectionRecovery,0.,1.);if(diseaseRandom_()<genes.infectionRecovery*1.5)infection.load=std::max(0.,infection.load-.15);if(infection.load==0){infection.protectedUntil=ticks_+600;infection.stress=infection.risk=0;addJournal(JournalKind::Recovery,id,"Recovered","Temporary protection until tick "+std::to_string(infection.protectedUntil));}}
    else if(ticks_>=infection.protectedUntil){double contact=0,carrion=0;const size_t n=std::min<size_t>(2,o->cells.size());for(size_t sample=0;sample<n;++sample){const auto& cell=o->cells[(size_t(ticks_/30)+sample)%o->cells.size()];const auto [rx,ry]=rotated(cell.x,cell.y,o->rotation);for(int direction=0;direction<4;++direction){const int index=wrappedIndex(o->x+rx+dx[direction],o->y+ry+dy[direction]);if(ResourceType(world.resources[index])==ResourceType::Carrion)carrion=std::max(carrion,std::min(4.,world.resourceAmount[index]/100.));const auto owner=ownerSlots_[index];if(owners[index]>=0&&owner<organisms_.size()&&organisms_[owner].id==owners[index]&&organisms_[owner].id!=id&&organisms_[owner].living)contact=std::max(contact,infectionPrepared_[owner]);}}const double probability=std::clamp((contact*.18+carrion*.0008)*(1-genes.resistance),0.,.25);if(probability>0&&diseaseRandom_()<probability){infection.load=.08;addJournal(JournalKind::Infection,id,"Infected",contact>0?"Local contact with an infected neighbor":"Rare exposure near decaying carrion");}}
    if(infection.load>.75)infection.stress=std::min(10.,infection.stress+(infection.load-.75)*.03*(1-genes.resistance));else infection.stress=std::max(0.,infection.stress-genes.infectionRecovery);
    infection.risk=std::max(0.,infection.stress-.75)*infection.load*(1-genes.resistance);
    if(previous.infection.load||infection.load||previous.infection.protectedUntil!=infection.protectedUntil)o->research->infection=infection;
  }
}
void NativeSimulation::processDiseaseMortality(){
  if(!mortalityEnabled_||ticks_-lastDiseaseDeath_<10)return;Organism* victim=nullptr;double risk=.05;
  for(size_t slot:activeSlots_){if(slot==SIZE_MAX)continue;auto& o=organisms_[slot];const double value=std::as_const(o).research->infection.risk;if(o.living&&value>=.05&&(!victim||value>risk||(value==risk&&o.id<victim->id))){victim=&o;risk=value;}}
  if(victim){const int id=victim->id;die(*victim,DeathCause::Disease);++diseaseDeaths_;lastDiseaseDeath_=ticks_;addJournal(JournalKind::DiseaseDeath,id,"Disease death","Severe sustained infection; risk "+std::to_string(risk));}
}
}

