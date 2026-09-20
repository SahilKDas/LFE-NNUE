#pragma once
#include "nnue.hpp"
#include "reference.hpp"
#include "atlas.hpp"
#include <cstdint>
#include <array>
#include <optional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace life {
enum class DeathCause : uint8_t { None, Starvation, Combat, OldAge, ThermalStress, Terrain, RespiratoryFailure };
const char* deathCauseName(DeathCause cause);
struct BodyCell { CellType type; int x, y; int durability{}; };
struct Organism {
  int id{}, parentId{-1}, generation{}, birthTick{}, deathTick{-1}, x{}, y{};
  std::vector<BodyCell> cells;
  int energy{StartEnergy}, minerals{}, lifetime{}, damage{}, mutability{5}; double neuralMutability{8};
  int birthDistance{4}, moveRange{4}, moveCount{}, direction{}, rotation{};
  bool living{true}, isProducer{}, isMover{}, isConsumer{}, isAttacker{};
  double thermalStress{};
  double hypoxiaStress{};
  double oxygenTolerance{.12},stressRecovery{.01},frailty{1.0},respiratoryRisk{};
  DeathCause deathCauseCode{DeathCause::None};
  std::string deathCause;
  int perceptionRadius{DefaultSensorRadius}; uint8_t senseChannels{DefaultSenseChannels}; int neuralCost{168};
  int totalDescendants{};
  std::vector<std::string> mutations;
  std::vector<uint16_t> lastFeatures;
  std::array<float,OutputSize> lastOutputs{};
  int lastAction{-1};
  size_t activeIndex{};
  uint64_t brainGenomeId{};
  int speciesId{-1};
  std::shared_ptr<Nnue> brain;
};

struct LineageSummary { int id{}, generation{}; bool alive{}; };
struct OrganismInspection {
  int id{},parentId{-1},generation{},birthTick{},deathTick{-1},age{},cellCount{},energy{},minerals{},damage{};
  double mutability{},neuralMutability{},thermalStress{},respiratoryStress{},oxygenTolerance{},stressRecovery{},frailty{},respiratoryRisk{},temperature{},fertility{},metabolicCost{};
  bool alive{},isMover{};int perceptionRadius{},senseChannels{},neuralCost{},totalDescendants{},attackDurability{},armorDurability{};
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
  int plantEaters{},scavengers{},mineralEaters{},predators{},nnueEvaluations{};
  int respiratoryDeaths{},criticalRespiratory{};
  size_t memoryEstimate{};int awakeRegions{},sleepingRegions{},dirtyTiles{};
};

class NativeSimulation {
  friend struct NativeSimulationTestAccess;
  int width_, height_; uint32_t worldSeed_; Mulberry32 random_; int nextId_{1}, ticks_{}, resets_{}, record_{}, largest_{}, selectedId_{-1}, deadCount_{};uint64_t nextBrainGenomeId_{1};
  double foodChance_; int lifespan_, lineageLimit_, nnueEvaluations_{}; std::vector<Organism> organisms_;
  double atmosphericOxygen_{.21},carbonDioxide_{.0004};
  bool reproductionEnabled_{true}, mortalityEnabled_{true}; int populationTarget_{};
  int births_{};
  std::array<int,7> deathsByCause_{},sampledDeaths_{};
  int sampledBirths_{},nextSpeciesId_{1},selectedSpeciesId_{-1},lastSeason_{-1},lastPopulationPeak_{},crashBaseline_{},crashMinimum_{};
  bool crashActive_{},atmosphereEventActive_{};
  uint64_t atlasRevision_{};
  std::vector<SpeciesSummary> species_;
  std::vector<EcologySample> timeline_;
  std::vector<EvolutionEvent> evolutionEvents_;
  std::vector<Organism*> classificationBuffer_;
  int respiratoryDeaths_{},lastRespiratoryDeathTick_{-10};
  struct RespiratoryCandidate { double risk{}; int id{}; };
  std::vector<RespiratoryCandidate> respiratoryCandidates_;
  std::unordered_map<int,size_t> slotById_;
  std::vector<size_t> activeSlots_;
  static constexpr int RegionSize=32,DirtyTileSize=16;
  int regionColumns_{},regionRows_{},dirtyColumns_{},dirtyRows_{};
  std::vector<std::vector<size_t>> regionBatches_;
  std::vector<uint8_t> regionAwake_,dirtyTiles_;
  std::vector<uint32_t> dirtyVersions_;
  std::vector<float> climateTemperature_, climateFertility_; std::vector<int8_t> climateGradient_; std::vector<uint8_t> climateBand_;
  int safeIndex(int x, int y) const;
  int wrappedIndex(int x,int y) const;
  int wrapX(int x) const;
  int wrapY(int y) const;
  void markDirtyIndex(int index);
  void markAllDirty();
  void rebuildRegionIndex();
  std::pair<int,int> rotated(int x, int y, int direction) const;
  void placeBody(const Organism& organism);
  void refreshCapabilities(Organism& organism);
  void updateOrganism(Organism& organism);
  void eat(Organism& organism, CellType mouth, int x, int y);
  void produce(Organism& organism, int x, int y);
  void reproduce(Organism& parent);
  void mutate(Organism& organism);
  void die(Organism& organism, DeathCause cause);
  void processRespiratoryMortality();
  GenomeDescriptor describeGenome(const Organism& organism) const;
  void classifySpecies();
  void sampleEcology();
  void addEvolutionEvent(EvolutionEventType type,int speciesId,float value,const std::string& label);
  void attack(Organism& attacker, BodyCell& weapon, int x, int y);
  void harm(Organism& organism);
  void harmAt(Organism& organism, int index);
  Organism* organismById(int id);
  const Organism* organismById(int id) const;
  BodyCell* localCellAt(Organism& organism, int x, int y);
  bool isClear(const Organism& organism, int x, int y, int rotation) const;
  bool straightPath(int x1, int y1, int x2, int y2, const Organism& parent) const;
  void features(const Organism& organism,std::vector<uint16_t>& result) const;
  bool attemptMove(Organism& organism);
  bool attemptRotate(Organism& organism);
  void clearBody(const Organism& organism);
  void updateClimateCache();
  void updateAtmosphere();
  void pruneLineage();
public:
  WorldLayers world;
  std::vector<uint8_t> cells;
  std::vector<int32_t> owners;
  NativeSimulation(int width, int height, double foodChance, int lifespan, uint32_t worldSeed, uint32_t randomSeed, int lineageLimit = 20'000);
  void reset();
  void regenerate(uint32_t seed);
  int seedBenchmarkMovers(int count);
  bool paintTerrain(int x, int y, TerrainType terrain);
  bool paintResource(int x, int y, ResourceType resource, uint16_t amount = 1000);
  const Organism* organismAt(int x, int y) const;
  void select(int id) { selectedId_ = id; }
  std::optional<OrganismInspection> inspect(int id) const;
  void step(int count = 1);
  NativeMetrics metrics() const;
  AtlasSnapshot atlasSnapshot() const;
  void selectSpecies(int id){selectedSpeciesId_=id;++atlasRevision_;}
  int selectedSpecies() const{return selectedSpeciesId_;}
  std::vector<uint16_t> takeDirtyTiles();
  std::vector<uint16_t> changedTiles(std::vector<uint32_t>& knownVersions) const;
  static constexpr int dirtyTileSize(){return DirtyTileSize;}
  int lineageRecordCount() const { return int(organisms_.size()); }
  int deadLineageRecordCount() const { return deadCount_; }
  int birthCount() const { return births_; }
  int respiratoryDeathCount() const { return respiratoryDeaths_; }
  const std::vector<Organism>& organisms() const { return organisms_; }
};
}
