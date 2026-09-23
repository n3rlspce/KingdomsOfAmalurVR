#pragma once
#include "melee_swing_event.hpp"
#include "melee_native_family.hpp"
#include "melee_family_policy.hpp"
#include <cstring>
namespace amalur {
inline bool currentHandRecipe(const MeleeSwingEvent& strike,unsigned side,uint32_t weapon,uint32_t model,
    unsigned generation,uint64_t now){
    return supportedMeleeHand(model,side)&&strike.hand==side&&strike.weapon==weapon&&strike.asset==model&&strike.generation==generation
        &&strike.tick&&strike.tick<=now&&strike.serial
        &&(knownLongswordModel(model)?supportedLongswordDamage(strike.attackAsset,strike.attackFlags)
            :supportedNativeFamilyAttack(model,strike.attackAsset,strike.attackFlags));
}
// The resolver temporarily borrows the actor's native dedup slots. Keep both
// native arrays together and harvest only the currently executing hand.
template<class T> inline void beginHandDedup(T* native,const T* hand,T* saved){
    std::memcpy(saved,native,2*sizeof(T));std::memcpy(native,hand,2*sizeof(T));
}
template<class T> inline void endHandDedup(T* native,T* hand,const T* saved){
    std::memcpy(hand,native,2*sizeof(T));std::memcpy(native,saved,2*sizeof(T));
}
}
