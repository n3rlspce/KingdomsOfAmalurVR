#pragma once
#include "back_sheath_native_action.hpp"
// Included after weapon_selection. Consume gestures on the native update thread,
// never from Present. Native animation retains ownership of its sounds/events.
namespace back_sheath_dispatch {
inline melee_feedback::longsword_audio::Driver drawAudio;
inline void showSelectedAfterDraw(){
    using namespace weapon_control;
    const auto root=currentWeaponRoot();
    if(!nativeShow||!root||(player_rig::word(root+0x1d0)&0xf2004)
        ||(*reinterpret_cast<unsigned char*>(root+0x1d5)>0&&!*reinterpret_cast<unsigned char*>(root+0x1d6)))return;
    const auto count=player_rig::word(root+0x28),children=player_rig::word(root+0x24);
    if(count>32||!children)return;
    for(unsigned i=0;i<count;++i){const auto object=fab(player_rig::word(children+i*4));
        if(object&&authoritativeSelectedWeapon(object)&&(capturedHeldKind(object)!=amalur::HeldWeaponKind::None||isPlayerDaggers(object))
            &&(player_rig::word(object+0x1d0)&4)){
            // Explicit draw may revive hidden paired daggers too. A native remap
            // must still publish a fresh pose before any physical hit is allowed.
            nativeShow(reinterpret_cast<void*>(object));return;
        }
    }
}
inline void sampleImpl(uintptr_t physics){
    using namespace weapon_control;
    if(!melee_probe::local(physics))return;
    const auto now=GetTickCount64();
    SelectedProof selected;mgs5vr::Pose handPose;unsigned currentGeneration;uint64_t handTick;amalur::HeavyChargePacket input;
    AcquireSRWLockShared(&poseLock);selected=selectedProof;handPose=desired;handTick=tick;input=heavyPacket;currentGeneration=generation;ReleaseSRWLockShared(&poseLock);
    const bool gameplay=headTracking.load()&&firstPerson.load()&&trackedCameraAvailable.load()&&!interfaceView.load()
        &&motion_controls::gameFocused()&&!motion_controls::dialogueActive.load()&&game_pause::sample(true)==0;
    const auto manager=melee_feedback::longsword_audio::currentManager();
    const bool audioAllowed=gameplay&&melee_feedback::longsword_audio::ready&&freshSelectedProof(selected)
        &&melee_feedback::longsword_audio::identity(selected.owner,selected.weapon);
    drawAudio.update(melee_feedback::longsword_audio::backend,manager,selected.owner,selected.weapon,currentGeneration,audioAllowed,now);
    amalur::BackSheathAction action;uint64_t requested;uint32_t owner,weapon,session,center;
    AcquireSRWLockExclusive(&poseLock);
    action=pendingBackAction;requested=pendingBackTick;owner=pendingBackOwner;weapon=pendingBackWeapon;
    session=pendingBackSession;center=pendingBackGeneration;pendingBackAction=amalur::BackSheathAction::None;
    const bool identity=owner==backOwner&&weapon==backWeapon&&session==heavySession&&center==generation;
    ReleaseSRWLockExclusive(&poseLock);
    if(action==amalur::BackSheathAction::None)return;
    if(!amalur::backSheathRequestCurrent(requested,now,handTick,input.tick,
        identity&&mgs5vr::valid(handPose)&&amalur::validHeavyChargePacket(input,now)&&input.active&&!input.spell)
        ||!melee_probe::local(physics)
        ||!headTracking.load()||!firstPerson.load()||!trackedCameraAvailable.load()||interfaceView.load()
        ||!motion_controls::gameFocused()||motion_controls::dialogueActive.load()
        ||motion_controls::explicitSpellActive(now)||game_pause::sample(true)!=0)return;
    __try{
        const auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());
        if(!player||player_rig::word(player)!=gameBase+0x1359f14||player_rig::word(player+0x1ec)!=owner
            ||player_rig::word(physics+0x18)!=owner||weapon_selection::equipped(player,motion_controls::viewControls().selectedWeapon)!=weapon)return;
        const bool draw=action==amalur::BackSheathAction::Draw,previous=weaponSheathed.load();
        if(draw!=previous)return;
        weaponSheathed.store(!draw);
        // VR drawing resumes the proven held remap; there is no invented
        // Weapon_Draw animation. Sheathing uses the actual game animation.
        if(!draw&&!back_sheath_native::start(owner,false)){weaponSheathed.store(previous);return;}
        if(draw&&audioAllowed&&mgs5vr::valid(handPose)){
            // Native bank cue maps six equally weighted swordout samples.
            // Recorded trace398/399: modeFFFFFFFF, flags1/0, successful starts.
            const bool queued=drawAudio.emit(melee_feedback::longsword_audio::backend,0x00ded040,0xffffffffu,handPose.position,now);
            log("VR back grip draw audio selector=00ded040 queued=%d\n",queued);
        }
        AcquireSRWLockExclusive(&poseLock);
        visualTick=bladeTick=0;longswordContactReady=false;for(auto& ready:basicContactReady)ready=false;
        ReleaseSRWLockExclusive(&poseLock);
        if(draw)showSelectedAfterDraw();
        log("VR back grip action=%s owner=%08x weapon=%08x tick=%llu\n",draw?"draw":"sheathe",owner,weapon,now);
    }__except(EXCEPTION_EXECUTE_HANDLER){
        // Do not retry an action after ambiguous native entry.
        log("VR back grip native animation exception; gesture consumed without retry\n");
    }
}
inline void sample(uintptr_t physics){
    static bool disabled=false;if(disabled)return;
    __try{sampleImpl(physics);}__except(EXCEPTION_EXECUTE_HANDLER){
        disabled=true;log("VR back grip dispatch disabled after validation exception\n");
    }
}

}
