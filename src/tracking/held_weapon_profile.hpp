#pragma once
#include <cstdint>
#include "weapon_family.hpp"

namespace amalur {
// Primary preserves existing behavior; secondary must have one unambiguous
// current player weapon attachment. Never draw both equipped inventory slots.
inline bool trackedWeaponSelection(uint32_t selected,bool uniqueAttachment){
    return selected==0||(selected==1&&uniqueAttachment);
}
enum class HeldWeaponKind {None,Longsword,Staff,Greatsword,Hammer,Faeblades};
// Exact captured representatives, not a claim of support for every family skin.
// Evidence: combat-022/captures/chakrams-77676-after.log, census asset IDs below.
struct CapturedHeldWeaponProfile {
    HeldWeaponKind kind;
    uint32_t asset,count;
    uint32_t ids[7];
    int16_t parents[7];
};
inline constexpr CapturedHeldWeaponProfile capturedHeldWeaponProfiles[]{
    {HeldWeaponKind::Longsword,2478,4,{0x00ae838d,0x006666f1,0x00ea7b92,0x00858053},{-1,0,1,1}},
    {HeldWeaponKind::Longsword,5457,4,{0x00ae838d,0x006666f1,0x00b1fe66,0x00858053},{-1,0,1,1}},
    {HeldWeaponKind::Staff,1514,4,{0x00ae838d,0x006666f1,0x00bbefd6,0x00858053},{-1,0,1,1}},
    {HeldWeaponKind::Greatsword,1250,5,{0x00ae838d,0x006666f1,0x00ed512c,0x00e6c85b,0x00858053},{-1,0,1,1,1}},
    {HeldWeaponKind::Hammer,1323,5,{0x00ae838d,0x006666f1,0x00aea294,0x0017311f,0x00858053},{-1,0,1,1,1}},
    {HeldWeaponKind::Faeblades,1689,7,{0x00ae838d,0x00b6fef2,0x01c55ae1,0x00dbd751,0x00a43243,0x01f4f7b1,0x00c7b304},{-1,0,1,1,0,4,4}}
};
inline HeldWeaponKind capturedHeldWeapon(uint32_t asset,unsigned count,const uint32_t* ids,const int16_t* parents){
    if(!ids||!parents)return HeldWeaponKind::None;
    for(const auto& profile:capturedHeldWeaponProfiles){
        if(asset!=profile.asset||count!=profile.count)continue;
        for(unsigned i=0;i<count;++i)
            if(ids[i]!=profile.ids[i]||parents[i]!=profile.parents[i])return HeldWeaponKind::None;
        return profile.kind;
    }
    return HeldWeaponKind::None;
}
inline unsigned expectedHeldSlot(HeldWeaponKind kind){
    switch(kind){
        case HeldWeaponKind::Longsword:case HeldWeaponKind::Staff:
        case HeldWeaponKind::Greatsword:case HeldWeaponKind::Hammer:return 5;
        case HeldWeaponKind::Faeblades:return 7;
        default:return 0;
    }
}
inline uintptr_t chooseHeldWeaponSlot(HeldWeaponKind kind,uintptr_t nativeSlot,bool rightFresh,bool leftFresh){
    const auto held=expectedHeldSlot(kind);
    const bool tracked=kind==HeldWeaponKind::Faeblades?(rightFresh||leftFresh):rightFresh;
    return held&&nativeSlot==8&&tracked?held:nativeSlot;
}
}
