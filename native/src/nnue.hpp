#pragma once
#include "reference.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <vector>
#include <immintrin.h>
#include <optional>
#include <atomic>

namespace life {
enum SenseChannel : uint16_t { Occupancy=1, Heading=2, Danger=4, Resources=8, Terrain=16, Temperature=32, Fertility=64, Internal=128, CarrionDensity=256, SoilNutrients=512 };
inline constexpr uint16_t DefaultSenseChannels = Occupancy | Heading | Danger | Resources | Internal | Temperature | Fertility;
struct Perception { int radius; uint16_t channels; int cost; };
inline Perception clampPerception(int radius, int channels) {
  int safe = std::clamp(radius, 1, MaximumSensorRadius); uint16_t mask = uint16_t(channels & 1023);
  const auto cost = [&] { return ((safe * 2 + 1) * (safe * 2 + 1) - 1) * std::max(1, std::popcount(mask)); };
  while (cost() > MaximumInferenceBudget && safe > 1) --safe;
  for (int bit = 512; cost() > MaximumInferenceBudget && bit >= 1; bit >>= 1) if (mask & bit) mask &= uint16_t(~bit);
  return {safe, mask, cost()};
}

class Nnue {
  friend class NativeSimulation;
  friend struct NativeSimulationTestAccess;
  struct alignas(64) InputRow : std::array<float,HiddenSize> {bool operator==(const InputRow&) const=default;};
  static_assert(sizeof(InputRow)==HiddenSize*sizeof(float));
  std::vector<InputRow> inputWeights;
  std::array<float, HiddenSize> hiddenBias{}, accumulator{};
  std::array<std::array<float, HiddenSize>, OutputSize> outputWeights{};
  std::array<float, OutputSize> outputBias{}, output{};
  std::vector<uint16_t> active;
  mutable uint64_t signatureCache_{};
  std::optional<uint32_t> deferredSeed_;
  static bool trainedFeature(int index) {
    return (index < 24 * FeatureCategories && index % FeatureCategories < 11) ||
      (index >= PositionCount * FeatureCategories && index < PositionCount * FeatureCategories + 8);
  }
  static float weight(Mulberry32& random, double scale) { return float((random() * 2.0 - 1.0) * scale); }
  void apply(int feature, float sign) {
    if (feature < 0 || feature >= int(inputWeights.size())) return;
    for (int hidden = 0; hidden < HiddenSize; ++hidden)
      accumulator[hidden] = float(accumulator[hidden] + sign * inputWeights[feature][hidden]);
  }
  inline static bool wideEnabled_=true;
  __attribute__((target("avx2"),always_inline)) void applyWide(int feature,float sign){
    if(feature<0||feature>=int(inputWeights.size()))return;
    const auto factor=_mm256_set1_ps(sign);
    for(int hidden=0;hidden<HiddenSize;hidden+=8){const auto old=_mm256_loadu_ps(accumulator.data()+hidden),weights=_mm256_loadu_ps(inputWeights[feature].data()+hidden);_mm256_storeu_ps(accumulator.data()+hidden,_mm256_add_ps(old,_mm256_mul_ps(factor,weights)));}
  }

public:
  struct MutationChange {uint32_t index;float delta;};
  static void captureMutation(Mulberry32& random,std::vector<MutationChange>& changes,double probability=.03,double magnitude=.2){
    constexpr uint32_t values=InputSize*HiddenSize+HiddenSize+OutputSize*HiddenSize+OutputSize;
    if(changes.capacity()<values)changes.reserve(values);changes.clear();
    for(uint32_t index=0;index<values;++index)if(random()<probability)changes.push_back({index,weight(random,magnitude)});
    if(changes.empty()){const auto index=uint32_t(random()*OutputSize);changes.push_back({InputSize*HiddenSize+HiddenSize+OutputSize*HiddenSize+index,weight(random,magnitude)});}
  }
  void applyMutation(const std::vector<MutationChange>& changes){
    materialize();for(const auto& change:changes){uint32_t index=change.index;float* value;
      if(index<InputSize*HiddenSize)value=&inputWeights[index/HiddenSize][index%HiddenSize];
      else if((index-=InputSize*HiddenSize)<HiddenSize)value=&hiddenBias[index];
      else if((index-=HiddenSize)<OutputSize*HiddenSize)value=&outputWeights[index/HiddenSize][index%HiddenSize];
      else value=&outputBias[index-OutputSize*HiddenSize];
      *value=float(*value+change.delta);
    }active.clear();accumulator=hiddenBias;signatureCache_=0;
  }
  struct DeferredInit {};
  Nnue(Mulberry32& random,DeferredInit):deferredSeed_(random.checkpoint()){
    constexpr uint32_t draws=(24*11+8)*HiddenSize+HiddenSize+OutputSize*HiddenSize+OutputSize;
    random.restore(random.checkpoint()+uint32_t(draws*0x6d2b79f5u));
  }
  void materialize(){if(!deferredSeed_)return;Mulberry32 random(*deferredSeed_);deferredSeed_.reset();Nnue initialized(random);*this=std::move(initialized);}
  explicit Nnue(Mulberry32& random) : inputWeights(InputSize), active() {
    for (int input = 0; input < InputSize; ++input)
      for (float& value : inputWeights[input]) value = trainedFeature(input) ? weight(random, .12) : 0.0f;
    for (float& value : hiddenBias) value = weight(random, .05);
    for (auto& row : outputWeights) for (float& value : row) value = weight(random, .18);
    for (float& value : outputBias) value = weight(random, .05);
    accumulator = hiddenBias;
  }
  Nnue(Nnue&&)=default;Nnue& operator=(Nnue&&)=default;
  Nnue(const Nnue& source){const_cast<Nnue&>(source).materialize();inputWeights=source.inputWeights;hiddenBias=source.hiddenBias;accumulator=source.hiddenBias;outputWeights=source.outputWeights;outputBias=source.outputBias;signatureCache_=source.signatureCache_;}
  void mutate(Mulberry32& random, double probability=.03, double magnitude=.2) {
    materialize();
    bool changed=false;const auto perturb=[&](float& value){if(random()<probability){value=float(value+weight(random,magnitude));changed=true;}};
    for(auto& row:inputWeights)for(float& value:row)perturb(value);for(float& value:hiddenBias)perturb(value);for(auto& row:outputWeights)for(float& value:row)perturb(value);for(float& value:outputBias)perturb(value);
    if(!changed){const int index=int(std::floor(random()*OutputSize));outputBias[index]=float(outputBias[index]+weight(random,magnitude));}
    active.clear();accumulator=hiddenBias;signatureCache_=0;
  }
  // Reserve precisely the mutation's random stream before placement is known.
  // Failed births need neither a genome allocation nor writes to 100+ KB of weights.
  static void consumeMutation(Mulberry32& random,double probability=.03){
    bool changed=false;
    constexpr int values=InputSize*HiddenSize+HiddenSize+OutputSize*HiddenSize+OutputSize;
    for(int index=0;index<values;++index)if(random()<probability){(void)random();changed=true;}
    if(!changed){(void)random();(void)random();}
  }
  uint64_t signature() const {const_cast<Nnue*>(this)->materialize();std::atomic_ref<uint64_t> cache(signatureCache_);if(const auto cached=cache.load(std::memory_order_relaxed))return cached;uint64_t hash=1469598103934665603ULL;const auto add=[&](float value){hash^=std::bit_cast<uint32_t>(value);hash*=1099511628211ULL;};for(const auto& row:inputWeights)for(const float value:row)add(value);for(const float value:hiddenBias)add(value);for(const auto& row:outputWeights)for(const float value:row)add(value);for(const float value:outputBias)add(value);const auto value=hash?hash:1;cache.store(value,std::memory_order_relaxed);return value;}
  bool featureRowZero(int feature) const {return feature>=0&&feature<int(inputWeights.size())&&std::all_of(inputWeights[feature].begin(),inputWeights[feature].end(),[](float value){return value==0;});}
  const std::array<float, OutputSize>& evaluateScalar(std::span<const uint16_t> features) {
    size_t oldIndex = 0, newIndex = 0;
    while (oldIndex < active.size() || newIndex < features.size()) {
      const int oldFeature = oldIndex < active.size() ? active[oldIndex] : InputSize;
      const int newFeature = newIndex < features.size() ? features[newIndex] : InputSize;
      if (oldFeature == newFeature) { ++oldIndex; ++newIndex; }
      else if (oldFeature < newFeature) { apply(oldFeature, -1.0f); ++oldIndex; }
      else { apply(newFeature, 1.0f); ++newIndex; }
    }
    active.assign(features.begin(), features.end());
    for (int action = 0; action < OutputSize; ++action) {
      double value = outputBias[action];
      for (int hidden = 0; hidden < HiddenSize; ++hidden)
        value += double(std::max(0.0f, accumulator[hidden])) * outputWeights[action][hidden];
      output[action] = float(value);
    }
    return output;
  }
  __attribute__((target("avx2")))
  const std::array<float, OutputSize>& evaluateWide(std::span<const uint16_t> features) {
    size_t oldIndex = 0, newIndex = 0;
    while (oldIndex < active.size() || newIndex < features.size()) {
      const int oldFeature = oldIndex < active.size() ? active[oldIndex] : InputSize;
      const int newFeature = newIndex < features.size() ? features[newIndex] : InputSize;
      if (oldFeature == newFeature) { ++oldIndex; ++newIndex; }
      else if (oldFeature < newFeature) { applyWide(oldFeature, -1.0f); ++oldIndex; }
      else { applyWide(newFeature, 1.0f); ++newIndex; }
    }
    active.assign(features.begin(), features.end());
    for (int action = 0; action < OutputSize; ++action) {
      double value = outputBias[action];
      for (int hidden = 0; hidden < HiddenSize; ++hidden)
        value += double(std::max(0.0f, accumulator[hidden])) * outputWeights[action][hidden];
      output[action] = float(value);
    }
    return output;
  }
  static bool wideAvailable(){static const bool supported=[] {__builtin_cpu_init();return bool(__builtin_cpu_supports("avx2"));}();return supported;}
  static void enableWide(bool value){wideEnabled_=value;}
  const std::array<float,OutputSize>& evaluate(std::span<const uint16_t> features){materialize();
    // Fetch independent rows together instead of stalling the accumulator chain
    // on each family's first cold row. Alignment keeps a row on one cache line.
    for(const auto feature:features)if(feature<inputWeights.size())__builtin_prefetch(inputWeights[feature].data(),0,3);
    for(const auto feature:active)if(feature<inputWeights.size())__builtin_prefetch(inputWeights[feature].data(),0,3);
    return wideEnabled_&&wideAvailable()?evaluateWide(features):evaluateScalar(features);}
  int action(std::span<const uint16_t> features) {
    const auto& scores = evaluate(features); int best = 0;
    for (int index = 1; index < OutputSize; ++index) if (scores[index] > scores[best]) best = index;
    return best;
  }
};
}
