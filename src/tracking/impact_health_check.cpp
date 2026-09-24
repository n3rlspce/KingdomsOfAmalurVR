#include <windows.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
static bool alive=true;static uint32_t hp[32]{};
namespace player_rig {uintptr_t resolve(uint32_t h){return alive&&h==12?123:0;}uintptr_t word(uintptr_t p){return *reinterpret_cast<uint32_t*>(p);}}
namespace melee_native {struct HitArray {void* data{};uint32_t count{},capacity{};};uintptr_t component(uint32_t h,unsigned){return alive&&h==12?reinterpret_cast<uintptr_t>(hp):0;}}
namespace amalur {uint32_t meleeHitActor(uintptr_t h){return uint32_t(h);}}
#include "../diagnostic/impact_health.hpp"
void ck(bool b){if(!b)std::abort();}
int main(){uint32_t hit[28]{};hit[5]=12;hp[18]=30;melee_native::HitArray hits{hit,1,1};
 auto s=impact_health::before(hits,9);ck(s.count==1&&!impact_health::decreased(s));
 hp[18]=20;ck(impact_health::decreased(s));hp[18]=0;ck(impact_health::decreased(s));
 alive=false;ck(!impact_health::decreased(s));alive=true;
 hp[18]=30;ck(impact_health::before(hits,12).count==0);
 puts("PASS native health confirmation unchanged/reduced/killing-blow/despawn and self filtering");}
