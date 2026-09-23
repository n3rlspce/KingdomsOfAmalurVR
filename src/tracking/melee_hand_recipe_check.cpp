#include "melee_hand_recipe.hpp"
#include <cassert>
int main(){
    using namespace amalur;
    NativeFamilyChain chains[2];MeleeSwingEvent events[2];
    auto commit=[&](unsigned side,uint64_t now){
        const auto r=chains[side].commit(9,1520,3,now);
        auto& e=events[side];e.weapon=9;e.asset=1520;e.hand=side;e.generation=3;e.tick=now;
        ++e.serial;e.attackAsset=r.attack;e.attackFlags=r.flags;e.chainStep=r.step;
    };
    commit(0,1000);commit(0,1100);commit(1,1150);
    assert(events[0].attackAsset==200&&events[1].attackAsset==199);
    assert(currentHandRecipe(events[0],0,9,1520,3,1150));
    assert(currentHandRecipe(events[1],1,9,1520,3,1150));
    assert(!currentHandRecipe(events[0],1,9,1520,3,1150));
    assert(!currentHandRecipe(events[1],0,9,1520,3,1150));
    commit(0,1200);commit(1,1250);
    assert(events[0].attackAsset==201&&events[0].attackFlags==2);
    assert(events[1].attackAsset==200&&events[1].attackFlags==0);
    const auto left=events[1];commit(0,1300);
    assert(events[0].attackAsset==202&&events[1].serial==left.serial&&events[1].attackAsset==left.attackAsset);
    commit(0,1400);assert(events[0].attackAsset==199);
    assert(!currentHandRecipe(events[0],0,10,1520,3,1400));
    assert(!currentHandRecipe(events[0],0,9,1250,3,1400));
    assert(!currentHandRecipe(events[0],0,9,1520,4,1400));
    assert(!currentHandRecipe(events[0],0,9,1520,3,1399));
    auto bad=events[0];bad.attackFlags=2;assert(!currentHandRecipe(bad,0,9,1520,3,1400));
    bad=events[0];bad.asset=1250;bad.hand=1;bad.attackAsset=417;bad.attackFlags=0;
    assert(!currentHandRecipe(bad,1,9,1250,3,1400));
    commit(1,2500);assert(events[1].attackAsset==199);
}
