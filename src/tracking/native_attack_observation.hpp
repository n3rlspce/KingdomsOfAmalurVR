#pragma once
#include <cstdint>
namespace amalur {
// Diagnostic correlation only: these captures do not authorize replay, damage
// recipes, combo progression, or model variants outside the exact observations.
inline constexpr uint32_t capturedNativeAttackModel(uint32_t asset){
 switch(asset){
 case 199:case 200:case 201:case 202:return 1520;
 case 50:return 2478;case 417:return 1250;
 case 16:case 17:case 18:return 1323;
 case 84:case 483:case 85:case 484:case 86:return 1689;
 case 160:case 161:return 1514;
 case 763:case 764:case 765:case 766:case 1095:return 1877;
 default:return 0;
 }
}
inline constexpr bool nativeAttackVisualCorrelation(uint32_t asset,uint32_t model,bool runtimeValid,uint64_t poseTick,uint64_t now,unsigned poseSelection,unsigned selection){
 return runtimeValid&&capturedNativeAttackModel(asset)==model&&model&&poseTick&&poseTick<=now&&now-poseTick<100&&selection<=1&&poseSelection==selection;
}
}
