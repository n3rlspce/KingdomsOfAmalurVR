#pragma once
#include "../tracking/support_grip.hpp"
namespace support_grip {
inline SRWLOCK lock=SRWLOCK_INIT;
inline mgs5vr::Pose handle{};inline uint64_t handleTick{};inline uint32_t handleOwner{},handleRootOwner{};
inline uint64_t lastApplyTick{};
inline unsigned handleGeneration{},handleSelected{2};
inline uintptr_t handleObject{};inline amalur::HeldWeaponKind handleKind{amalur::HeldWeaponKind::None};
inline unsigned handleAsset{};
inline unsigned lastSelected{2};
inline unsigned handleSession{};
inline unsigned lastSession{};
inline amalur::SupportGrip attachment;
inline void record(uintptr_t object,uintptr_t slot,mgs5vr::Pose root){
    __try{
        const auto kind=weapon_control::capturedHeldKind(object);
        const auto view=motion_controls::viewControls();
        if(slot!=5||!amalur::supportGripEligible(kind,view.selectedWeapon,true))return;
        auto bones=reinterpret_cast<const amalur::RigBone*>(player_rig::word(object+0x34));if(!bones)return;
        auto pose=mgs5vr::compose(root,amalur::bonePose(bones[1]));if(!mgs5vr::valid(pose))return;
        unsigned gen;AcquireSRWLockShared(&weapon_control::poseLock);gen=weapon_control::generation;ReleaseSRWLockShared(&weapon_control::poseLock);
        const auto assetId=player_rig::word(object+0xf0);
        auto owner=player_rig::word(object+0xf8),player=reinterpret_cast<uintptr_t>(player_rig::player.load());
        auto rootOwner=player?player_rig::word(player+0x1ec):0;
        AcquireSRWLockExclusive(&lock);handle=pose;handleTick=GetTickCount64();handleOwner=owner;handleRootOwner=rootOwner;handleGeneration=gen;handleObject=object;handleAsset=assetId;handleKind=kind;handleSelected=view.selectedWeapon;handleSession=view.session;ReleaseSRWLockExclusive(&lock);
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
inline bool freeOffhand(uintptr_t object,uint32_t owner,unsigned asset,amalur::HeldWeaponKind kind,uint32_t rootOwner){
    __try{
        const auto root=weapon_control::currentWeaponRoot();
        if(!root||player_rig::word(root+0xf8)!=rootOwner||!object
            ||weapon_control::fab(player_rig::word(object+0x194))!=object
            ||player_rig::word(object+0xf8)!=owner||player_rig::word(object+0xf0)!=asset
            ||weapon_control::capturedHeldKind(object)!=kind)return false;
        auto children=player_rig::word(root+0x24),n=player_rig::word(root+0x28);
        if(n>32||(!children&&n))return false;
        for(unsigned i=0;i<n;++i){
            auto child=weapon_control::fab(player_rig::word(children+i*4));if(!child||child==object)continue;
            // Known raised shield. Block input also excludes its transition frames.
            if(player_rig::word(child+0xf0)==2322&&!(player_rig::word(child+0x1d0)&4))return false;
        }
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline void apply(amalur::RigBone* bones,unsigned count,const int16_t* parents,const uint32_t* ids,
    mgs5vr::Pose root,mgs5vr::Pose left,uint32_t rootOwner,unsigned generation,float scale,bool tracking){
    amalur::MotionInputPacket input{};AcquireSRWLockExclusive(&motion_controls::lock);
    bool active=motion_controls::channel.open(false)&&motion_controls::channel.read(input);ReleaseSRWLockExclusive(&motion_controls::lock);
    const auto now=GetTickCount64();
    bool allowed=tracking&&firstPerson.load()&&headTracking.load()&&!interfaceView.load()&&motion_controls::gameFocused()
        &&!motion_controls::dialogueActive.load()&&active&&input.selectedWeapon<=1&&input.block<.25f;
    uintptr_t object;uint32_t owner;unsigned asset;amalur::HeldWeaponKind kind;
    AcquireSRWLockShared(&lock);object=handleObject;owner=handleOwner;asset=handleAsset;kind=handleKind;ReleaseSRWLockShared(&lock);
    const bool offhandFree=allowed&&freeOffhand(object,owner,asset,kind,rootOwner);
    AcquireSRWLockExclusive(&lock);
    const bool selectionChanged=lastSelected!=input.selectedWeapon||lastSession!=input.session;
    if(selectionChanged)attachment.release();
    lastSelected=input.selectedWeapon;lastSession=input.session;
    if(!lastApplyTick||now<lastApplyTick||now-lastApplyTick>=100)attachment.release();
    lastApplyTick=now;
    allowed=allowed&&!selectionChanged&&offhandFree&&object==handleObject&&owner==handleOwner
        &&amalur::supportGripEligible(handleKind,input.selectedWeapon,offhandFree)
        &&input.selectedWeapon==handleSelected&&input.session==handleSession&&handleTick&&handleTick<=now&&now-handleTick<100&&rootOwner==handleRootOwner&&generation==handleGeneration;
    static uint64_t lastReport{};if(active&&input.supportGrip>.65f&&now-lastReport>2000){lastReport=now;
        log("Weapon support grip: eligible=%d tracked=%d handleOwner=%08x handleAge=%llu held=%.2f attached=%d\n",allowed,tracking,handleOwner,handleTick&&now>=handleTick?now-handleTick:0,input.supportGrip,attachment.attached);
    }
    bool was=attachment.attached;mgs5vr::Pose target;
    if(attachment.update(allowed,input.supportGrip>.65f,handleOwner,generation,scale,handle,left,target,handleKind,input.selectedWeapon)){
        auto wrist=mgs5vr::compose(mgs5vr::inverse(root),amalur::wristFromControllerGrip(amalur::ArmSide::Left,target));
        // Verified native left-hand weapon mapping uses source bone 36.
        // Seat that socket at the handle instead of putting the wrist there.
        if(!amalur::attachLeftHandAtSocket(bones,count,parents,ids,36,wrist))attachment.release();
    }
    if(was!=attachment.attached)log("Weapon support hand %s (native grip socket anchor)\n",attachment.attached?"attached":"released");
    ReleaseSRWLockExclusive(&lock);
}
}
