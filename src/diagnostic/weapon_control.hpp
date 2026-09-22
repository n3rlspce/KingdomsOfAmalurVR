namespace body_visibility { void observeHide(uintptr_t object); }
#pragma once
#include "../tracking/native_weapon_slot.hpp"
#include "../tracking/selected_weapon_proof.hpp"
#include "../tracking/weapon_pose.hpp"
#include "../tracking/held_weapon_profile.hpp"
#include "../tracking/held_visibility.hpp"
#include "../tracking/back_sheath.hpp"
#include "../tracking/dagger_orientation.hpp"
#include "../tracking/grip_settings.hpp"
#include "../tracking/melee_swing.hpp"
#include "../tracking/melee_swing_event.hpp"
#include "../tracking/longsword_gesture.hpp"
#include "../tracking/longsword_hold.hpp"
#include "../tracking/longsword_debug_view.hpp"
#include "../tracking/longsword_swing.hpp"
#include "../tracking/longsword_stroke.hpp"
#include "../tracking/melee_family_policy.hpp"
#include "../tracking/heavy_charge_input.hpp"
#include "../tracking/charged_feedback_channel.hpp"
#include "../tracking/grip_filter.hpp"
namespace arm_rig {extern std::atomic<bool> enabled;}
namespace weapon_drawn_scale {inline void restore(uintptr_t object);}
namespace weapon_control {
using Evaluate=void(__thiscall*)(void*,uintptr_t,uintptr_t);
inline Evaluate original{};
inline SRWLOCK poseLock=SRWLOCK_INIT;
inline amalur::BackSheathGesture backGesture;
inline std::atomic<bool> weaponSheathed{false},backGripClaimed{false};
inline uint32_t backOwner{},backWeapon{};inline uint64_t backSelectionTick{};
inline amalur::BackSheathAction pendingBackAction{};inline uint64_t pendingBackTick{};
inline uint32_t pendingBackOwner{},pendingBackWeapon{},pendingBackSession{},pendingBackGeneration{};
inline mgs5vr::Pose desired{};
inline amalur::LocomotionFrame locomotionFrame;
inline mgs5vr::Pose desiredLeft{};
inline uint64_t leftTick{};
inline mgs5vr::Pose bladeWorld[2]{};
inline uint64_t bladeTick{};inline uint32_t bladeOwner{};
inline unsigned swingSerial[2]{};inline float swingSpeed[2]{};
inline amalur::MeleeSwingEvent swingEvents[2];
inline amalur::MeleeSwingChain swingChain;
inline amalur::LongswordGesture longswordGesture;
inline amalur::LongswordHold longswordHold;
inline amalur::HeavyChargeChannel heavyInput;
inline amalur::HeavyChargePacket heavyPacket;
inline amalur::ChargedFeedbackWriter chargedFeedback;
inline amalur::ChargedFeedbackChannel chargedFeedbackChannel;
inline void publishChargedFeedback(){auto packet=chargedFeedback.packet();chargedFeedbackChannel.transfer(packet,true);}
inline amalur::GripChargeGate gripCharge;
inline unsigned heavyMode{},heavySession{},heavyGeneration{};
inline amalur::LongswordStroke longswordStroke;
inline amalur::MeleeSwingEvent longswordContact;
inline uint64_t contactStroke{};
inline bool longswordContactReady{};
inline amalur::LongswordStroke basicStrokes[2];
inline amalur::NativeFamilyChain familyChains[2];
inline amalur::MeleeSwingEvent basicContacts[2];
inline uint64_t basicStrokeIds[2]{};
inline bool basicContactReady[2]{};
inline mgs5vr::Vec3 frameHeadForward{0,0,-1};
inline amalur::LongswordDebugStatus longswordDebug;
inline float longswordCharge{};
inline bool longswordReady{};
inline std::atomic<uint32_t> physicalActor{0};
inline uint32_t visualWeapon{},visualAsset{},gestureWeapon{},gestureAsset{};
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
// Called with poseLock exclusive: contact and peak share one publication.
inline bool commitLongswordLocked(unsigned serial,unsigned center,uint32_t weapon,uint32_t model,uint64_t now){
    if(!serial||longswordContact.serial!=serial||longswordContact.generation!=center||generation!=center
        ||longswordContact.weapon!=weapon||visualWeapon!=weapon||visualAsset!=model||longswordContact.asset!=model
        ||!longswordStroke.active())return false;
    if(longswordStroke.emitted())return swingEvents[0].serial==serial;
    auto next=longswordGesture;const auto strike=next.commit(now);
    if(strike.attack!=longswordContact.attackAsset||strike.flags!=longswordContact.attackFlags)return false;
    if(!longswordStroke.commit(now))return false;
    longswordGesture=next;
    if(strike.heavy&&heavyMode==1)gripCharge.consume();
    // Recipe was reserved from the same charge/combo state at stroke admission.
    longswordContact.attackAsset=strike.attack;longswordContact.attackFlags=strike.flags;
    longswordContact.chainStep=strike.step;longswordContact.heavy=strike.heavy;
    longswordContact.tick=now;longswordContact.weaponPose=visualPoses[0];
    swingEvents[0]=longswordContact;
    if(strike.heavy&&chargedFeedback.committed(weapon,center,serial,GetTickCount64()))publishChargedFeedback();
    return true;
}
// All access to these proposals is protected by poseLock.
inline amalur::MeleeSwingEvent contactEvent(unsigned side){
    if(side>=2)return {};
    return amalur::knownLongswordModel(visualAsset)?(side==0?longswordContact:amalur::MeleeSwingEvent{}):basicContacts[side];
}
inline bool contactReady(unsigned side){
    return side<2&&(amalur::knownLongswordModel(visualAsset)?(side==0&&longswordContactReady):basicContactReady[side]);
}
inline bool commitPhysicalLocked(unsigned side,const amalur::MeleeSwingEvent& proposed,uint64_t frame,uint64_t now){
    const auto live=contactEvent(side);
    if(!amalur::sameMeleeContact(proposed,live,visualWeapon,visualAsset,generation,side,
        frame,visualTick,proposed.attackAsset,proposed.attackFlags))return false;
    if(amalur::knownLongswordModel(visualAsset))return commitLongswordLocked(proposed.serial,generation,visualWeapon,visualAsset,now);
    auto& stroke=basicStrokes[side];
    if(!stroke.active())return false;
    if(stroke.emitted())return swingEvents[side].serial==proposed.serial;
    auto chain=familyChains[side];
    if(visualAsset==1250||visualAsset==1323){
        const auto recipe=chain.commit(visualWeapon,visualAsset,generation,now);
        if(recipe.attack!=proposed.attackAsset||recipe.flags!=proposed.attackFlags||recipe.step!=proposed.chainStep)return false;
    }
    if(!stroke.commit(now))return false;
    familyChains[side]=chain;
    basicContacts[side].tick=now;basicContacts[side].weaponPose=visualPoses[side];
    swingEvents[side]=basicContacts[side];return true;
}
inline bool sampleBasicStroke(unsigned side,const amalur::PosePacket& packet,mgs5vr::Vec3 head,unsigned center,bool allowed){
    auto& stroke=basicStrokes[side];auto& event=basicContacts[side];
    const bool peak=stroke.sample({packet.position[0],packet.position[1],packet.position[2]},head,frameHeadForward,
        packet.tick,center,allowed&&amalur::supportedMeleeHand(visualAsset,side),false,amalur::strokeRecoveryMs(visualAsset));
    if(!stroke.active())event={};
    else if(basicStrokeIds[side]!=stroke.stroke()||!event.serial){
        basicStrokeIds[side]=stroke.stroke();event={};event.weapon=visualWeapon;event.asset=visualAsset;
        event.hand=side;event.serial=++swingSerial[side];event.generation=center;event.tick=packet.tick;
        event.weaponPose=visualPoses[side];amalur::assignBasicStrokeRecipe(event);
    }
    if(event.serial&&!stroke.emitted()&&(visualAsset==1250||visualAsset==1323)){
        const auto recipe=familyChains[side].preview(visualWeapon,visualAsset,center,packet.tick);
        event.attackAsset=recipe.attack;event.attackFlags=recipe.flags;event.chainStep=recipe.step;
    }
    basicContactReady[side]=stroke.contactReady();
    return peak&&commitPhysicalLocked(side,event,visualTick,packet.tick);
}
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
    frameHeadForward=mgs5vr::rotate(mgs5vr::Quat{snapshot.head.orientation[0],snapshot.head.orientation[1],snapshot.head.orientation[2],snapshot.head.orientation[3]},{0,0,-1});
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
        &&motion_controls::gameFocused()&&!motion_controls::dialogueActive.load()&&!motion_controls::explicitSpellActive(now);
    AcquireSRWLockExclusive(&poseLock);locomotionFrame=locomotion;desired=result;tick=valid?p.tick:0;desiredLeft=leftResult;leftTick=leftValid?l.tick:0;generation=recenter;worldScale=scale;
    const bool backEligible=gestures&&valid&&headTracking.load()&&trackedCameraAvailable.load()
        &&backWeapon&&backSelectionTick&&backSelectionTick<=now&&now-backSelectionTick<100;
    gestures=gestures&&visualWeapon&&visualTick&&visualTick<=now&&now-visualTick<100;
    amalur::HeavyChargePacket incoming;
    if(heavyInput.transfer(incoming,false)&&amalur::validHeavyChargePacket(incoming,now))heavyPacket=incoming;
    const bool heavyFresh=amalur::validHeavyChargePacket(heavyPacket,now);
    if(!backEligible||!heavyFresh||!heavyPacket.active||heavyPacket.spell)
        pendingBackAction=amalur::BackSheathAction::None;
    const auto back=backGesture.sample({{p.position[0]-headLocal.x,p.position[1]-headLocal.y,p.position[2]-headLocal.z},
        frameHeadForward,p.tick,heavyPacket.session,recenter,backWeapon,heavyPacket.grip,
        backEligible&&heavyFresh&&heavyPacket.active,heavyPacket.spell!=0,weaponSheathed.load()});
    backGripClaimed.store(back.claimed);
    if(back.action!=amalur::BackSheathAction::None){
        pendingBackAction=back.action;pendingBackTick=now;pendingBackOwner=backOwner;pendingBackWeapon=backWeapon;
        pendingBackSession=heavyPacket.session;pendingBackGeneration=recenter;
    }
    gestures=gestures&&!weaponSheathed.load()&&!back.claimed;
    const unsigned chosenMode=heavyFresh?heavyPacket.mode:heavyMode;
    const bool heavyChanged=chosenMode!=heavyMode||(heavyFresh&&heavyPacket.session!=heavySession)||heavyGeneration!=recenter;
    heavyMode=chosenMode;heavyGeneration=recenter;if(heavyFresh)heavySession=heavyPacket.session;
    if(heavyChanged)log("VR heavy input mode=%s session=%u generation=%u\n",heavyMode?"right-grip":"position",heavySession,recenter);
    if(!gestures||gestureWeapon!=visualWeapon||gestureAsset!=visualAsset||heavyChanged){gripCharge.reset();rightSwing.reset();leftSwing.reset();swingChain.reset();longswordGesture.reset();longswordHold.reset();longswordStroke.reset();longswordContact={};contactStroke=0;longswordContactReady=false;for(unsigned side=0;side<2;++side){basicStrokes[side].reset();familyChains[side].reset();basicContacts[side]={};basicStrokeIds[side]=0;basicContactReady[side]=false;}gestureWeapon=gestures?visualWeapon:0;gestureAsset=gestures?visualAsset:0;}
    if(offsetValid)weaponCentimetres=offset;
    auto tip=[&](const amalur::PosePacket& v){return amalur::meleeTipRelative(mgs5vr::Pose{{v.orientation[0],v.orientation[1],v.orientation[2],v.orientation[3]},{v.position[0],v.position[1],v.position[2]}},headLocal);};
    const bool selectedPose=visualSelection<=1&&visualSelection==motion_controls::viewControls().selectedWeapon;
    // Keep actor heading aligned during staff attacks; only a fresh selected staff may
    // bypass locomotion's heading ownership, never the dodge's ownership.
    staff_aim::publish({backOwner,visualWeapon,visualSelection,generation,visualTick,
        gestures&&valid&&selectedPose&&visualAsset==1514&&backOwner&&backWeapon==visualWeapon
        &&backSelectionTick&&backSelectionTick<=now&&now-backSelectionTick<100});
    const bool longsword=amalur::knownLongswordModel(visualAsset)&&selectedPose;
    const bool basic=amalur::basicStrokeModel(visualAsset)&&selectedPose;
    const auto bladeUp=mgs5vr::rotate(visualPoses[0].orientation,{0,0,1});
    const mgs5vr::Vec3 relativeHand{p.position[0]-headLocal.x,p.position[1]-headLocal.y,p.position[2]-headLocal.z};
    const bool stableHold=longswordHold.sample(relativeHand,p.tick,recenter,gestures&&valid&&longsword);
    const bool gripMode=heavyMode==1;
    const bool gripHeld=gripCharge.sample(heavyPacket,now,gestures&&valid&&longsword&&gripMode);
    const bool chargeAllowed=gestures&&valid&&longsword&&(!gripMode||(heavyFresh&&heavyPacket.active&&!heavyPacket.spell));
    const bool preparation=longsword&&!gripMode&&relativeHand.y>-.45f
        &&(longswordGesture.ready()?longswordHold.risingNow():longswordHold.raising());
    bool r=longsword?longswordStroke.sample({p.position[0],p.position[1],p.position[2]},headLocal,frameHeadForward,p.tick,recenter,gestures&&valid,preparation,amalur::strokeRecoveryMs(visualAsset))
        :basic?sampleBasicStroke(0,p,headLocal,recenter,gestures&&valid):rightSwing.sample(tip(p),p.tick,recenter,gestures&&valid);
    bool left=basic?sampleBasicStroke(1,l,headLocal,recenter,gestures&&visualDual&&leftValid):leftSwing.sample(tip(l),l.tick,recenter,gestures&&visualDual&&leftValid);
    if(r&&!longsword&&!basic)++swingSerial[0];if(left&&!basic)++swingSerial[1];
    const bool raised=gripMode?gripHeld:amalur::longswordChargePose(relativeHand);
    auto swordStrike=longswordGesture.sample(visualWeapon,recenter,p.tick,
        chargeAllowed,raised,(gripMode||stableHold)?0.f:1.f,false);
    chargedFeedback.sample(GetCurrentProcessId(),heavySession,recenter,visualWeapon,now,
        chargeAllowed&&heavyFresh&&heavyPacket.active&&!heavyPacket.spell,longswordGesture.ready());
    publishChargedFeedback();
    if(longsword){
        if(!longswordStroke.active())longswordContact={};
        else if(contactStroke!=longswordStroke.stroke()||!longswordContact.serial){
            contactStroke=longswordStroke.stroke();
            auto preview=longswordGesture;const auto recipe=preview.commit(p.tick);
            longswordContact={};longswordContact.weapon=visualWeapon;longswordContact.asset=visualAsset;
            longswordContact.serial=++swingSerial[0];longswordContact.generation=recenter;
            longswordContact.tick=p.tick;longswordContact.weaponPose=visualPoses[0];
            longswordContact.attackAsset=recipe.attack;longswordContact.attackFlags=recipe.flags;
            longswordContact.chainStep=recipe.step;longswordContact.heavy=recipe.heavy;
        }
        if(longswordContact.serial&&!longswordStroke.emitted()){
            // Heavy departure and combo timeouts may expire before contact.
            // Refresh the uncommitted proposal; the native adapter rechecks it.
            auto preview=longswordGesture;const auto recipe=preview.commit(p.tick);
            longswordContact.attackAsset=recipe.attack;longswordContact.attackFlags=recipe.flags;
            longswordContact.chainStep=recipe.step;longswordContact.heavy=recipe.heavy;
        }
        longswordContactReady=longswordStroke.contactReady();
        if(r)r=commitLongswordLocked(longswordContact.serial,recenter,visualWeapon,visualAsset,p.tick);
        if(r)swordStrike={longswordContact.chainStep,longswordContact.attackAsset,longswordContact.attackFlags,longswordContact.heavy};
    }else {longswordStroke.reset();longswordContact={};longswordContactReady=false;contactStroke=0;}
    longswordCharge=longswordGesture.progress();longswordReady=longswordGesture.ready();
    longswordDebug={chargeAllowed,raised,true,gripMode||stableHold,
        preparation,longswordReady,longswordCharge,longswordStroke.speed(),relativeHand.y,bladeUp.z,swingSerial[0],now,longswordStroke.gate()};
    longswordDebug.gripMode=gripMode;
    const auto chainStep=longsword?swordStrike.step:basic?1u:(r||left)?swingChain.advance(visualWeapon,recenter,now):0;
    auto publish=[&](unsigned side,uint64_t stamp){
        auto& event=swingEvents[side];event={};event.weapon=visualWeapon;event.asset=visualAsset;
        event.hand=side;event.serial=swingSerial[side];event.generation=recenter;event.tick=stamp;
        event.chainStep=chainStep;event.weaponPose=visualPoses[side];
        if(longsword&&side==0){event.attackAsset=swordStrike.attack;event.attackFlags=swordStrike.flags;event.heavy=swordStrike.heavy;}
    };
    if(r&&!longsword&&!basic)publish(0,p.tick);if(left&&!basic)publish(1,l.tick);
    swingSpeed[0]=longsword?longswordStroke.speed():basic?basicStrokes[0].speed():rightSwing.speed();swingSpeed[1]=basic?basicStrokes[1].speed():leftSwing.speed();
    if(r||left)motion_controls::swingUntil.store(now+90);
    const auto reportModel=visualAsset,reportWeapon=visualWeapon,reportSelection=visualSelection;
    const auto reportFrame=visualTick;const auto rs=swingSpeed[0],ls=swingSpeed[1];
    const auto rg=longsword?longswordStroke.gate():basic?basicStrokes[0].gate():rightSwing.gate(),lg=basic?basicStrokes[1].gate():leftSwing.gate();
    const auto rightSerial=swingSerial[0],leftSerial=swingSerial[1];
    const auto charge=longswordCharge;const bool charged=longswordReady;
    ReleaseSRWLockExclusive(&poseLock);
    static uint64_t nextDebugLog{};
    if(longsword&&now>=nextDebugLog){nextDebugLog=now+250;log("VR longsword guide tick=%llu height=%.3f heightOK=%d up=%.3f angleOK=%d steady=%d preparation=%d progress=%.3f ready=%d gate=%s\n",now,relativeHand.y,raised,bladeUp.z,true,stableHold,preparation,charge,charged,rg);}
    static int chargeState=-1;const int state=charged?2:charge>0?1:0;
    if(chargeState!=state){chargeState=state;log("VR longsword charge tick=%llu model=%u weapon=%08x state=%d progress=%.3f raised=%d speed=%.3f\n",now,reportModel,reportWeapon,state,charge,raised,rs);}
    if(r&&longsword)log("VR longsword strike tick=%llu weapon=%08x serial=%u step=%u attack=%u flags=%u heavy=%d speed=%.3f\n",now,reportWeapon,rightSerial,swordStrike.step,swordStrike.attack,swordStrike.flags,swordStrike.heavy,rs);
    // Log accepted gesture speed itself; the interval peak may belong to a
    // different sample and must not be mistaken for the acceptance speed.
    if(r)log("VR swing accepted tick=%llu poseTick=%llu model=%u weapon=%08x hand=0 serial=%u chain=%u speed=%.4f threshold=%.2f\n",now,p.tick,reportModel,reportWeapon,rightSerial,chainStep,rs,(longsword||basic)?double(amalur::LongswordStroke::airSpeed):.9);
    if(left)log("VR swing accepted tick=%llu poseTick=%llu model=%u weapon=%08x hand=1 serial=%u chain=%u speed=%.4f threshold=%.2f\n",now,l.tick,reportModel,reportWeapon,leftSerial,chainStep,ls,basic?4.5:.9);
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
        log("VR swing speed tick=%llu model=%u weapon=%08x generation=%u allowed=%d speed=%.4f,%.4f peak=%.4f,%.4f fired=%u,%u gate=%s,%s threshold=%.2f\n",
            now,reportModel,reportWeapon,recenter,gestures,rs,ls,peaks[0],peaks[1],fired[0],fired[1],gates[0],gates[1],(longsword||basic)?double(amalur::LongswordStroke::airSpeed):.9);
        peaks[0]=peaks[1]=0;fired[0]=fired[1]=0;
    }
    static uint64_t nextReport{};
    if(now>=nextReport){nextReport=now+2000;
        log("VR swing detector tick=%llu model=%u weapon=%08x selection=%u currentSelection=%u poseTick=%llu allowed=%d rightTracked=%d leftTracked=%d speed=%.3f,%.3f threshold=%.2f focused=%d firstPerson=%d interface=%d dialogue=%d nativeArms=%d\n",
            now,reportModel,reportWeapon,reportSelection,motion_controls::viewControls().selectedWeapon,reportFrame,gestures,valid,leftValid,rs,ls,(longsword||basic)?double(amalur::LongswordStroke::airSpeed):.9,
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
using SelectedProof=amalur::SelectedWeaponProof;
inline SelectedProof selectedProof;
inline void publishSelectedWeapon(uintptr_t player,uint32_t owner,uint32_t weapon,uint32_t selection,uint32_t session,uint64_t now){
    AcquireSRWLockExclusive(&poseLock);
    if(backOwner!=owner||backWeapon!=weapon){
        backGesture.reset();weaponSheathed.store(false);backGripClaimed.store(false);pendingBackAction=amalur::BackSheathAction::None;
        backOwner=owner;backWeapon=weapon;
    }
    backSelectionTick=now;selectedProof={player,owner,weapon,selection,session,now};ReleaseSRWLockExclusive(&poseLock);
}
inline bool freshSelectedProof(const SelectedProof& proof){
    const auto now=GetTickCount64();const auto p=reinterpret_cast<uintptr_t>(player_rig::player.load());
    const auto controls=motion_controls::viewControls();
    return amalur::selectedWeaponProofCurrent(proof,p,p?player_rig::word(p+0x1ec):0,
        controls.selectedWeapon,controls.session,now);
}
inline void logSelectionProofRejected(uintptr_t object,const SelectedProof& proof){
    static std::atomic<uint64_t> lastLog{};const auto now=GetTickCount64();auto last=lastLog.load();
    if(now>=last&&now-last<2000)return;
    if(!lastLog.compare_exchange_strong(last,now))return;
    const auto controls=motion_controls::viewControls();
    log("Tracked weapon proof rejected expected=%08x observed=%08x asset=%u expectedSlot=%u selected=%u proofSession=%u session=%u proofAge=%llu fresh=%u\n",
        proof.weapon,player_rig::word(object+0xf8),player_rig::word(object+0xf0),proof.selection,controls.selectedWeapon,
        proof.session,controls.session,now>=proof.tick?now-proof.tick:~0ull,unsigned(freshSelectedProof(proof)));
}
inline bool authoritativeSelectedWeapon(uintptr_t object,SelectedProof* accepted=nullptr){
    __try{
        SelectedProof proof;AcquireSRWLockShared(&poseLock);proof=selectedProof;ReleaseSRWLockShared(&poseLock);
        if(!freshSelectedProof(proof)||player_rig::word(object+0xf8)!=proof.weapon){logSelectionProofRejected(object,proof);return false;}
        auto root=currentWeaponRoot();if(!root||player_rig::word(root+0xf8)!=proof.owner)return false;
        const auto entity=player_rig::resolve(proof.weapon);
        if(!entity||!player_rig::part(entity,11,proof.weapon,0x135745c))return false;
        const auto item=player_rig::word(entity+0x3c+10*4);
        if(!item||player_rig::word(item+0x18)!=proof.weapon||player_rig::word(item+0x1c)!=10
            ||!(player_rig::word(item+0x20)&1)||player_rig::word(item+0x8c)!=proof.owner)return false;
        const auto count=player_rig::word(root+0x28),children=player_rig::word(root+0x24);
        if(!children||count>32)return false;
        bool found=false;
        for(unsigned i=0;i<count;++i){const auto child=fab(player_rig::word(children+i*4));
            if(!child||player_rig::word(child+0xf8)!=proof.weapon)continue;
            if(child!=object)return false;found=true;
        }
        if(found&&accepted)*accepted=proof;
        return found;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
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
    // Attachment visibility is not inventory selection: a stowed secondary may
    // leave the primary as the sole visible child. Missing proof fails closed.
    return authoritativeSelectedWeapon(self);
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
struct HeldProof {uintptr_t object{},root{},buffer{};uint32_t owner{},rootOwner{},asset{};uint64_t tick{};uint32_t selection{};uintptr_t mapper{},table{};};
inline HeldProof heldProof;
inline bool keepCapturedVisible(uintptr_t object){
    __try{
        if(weaponSheathed.load())return false;
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
        bool tracked=amalur::heldWeaponTracked(kind,amalur::freshWeaponPose(right,rt,now),amalur::freshWeaponPose(left,lt,now));
        return tracked&&proof.selection==motion_controls::viewControls().selectedWeapon
            &&proof.object==object&&proof.root==root&&proof.owner==player_rig::word(object+0xf8)
            &&proof.rootOwner==player_rig::word(root+0xf8)&&proof.buffer==player_rig::word(object+0x34)
            &&proof.asset==player_rig::word(object+0xf0)&&proof.tick&&proof.tick<=now&&now-proof.tick<100
            &&game_pause::sample(true)==0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline void recordHeld(uintptr_t object,uintptr_t slot,mgs5vr::Pose worldRoot,void* mapper=nullptr){
    __try{
        if(weaponSheathed.load())return;
        auto kind=capturedHeldKind(object);if(kind==amalur::HeldWeaponKind::None||slot!=amalur::expectedHeldSlot(kind))return;
        SelectedProof selected;if(!authoritativeSelectedWeapon(object,&selected))return;
        auto root=currentWeaponRoot();if(!root)return;
        HeldProof proof{object,root,player_rig::word(object+0x34),player_rig::word(object+0xf8),
            player_rig::word(root+0xf8),player_rig::word(object+0xf0),GetTickCount64(),selected.selection};
        proof.mapper=reinterpret_cast<uintptr_t>(mapper);proof.table=mapper?player_rig::word(proof.mapper):0;
        auto bones=reinterpret_cast<const amalur::RigBone*>(proof.buffer);
        bool dual=kind==amalur::HeldWeaponKind::Faeblades;
        auto right=mgs5vr::compose(worldRoot,amalur::bonePose(bones[dual?4:1]));
        auto left=dual?mgs5vr::compose(worldRoot,amalur::bonePose(bones[1])):right;
        if(!freshSelectedProof(selected))return;
        AcquireSRWLockExclusive(&poseLock);
        if(!amalur::sameSelectedWeaponProof(selected,selectedProof)){
            ReleaseSRWLockExclusive(&poseLock);return;
        }
        bool changed=heldProof.object!=object||heldProof.owner!=proof.owner||heldProof.selection!=proof.selection;
        heldProof=proof;
        if(mgs5vr::valid(right)&&mgs5vr::valid(left)){visualWeapon=proof.owner;visualAsset=proof.asset;
            visualTick=proof.tick;visualSelection=proof.selection;visualDual=dual;visualPoses[0]=right;visualPoses[1]=left;}
        ReleaseSRWLockExclusive(&poseLock);
        if(changed)log("Tracked held weapon ready owner=%08x asset=%u kind=%u slot=%u selection=%u\n",proof.owner,proof.asset,unsigned(kind),unsigned(slot),proof.selection);
        if(nativeShow&&(player_rig::word(object+0x1d0)&4)&&keepCapturedVisible(object))nativeShow(reinterpret_cast<void*>(object));
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
// Local game-update only. Showing the exact previously remapped active Fab lets
// native evaluation resume after a menu/cutscene hide, without fabricating a
// fresh weapon pose or issuing an attack.
inline void recoverCapturedVisibility(){
    __try{
        if(weaponSheathed.load())return;
        if(!nativeShow||amalur::bodyDebug.enabled(amalur::nativeArms)||!firstPerson.load()||!headTracking.load()
            ||!trackedCameraAvailable.load()||!arm_rig::enabled.load()||interfaceView.load()
            ||!motion_controls::gameFocused()||motion_controls::dialogueActive.load()||game_pause::sample(true)!=0)return;
        HeldProof proof;mgs5vr::Pose right,left;uint64_t rt,lt;
        AcquireSRWLockShared(&poseLock);proof=heldProof;right=desired;left=desiredLeft;rt=tick;lt=leftTick;ReleaseSRWLockShared(&poseLock);
        const auto object=proof.object,root=currentWeaponRoot();
        if(!object||!root||!proof.mapper||!proof.table||!(player_rig::word(object+0x1d0)&4)
            ||(player_rig::word(root+0x1d0)&0xf2004)
            ||(*reinterpret_cast<unsigned char*>(root+0x1d5)>0&&!*reinterpret_cast<unsigned char*>(root+0x1d6)))return;
        const amalur::HeldVisibilityIdentity saved{proof.object,proof.root,proof.buffer,proof.mapper,proof.table,
            proof.owner,proof.rootOwner,proof.asset,proof.selection};
        const amalur::HeldVisibilityIdentity live{object,root,player_rig::word(object+0x34),proof.mapper,player_rig::word(proof.mapper),
            player_rig::word(object+0xf8),player_rig::word(root+0xf8),player_rig::word(object+0xf0),motion_controls::viewControls().selectedWeapon};
        if(!amalur::sameHeldVisibilityIdentity(saved,live))return;
        if(!authoritativeSelectedWeapon(object))return;
        const auto kind=capturedHeldKind(object);const auto now=GetTickCount64();
        if(kind==amalur::HeldWeaponKind::None||!amalur::heldWeaponTracked(kind,
            amalur::freshWeaponPose(right,rt,now),amalur::freshWeaponPose(left,lt,now)))return;
        const auto held=proof.table+amalur::expectedHeldSlot(kind)*32,stowed=proof.table+8*32;
        constexpr uint32_t stowedMap[]{62,0,0};
        if(player_rig::word(stowed+4)!=1||!player_rig::word(stowed)
            ||memcmp(reinterpret_cast<void*>(player_rig::word(stowed)),stowedMap,sizeof(stowedMap))
            ||!amalur::capturedHeldMap(kind,player_rig::word(held+4),reinterpret_cast<const uint32_t*>(player_rig::word(held))))return;
        nativeShow(reinterpret_cast<void*>(object));
        static uint64_t lastLog{};if(now-lastLog>=2000){lastLog=now;
            log("Tracked held weapon visibility recovered owner=%08x asset=%u previousPoseAge=%llu\n",proof.owner,proof.asset,now>=proof.tick?now-proof.tick:0);}
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
inline bool keepDaggersVisible(uintptr_t object){
    __try{
        if(weaponSheathed.load())return false;
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
    body_visibility::observeHide(reinterpret_cast<uintptr_t>(self));
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
        if(weaponSheathed.load())return;
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
        if(weaponSheathed.load())return;
        if(!isPlayerDaggers(object)||!isOnlyActivePlayerWeapon(object))return;
        auto native=[](mgs5vr::Pose p){p.orientation={-p.orientation.x,-p.orientation.y,-p.orientation.z,p.orientation.w};return p;};
        mgs5vr::Pose root;memcpy(&root.position,reinterpret_cast<void*>(object+0x124),12);memcpy(&root.orientation,reinterpret_cast<void*>(object+0x134),16);
        root=native(root);if(!mgs5vr::valid(root))return;
        auto bones=reinterpret_cast<const Bone*>(player_rig::word(object+0x34));mgs5vr::Pose p[2];
        p[0]=mgs5vr::compose(root,native({bones[4].orientation,bones[4].position}));
        p[1]=mgs5vr::compose(root,native({bones[1].orientation,bones[1].position}));
        if(!mgs5vr::valid(p[0])||!mgs5vr::valid(p[1]))return;
        auto owner=player_rig::word(object+0xf8),visualModel=player_rig::word(object+0xf0);
        SelectedProof selected;if(!authoritativeSelectedWeapon(object,&selected))return;
        const auto selection=selected.selection;
        if(!freshSelectedProof(selected))return;
        AcquireSRWLockExclusive(&poseLock);
        if(!amalur::sameSelectedWeaponProof(selected,selectedProof)){
            ReleaseSRWLockExclusive(&poseLock);return;
        }
        bladeWorld[0]=p[0];bladeWorld[1]=p[1];bladeTick=GetTickCount64();bladeOwner=owner;
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
    weapon_drawn_scale::restore(reinterpret_cast<uintptr_t>(self));
    AcquireSRWLockExclusive(&editLock);restore(reinterpret_cast<uintptr_t>(self));ReleaseSRWLockExclusive(&editLock);
    original(self,first,second);
    if(weaponSheathed.load())return;
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
