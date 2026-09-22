#pragma once
#include "melee_swing_event.hpp"
#include "longsword_damage_recipe.hpp"
#include "weapon_family.hpp"
#include <cmath>
namespace amalur {
inline bool sameLongswordContact(const MeleeSwingEvent& proposed,const MeleeSwingEvent& live,
    uint32_t weapon,uint32_t model,unsigned generation,uint64_t proposedFrame,uint64_t liveFrame,
    uint32_t attack,uint32_t flags){
    return proposed.serial&&proposed.serial==live.serial&&proposed.weapon==weapon&&live.weapon==weapon
        &&knownLongswordModel(model)&&proposed.asset==model&&live.asset==model
        &&proposed.generation==generation&&live.generation==generation&&proposed.hand==0&&live.hand==0
        &&proposedFrame&&proposedFrame==liveFrame
        &&proposed.attackAsset==attack&&live.attackAsset==attack&&proposed.attackFlags==flags&&live.attackFlags==flags
        &&supportedLongswordDamage(attack,flags);
}
inline bool longswordContactSpeed(mgs5vr::Vec3 travel,mgs5vr::Vec3 axis,uint64_t elapsedMs,float unitsPerMetre){
    if(!elapsedMs||elapsedMs>100||!std::isfinite(unitsPerMetre)||unitsPerMetre<=1)return false;
    const float length=std::sqrt(mgs5vr::dot(travel,travel));
    if(!std::isfinite(length)||length<=.001f)return false;
    const float alignment=mgs5vr::dot(travel*(1.f/length),axis);
    if(!std::isfinite(alignment))return false;
    const float speed=length*1000.f/(float(elapsedMs)*unitsPerMetre);
    return speed>=(alignment>.8f?2.f:5.f);
}
}
