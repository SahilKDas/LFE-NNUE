#pragma once
#include "nnue.hpp"
#include "research_types.hpp"
#include "reference.hpp"
#include "atlas.hpp"
#include "watch.hpp"
#include "analysis_pool.hpp"
#include <cstdint>
#include <array>
#include <optional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace life {
enum class DeathCause : uint8_t { None, Starvation, Combat, OldAge, ThermalStress, Terrain, RespiratoryFailure, Disease };
const char* deathCauseName(DeathCause cause);
struct OrganismLineage {
  int generation{},birthTick{},deathTick{-1},totalDescendants{},directOffspring{};
  bool offspringCountComplete{true};
  std::vector<std::string> mutations;std::string deathCause;
};
class LineageStorage {
  std::shared_ptr<OrganismLineage> value_;
public:
  LineageStorage()=default;
  LineageStorage(const LineageStorage&)=default;
  LineageStorage& operator=(const LineageStorage&)=default;
  LineageStorage(LineageStorage&&) noexcept=default;LineageStorage& operator=(LineageStorage&&) noexcept=default;
  OrganismLineage* operator->(){if(!value_)value_=std::make_shared<OrganismLineage>();else if(!value_.unique())value_=std::make_shared<OrganismLineage>(*value_);return value_.get();}
  const OrganismLineage* operator->()const{static const OrganismLineage empty;return value_?value_.get():&empty;}
};
struct Organism {
  // Keep the simulation's working set together; cold lineage text follows it.
  // Archive fields are written explicitly, so this does not change save layout.
  int id{},parentId{-1},x{},y{};
  std::vector<BodyCell> cells;
  int energy{StartEnergy}, minerals{}, lifetime{}, damage{};
  int birthDistance{4}, moveRange{4}, moveCount{}, direction{}, rotation{};
  uint16_t senseChannels{DefaultSenseChannels};
  bool living{true}, isProducer{}, isMover{}, isConsumer{}, isAttacker{};
  DeathCause deathCauseCode{DeathCause::None};
  int neuralCost{168};
  double thermalStress{};
  double hypoxiaStress{};
  double oxygenTolerance{.12},stressRecovery{.01},frailty{1.0},respiratoryRisk{};
  int perceptionRadius{DefaultSensorRadius},lastAction{-1};
  std::shared_ptr<Nnue> brain;
  std::vector<uint16_t> lastFeatures;
  std::array<float,OutputSize> lastOutputs{};
  int mutability{5};double neuralMutability{8};
  size_t activeIndex{};
  uint64_t brainGenomeId{};
  int speciesId{-1};uint32_t bodyRevision{};
  uint64_t carrionConsumed{},nutrientsReleased{},decompositionEnergy{};
  ResearchStorage research;
  LineageStorage lineage;
};

struct LineageSummary { int id{}, generation{}; bool alive{}; };
struct OrganismInspection {
  int id{},parentId{-1},generation{},birthTick{},deathTick{-1},age{},cellCount{},energy{},minerals{},damage{};
  double mutability{},neuralMutability{},thermalStress{},respiratoryStress{},oxygenTolerance{},stressRecovery{},frailty{},respiratoryRisk{},temperature{},fertility{},metabolicCost{};
  bool alive{},isMover{};int perceptionRadius{},senseChannels{},neuralCost{},totalDescendants{},attackDurability{},armorDurability{};uint64_t carrionConsumed{},nutrientsReleased{},decompositionEnergy{};int localNutrients{};
  ResearchGenes genes;LifeStage stage{LifeStage::Adult};double infection{},immunity{};int incubationProgress{};
  std::string action,diet,terrain,resource,deathCause;
  std::vector<std::string> mutations;
  std::vector<LineageSummary> ancestors,descendants;
  std::vector<uint16_t> senses;
  std::array<float,OutputSize> outputs{};bool hasOutputs{};
};

struct NativeMetrics {
  int organisms{},record{},generation{},ticks{},largest{};
  double averageEnergy{},averageMutation{},averageStress{},temperature{},fertility{};
  double oxygen{},carbonDioxide{};
  double averageOxygenTolerance{},averageRespiratoryStress{};
  int plantEaters{},scavengers{},mineralEaters{},decomposers{},predators{},nnueEvaluations{};
  int respiratoryDeaths{},criticalRespiratory{};
  size_t memoryEstimate{};int awakeRegions{},sleepingRegions{},dirtyTiles{};
  bool moverProducersEnabled{};
};
struct TickWork { double organisms{},environment{},lineage{},atlas{},physiology{},reproduction{},feeding{},neural{}; };

struct WatchedPose {int id{-1},x{},y{},rotation{};bool alive{},mover{};};
struct WatchedSnapshot {
  WatchedPose pose;int tick{},age{},generation{},energy{},directChildren{},livingChildren{},livingDescendants{},nextDescendant{-1},decisionCount{};
  std::string habitat,deathCause;std::vector<BodyCell> body;DecisionRecord decision;bool hasDecision{},offspringCountComplete{true};
};

class NativeSimulation {
  friend struct NativeSimulationTestAccess;
  int width_, height_; uint32_t worldSeed_; Mulberry32 random_; int nextId_{1}, ticks_{}, resets_{}, record_{}, largest_{}, selectedId_{-1}, deadCount_{};uint64_t nextBrainGenomeId_{1};
  double foodChance_; int lifespan_, lineageLimit_, nnueEvaluations_{}; std::vector<Organism> organisms_;
  double atmosphericOxygen_{.21},carbonDioxide_{.0004};
  bool moverProducersEnabled_{};
  bool researchRules_{true}; // Friend-only legacy regression control; normal worlds always enable research biology.
  bool reproductionEnabled_{true}, mortalityEnabled_{true}; int populationTarget_{};
  int births_{};
  bool profileTicks_{};
  TickWork tickWork_;
  std::array<int,7> deathsByCause_{},sampledDeaths_{};
  int sampledBirths_{},nextSpeciesId_{1},selectedSpeciesId_{-1},lastSeason_{-1},lastPopulationPeak_{},crashBaseline_{},crashMinimum_{};
  bool crashActive_{},atmosphereEventActive_{};
  bool carrionEventActive_{},nutrientCollapseActive_{};
  int lastDecomposerPopulation_{};
  std::array<uint8_t,3> resourceShortageRuns_{};
  std::array<bool,3> resourceShortageActive_{};
  uint64_t atlasRevision_{};
  std::vector<SpeciesSummary> species_;
  std::vector<EcologySample> timeline_;
  std::vector<EvolutionEvent> evolutionEvents_;
  std::vector<Organism*> classificationBuffer_;
  SpeciesDistanceCache classificationCache_;
  std::unordered_map<int,size_t> speciesSlotById_;
  std::shared_ptr<AnalysisPool> analysisPool_;
  bool parallelAnalysis_{true};
  std::vector<Nnue::MutationChange> neuralMutationScratch_;
  std::vector<GenomeDescriptor> classificationDescriptors_;
  struct BodyDigest {uint64_t hash{};std::array<uint8_t,13> counts{};int id{-1};uint32_t revision{};uint8_t diet{};};
  mutable std::vector<BodyDigest> bodyDigests_;
  std::vector<SpeciesDistanceCache::Entry*> classificationJobs_;
  std::vector<SpeciesDistanceCache::Entry*> classificationMatches_;
  struct SpeciesPair {uint64_t body{},neural{};size_t slot{};};
  std::vector<SpeciesPair> classificationPairs_;
  uint32_t classificationJobEpoch_{};
  struct PreparedPhysiology {int energy{};double temperature{},discomfort{},stressDelta{},agePressure{},energyPressure{},bodyPressure{};};
  std::vector<PreparedPhysiology> preparedPhysiology_;
  int respiratoryDeaths_{},lastRespiratoryDeathTick_{-10};
  struct RespiratoryCandidate { double risk{}; int id{}; };
  std::vector<RespiratoryCandidate> respiratoryCandidates_;
  std::unordered_map<int,size_t> slotById_;
  std::vector<size_t> activeSlots_;
  std::vector<uint32_t> ownerSlots_;
  struct OwnerSense {int id{-1};uint8_t category{4};};
  std::vector<OwnerSense> ownerSense_;
  std::vector<int> pruneChildren_,pruneActiveIds_;
  std::vector<uint8_t> pruneProtected_,pruneRemoved_;
  std::vector<size_t> pruneParents_,pruneSlotRemap_;
  int64_t atmosphereCells_{},atmosphereMovers_{},atmosphereProducers_{};
  static constexpr int RegionSize=32,DirtyTileSize=16;
  int regionColumns_{},regionRows_{},dirtyColumns_{},dirtyRows_{};
  std::vector<std::vector<size_t>> regionBatches_;
  std::vector<uint8_t> regionAwake_,dirtyTiles_;
  std::vector<uint32_t> dirtyVersions_;
  std::array<std::vector<uint32_t>,4> dirtyLayerVersions_;
  std::vector<uint32_t> regionNutrientTotals_;
  std::vector<uint16_t> nutrientScratch_;
  std::vector<uint8_t> nutrientResourceDirty_;
  std::vector<uint32_t> nutrientDecayRows_;
  std::vector<uint32_t> soilResourceTileChanges_,soilNutrientTileChanges_;
  double birthOxygenCache_{-1},birthCarbonCache_{-1};uint64_t birthThresholdCache_{};
  uint64_t carrionProcessed_{},sampledCarrionProcessed_{},plantProduced_{},sampledPlantProduced_{};
  std::vector<float> climateTemperature_, climateFertility_; std::vector<int8_t> climateGradient_; std::vector<uint8_t> climateBand_;
  int watchedId_{-1};std::vector<DecisionRecord> watchedDecisions_;size_t decisionCursor_{},decisionCount_{};
  mutable int familyRoot_{-2},familyBirths_{-1},familyDeaths_{-1};mutable size_t familyRecords_{SIZE_MAX};
  mutable std::unordered_map<int,FamilyRole> familyRoles_;
  mutable std::vector<int> familyPath_;
  mutable std::vector<LineageSummary> familyDescendants_;
  void refreshFamily(int root) const;
  void captureDecision(const Organism& organism);
  int safeIndex(int x, int y) const;
  int wrappedIndex(int x,int y) const;
  int wrapX(int x) const;
  int wrapY(int y) const;
  void markDirtyIndex(int index,uint8_t layers=15);
  void markAllDirty();
  void rebuildRegionIndex();
  void rebuildOwnerSlots();
  std::pair<int,int> rotated(int x, int y, int direction) const;
  void placeBody(const Organism& organism);
  void refreshCapabilities(Organism& organism);
  PreparedPhysiology preparePhysiology(const Organism& organism) const;
  void updateSignals();
  void updateDisease();
  void processDiseaseMortality();
  void updateOrganism(Organism& organism,size_t slot=SIZE_MAX);
  void eat(Organism& organism, CellType mouth, int x, int y);
  void produce(Organism& organism, int x, int y);
  void reproduce(Organism& parent);
  void mutate(Organism& organism);
  void die(Organism& organism, DeathCause cause);
  void processRespiratoryMortality();
  GenomeDescriptor describeGenome(const Organism& organism,size_t slot=SIZE_MAX) const;
  void classifySpecies(bool useWorkers=true);
  void sampleEcology();
  void addEvolutionEvent(EvolutionEventType type,int speciesId,float value,const std::string& label);
  void attack(Organism& attacker, BodyCell& weapon, int x, int y);
  void harm(Organism& organism);
  void harmAt(Organism& organism, int index);
  Organism* organismById(int id);
  const Organism* organismById(int id) const;
  BodyCell* localCellAt(Organism& organism, int x, int y);
  bool isClear(const Organism& organism, int x, int y, int rotation) const;
  bool isClearBody(std::span<const BodyCell> body,int id,int x,int y,int rotation) const;
  bool straightPath(int x1, int y1, int x2, int y2, const Organism& parent) const;
  void features(const Organism& organism,std::vector<uint16_t>& result) const;
  bool attemptMove(Organism& organism);
  bool attemptRotate(Organism& organism);
  void clearBody(const Organism& organism);
  void updateClimateCache();
  void updateAtmosphere();
  void registerAtmosphere(const Organism& organism,int sign);
  void rebuildAtmosphereCounts();
  void updateNutrients();
  ResearchGenes mutateResearchGenes(ResearchGenes);
  bool updateDevelopment(Organism&);

  void archiveState(struct StateArchive& archive);
  void archiveResearch(struct StateArchive& archive);
  void saveArchive(const std::filesystem::path&,uint32_t,const std::filesystem::path&);
  void loadArchive(const std::filesystem::path&,const std::filesystem::path&);
  void recordCommand(SimulationCommand command);
  bool recordingCommands_{true},initialized_{};uint64_t commandSequence_{},worldEpoch_{};
  uint64_t runId_{},branchId_{1};Mulberry32 developmentRandom_{0x127a5u},diseaseRandom_{0x8f17cu};
  std::vector<SimulationCommand> commands_;std::vector<JournalEntry> journal_;std::vector<ResearchEcology> researchTimeline_;
  uint64_t journalSequence_{};int diseaseDeaths_{},lastDiseaseDeath_{-10},hatches_{},maturations_{};
  std::array<std::vector<uint16_t>,2> signals_;std::array<std::vector<uint16_t>,2> signalScratch_;
  std::vector<double> infectionPrepared_;std::vector<int> diseaseOrder_;

  void pruneLineage();
public:
  int width()const{return width_;}int height()const{return height_;}
  size_t freezeBytes()const;
  static constexpr int MoverProducerSetting=2;
  bool moverProducersEnabled() const { return moverProducersEnabled_; }
  void executeCommand(SimulationCommand command){command.tick=ticks_;command.sequence=commandSequence_+1;applyCommand(command);}
  std::shared_ptr<NativeSimulation> freeze(size_t budget=128u*1024u*1024u)const;
  uint64_t stateDigest()const;
  uint64_t worldEpoch()const{return worldEpoch_;}
  uint64_t commandSequence()const{return commandSequence_;}
  const std::vector<SimulationCommand>& commands()const{return commands_;}
  void applyCommand(const SimulationCommand& command);
  GenomeRecord genomeRecord(int id)const;
  static GenomeComparison compareGenomes(const GenomeRecord& left,const GenomeRecord& right);
  static void saveGenome(const GenomeRecord&,const std::filesystem::path&);
  static GenomeRecord loadGenome(const std::filesystem::path&);
  int introduceGenome(const GenomeRecord&,int x,int y,const std::string& artifact="");
  void addJournal(JournalKind,int organism,const std::string& label,const std::string& evidence="");
  const std::vector<JournalEntry>& journal()const{return journal_;}
  const std::vector<ResearchEcology>& researchTimeline()const{return researchTimeline_;}
  ResearchEcology researchEcology()const;
  uint16_t signalAt(int channel,int x,int y)const {const auto& layer=signals_[channel&1];return layer.empty()?0:layer[size_t(wrapY(y)/8)*((width_+7)/8)+wrapX(x)/8];}
  const std::array<std::vector<uint16_t>,2>& signalLayers()const{return signals_;}
  void researchIdentity(uint64_t run,uint64_t branch){runId_=run;branchId_=branch;}
  void profileTicks(bool enabled){profileTicks_=enabled;}
  void setParallelAnalysis(bool enabled){parallelAnalysis_=enabled;}
  TickWork tickWork() const{return tickWork_;}
  WorldLayers world;
  FollowPreferences followPreferences;
  struct SavedView {float zoom{1},cameraX{},cameraY{};int overlay{},selectedOrganism{-1},fps{30},tps{60},atlasTab{},timelineWindow{600};bool weather{false},atlasOpen{},paused{};int tool{};uint32_t nextWorldSeed{2};bool habitatHistory{};} savedView;
  void save(const std::filesystem::path& path,uint32_t archiveVersion=3);
  void saveCheckpoint(const std::filesystem::path& path,const std::filesystem::path& genomes){saveArchive(path,3,genomes);}
  void loadCheckpoint(const std::filesystem::path& path,const std::filesystem::path& genomes){loadArchive(path,genomes);}
  void load(const std::filesystem::path& path);
  std::vector<uint8_t> cells;
  std::vector<int32_t> owners;
  NativeSimulation(int width, int height, double foodChance, int lifespan, uint32_t worldSeed, uint32_t randomSeed, int lineageLimit = 20'000);
  void reset();
  void regenerate(uint32_t seed);
  int seedBenchmarkMovers(int count);
  int seedBenchmarkFamily(int stride=4);
  void setBenchmarkLifecycle(bool enabled){reproductionEnabled_=mortalityEnabled_=enabled;}
  bool paintTerrain(int x, int y, TerrainType terrain);
  bool paintResource(int x, int y, ResourceType resource, uint16_t amount = 1000);
  bool paintNutrients(int x,int y,uint16_t amount=1200);
  const Organism* organismAt(int x, int y) const;
  void select(int id) { selectedId_ = id; }
  void watch(int id);
  WatchedPose watchedPose() const;
  WatchedSnapshot watchedSnapshot(size_t stepsBack=0) const;
  const std::unordered_map<int,FamilyRole>& familyRoles(int root) const{refreshFamily(root);return familyRoles_;}
  std::optional<OrganismInspection> inspect(int id) const;
  void step(int count = 1);
  NativeMetrics metrics() const;
  AtlasSnapshot atlasSnapshot() const;
  uint64_t atlasRevision() const{return atlasRevision_;}
  void selectSpecies(int id){selectedSpeciesId_=id;++atlasRevision_;}
  int selectedSpecies() const{return selectedSpeciesId_;}
  std::vector<uint16_t> takeDirtyTiles();
  std::vector<uint16_t> changedTiles(std::vector<uint32_t>& knownVersions) const;
  void changedTilesInto(std::vector<uint32_t>& knownVersions,std::vector<uint16_t>& result) const;
  void changedLayerTilesInto(size_t layer,std::vector<uint32_t>& knownVersions,std::vector<uint16_t>& result) const;
  static constexpr int dirtyTileSize(){return DirtyTileSize;}
  int lineageRecordCount() const { return int(organisms_.size()); }
  int deadLineageRecordCount() const { return deadCount_; }
  int birthCount() const { return births_; }
  int tickCount() const{return ticks_;}
  int respiratoryDeathCount() const { return respiratoryDeaths_; }
  const std::vector<Organism>& organisms() const { return organisms_; }
};
}

