#pragma once
#include <mgs5vr/core.hpp>
#include <array>
#include "weapon_family.hpp"
namespace amalur {
inline constexpr unsigned maxWeaponContactSamples=16;
// Landmark envelopes in native game units, not measured mesh silhouettes.
// Endpoint evidence: combat-022/chakrams-77676-after.log bone positions expressed
// in each handle frame. Radii remain provisional and require headset fitting.
struct WeaponContactProfile {
    std::array<mgs5vr::Vec3,maxWeaponContactSamples> centers{};
    float radius{};unsigned count{};
};
inline constexpr WeaponContactProfile contactLine(mgs5vr::Vec3 start,mgs5vr::Vec3 end,unsigned count,float radius){
    WeaponContactProfile p{};p.radius=radius;
    if(count==0||count>maxWeaponContactSamples)return p;
    p.count=count;
    for(unsigned i=0;i<count;++i){const float t=count==1?0.f:float(i)/float(count-1);
        p.centers[i]={start.x+(end.x-start.x)*t,start.y+(end.y-start.y)*t,start.z+(end.z-start.z)*t};}
    return p;
}
// Headset fitting, 2026-09-21 screenshots121932/121940/121951/121959:
// authored terminal markers stopped short of the visible cutting surfaces.
// These are first-pass visual fits, not extracted mesh bounds. Keep radius4,
// add overlapping samples, and exclude the dagger's grip from damage geometry.
inline constexpr auto prototypeDaggers=contactLine({0,0,8},{0,0,46},6,4.f);
// Add one terminal sphere at the existing interval, preserving every fitted
// sphere and radius below it. User headset fitting: blade extends above V5.
inline constexpr WeaponContactProfile extendContactTip(WeaponContactProfile p){
    if(p.count<2||p.count>=maxWeaponContactSamples)return p;
    const auto end=p.centers[p.count-1],previous=p.centers[p.count-2];
    p.centers[p.count++]={2*end.x-previous.x,2*end.y-previous.y,2*end.z-previous.z};
    return p;
}
inline constexpr auto longswordContact=extendContactTip(contactLine({0,0,0},{0,0,85.8f},12,4.f));
// Rusty5457 native terminal bone transformed into handle frame: (-4.706,-0.089,64.007).
// Separate provisional landmark envelope; no mesh-tip fit claimed for this skin.
inline constexpr auto rustyLongswordContact=extendContactTip(contactLine({0,0,0},{-4.706f,-.089f,64.007f},10,4.f));
inline constexpr auto staffContact=contactLine({0,0,0},{0,0,81.89f},12,4.f);
inline constexpr auto greatswordContact=contactLine({0,0,18.79f},{.32f,0,136},16,4.f);
// Only a head-centred provisional volume: do not treat the long handle as a blade.
inline constexpr auto hammerContact=contactLine({0,0,92.33f},{0,0,92.33f},1,10.f);
// Reckoning5215 captured head marker0017311f in handle006666f1 frame:
// (.4039895,.00003048,92.341949), resident bind bone3 at blob+0x100;
// corroborated by live census9268687. Same provisional
// head radius; distinct profile avoids assuming identical authored geometry.
inline constexpr auto reckoningHammerContact=contactLine({.404f,0,92.342f},{.404f,0,92.342f},1,10.f);
// Screenshot122007 disproves the old +X envelope: it crossed the hand.
// No fitted collision volume until the handle's blade axis is established.
// Diagnostic renderer shows coloured axis guides for this captured model.
inline constexpr WeaponContactProfile faebladesContact{};
inline const WeaponContactProfile* capturedContactProfile(uint32_t asset){
    switch(asset){
        case 1520:return &prototypeDaggers;case 2478:return &longswordContact;
        case 5457:return &rustyLongswordContact;
        case 1514:return &staffContact;case 1250:return &greatswordContact;
        case 1323:return &hammerContact;case 5215:return &reckoningHammerContact;case 1689:return &faebladesContact;
        default:return nullptr; // Chakrams thrown/held transitions unverified.
    }
}
inline mgs5vr::Vec3 contactCenter(const WeaponContactProfile& profile,unsigned sample,const mgs5vr::Pose& weapon){
    return mgs5vr::compose(weapon,mgs5vr::Pose{{},profile.centers.at(sample)}).position;
}
}
