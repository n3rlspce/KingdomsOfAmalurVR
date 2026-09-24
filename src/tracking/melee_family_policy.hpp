#pragma once
#include "longsword_contact_policy.hpp"
#include "physical_melee_recipe.hpp"
#include "melee_native_family.hpp"
#include <cmath>
namespace amalur {
// Only captured representatives with a native direct-damage definition are
// admitted. This does not generalize support to other skins in these families.
inline constexpr bool supportedMeleeHand(uint32_t model,unsigned hand){
    const auto recipe=physicalMeleeRecipe(model);
    return recipe.attack&&hand<recipe.hands;
}
inline constexpr bool basicStrokeModel(uint32_t model){
    return physicalMeleeRecipe(model).attack&&!knownLongswordModel(model);
}
// Fixed basic attacks until per-family combo/heavy captures prove otherwise.
// Called when creating a candidate, before feedback or native contact executes.
inline bool assignBasicStrokeRecipe(MeleeSwingEvent& event){
    if(!basicStrokeModel(event.asset)||!supportedMeleeHand(event.asset,event.hand))return false;
    const auto recipe=physicalMeleeRecipe(event.asset);
    event.attackAsset=recipe.attack;event.attackFlags=recipe.flags;
    event.chainStep=1;event.heavy=false;return true;
}
inline bool sameMeleeContact(const MeleeSwingEvent& proposed,const MeleeSwingEvent& live,
    uint32_t weapon,uint32_t model,unsigned generation,unsigned hand,
    uint64_t proposedFrame,uint64_t liveFrame,uint32_t attack,uint32_t flags){
    if(!supportedMeleeHand(model,hand)||!weapon||!proposed.serial
        ||proposed.serial!=live.serial||proposed.weapon!=weapon||live.weapon!=weapon
        ||proposed.asset!=model||live.asset!=model||proposed.owner!=live.owner
        ||proposed.generation!=generation||live.generation!=generation
        ||proposed.hand!=hand||live.hand!=hand||!proposedFrame||proposedFrame!=liveFrame
        ||proposed.attackAsset!=attack||live.attackAsset!=attack
        ||proposed.attackFlags!=flags||live.attackFlags!=flags)return false;
    if(knownLongswordModel(model))return sameLongswordContact(proposed,live,weapon,model,generation,
        proposedFrame,liveFrame,attack,flags);
    const auto recipe=nativeFamilyAttack(attack);
    return supportedNativeFamilyAttack(model,attack,flags)&&!proposed.heavy&&!live.heavy
        &&proposed.chainStep==recipe.step&&live.chainStep==recipe.step;
}
// Travel is blade/contact-point travel in game units, elapsed is milliseconds.
// This is NOT target-relative velocity: moving-target subtraction is not yet
// available. Shared V5 blade thresholds remain provisional headset tuning.
inline bool meleeContactSpeed(uint32_t model,mgs5vr::Vec3 travel,mgs5vr::Vec3 axis,
    uint64_t elapsedMs,float unitsPerMetre){
    if(!physicalMeleeRecipe(model).attack)return false;
    const float axisLength=std::sqrt(mgs5vr::dot(axis,axis));
    if(!std::isfinite(axisLength)||axisLength<.001f)return false;
    axis=axis*(1.f/axisLength);
    // The hammer damages with its head: thrusting along its handle is not a
    // blade stab and must not receive the 2 m/s stab threshold.
    if(knownHammerModel(model))axis={};
    return longswordContactSpeed(travel,axis,elapsedMs,unitsPerMetre);
}
}
