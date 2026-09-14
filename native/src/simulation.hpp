#pragma once
#include "nnue.hpp"
#include "reference.hpp"
#include <cstdint>
#include <optional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace life {
struct BodyCell { CellType type; int x, y; int durability{}; };
struct Organism {
  int id{}, parentId{-1}, generation{}, birthTick{}, x{}, y{};
  std::vector<BodyCell> cells;
  int energy{StartEnergy}, minerals{}, lifetime{}, damage{}, mutability{5}; double neuralMutability{8};
  int birthDistance{4}, moveRange{4}, moveCount{}, direction{}, rotation{};
  bool living{true}, isProducer{}, isMover{}, isConsumer{}, isAttacker{};
  double thermalStress{};
  int perceptionRadius{DefaultSensorRadius}; uint8_t senseChannels{DefaultSenseChannels}; int neuralCost{168};
  std::shared_ptr<Nnue> brain;
};

struct NativeMetrics { int organisms{}, record{}, generation{}, ticks{}, largest{}; double averageEnergy{}, temperature{}, fertility{}; int nnueEvaluations{}; };

class NativeSimulation {
  int width_, height_; uint32_t worldSeed_; Mulberry32 random_; int nextId_{1}, ticks_{}, resets_{}, record_{}, largest_{};
  double foodChance_; int lifespan_, nnueEvaluations_{}; std::vector<Organism> organisms_;
  bool reproductionEnabled_{true}, mortalityEnabled_{true};
  std::unordered_map<int,size_t> slotById_;
  std::vector<float> climateTemperature_, climateFertility_; std::vector<int8_t> climateGradient_; std::vector<uint8_t> climateBand_;
  int safeIndex(int x, int y) const;
  std::pair<int,int> rotated(int x, int y, int direction) const;
  void placeBody(const Organism& organism);
  void refreshCapabilities(Organism& organism);
  void updateOrganism(Organism& organism);
  void eat(Organism& organism, CellType mouth, int x, int y);
  void produce(Organism& organism, int x, int y);
  void reproduce(Organism& parent);
  void mutate(Organism& organism);
  void die(Organism& organism);
  void attack(Organism& attacker, BodyCell& weapon, int x, int y);
  void harm(Organism& organism);
  void harmAt(Organism& organism, int index);
  Organism* organismById(int id);
  const Organism* organismById(int id) const;
  BodyCell* localCellAt(Organism& organism, int x, int y);
  bool isClear(const Organism& organism, int x, int y, int rotation) const;
  bool straightPath(int x1, int y1, int x2, int y2, const Organism& parent) const;
  std::vector<uint16_t> features(const Organism& organism) const;
  bool attemptMove(Organism& organism);
  bool attemptRotate(Organism& organism);
  void clearBody(const Organism& organism);
  void updateClimateCache();
public:
  WorldLayers world;
  std::vector<uint8_t> cells;
  std::vector<int32_t> owners;
  NativeSimulation(int width, int height, double foodChance, int lifespan, uint32_t worldSeed, uint32_t randomSeed);
  void reset();
  void regenerate(uint32_t seed);
  int seedBenchmarkMovers(int count);
  bool paintTerrain(int x, int y, TerrainType terrain);
  bool paintResource(int x, int y, ResourceType resource, uint16_t amount = 1000);
  const Organism* organismAt(int x, int y) const;
  void step(int count = 1);
  NativeMetrics metrics() const;
  const std::vector<Organism>& organisms() const { return organisms_; }
};
}
