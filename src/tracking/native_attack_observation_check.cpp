#include "native_attack_observation.hpp"
#include "native_weapon_slot.hpp"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
static void check(bool b){if(!b)std::abort();}
int main(){using namespace amalur;
 // Same attached dagger can be visited twice during native dual remapping.
 ActiveWeaponSet dagger;dagger.observe(true,false);dagger.observe(true,false);check(dagger.accepts());
 ActiveWeaponSet besideChakram;besideChakram.observe(true,true);besideChakram.observe(false,false);check(!besideChakram.accepts());
 ActiveWeaponSet missingStowProof;missingStowProof.observe(true,false);missingStowProof.observe(false,false);check(!missingStowProof.accepts());
 ActiveWeaponSet heldAndBack;heldAndBack.observe(true,false);heldAndBack.observe(false,true);check(heldAndBack.accepts());
 ActiveWeaponSet onlyBack;onlyBack.observe(true,true);check(!onlyBack.accepts());
 ActiveWeaponSet absent;absent.observe(false,true);check(!absent.accepts());
 NativeWeaponSlot back{100,200,300,400,1000,0,true};
 check(freshBackSocket(back,100,300,200,400,0,1099));check(!freshBackSocket(back,100,300,200,400,0,1100));
 check(!freshBackSocket(back,100,300,200,400,1,1001));check(!freshBackSocket(back,100,301,200,400,0,1001));
 for(auto attack:{763u,764u,765u,766u,1095u}){check(capturedNativeAttackModel(attack)==1877);check(!nativeAttackVisualCorrelation(attack,1520,true,1000,1001,0,0));}
 check(nativeAttackVisualCorrelation(199,1520,true,1000,1001,0,0));
 check(!nativeAttackVisualCorrelation(199,1520,true,1000,1001,1,0));
 check(!nativeAttackVisualCorrelation(199,1520,false,1000,1001,0,0));
 check(!nativeAttackVisualCorrelation(199,1520,true,1000,1100,0,0));
 check(!nativeAttackVisualCorrelation(199,1520,true,1002,1001,0,0));
 check(!nativeAttackVisualCorrelation(999,1520,true,1000,1001,0,0));
 check(nativeAttackVisualCorrelation(16,1323,true,1000,1001,0,0));
 check(nativeAttackVisualCorrelation(483,1689,true,1000,1001,0,0));
 check(nativeAttackVisualCorrelation(160,1514,true,1000,1001,1,1));
 std::puts("native attack attribution and active weapon policy: PASS");
}
