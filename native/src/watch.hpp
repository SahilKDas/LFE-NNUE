#pragma once
#include "nnue.hpp"
#include <cmath>
#include <string>

namespace life {
enum class FamilyRole : uint8_t { Unrelated, Root, Child, Descendant };
struct DecisionRecord {
  int tick{},x{},y{},rotation{},radius{},action{-1};uint16_t channels{},featureCount{};
  int direction{},temperatureBand{},gradient{},season{},fertilityBand{};bool highEnergy{},damaged{};
  std::array<uint16_t,InputSize> features{};
  std::array<float,OutputSize> outputs{};
};
struct DecodedSense {
  int dx{},dy{};uint32_t categories{};bool nutrients{},carrionDensity{};
};
inline std::vector<DecodedSense> decodeSenses(const DecisionRecord& decision){
  std::vector<DecodedSense> result;result.reserve(PositionCount);int position=0;
  for(int ring=1;ring<=MaximumSensorRadius;++ring)for(int dy=-ring;dy<=ring;++dy)for(int dx=-ring;dx<=ring;++dx){
    if((!dx&&!dy)||std::max(std::abs(dx),std::abs(dy))!=ring)continue;
    const int index=position++;if(ring>decision.radius)continue;DecodedSense cell{dx,dy};
    for(size_t feature=0;feature<decision.featureCount;++feature){const int value=decision.features[feature];if(value<PositionCount*FeatureCategories&&value/FeatureCategories==index)cell.categories|=1u<<(value%FeatureCategories);else if(value==NutrientFeatureOffset+index)cell.nutrients=true;else if(value==CarrionDensityFeatureOffset+index)cell.carrionDensity=true;}
    result.push_back(cell);
  }return result;
}
inline std::string senseLabel(const DecodedSense& cell){
  constexpr const char* labels[]={"Empty","Plant","Wall","Self","Organism","Killer","Danger","Heading up","Heading down","Heading left","Heading right","Unused","Carrion","Mineral","Unused","Plains","Fertile","Desert","Water","Mountain"};
  std::string result;for(int bit=0;bit<FeatureCategories;++bit)if(cell.categories&(1u<<bit)){if(!result.empty())result+=" / ";result+=labels[bit];}if(cell.nutrients)result+=(result.empty()?"":" / ")+std::string("Nutrients");if(cell.carrionDensity)result+=(result.empty()?"":" / ")+std::string("Dense carrion");return result.empty()?"No enabled signal":result;
}
inline int wrapCoordinate(int value,int size){return (value%size+size)%size;}
inline double nearestWrapped(double value,double reference,double size){return value+std::round((reference-value)/size)*size;}
struct FollowCamera {
  bool following{},wrapped{};int target{-1};double anchorX{},anchorY{},offsetX{},offsetY{};
  void begin(int id,double x,double y,double cameraX,double cameraY,double zoom,double viewportWidth,double viewportHeight){target=id;following=wrapped=true;anchorX=nearestWrapped(x,(viewportWidth*.5-cameraX)/zoom,WorldWidth);anchorY=nearestWrapped(y,(viewportHeight*.5-cameraY)/zoom,WorldHeight);offsetX=cameraX;offsetY=cameraY;}
  void release(){following=false;}
  void clear(){following=wrapped=false;target=-1;}
  void advance(double x,double y,bool alive,double zoom,double width,double height,double seconds,bool suspended=false){
    if(!following||suspended)return;
    anchorX=nearestWrapped(x,anchorX,WorldWidth);anchorY=nearestWrapped(y,anchorY,WorldHeight);
    if(!alive){following=false;return;}
    const double alpha=1-std::exp(-std::max(0.0,seconds)/.12);offsetX+=(width*.5-(anchorX+.5)*zoom-offsetX)*alpha;offsetY+=(height*.5-(anchorY+.5)*zoom-offsetY)*alpha;
  }
};
struct FollowPreferences {int target{-1};bool enabled{},family{true},compact{},wrapped{};};
struct FollowHudLayout {
  float left{16},top{96},right{},bottom{},buttonWidth{};bool compact{};
  FollowHudLayout(float viewportRight,bool requestedCompact):right(std::min(viewportRight-16,456.f)),bottom(top+((requestedCompact||viewportRight<950)?116:434)),buttonWidth((right-left-30)/4),compact(requestedCompact||viewportRight<950){}
  bool visible() const{return right-left>=280;}
  bool contains(float x,float y) const{return visible()&&x>=left&&x<=right&&y>=top&&y<=bottom;}
  int buttonAt(float x,float y) const{if(!visible()||y<top+69||y>top+92)return -1;for(int index=0;index<4;++index){const float begin=left+6+index*(buttonWidth+6);if(x>=begin&&x<=begin+buttonWidth)return index;}return -1;}
};
}
