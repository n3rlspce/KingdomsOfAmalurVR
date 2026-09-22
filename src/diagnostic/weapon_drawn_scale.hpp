#pragma once
#include "../tracking/weapon_drawn_scale.hpp"
namespace weapon_drawn_scale {
inline amalur::DrawnScaleLease lease;
inline SRWLOCK lock=SRWLOCK_INIT;
inline amalur::DrawnScaleIdentity identity(uintptr_t object){
    const auto root=weapon_control::currentWeaponRoot();
    if(!object||!root||weapon_control::fab(player_rig::word(object+0x194))!=object)return {};
    return {object,root,player_rig::word(object+0x34),player_rig::word(object+0xf8),
        player_rig::word(root+0xf8),player_rig::word(object+0xf0),player_rig::word(object+0x38),player_rig::word(object+0x194)};
}
// Resolve through the current Fab table before touching a saved object. A removed
// weapon may never receive another callback; it must not block its replacement.
inline void retire(){
    if(!lease.active())return;
    const auto saved=lease.identity();
    const auto object=weapon_control::fab(saved.fabIndex);
    if(object!=saved.object){lease.clear();return;}
    const auto live=identity(object);
    lease.restore(reinterpret_cast<amalur::RigBone*>(live.buffer),live);
}
inline void discardStale(){
    if(!lease.active())return;
    const auto saved=lease.identity();const auto object=weapon_control::fab(saved.fabIndex);
    if(object!=saved.object||!amalur::sameDrawnScale(saved,identity(object)))lease.clear();
}
inline void restore(uintptr_t object){
    AcquireSRWLockExclusive(&lock);
    __try{
        if(lease.active()&&lease.identity().object==object)retire();
    }__except(EXCEPTION_EXECUTE_HANDLER){lease.clear();}
    ReleaseSRWLockExclusive(&lock);
}
inline void prepare(void* mapper,uintptr_t source,uintptr_t output,uintptr_t slot,bool solved){
    AcquireSRWLockExclusive(&lock);
    __try{
        // Unrelated armor callbacks must not undo the weapon before rendering.
        // Drop stale ownership here; retire a still-live lease only for its own
        // evaluation or when admitting another verified measured weapon below.
        discardStale();
        // Restoring is unconditional elsewhere; eligibility only governs new writes.
        if(!solved||slot!=5||output<0x34||weapon_control::weaponSheathed.load()
            ||!firstPerson.load()||!headTracking.load()||!trackedCameraAvailable.load()||interfaceView.load()
            ||!motion_controls::gameFocused()||motion_controls::dialogueActive.load()
            ||amalur::bodyDebug.enabled(amalur::nativeArms)||game_pause::sample(true)!=0)goto done;
        {
            const auto live=identity(output-0x34);
            if(!amalur::sameDrawnScale(live,live)||source!=live.root+0x34
                ||!weapon_control::authoritativeSelectedWeapon(live.object)
                ||weapon_control::capturedHeldKind(live.object)!=amalur::HeldWeaponKind::Longsword)goto done;
            mgs5vr::Pose hand;uint64_t handTick;
            AcquireSRWLockShared(&weapon_control::poseLock);hand=weapon_control::desired;handTick=weapon_control::tick;ReleaseSRWLockShared(&weapon_control::poseLock);
            if(!amalur::freshWeaponPose(hand,handTick,GetTickCount64()))goto done;
            const auto table=player_rig::word(reinterpret_cast<uintptr_t>(mapper));if(!table)goto done;
            const auto entry=table+5*32,tuples=player_rig::word(entry);
            constexpr uint32_t held[]{62,0,0,55,1,0};
            if(player_rig::word(entry+4)!=2||!tuples||memcmp(reinterpret_cast<void*>(tuples),held,sizeof(held)))goto done;
            // Capture proved unit world scale. A different actor/item scale must
            // remain native; this fix cannot normalize unrelated sizing effects.
            const auto world=reinterpret_cast<const unsigned char*>(live.object+0x124);
            if(world[0x2c]&0x40){float scale[3];memcpy(scale,world+0x20,12);for(float v:scale)if(v!=1.f)goto done;}
            retire();
            lease.begin(reinterpret_cast<amalur::RigBone*>(live.buffer),live);
        }
    done:;
    }__except(EXCEPTION_EXECUTE_HANDLER){lease.clear();}
    ReleaseSRWLockExclusive(&lock);
}
inline void finish(uintptr_t object){
    AcquireSRWLockExclusive(&lock);
    __try{if(lease.active()&&lease.identity().object==object){const auto live=identity(object);lease.finish(reinterpret_cast<const amalur::RigBone*>(live.buffer),live);}}
    __except(EXCEPTION_EXECUTE_HANDLER){lease.clear();}
    ReleaseSRWLockExclusive(&lock);
}
}
