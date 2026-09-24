#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <map>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
inline uintptr_t gameBase=0x1000000;
inline unsigned runtimeLines{},recordLines{},failedLines{},badReads{};
static void log(const char* format,...){if(strstr(format,"capture runtime tick"))++runtimeLines;if(strstr(format,"capture record tick"))++recordLines;if(strstr(format,"constructor failed"))++failedLines;}
namespace player_rig {
inline std::atomic<void*> player{reinterpret_cast<void*>(0x2000)};
inline std::map<uintptr_t,uint32_t> memory;
inline uint32_t word(uintptr_t a){auto i=memory.find(a);if(i==memory.end()){++badReads;return 0;}return i->second;}
}
namespace melee_native {inline uintptr_t component(uint32_t owner,unsigned part){return owner==7?(part==15?0x4000:part==18?0x5000:0):0;}}
namespace melee_owned_source {inline uintptr_t resident(uintptr_t,uint32_t){return 0;}}
namespace native_cast_haptics {inline void created(uintptr_t,uint32_t,uint32_t,uint32_t,int,bool){}}
#include "../diagnostic/damage_capture.hpp"
void check(bool b,const char* why){if(!b){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
int main(){
 auto& m=player_rig::memory;
 m={{0x2000,static_cast<uint32_t>(gameBase+0x1359f14)},{0x21ec,7},{gameBase+0x15f4dfc,0},
 {gameBase+0x15fec38,0x6000},{0x5028,1},{0x5038,1},{0x5024,0x7000},{0x5034,0x7100},
 {0x60d0,0x7300},{0x60d4,4},{0x7000,99},{0x7100,2},{0x7104,160},{0x7108,0x7200},{0x710c,1},{0x7200,3},
 {0x7308,0x8000},{0x730c,0x9000},{0x8004,160},{0x801c,1},{0x8020,2},{0x8024,7},
 {0x9004,84},{0x901c,1},{0x9020,3},{0x9024,7}};
 const auto before=m;
 melee_recipe_capture::created(0x8000,160,7,2,0,false);
 check(runtimeLines==1,"staff-like constructor captured without any physics window or record registration");
 melee_recipe_capture::created(0x8000,160,7,2,0,true);check(runtimeLines==1,"owned request excluded");
 const auto reads=badReads;
 melee_recipe_capture::created(0xdeadbeef,160,7,2,1,false);check(failedLines==1&&badReads==reads,"failed init logged without reading failed object");
 melee_recipe_capture::sample(0x4000);check(runtimeLines==3&&recordLines==1,"base and secondary runtime records captured");
 melee_recipe_capture::lastSample=0;melee_recipe_capture::sample(0x4000);check(runtimeLines==3,"unchanged records suppressed");
 melee_recipe_capture::created(0x8000,160,8,2,0,false);check(runtimeLines==3,"foreign owner excluded");
 check(m==before&&badReads==0,"native data unchanged and accesses bounded to fixture");
 puts("PASS: constructor-only evidence, failed-init isolation, owned/foreign exclusion, paired records and readonly sampling");
}
