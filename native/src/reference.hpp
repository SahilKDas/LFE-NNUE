#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

namespace life {
inline constexpr int WorldWidth = 1024, WorldHeight = 640, ClimateCycleTicks = 24'000;
inline constexpr int EnergyScale = 1'000, StartEnergy = 12'000, PlantEnergy = 4'000, CarrionEnergy = 3'000;
inline constexpr int CellBaseCost = 2, MoveCost = 15, ProduceCost = 5, AttackCost = 120;
inline constexpr int KillerDurability = 12, ArmorDurability = 18;
inline constexpr int BrainSchemaVersion = 3, FeatureCategories = 20, PositionCount = 80;
inline constexpr int HiddenSize = 16, OutputSize = 5, InputSize = 1'625;
inline constexpr int DefaultSensorRadius = 2, MaximumSensorRadius = 4, MaximumInferenceBudget = 400;

enum class CellType : uint8_t { Empty, Food, Wall, Mouth, Producer, Mover, Killer, Armor, PlantMouth, ScavengerMouth, MineralMouth, Inert };
enum class TerrainType : uint8_t { Plains, Fertile, Desert, Water, Mountain };
enum class ResourceType : uint8_t { None, Plant, Carrion, Mineral };
enum class Action : uint8_t { Up, Down, Left, Right, Wait };

class Mulberry32 {
  uint32_t state;
public:
  explicit Mulberry32(uint32_t seed) : state(seed) {}
  double operator()() {
    state += 0x6d2b79f5u;
    uint32_t value = (state ^ (state >> 15)) * (1u | state);
    value ^= value + (value ^ (value >> 7)) * (61u | value);
    return double(value ^ (value >> 14)) / 4294967296.0;
  }
};

inline double hash2d(int32_t x, int32_t y, uint32_t seed) {
  uint32_t value = uint32_t(x) ^ seed;
  value *= 0x045d9f3bu;
  value ^= (uint32_t(y) + seed) * 0x119de1f3u;
  value = (value ^ (value >> 16)) * 0x045d9f3bu;
  return double(value ^ (value >> 16)) / 4294967295.0;
}

struct ClimateSample { int season; double phase, temperature, fertility; int gradient, temperatureBand; };
inline double cyclePhase(int64_t tick) {
  int64_t wrapped = tick % ClimateCycleTicks;
  if (wrapped < 0) wrapped += ClimateCycleTicks;
  return double(wrapped) / ClimateCycleTicks;
}
inline ClimateSample climateAt(int y, int height, int64_t tick) {
  const double phase = cyclePhase(tick);
  const double latitude = height <= 1 ? 0.0 : 1.0 - 2.0 * std::clamp(y, 0, height - 1) / (height - 1.0);
  const double wave = std::sin(phase * std::numbers::pi * 2.0);
  const auto clamp = [](double value) { return std::clamp(value, 0.0, 1.0); };
  const double temperature = clamp(0.5 + 0.5 * latitude * wave);
  const double step = 2.0 / std::max(2, height - 1);
  const double north = clamp(0.5 + 0.5 * std::clamp(latitude + step, -1.0, 1.0) * wave);
  const double south = clamp(0.5 + 0.5 * std::clamp(latitude - step, -1.0, 1.0) * wave);
  const double difference = north - south;
  return {int(std::floor(phase * 4.0)) % 4, phase, temperature,
    std::clamp(0.35 + temperature, 0.35, 1.35),
    std::abs(difference) < 0.002 ? 0 : difference > 0 ? -1 : 1,
    std::min(4, int(std::floor(temperature * 5.0)))};
}

struct WorldLayers { std::vector<uint8_t> terrain, resources; std::vector<uint16_t> resourceAmount; };
inline WorldLayers generateWorld(int width, int height, uint32_t seed) {
  WorldLayers world{std::vector<uint8_t>(width * height), std::vector<uint8_t>(width * height), std::vector<uint16_t>(width * height)};
  for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
    const int index = y * width + x;
    const double coarse = hash2d(x / 24, y / 24, seed);
    const double detail = hash2d(x, y, seed ^ 0x9e3779b9u);
    const double value = coarse * .78 + detail * .22;
    const auto type = value < .13 ? TerrainType::Water : value > .9 ? TerrainType::Mountain :
      value < .3 ? TerrainType::Desert : value > .67 ? TerrainType::Fertile : TerrainType::Plains;
    world.terrain[index] = uint8_t(type);
    if (type == TerrainType::Fertile && detail > .7) { world.resources[index] = uint8_t(ResourceType::Plant); world.resourceAmount[index] = 600; }
    else if (type != TerrainType::Water && type != TerrainType::Mountain && detail < .018) { world.resources[index] = uint8_t(ResourceType::Mineral); world.resourceAmount[index] = 1'000; }
  }
  return world;
}
}
