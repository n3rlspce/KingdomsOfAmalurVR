#pragma once
#include <cstdint>
#include "weapon_family.hpp"

namespace amalur {
// Primary preserves existing behavior; secondary must have one unambiguous
// current player weapon attachment. Never draw both equipped inventory slots.
inline bool trackedWeaponSelection(uint32_t selected,bool uniqueAttachment){
    return selected==0||(selected==1&&uniqueAttachment);
}
enum class HeldWeaponKind {None,Longsword,Staff,Greatsword,Hammer,Faeblades,Bow};
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
    // Live Reckoning capture20260924, owner000900e0; distinct mesh bone2.
    {HeldWeaponKind::Hammer,5215,5,{0x00ae838d,0x006666f1,0x01947c74,0x0017311f,0x00858053},{-1,0,1,1,1}},
    {HeldWeaponKind::Faeblades,1689,7,{0x00ae838d,0x00b6fef2,0x01c55ae1,0x00dbd751,0x00a43243,0x01f4f7b1,0x00c7b304},{-1,0,1,1,0,4,4}},
    // PID29620 bow census: native held6 maps left finger36 to handle1.
    {HeldWeaponKind::Bow,1423,6,{0x00ae838d,0x006666f1,0x00963ee1,0x002d04aa,0x0027bb54,0x00168174},{-1,0,1,1,1,1}}
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
        case HeldWeaponKind::Bow:return 6;
        default:return 0;
    }
}
inline bool heldWeaponTracked(HeldWeaponKind kind,bool rightFresh,bool leftFresh){
    if(kind==HeldWeaponKind::None)return false;
    return kind==HeldWeaponKind::Bow?leftFresh:kind==HeldWeaponKind::Faeblades?(rightFresh||leftFresh):rightFresh;
}
inline bool capturedHeldMap(HeldWeaponKind kind,unsigned count,const uint32_t* tuples){
    if(!tuples||!expectedHeldSlot(kind))return false;
    constexpr uint32_t right[]{62,0,0,55,1,0},left[]{62,0,0,36,1,0},dual[]{62,0,0,36,1,0,55,4,0};
    const bool paired=kind==HeldWeaponKind::Faeblades;
    if(count!=(paired?3u:2u))return false;
    const auto expected=paired?dual:kind==HeldWeaponKind::Bow?left:right;
    for(unsigned i=0;i<count*3;++i)if(tuples[i]!=expected[i])return false;
    return true;
}
inline uintptr_t chooseHeldWeaponSlot(HeldWeaponKind kind,uintptr_t nativeSlot,bool rightFresh,bool leftFresh){
    const auto held=expectedHeldSlot(kind);
    const bool tracked=heldWeaponTracked(kind,rightFresh,leftFresh);
    return held&&nativeSlot==8&&tracked?held:nativeSlot;
}
}
