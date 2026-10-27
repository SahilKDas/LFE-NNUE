#include "atlas.hpp"
#include <algorithm>
#include <bit>
#include <cmath>

namespace life {
double genomeDistanceScalar(const GenomeDescriptor& left,const GenomeDescriptor& right){
  int cellDelta=0,cellTotal=0;for(size_t index=0;index<left.cellCounts.size();++index){cellDelta+=std::abs(int(left.cellCounts[index])-int(right.cellCounts[index]));cellTotal+=std::max(int(left.cellCounts[index]),int(right.cellCounts[index]));}
  const double topology=left.bodyHash==right.bodyHash?0.0:std::min(1.0,.35+double(cellDelta)/std::max(1,cellTotal));
  const double neural=double(std::popcount(left.neuralSignature^right.neuralSignature))/64.0;
  const double behavior=(std::abs(int(left.perceptionRadius)-int(right.perceptionRadius))/3.0+double(std::popcount(uint8_t(left.senseChannels^right.senseChannels)))/8.0+std::min(1.0,std::abs(int(left.moveRange)-int(right.moveRange))/8.0)+std::min(1.0,std::abs(int(left.birthDistance)-int(right.birthDistance))/8.0))/4.0;
  const double physiology=((left.dietMask==right.dietMask?0.0:1.0)+std::abs(int(left.oxygenTolerance)-int(right.oxygenTolerance))/800.0+std::abs(int(left.stressRecovery)-int(right.stressRecovery))/230.0+std::abs(int(left.frailty)-int(right.frailty))/1650.0)/4.0;
  return topology*.40+neural*.30+behavior*.15+physiology*.15;
}
__attribute__((target("popcnt")))
double genomeDistancePopcnt(const GenomeDescriptor& left,const GenomeDescriptor& right){
  int cellDelta=0,cellTotal=0;for(size_t index=0;index<left.cellCounts.size();++index){cellDelta+=std::abs(int(left.cellCounts[index])-int(right.cellCounts[index]));cellTotal+=std::max(int(left.cellCounts[index]),int(right.cellCounts[index]));}
  const double topology=left.bodyHash==right.bodyHash?0.0:std::min(1.0,.35+double(cellDelta)/std::max(1,cellTotal));
  const double neural=double(std::popcount(left.neuralSignature^right.neuralSignature))/64.0;
  const double behavior=(std::abs(int(left.perceptionRadius)-int(right.perceptionRadius))/3.0+double(std::popcount(uint8_t(left.senseChannels^right.senseChannels)))/8.0+std::min(1.0,std::abs(int(left.moveRange)-int(right.moveRange))/8.0)+std::min(1.0,std::abs(int(left.birthDistance)-int(right.birthDistance))/8.0))/4.0;
  const double physiology=((left.dietMask==right.dietMask?0.0:1.0)+std::abs(int(left.oxygenTolerance)-int(right.oxygenTolerance))/800.0+std::abs(int(left.stressRecovery)-int(right.stressRecovery))/230.0+std::abs(int(left.frailty)-int(right.frailty))/1650.0)/4.0;
  return topology*.40+neural*.30+behavior*.15+physiology*.15;
}
double genomeDistance(const GenomeDescriptor& left,const GenomeDescriptor& right){
  static const auto kernel=[] {__builtin_cpu_init();return __builtin_cpu_supports("popcnt")?genomeDistancePopcnt:genomeDistanceScalar;}();return kernel(left,right);
}
double genomeDistanceBoundedScalar(const GenomeDescriptor& left,const GenomeDescriptor& right,double bound){
  if(left.bodyHash==right.bodyHash&&left.neuralSignature==right.neuralSignature&&left.perceptionRadius==right.perceptionRadius&&uint8_t(left.senseChannels)==uint8_t(right.senseChannels)&&left.moveRange==right.moveRange&&left.birthDistance==right.birthDistance&&left.dietMask==right.dietMask&&left.oxygenTolerance==right.oxygenTolerance&&left.stressRecovery==right.stressRecovery&&left.frailty==right.frailty)return 0;
  const double neural=double(std::popcount(left.neuralSignature^right.neuralSignature))/64.0;
  double topology=0;if(left.bodyHash!=right.bodyHash){if(.35*.40+neural*.30>bound)return 2;int cellDelta=0,cellTotal=0;for(size_t index=0;index<left.cellCounts.size();++index){cellDelta+=std::abs(int(left.cellCounts[index])-int(right.cellCounts[index]));cellTotal+=std::max(int(left.cellCounts[index]),int(right.cellCounts[index]));}topology=std::min(1.0,.35+double(cellDelta)/std::max(1,cellTotal));}
  const double partial=topology*.40+neural*.30;if(partial>bound)return 2;
  // Descriptor fields are bounded unsigned integers; this estimate's total
  // rounding error is below 1e-11. The 1e-9 rejection margin is conservative.
  // All potentially winning candidates still use the exact reference divisions.
  const double approximateBehavior=(std::abs(int(left.perceptionRadius)-int(right.perceptionRadius))*(1.0/3.0)+double(std::popcount(uint8_t(left.senseChannels^right.senseChannels)))*.125+std::min(1.0,std::abs(int(left.moveRange)-int(right.moveRange))*.125)+std::min(1.0,std::abs(int(left.birthDistance)-int(right.birthDistance))*.125))*.25;
  const double approximatePhysiology=((left.dietMask==right.dietMask?0.0:1.0)+std::abs(int(left.oxygenTolerance)-int(right.oxygenTolerance))*(1.0/800.0)+std::abs(int(left.stressRecovery)-int(right.stressRecovery))*(1.0/230.0)+std::abs(int(left.frailty)-int(right.frailty))*(1.0/1650.0))*.25;
  if(partial+approximateBehavior*.15+approximatePhysiology*.15>bound+1e-9)return 2;
  const double behavior=(std::abs(int(left.perceptionRadius)-int(right.perceptionRadius))/3.0+double(std::popcount(uint8_t(left.senseChannels^right.senseChannels)))/8.0+std::min(1.0,std::abs(int(left.moveRange)-int(right.moveRange))/8.0)+std::min(1.0,std::abs(int(left.birthDistance)-int(right.birthDistance))/8.0))/4.0;
  const double physiology=((left.dietMask==right.dietMask?0.0:1.0)+std::abs(int(left.oxygenTolerance)-int(right.oxygenTolerance))/800.0+std::abs(int(left.stressRecovery)-int(right.stressRecovery))/230.0+std::abs(int(left.frailty)-int(right.frailty))/1650.0)/4.0;
  return partial+behavior*.15+physiology*.15;
}
__attribute__((target("popcnt")))
double genomeDistanceBoundedPopcnt(const GenomeDescriptor& left,const GenomeDescriptor& right,double bound){
  if(left.bodyHash==right.bodyHash&&left.neuralSignature==right.neuralSignature&&left.perceptionRadius==right.perceptionRadius&&uint8_t(left.senseChannels)==uint8_t(right.senseChannels)&&left.moveRange==right.moveRange&&left.birthDistance==right.birthDistance&&left.dietMask==right.dietMask&&left.oxygenTolerance==right.oxygenTolerance&&left.stressRecovery==right.stressRecovery&&left.frailty==right.frailty)return 0;
  const double neural=double(std::popcount(left.neuralSignature^right.neuralSignature))/64.0;
  double topology=0;if(left.bodyHash!=right.bodyHash){if(.35*.40+neural*.30>bound)return 2;int cellDelta=0,cellTotal=0;for(size_t index=0;index<left.cellCounts.size();++index){cellDelta+=std::abs(int(left.cellCounts[index])-int(right.cellCounts[index]));cellTotal+=std::max(int(left.cellCounts[index]),int(right.cellCounts[index]));}topology=std::min(1.0,.35+double(cellDelta)/std::max(1,cellTotal));}
  const double partial=topology*.40+neural*.30;if(partial>bound)return 2;
  // Descriptor fields are bounded unsigned integers; this estimate's total
  // rounding error is below 1e-11. The 1e-9 rejection margin is conservative.
  // All potentially winning candidates still use the exact reference divisions.
  const double approximateBehavior=(std::abs(int(left.perceptionRadius)-int(right.perceptionRadius))*(1.0/3.0)+double(std::popcount(uint8_t(left.senseChannels^right.senseChannels)))*.125+std::min(1.0,std::abs(int(left.moveRange)-int(right.moveRange))*.125)+std::min(1.0,std::abs(int(left.birthDistance)-int(right.birthDistance))*.125))*.25;
  const double approximatePhysiology=((left.dietMask==right.dietMask?0.0:1.0)+std::abs(int(left.oxygenTolerance)-int(right.oxygenTolerance))*(1.0/800.0)+std::abs(int(left.stressRecovery)-int(right.stressRecovery))*(1.0/230.0)+std::abs(int(left.frailty)-int(right.frailty))*(1.0/1650.0))*.25;
  if(partial+approximateBehavior*.15+approximatePhysiology*.15>bound+1e-9)return 2;
  const double behavior=(std::abs(int(left.perceptionRadius)-int(right.perceptionRadius))/3.0+double(std::popcount(uint8_t(left.senseChannels^right.senseChannels)))/8.0+std::min(1.0,std::abs(int(left.moveRange)-int(right.moveRange))/8.0)+std::min(1.0,std::abs(int(left.birthDistance)-int(right.birthDistance))/8.0))/4.0;
  const double physiology=((left.dietMask==right.dietMask?0.0:1.0)+std::abs(int(left.oxygenTolerance)-int(right.oxygenTolerance))/800.0+std::abs(int(left.stressRecovery)-int(right.stressRecovery))/230.0+std::abs(int(left.frailty)-int(right.frailty))/1650.0)/4.0;
  return partial+behavior*.15+physiology*.15;
}
double genomeDistanceBounded(const GenomeDescriptor& left,const GenomeDescriptor& right,double bound){
  static const auto kernel=[] {__builtin_cpu_init();return __builtin_cpu_supports("popcnt")?genomeDistanceBoundedPopcnt:genomeDistanceBoundedScalar;}();return kernel(left,right,bound);
}
const char* evolutionEventName(EvolutionEventType type){constexpr const char* names[]={"Season","Population peak","Population crash","Recovery","Resource shortage","Atmospheric stress","Speciation","Extinction","World regeneration","Carrion accumulation","Nutrient collapse","Nutrient recovery","Decomposer emergence","Decomposer extinction","Habitat shift"};return names[size_t(type)];}
}
