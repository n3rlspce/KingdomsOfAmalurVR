#include "weapon_selection.hpp"
#include <cstdio>
#include <cstdlib>
using namespace amalur;
void check(bool ok,const char* label){if(!ok){std::printf("FAIL: %s\n",label);std::exit(1);}}
int main(){
    unsigned index=999;
    check(equippedWeaponIndex(7,33,8,2,0,45,index)&&index==8,"observed primary inventory index");
    check(equippedWeaponIndex(7,33,8,2,1,45,index)&&index==9,"observed secondary inventory index");
    check(equippedWeaponIndex(2,10,17,2,1,30,index)&&index==18,"runtime type/base not hardcoded");
    check(!equippedWeaponIndex(33,33,8,2,1,45,index),"invalid type rejected");
    check(!equippedWeaponIndex(7,33,8,2,2,45,index),"invalid VR slot rejected");
    check(!equippedWeaponIndex(7,33,44,2,1,45,index),"truncated equipped handles rejected");
    check(!equippedWeaponIndex(7,33,0xffffffff,2,1,45,index),"overflow index rejected");
    check(!equippedWeaponIndex(7,33,8,1,1,45,index),"wrong equipment bucket rejected");
    WeaponSelectionLatch latch;WeaponSelectionRequest primary{0x81dc9800,0x26d0005,1,0,0x910090};
    auto secondary=primary;secondary.slot=1;secondary.weapon=0x1190148;
    check(latch.pending(primary),"initial valid selection synchronizes");latch.commit(primary);
    check(!latch.pending(primary),"same selection never repeatedly queues");
    check(latch.pending(secondary),"Y primary to bow dispatches without attack");latch.commit(secondary);
    for(unsigned i=0;i<1000;++i)check(!latch.pending(secondary),"native deferred switch sent once");
    check(latch.pending(primary),"Y return to primary dispatches");latch.commit(primary);
    auto changed=primary;changed.weapon=0x3f00ba;check(latch.pending(changed),"new equipment in same slot resynchronizes");
    changed=primary;++changed.session;check(latch.pending(changed),"bridge session change resynchronizes");
    changed=primary;++changed.owner;check(latch.pending(changed),"load/player owner change resynchronizes");
    changed=primary;++changed.player;check(latch.pending(changed),"replaced player object resynchronizes");
    changed=primary;changed.weapon=0;check(!latch.pending(changed),"empty target never dispatches");latch.commit(changed);
    check(!latch.pending(primary),"rejected target cannot consume valid state");
    puts("PASS: equipped-slot bounds, real owner identity, switch/back/equipment/session transitions and deferred request dedup");
}
