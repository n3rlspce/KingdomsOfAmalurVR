#pragma once
#include "../tracking/native_weapon_slot.hpp"
#include "../tracking/weapon_pose.hpp"
#include "../tracking/held_weapon_profile.hpp"
#include "../tracking/dagger_orientation.hpp"
#include "../tracking/grip_settings.hpp"
#include "../tracking/melee_swing.hpp"
#include "../tracking/melee_swing_event.hpp"
#include "../tracking/grip_filter.hpp"
namespace arm_rig {extern std::atomic<bool> enabled;}
namespace weapon_control {
using Evaluate=void(__thiscall*)(void*,uintptr_t,uintptr_t);
inline Evaluate original{};
inline SRWLOCK poseLock=SRWLOCK_INIT;
inline mgs5vr::Pose desired{};
inline amalur::LocomotionFrame locomotionFrame;
inline mgs5vr::Pose desiredLeft{};
inline uint64_t leftTick{};
inline mgs5vr::Pose bladeWorld[2]{};
inline uint64_t bladeTick{};inline uint32_t bladeOwner{};
inline unsigned swingSerial[2]{};inline float swingSpeed[2]{};
inline amalur::MeleeSwingEvent swingEvents[2];
inline amalur::MeleeSwingChain swingChain;
inline uint32_t visualWeapon{},visualAsset{},gestureWeapon{};
inline uint32_t visualSelection{};
inline uint64_t visualTick{};inline bool visualDual{};
inline mgs5vr::Pose visualPoses[2];
inline amalur::GripSettingsChannel positionSettings;
inline mgs5vr::Vec3 weaponCentimetres{};
inline uint64_t tick{};
inline unsigned generation{};
inline float worldScale{100.f};
inline std::atomic<bool> enabled{false};
inline std::atomic<bool> desktopPose{false};
inline amalur::PoseChannel hand{L"Local\\AmalurVRRightHandV3",L"Local\\AmalurVRRightHandMutexV3"};
inline amalur::PoseChannel leftHand{L"Local\\AmalurVRLeftHandV3",L"Local\\AmalurVRLeftHandMutexV3"};
inline amalur::MeleeSwing rightSwing,leftSwing;
inline amalur::PosePacket frameRight{},frameLeft{};
inline bool frameRightValid{},frameLeftValid{};
inline double frameSeconds{};
inline amalur::GripFilter rightFilter,leftFilter;
// Latch hands alongside the headset once per game Present. Camera callbacks
// can run repeatedly while armor and weapon remaps consume their targets.
// Re-reading XR packets in those callbacks mixes poses within one game frame.
inline void sampleHands(const amalur::TrackingSnapshot& snapshot){
    const auto& right=snapshot.right;const auto& left=snapshot.left;
    const bool rightValid=snapshot.head.valid&&right.valid;
    const bool leftValid=snapshot.head.valid&&left.valid;
    LARGE_INTEGER counter{},frequency{};QueryPerformanceCounter(&counter);QueryPerformanceFrequency(&frequency);
    const double seconds=double(counter.QuadPart)/double(frequency.QuadPart);
    AcquireSRWLockExclusive(&poseLock);
    frameRight=right;frameLeft=left;frameRightValid=rightValid;frameLeftValid=leftValid;
    frameSeconds=seconds;
    ReleaseSRWLockExclusive(&poseLock);
}
inline void sample(amalur::CameraPose rig,mgs5vr::Pose origin,float scale,unsigned recenter,mgs5vr::Vec3 headLocal,const amalur::LocomotionFrame& locomotion){
    float pitch,yaw,roll;mgs5vr::Vec3 offset;
    bool offsetValid=positionSettings.open(false)&&positionSettings.read(pitch,yaw,roll,offset.x,offset.y,offset.z);
    amalur::PosePacket p,l;bool valid,leftValid;double visualTime;
    AcquireSRWLockShared(&poseLock);
    p=frameRight;l=frameLeft;valid=frameRightValid;leftValid=frameLeftValid;
    visualTime=frameSeconds;
    ReleaseSRWLockShared(&poseLock);
    const auto sampleNow=GetTickCount64();
    valid=valid&&p.tick<=sampleNow&&sampleNow-p.tick<250;
    leftValid=leftValid&&l.tick<=sampleNow&&sampleNow-l.tick<250;
    mgs5vr::Pose rightLocal{{p.orientation[0],p.orientation[1],p.orientation[2],p.orientation[3]},{p.position[0],p.position[1],p.position[2]}},
        leftLocal{{l.orientation[0],l.orientation[1],l.orientation[2],l.orientation[3]},{l.position[0],l.position[1],l.position[2]}};
    AcquireSRWLockExclusive(&poseLock);
    valid=rightFilter.sample(rightLocal,p.tick,recenter,valid,rightLocal,visualTime);
    leftValid=leftFilter.sample(leftLocal,l.tick,recenter,leftValid,leftLocal,visualTime);
    ReleaseSRWLockExclusive(&poseLock);
    mgs5vr::Pose result{};
    if(valid)valid=amalur::gripInGame(rig,mgs5vr::compose(mgs5vr::inverse(origin),rightLocal),scale,result);
    mgs5vr::Pose leftResult{};
    if(leftValid)leftValid=amalur::gripInGame(rig,mgs5vr::compose(mgs5vr::inverse(origin),leftLocal),scale,leftResult);
    auto now=GetTickCount64();
    bool gestures=!amalur::bodyDebug.enabled(amalur::nativeArms)&&motion_controls::contactEnabled.load()&&firstPerson.load()&&!interfaceView.load()
        &&motion_controls::gameFocused()&&!motion_controls::dialogueActive.load();
    AcquireSRWLockExclusive(&poseLock);locomotionFrame=locomotion;desired=result;tick=valid?p.tick:0;desiredLeft=leftResult;leftTick=leftValid?l.tick:0;generation=recenter;worldScale=scale;
    gestures=gestures&&visualWeapon&&visualTick&&visualTick<=now&&now-visualTick<100;
    if(!gestures||gestureWeapon!=visualWeapon){rightSwing.reset();leftSwing.reset();swingChain.reset();gestureWeapon=gestures?visualWeapon:0;}
    if(offsetValid)weaponCentimetres=offset;
    auto tip=[&](const amalur::PosePacket& v){return amalur::meleeTipRelative(mgs5vr::Pose{{v.orientation[0],v.orientation[1],v.orientation[2],v.orientation[3]},{v.position[0],v.position[1],v.position[2]}},headLocal);};
    bool r=rightSwing.sample(tip(p),p.tick,recenter,gestures&&valid);
    bool left=leftSwing.sample(tip(l),l.tick,recenter,gestures&&visualDual&&leftValid);
    if(r)++swingSerial[0];if(left)++swingSerial[1];
    const auto chainStep=(r||left)?swingChain.advance(visualWeapon,recenter,now):0;
    auto publish=[&](unsigned side,uint64_t stamp){
        auto& event=swingEvents[side];event={};event.weapon=visualWeapon;event.asset=visualAsset;
        event.hand=side;event.serial=swingSerial[side];event.generation=recenter;event.tick=stamp;
        event.chainStep=chainStep;event.weaponPose=visualPoses[side];
    };
    if(r)publish(0,p.tick);if(left)publish(1,l.tick);
    swingSpeed[0]=rightSwing.speed();swingSpeed[1]=leftSwing.speed();
    if(r||left)motion_controls::swingUntil.store(now+90);
    const auto reportModel=visualAsset,reportWeapon=visualWeapon,reportSelection=visualSelection;
    const auto reportFrame=visualTick;const auto rs=swingSpeed[0],ls=swingSpeed[1];
    const auto rg=rightSwing.gate(),lg=leftSwing.gate();
    ReleaseSRWLockExclusive(&poseLock);
    // Preserve the peak from EVERY detector sample, publishing at most20Hz.
    // Failed/slow motions remain visible, not just accepted attacks.
    static uint64_t speedReport{};static uint32_t speedWeapon{};static unsigned speedGeneration{};
    static float peaks[2]{};static unsigned fired[2]{};static const char* gates[2]{"warming-up","warming-up"};
    if(speedWeapon!=reportWeapon||speedGeneration!=recenter){
        peaks[0]=peaks[1]=0;fired[0]=fired[1]=0;speedWeapon=reportWeapon;speedGeneration=recenter;speedReport=0;
    }
    if(rs>=peaks[0]){peaks[0]=rs;gates[0]=rg;}if(ls>=peaks[1]){peaks[1]=ls;gates[1]=lg;}
    fired[0]+=r;fired[1]+=left;
    if(now>=speedReport&&(peaks[0]>=.2f||peaks[1]>=.2f||fired[0]||fired[1])){
        speedReport=now+50;
        log("VR swing speed tick=%llu model=%u weapon=%08x generation=%u allowed=%d speed=%.4f,%.4f peak=%.4f,%.4f fired=%u,%u gate=%s,%s threshold=0.9\n",
            now,reportModel,reportWeapon,recenter,gestures,rs,ls,peaks[0],peaks[1],fired[0],fired[1],gates[0],gates[1]);
        peaks[0]=peaks[1]=0;fired[0]=fired[1]=0;
    }
    static uint64_t nextReport{};
    if(now>=nextReport){nextReport=now+2000;
        log("VR swing detector tick=%llu model=%u weapon=%08x selection=%u currentSelection=%u poseTick=%llu allowed=%d rightTracked=%d leftTracked=%d speed=%.3f,%.3f threshold=0.9 focused=%d firstPerson=%d interface=%d dialogue=%d nativeArms=%d\n",
            now,reportModel,reportWeapon,reportSelection,motion_controls::viewControls().selectedWeapon,reportFrame,gestures,valid,leftValid,rs,ls,
            motion_controls::gameFocused(),firstPerson.load(),interfaceView.load(),motion_controls::dialogueActive.load(),amalur::bodyDebug.enabled(amalur::nativeArms));
    }
}
inline uintptr_t fab(uint32_t index){
    auto mgr=player_rig::word(gameBase+0x15fdf54);if(!mgr||index<2||index>=player_rig::word(mgr+0xc8))return 0;
    auto table=player_rig::word(mgr+0xc4);auto p=player_rig::word(table+index*4);
    return p&&player_rig::word(p)==gameBase+0x1340f3c&&player_rig::word(p+0x194)==index?p:0;
}
inline bool isSinglePlayerWeapon(uintptr_t self){
    auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());if(!player)return false;
    if(player_rig::word(player)!=gameBase+0x1359f14&&player_rig::word(player)!=gameBase+0x1359e94)return false;
    auto owner=player_rig::word(player+0x1ec);auto entity=player_rig::resolve(owner);
    auto render=player_rig::part(entity,7,owner,0x13560e4);if(!render)return false;
    auto parent=fab(player_rig::word(render+0x9c));if(!parent||player_rig::word(parent+0xf8)!=owner)return false;
    auto count=player_rig::word(parent+0x28);if(count>32)return false;
    auto children=player_rig::word(parent+0x24);uintptr_t selected=0;
    for(unsigned i=0;i<count;++i){auto child=fab(player_rig::word(children+i*4));if(!child)continue;
        auto childOwner=player_rig::word(child+0xf8);auto childEntity=player_rig::resolve(childOwner);
        if(player_rig::part(childEntity,11,childOwner,0x135745c)){
            // Native draw/sheath transitions can reference one Fab in two slots.
            if(selected&&selected!=child)return false;selected=child;
        }}
    // Dual weapons and ambiguous equipment deliberately await explicit slot mapping.
    return selected==self;
}
inline uintptr_t currentWeaponRoot();
// Native remap observations, before our held-slot substitution. Unknown or
// stale sibling states remain ambiguous; only the captured back socket excludes it.
using NativeWeaponSlot=amalur::NativeWeaponSlot;
inline NativeWeaponSlot nativeWeaponSlots[32]{};inline unsigned nativeWeaponCursor{};
inline void observeNativeWeaponSlot(void* mapper,uintptr_t slot,uintptr_t source,uintptr_t output){
    __try{
        if(output<0x34||slot>=32)return;
        const auto object=output-0x34,root=currentWeaponRoot();
        if(!root||source!=root+0x34||fab(player_rig::word(object+0x194))!=object)return;
        const auto owner=player_rig::word(object+0xf8);
        if(!player_rig::part(player_rig::resolve(owner),11,owner,0x135745c))return;
        const auto entry=player_rig::word(reinterpret_cast<uintptr_t>(mapper))+slot*32;
        const auto tuple=player_rig::word(entry);
        const bool back=slot==9&&player_rig::word(entry+4)==1&&tuple
            &&player_rig::word(tuple)==16&&player_rig::word(tuple+4)==0&&player_rig::word(tuple+8)==0;
        NativeWeaponSlot next{object,root,owner,player_rig::word(root+0xf8),GetTickCount64(),motion_controls::viewControls().selectedWeapon,back};
        AcquireSRWLockExclusive(&poseLock);unsigned index=32;
        for(unsigned i=0;i<32;++i)if(nativeWeaponSlots[i].object==object){index=i;break;}
        if(index==32)index=nativeWeaponCursor++%32;nativeWeaponSlots[index]=next;ReleaseSRWLockExclusive(&poseLock);
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
inline bool isOnlyActivePlayerWeapon(uintptr_t self){
    if(isSinglePlayerWeapon(self))return true;
    const auto root=currentWeaponRoot();if(!root)return false;
    auto n=player_rig::word(root+0x28),children=player_rig::word(root+0x24);if(n>32||!children)return false;
    NativeWeaponSlot seen[32];AcquireSRWLockShared(&poseLock);memcpy(seen,nativeWeaponSlots,sizeof(seen));ReleaseSRWLockShared(&poseLock);
    const auto now=GetTickCount64();const auto selection=motion_controls::viewControls().selectedWeapon;
    bool found=false;
    for(unsigned i=0;i<n;++i){auto object=fab(player_rig::word(children+i*4));if(!object)continue;
        auto owner=player_rig::word(object+0xf8);
        if(!player_rig::part(player_rig::resolve(owner),11,owner,0x135745c))continue;
        if(object==self){found=true;continue;}
        bool stowed=false;for(const auto& entry:seen)
            if(amalur::freshBackSocket(entry,object,owner,root,player_rig::word(root+0xf8),selection,now)){stowed=true;break;}
        if(!stowed)return false;
    }
    return found;
}
inline bool isPlayerDaggers(uintptr_t self){
    auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());if(!player)return false;
    if(player_rig::word(player)!=gameBase+0x1359f14&&player_rig::word(player)!=gameBase+0x1359e94)return false;
    auto owner=player_rig::word(player+0x1ec),entity=player_rig::resolve(owner);
    auto render=player_rig::part(entity,7,owner,0x13560e4);if(!render)return false;
    auto root=fab(player_rig::word(render+0x9c));if(!root||player_rig::word(root+0xf8)!=owner)return false;
    auto n=player_rig::word(root+0x28);if(n>32)return false;bool child=false;
    for(unsigned i=0;i<n;++i)if(fab(player_rig::word(player_rig::word(root+0x24)+i*4))==self)child=true;
    if(!child||player_rig::word(self+0x38)!=7)return false;
    auto wo=player_rig::word(self+0xf8);if(!player_rig::part(player_rig::resolve(wo),11,wo,0x135745c))return false;
    auto manager=player_rig::word(gameBase+0x15fdf54),id=player_rig::word(self+0xf0);
    if(id<2||id>=100000)return false;
    auto flags=*reinterpret_cast<unsigned char*>(player_rig::word(manager+0x28)+id);
    if(!(flags&4)||(flags&16))return false;
    auto asset=player_rig::word(player_rig::word(manager+0x18)+id*4),blob=player_rig::word(asset+0x1c);
    if(player_rig::word(blob)!=0x45533033||player_rig::word(blob+0x10)!=7)return false;
    auto off=player_rig::word(blob+0x20);if(!off||off>65536)return false;
    constexpr uint32_t ids[]{11436941,11992818,11092278,14407505,10760771,15110326,13087492};
    return !memcmp(reinterpret_cast<void*>(blob+0x20+off),ids,sizeof(ids));
}
struct Bone {mgs5vr::Vec3 position;float positionW;mgs5vr::Quat orientation;float scale[3];uint32_t flags;};
static_assert(sizeof(Bone)==48);
// Owned by the native remapper thread, never the Present/settings thread.
// Keep a single currently held dagger snapshot, not a cache of engine pointers.
struct HeldTranslation {
    amalur::HeldWeaponIdentity identity;
    amalur::RigBone before[7]{},after[7]{};
};
inline HeldTranslation heldTranslation;
inline uintptr_t currentWeaponRoot(){
    auto p=reinterpret_cast<uintptr_t>(player_rig::player.load());
    if(!p||(player_rig::word(p)!=gameBase+0x1359f14&&player_rig::word(p)!=gameBase+0x1359e94))return 0;
    auto owner=player_rig::word(p+0x1ec);
    auto render=player_rig::part(player_rig::resolve(owner),7,owner,0x13560e4);if(!render)return 0;
    auto root=fab(player_rig::word(render+0x9c));
    return root&&player_rig::word(root+0xf8)==owner?root:0;
}
using Visibility=void(__thiscall*)(void*);
inline Visibility originalHide{},nativeShow{};
inline amalur::HeldWeaponKind capturedHeldKind(uintptr_t object){
    if(!isOnlyActivePlayerWeapon(object))return amalur::HeldWeaponKind::None;
    auto count=player_rig::word(object+0x38),id=player_rig::word(object+0xf0);
    if(count<4||count>7||id<2||id>=100000)return amalur::HeldWeaponKind::None;
    auto manager=player_rig::word(gameBase+0x15fdf54);if(!manager)return amalur::HeldWeaponKind::None;
    auto states=player_rig::word(manager+0x28),table=player_rig::word(manager+0x18);
    if(!states||!table)return amalur::HeldWeaponKind::None;
    auto flags=*reinterpret_cast<const unsigned char*>(states+id);
    if(!(flags&4)||(flags&16))return amalur::HeldWeaponKind::None;
    auto asset=player_rig::word(table+id*4),blob=asset?player_rig::word(asset+0x1c):0;
    if(!blob||player_rig::word(blob)!=0x45533033||player_rig::word(blob+0x10)!=count)return amalur::HeldWeaponKind::None;
    auto ids=player_rig::word(blob+0x20),parents=player_rig::word(blob+0x1c);
    if(!ids||ids>65536||!parents||parents>65536)return amalur::HeldWeaponKind::None;
    return amalur::capturedHeldWeapon(id,count,reinterpret_cast<const uint32_t*>(blob+0x20+ids),
        reinterpret_cast<const int16_t*>(blob+0x1c+parents));
}
struct HeldProof {uintptr_t object{},root{},buffer{};uint32_t owner{},rootOwner{},asset{};uint64_t tick{};uint32_t selection{};};
inline HeldProof heldProof;
inline bool keepCapturedVisible(uintptr_t object){
    __try{
        if(amalur::bodyDebug.enabled(amalur::nativeArms)||!firstPerson.load()||!headTracking.load()||!trackedCameraAvailable.load()
            ||!arm_rig::enabled.load()||interfaceView.load()||!motion_controls::gameFocused()
            ||motion_controls::viewControls().selectedWeapon>1)return false;
        auto kind=capturedHeldKind(object);if(kind==amalur::HeldWeaponKind::None)return false;
        auto root=currentWeaponRoot();
        if(!root||(player_rig::word(root+0x1d0)&0xf2004)
            ||(*reinterpret_cast<unsigned char*>(root+0x1d5)>0&&!*reinterpret_cast<unsigned char*>(root+0x1d6)))return false;
        HeldProof proof;mgs5vr::Pose right,left;uint64_t rt,lt;
        AcquireSRWLockShared(&poseLock);proof=heldProof;right=desired;left=desiredLeft;rt=tick;lt=leftTick;ReleaseSRWLockShared(&poseLock);
        auto now=GetTickCount64();
        bool tracked=amalur::freshWeaponPose(right,rt,now)
            ||(kind==amalur::HeldWeaponKind::Faeblades&&amalur::freshWeaponPose(left,lt,now));
        return tracked&&proof.selection==motion_controls::viewControls().selectedWeapon
            &&proof.object==object&&proof.root==root&&proof.owner==player_rig::word(object+0xf8)
            &&proof.rootOwner==player_rig::word(root+0xf8)&&proof.buffer==player_rig::word(object+0x34)
            &&proof.asset==player_rig::word(object+0xf0)&&proof.tick&&proof.tick<=now&&now-proof.tick<100
            &&game_pause::sample(true)==0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline void recordHeld(uintptr_t object,uintptr_t slot,mgs5vr::Pose worldRoot){
    __try{
        auto kind=capturedHeldKind(object);if(kind==amalur::HeldWeaponKind::None||slot!=amalur::expectedHeldSlot(kind))return;
        auto root=currentWeaponRoot();if(!root)return;
        HeldProof proof{object,root,player_rig::word(object+0x34),player_rig::word(object+0xf8),
            player_rig::word(root+0xf8),player_rig::word(object+0xf0),GetTickCount64(),motion_controls::viewControls().selectedWeapon};
        auto bones=reinterpret_cast<const amalur::RigBone*>(proof.buffer);
        bool dual=kind==amalur::HeldWeaponKind::Faeblades;
        auto right=mgs5vr::compose(worldRoot,amalur::bonePose(bones[dual?4:1]));
        auto left=dual?mgs5vr::compose(worldRoot,amalur::bonePose(bones[1])):right;
        AcquireSRWLockExclusive(&poseLock);bool changed=heldProof.object!=object||heldProof.owner!=proof.owner||heldProof.selection!=proof.selection;
        heldProof=proof;
        if(mgs5vr::valid(right)&&mgs5vr::valid(left)){visualWeapon=proof.owner;visualAsset=proof.asset;
            visualTick=proof.tick;visualSelection=proof.selection;visualDual=dual;visualPoses[0]=right;visualPoses[1]=left;}
        ReleaseSRWLockExclusive(&poseLock);
        if(changed)log("Tracked held weapon ready owner=%08x asset=%u kind=%u slot=%u selection=%u\n",proof.owner,proof.asset,unsigned(kind),unsigned(slot),proof.selection);
        if(nativeShow&&(player_rig::word(object+0x1d0)&4)&&keepCapturedVisible(object))nativeShow(reinterpret_cast<void*>(object));
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
inline bool keepDaggersVisible(uintptr_t object){
    __try{
        if(amalur::bodyDebug.enabled(amalur::nativeArms)||!firstPerson.load()||!headTracking.load()||!trackedCameraAvailable.load()
            ||!arm_rig::enabled.load()||interfaceView.load()
            ||!isPlayerDaggers(object)||!amalur::trackedWeaponSelection(motion_controls::viewControls().selectedWeapon,isSinglePlayerWeapon(object)))return false;
        auto root=currentWeaponRoot();
        // Native parent visibility (loading/cutscene/whole actor hiding) wins.
        if(!root||(player_rig::word(root+0x1d0)&0xf2004)
            ||(*reinterpret_cast<unsigned char*>(root+0x1d5)>0&&!*reinterpret_cast<unsigned char*>(root+0x1d6)))return false;
        mgs5vr::Pose right,left;uint64_t rightTimestamp,leftTimestamp,frame;uint32_t owner;
        AcquireSRWLockShared(&poseLock);
        right=desired;left=desiredLeft;rightTimestamp=tick;leftTimestamp=leftTick;frame=bladeTick;owner=bladeOwner;
        ReleaseSRWLockShared(&poseLock);
        return amalur::freshHeldDaggers(static_cast<uint32_t>(player_rig::word(object+0xf8)),owner,frame,GetTickCount64(),
            right,rightTimestamp,left,leftTimestamp)&&game_pause::sample(true)==0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline void __fastcall hide(void* self,void*){
    if(keepDaggersVisible(reinterpret_cast<uintptr_t>(self))||keepCapturedVisible(reinterpret_cast<uintptr_t>(self))){
        static unsigned logged=0;if(logged++<8)log("Tracked weapon: native hide suppressed after verified held remap\n");
        return;
    }
    originalHide(self);
}
// Install after body_visibility has validated the original hide/show entrypoints.
inline void installHeldVisibility(){
    auto h=reinterpret_cast<unsigned char*>(gameBase+0x8ae970),s=reinterpret_cast<unsigned char*>(gameBase+0x8ae900);
    constexpr unsigned char prologue[]{0x56,0x8b,0xf1};
    constexpr unsigned char hideFlag[]{0x83,0x8e,0xd0,0x01,0,0,0x04};
    if(h[0]!=0x53||h[1]!=0x8b||h[2]!=0x1d||s[0]!=0x53||s[1]!=0x8b||s[2]!=0x1d
        ||player_rig::word(reinterpret_cast<uintptr_t>(h)+3)!=gameBase+0x15fdf54
        ||player_rig::word(reinterpret_cast<uintptr_t>(s)+3)!=gameBase+0x15fdf54
        ||memcmp(h+7,prologue,3)||memcmp(s+7,prologue,3)||memcmp(h+10,hideFlag,sizeof(hideFlag)))return;
    nativeShow=reinterpret_cast<Visibility>(s);
    hook(h,reinterpret_cast<void*>(&hide),reinterpret_cast<void**>(&originalHide),"Keep tracked daggers visible");
}
inline void restoreHeldTranslation(uintptr_t object){
    __try{
        auto& saved=heldTranslation;auto identity=saved.identity;if(!identity.object)return;
        // Resolve fresh identities before accessing any saved buffer. Removed,
        // replaced or reallocated attachments lose their snapshot without writes.
        auto root=currentWeaponRoot();auto live=fab(identity.index);
        if(!root||!live||!isPlayerDaggers(live)){saved={};return;}
        amalur::HeldWeaponIdentity current{live,root,player_rig::word(live+0x34),
            static_cast<uint32_t>(player_rig::word(live+0x194)),static_cast<uint32_t>(player_rig::word(live+0xf8)),
            static_cast<uint32_t>(player_rig::word(root+0xf8)),static_cast<uint32_t>(player_rig::word(live+0xf0)),
            static_cast<uint32_t>(player_rig::word(live+0x38))};
        if(!amalur::sameHeldWeapon(identity,current)){saved={};return;}
        if(object!=live)return;
        amalur::restoreDaggerTranslation(reinterpret_cast<amalur::RigBone*>(current.buffer),saved.before,saved.after,identity,current);
        amalur::restoreDaggerOrientation(reinterpret_cast<amalur::RigBone*>(current.buffer),saved.before,saved.after,identity,current);
        saved={};
    }__except(EXCEPTION_EXECUTE_HANDLER){heldTranslation={};}
}
inline void translateHeld(uintptr_t object,uintptr_t slot,uintptr_t source,uintptr_t solvedSource){
    __try{
        if(amalur::bodyDebug.enabled(amalur::nativeArms)||heldTranslation.identity.object||slot!=7||!firstPerson.load()||!headTracking.load()||interfaceView.load()
            ||!isPlayerDaggers(object)||!amalur::trackedWeaponSelection(motion_controls::viewControls().selectedWeapon,isSinglePlayerWeapon(object)))return;
        auto root=currentWeaponRoot();if(!root||source!=root+0x34)return;
        mgs5vr::Vec3 offset;float scale;uint64_t rightTimestamp,leftTimestamp;mgs5vr::Pose right,left;
        AcquireSRWLockShared(&poseLock);
        offset=weaponCentimetres;scale=worldScale;rightTimestamp=tick;leftTimestamp=leftTick;right=desired;left=desiredLeft;
        ReleaseSRWLockShared(&poseLock);
        auto now=GetTickCount64();
        bool rightTracked=amalur::freshWeaponPose(right,rightTimestamp,now),leftTracked=amalur::freshWeaponPose(left,leftTimestamp,now);
        if(!rightTracked&&!leftTracked)return;
        auto count=player_rig::word(solvedSource+4);
        if(count<3||count>64||count!=player_rig::word(source+4))return;
        auto manager=player_rig::word(gameBase+0x15fdf54),rootAsset=player_rig::word(root+0xf0);
        if(rootAsset<2||rootAsset>=100000)return;
        auto state=*reinterpret_cast<unsigned char*>(player_rig::word(manager+0x28)+rootAsset);
        if(!(state&4)||(state&16))return;
        auto blob=player_rig::word(player_rig::word(player_rig::word(manager+0x18)+rootAsset*4)+0x1c);
        if(player_rig::word(blob)!=0x45533033||player_rig::word(blob+0x10)!=count)return;
        auto idsOffset=player_rig::word(blob+0x20),parentsOffset=player_rig::word(blob+0x1c);
        if(!idsOffset||idsOffset>65536||!parentsOffset||parentsOffset>65536)return;
        auto ids=reinterpret_cast<const uint32_t*>(blob+0x20+idsOffset);
        auto parents=reinterpret_cast<const int16_t*>(blob+0x1c+parentsOffset);
        unsigned shoulder,elbow,rightWrist,leftWrist;
        if(!amalur::rightArmIndices(count,parents,ids,shoulder,elbow,rightWrist)
            ||!amalur::leftArmIndices(count,parents,ids,shoulder,elbow,leftWrist))return;
        auto solved=reinterpret_cast<const amalur::RigBone*>(player_rig::word(solvedSource));if(!solved)return;
        auto world=[](uintptr_t p){mgs5vr::Pose pose;memcpy(&pose.position,reinterpret_cast<void*>(p+0x124),12);
            memcpy(&pose.orientation,reinterpret_cast<void*>(p+0x134),16);return amalur::nativePose(pose);};
        auto rootWorld=world(root),weaponWorld=world(object);if(!mgs5vr::valid(rootWorld))return;
        // Sliders remain in the controller grip frame, independent of the
        // authored bone-axis conversion used to orient the visible hand.
        auto rightWorld=amalur::controllerGripFromWrist(amalur::ArmSide::Right,
            mgs5vr::compose(rootWorld,amalur::bonePose(solved[rightWrist])));
        auto leftWorld=amalur::controllerGripFromWrist(amalur::ArmSide::Left,
            mgs5vr::compose(rootWorld,amalur::bonePose(solved[leftWrist])));
        auto asset=player_rig::word(object+0xf0);
        blob=player_rig::word(player_rig::word(player_rig::word(manager+0x18)+asset*4)+0x1c);
        idsOffset=player_rig::word(blob+0x20);parentsOffset=player_rig::word(blob+0x1c);
        if(!idsOffset||idsOffset>65536||!parentsOffset||parentsOffset>65536)return;
        auto buffer=player_rig::word(object+0x34);if(!buffer)return;
        HeldTranslation next{};next.identity={object,root,buffer,
            static_cast<uint32_t>(player_rig::word(object+0x194)),static_cast<uint32_t>(player_rig::word(object+0xf8)),
            static_cast<uint32_t>(player_rig::word(root+0xf8)),static_cast<uint32_t>(asset),7};
        memcpy(next.before,reinterpret_cast<void*>(buffer),sizeof(next.before));
        if(!amalur::translateDaggerGrip(next.before,next.after,7,
            reinterpret_cast<const int16_t*>(blob+0x1c+parentsOffset),reinterpret_cast<const uint32_t*>(blob+0x20+idsOffset),
            weaponWorld,rightWorld,leftWorld,offset,scale,rightTracked,leftTracked))return;
        // Correct only the tracked left blade branch, around its handle anchor.
        // recordBlades runs afterward, so sweeps and debug geometry follow it.
        if(leftTracked&&!amalur::uprightLeftDagger(next.after,7,
            reinterpret_cast<const int16_t*>(blob+0x1c+parentsOffset),reinterpret_cast<const uint32_t*>(blob+0x20+idsOffset)))return;
        heldTranslation=next;
        for(unsigned i=1;i<7;++i){
            memcpy(reinterpret_cast<void*>(buffer+i*48),&next.after[i].position,sizeof(mgs5vr::Vec3));
            if(i<4&&leftTracked)memcpy(reinterpret_cast<void*>(buffer+i*48+16),&next.after[i].orientation,sizeof(mgs5vr::Quat));
        }
        static bool reported=false;if(leftTracked&&!reported){reported=true;
            log("Tracked left dagger: upright grip correction applied before collision pose capture\n");}
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
inline void recordBlades(uintptr_t object){
    __try{
        if(!isPlayerDaggers(object))return;
        auto native=[](mgs5vr::Pose p){p.orientation={-p.orientation.x,-p.orientation.y,-p.orientation.z,p.orientation.w};return p;};
        mgs5vr::Pose root;memcpy(&root.position,reinterpret_cast<void*>(object+0x124),12);memcpy(&root.orientation,reinterpret_cast<void*>(object+0x134),16);
        root=native(root);if(!mgs5vr::valid(root))return;
        auto bones=reinterpret_cast<const Bone*>(player_rig::word(object+0x34));mgs5vr::Pose p[2];
        p[0]=mgs5vr::compose(root,native({bones[4].orientation,bones[4].position}));
        p[1]=mgs5vr::compose(root,native({bones[1].orientation,bones[1].position}));
        if(!mgs5vr::valid(p[0])||!mgs5vr::valid(p[1]))return;
        auto owner=player_rig::word(object+0xf8),visualModel=player_rig::word(object+0xf0);
        const auto selection=motion_controls::viewControls().selectedWeapon;
        AcquireSRWLockExclusive(&poseLock);bladeWorld[0]=p[0];bladeWorld[1]=p[1];bladeTick=GetTickCount64();bladeOwner=owner;
        visualWeapon=owner;visualAsset=visualModel;visualTick=bladeTick;visualSelection=selection;visualDual=true;visualPoses[0]=p[0];visualPoses[1]=p[1];
        ReleaseSRWLockExclusive(&poseLock);
        if(nativeShow&&(player_rig::word(object+0x1d0)&4)&&keepDaggersVisible(object))
            nativeShow(reinterpret_cast<void*>(object));
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
inline SRWLOCK editLock=SRWLOCK_INIT;
inline uintptr_t editedSelf{},editedOwner{};inline Bone* editedBones{};inline unsigned editedCount{};
inline Bone before[128],after[128];
inline void restore(uintptr_t self){
    if(editedSelf!=self)return;
    __try {
        if(player_rig::word(self+0xf8)==editedOwner&&player_rig::word(self+0x34)==reinterpret_cast<uintptr_t>(editedBones)
            &&player_rig::word(self+0x38)==editedCount&&memcmp(editedBones,after,editedCount*sizeof(Bone))==0)
            memcpy(editedBones,before,editedCount*sizeof(Bone));
    } __except(EXCEPTION_EXECUTE_HANDLER){}
    editedSelf=0;
}
inline bool apply(uintptr_t self,mgs5vr::Pose grip,unsigned center){
    __try {
        if(!isSinglePlayerWeapon(self))return false;
        auto count=player_rig::word(self+0x38);auto bones=reinterpret_cast<Bone*>(player_rig::word(self+0x34));
        if(!bones||count<1||count>128)return false;
        mgs5vr::Pose root;memcpy(&root.position,reinterpret_cast<void*>(self+0x124),12);memcpy(&root.orientation,reinterpret_cast<void*>(self+0x134),16);
        mgs5vr::Pose anchor{bones[0].orientation,bones[0].position};if(!mgs5vr::valid(root)||!mgs5vr::valid(anchor))return false;
        static uintptr_t calibratedSelf{};static uint32_t calibratedOwner{};static unsigned calibratedCenter{};static mgs5vr::Pose trim{};
        auto owner=player_rig::word(self+0xf8);
        if(calibratedSelf!=self||calibratedOwner!=owner||calibratedCenter!=center){
            auto world=mgs5vr::compose(root,anchor);
            trim=mgs5vr::compose(mgs5vr::inverse(grip),world);trim.position={};
            calibratedSelf=self;calibratedOwner=owner;calibratedCenter=center;
            log("Right weapon candidate calibrated: bones=%u owner=%08x (visual prototype)\n",count,owner);
        }
        auto target=mgs5vr::compose(mgs5vr::inverse(root),mgs5vr::compose(grip,trim));
        auto delta=mgs5vr::compose(target,mgs5vr::inverse(anchor));
        Bone adjusted[128];
        for(unsigned i=0;i<count;++i){adjusted[i]=bones[i];auto pose=mgs5vr::compose(delta,{bones[i].orientation,bones[i].position});
            if(!mgs5vr::valid(pose))return false;adjusted[i].position=pose.position;adjusted[i].orientation=pose.orientation;}
        // Restore our exact output before the next native evaluation, including
        // disable/tracking loss, so an idle animator cannot accumulate overrides.
        memcpy(before,bones,count*sizeof(Bone));memcpy(after,adjusted,count*sizeof(Bone));
        editedSelf=self;editedOwner=owner;editedBones=bones;editedCount=count;
        memcpy(bones,adjusted,count*sizeof(Bone));return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline void __fastcall evaluate(void* self,void*,uintptr_t first,uintptr_t second){
    AcquireSRWLockExclusive(&editLock);restore(reinterpret_cast<uintptr_t>(self));ReleaseSRWLockExclusive(&editLock);
    original(self,first,second);
    if(amalur::bodyDebug.enabled(amalur::nativeArms)||interfaceView.load()||!firstPerson.load()||!headTracking.load()||!enabled.load())return;
    mgs5vr::Pose grip;uint64_t timestamp;unsigned center;
    AcquireSRWLockShared(&poseLock);grip=desired;timestamp=tick;center=generation;ReleaseSRWLockShared(&poseLock);
    auto now=GetTickCount64();if(!timestamp||timestamp>now||now-timestamp>150)return;
    AcquireSRWLockExclusive(&editLock);apply(reinterpret_cast<uintptr_t>(self),grip,center);ReleaseSRWLockExclusive(&editLock);
}
inline void install(){
    // Full FabInstancePhysics evaluation includes the alternate animation path
    // that bypasses 0x91b4f0. Apply only after both native paths have completed.
    auto target=reinterpret_cast<unsigned char*>(gameBase+0x96f600);
    const unsigned char expected[]={0x83,0xec,0x34,0xa1};
    if(memcmp(target,expected,sizeof(expected))){log("Weapon evaluation signature mismatch; skipped\n");return;}
    if(*reinterpret_cast<uintptr_t*>(target+4)!=gameBase+0x157713c||target[8]!=0x33||target[9]!=0xc4){log("Weapon evaluation guard mismatch; skipped\n");return;}
    hook(target,reinterpret_cast<void*>(&evaluate),reinterpret_cast<void**>(&original),"Weapon pose after native evaluation");
}
}
