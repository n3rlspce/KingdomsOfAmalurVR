#pragma once
#include <Xinput.h>
#include "controller_ui_mode.hpp"
#include "interaction_prompt_trace.hpp"
#include "native_finisher_state.hpp"
#include "../tracking/finisher_automation.hpp"
#include "../tracking/finisher_manual.hpp"
#include "../tracking/motion_input.hpp"
#include "walk_input_trace.hpp"
#include "../tracking/native_attack_input.hpp"
#include "../tracking/dodge_facing.hpp"
#include "../tracking/movement_basis.hpp"
namespace motion_controls {
using GetState=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
inline GetState original{};
using GetCapabilities=DWORD(WINAPI*)(DWORD,DWORD,XINPUT_CAPABILITIES*);
inline GetCapabilities originalCapabilities{};
inline amalur::MotionInputChannel channel;
inline SRWLOCK lock=SRWLOCK_INIT;
inline DWORD packetNumber{};
inline std::atomic<bool> contactEnabled{false};
inline std::atomic<bool> dialogueActive{false};
inline std::atomic<uint64_t> daggerSeen{},swingUntil{};
inline std::atomic<bool> meleeContextReady{false};
inline std::atomic<uint64_t> primaryAttackUntil{};
inline amalur::NativeAttackInput nativeAttackInput;
inline amalur::MeleeSpellSequence spellSequence;
inline amalur::FinisherAutomation finisherAutomation;
inline amalur::ManualFinisher manualFinisher;
inline std::atomic<uint64_t> explicitSpellUntil{};
inline bool explicitSpellActive(uint64_t now){return now<explicitSpellUntil.load();}
inline amalur::MovementBasis movementBasis;
inline void sampleMovementBasis(mgs5vr::Vec3 nativeForward,mgs5vr::Vec3 headForward,bool enabled,uint64_t tick){
    AcquireSRWLockExclusive(&lock);
    movementBasis.sample(nativeForward,headForward,enabled,tick);
    ReleaseSRWLockExclusive(&lock);
}
struct ViewControls {uint32_t selectedWeapon{},session{};float turnYawDegrees{};};
inline ViewControls cachedViewControls;
inline ViewControls viewControls(){
    // Camera updates must not depend on the game's XInput polling cadence.
    // Keep the last valid yaw/selection while focus, panel or tracking gating
    // makes input inactive; resetting those fields would visibly undo a turn.
    AcquireSRWLockExclusive(&lock);
    amalur::MotionInputPacket motion;
    if(channel.open(false)&&channel.read(motion))
        cachedViewControls={motion.selectedWeapon,motion.session,motion.turnYawDegrees};
    const auto result=cachedViewControls;
    ReleaseSRWLockExclusive(&lock);
    return result;
}
inline bool gameFocused(){DWORD pid{};GetWindowThreadProcessId(GetForegroundWindow(),&pid);return pid==GetCurrentProcessId();}
inline bool nativeAttackContext(){
    return gameFocused()&&firstPerson.load()&&!interfaceView.load()&&!dialogueActive.load();
}
inline bool nativeAttackHeld(uint64_t now){return nativeAttackInput.held(now,nativeAttackContext());}

// Movies can stop XInput polling; read the fresh bridge input independently.
inline bool vrCursorHidden(){
    AcquireSRWLockExclusive(&lock);
    amalur::MotionInputPacket motion;
    const bool active=gameFocused()&&headTracking.load()&&!interfaceView.load()
        &&channel.open(false)&&channel.read(motion);
    ReleaseSRWLockExclusive(&lock);
    return active;
}
// Native ACTOR.get_active_interact_target reads Globals+3C5C (RVA A40B40).
// Validate the entity generation so loads/stale handles fail closed.
inline uint32_t nativeInteractionTarget(){
    __try {
        const auto word=player_rig::word;
        const auto globals=word(gameBase+0x15fe9c4);if(!globals)return 0;
        const auto handle=static_cast<uint32_t>(word(globals+0x3c5c));
        if(!handle)return 0;
        const auto entity=player_rig::resolve(handle);
        if(!entity||!(word(entity+0x10c)&1)||word(globals+0x3c5c)!=handle)return 0;
        return handle;
    } __except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}
inline DWORD WINAPI getState(DWORD index,XINPUT_STATE* state){
    DWORD result=original(index,state);
    if(index||!state)return result;
    // XR buttons also work in menus. Focus/overlay/expiry gate the complete pad,
    // not just locomotion, so released or disconnected triggers cannot stick.
    amalur::MotionInputPacket motion;
    AcquireSRWLockExclusive(&lock);
    const auto inputNow=GetTickCount64();
    bool connected=channel.open(false);
    static int traceConnection=-1;
    if(traceConnection!=int(connected)){
        log("VR interaction input-channel tick=%llu connected=%d nativeResult=%lu\n",inputNow,connected,result);
        traceConnection=int(connected);
    }
    if(!connected){
        finisherAutomation.sample({},inputNow);
        manualFinisher.sample({},false,inputNow);
        controller_ui_mode::update(false);spellSequence.reset();explicitSpellUntil.store(0);
        staff_aim::observe(0,0,false,inputNow);
        const bool delivered=result==ERROR_SUCCESS;
        nativeAttackInput.observe(delivered,nativeAttackContext(),delivered?state->Gamepad.wButtons:0,
            delivered?state->Gamepad.bRightTrigger:0,inputNow);
        interaction_prompt_trace::observe(delivered,delivered?state->Gamepad.wButtons:0,inputNow,
            delivered?state->Gamepad.bRightTrigger:0,delivered?state->Gamepad.sThumbLX:0,delivered?state->Gamepad.sThumbLY:0);
        ReleaseSRWLockExclusive(&lock);return result;
    }
    const bool focused=gameFocused();
    bool active=focused&&channel.read(motion);
    const float walkRawX=active?motion.moveX:0.f,walkRawY=active?motion.moveY:0.f;
    const auto rawButtons=active?motion.buttons:0;
    const bool rawRecoveryNeutral=active&&!motion.buttons&&motion.block<.1f&&motion.abilities<.1f&&motion.supportGrip<.1f;
    const bool rawFinisherNeutral=active&&!motion.buttons&&motion.block<.1f&&motion.abilities<.1f
        &&motion.supportGrip<.1f&&std::abs(motion.moveX)<.1f&&std::abs(motion.moveY)<.1f
        &&std::abs(motion.lookX)<.1f&&std::abs(motion.lookY)<.1f;
    const auto nativeButtons=result==ERROR_SUCCESS?state->Gamepad.wButtons:0;
    // Fresh focused XR input remains a controller during UI/camera transitions.
    // Head-pose availability is independent of button input.
    controller_ui_mode::update(active);
    if(result!=ERROR_SUCCESS)*state={};
    const bool physicalMelee=active&&contactEnabled.load()&&firstPerson.load()
        &&!interfaceView.load()&&!dialogueActive.load();
    if(!physicalMelee){spellSequence.reset();explicitSpellUntil.store(0);}
    if(active){
        // Item radial directions belong to its screen-space selector, not the
        // world. Rotate virtual locomotion only; keep physical pads untouched.
        if(!dialogueActive.load()&&!(motion.buttons&XINPUT_GAMEPAD_LEFT_SHOULDER))
            movementBasis.transform(motion.moveX,motion.moveY,GetTickCount64());
        spellSequence.sample(motion,inputNow,physicalMelee);
        if(physicalMelee&&spellSequence.active())explicitSpellUntil.store(inputNow+250);
        amalur::mergeMotion(state->Gamepad,motion);
        if(!dialogueActive.load()&&contactEnabled.load()&&(state->Gamepad.wButtons&XINPUT_GAMEPAD_X)&&state->Gamepad.bRightTrigger<XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
            primaryAttackUntil.store(GetTickCount64()+2000);
    }
    // Initiate native finishers from their incapacitated state. Native mode84/state314 ends
    // automation immediately so the player's native QTE buttons pass untouched.
    const auto finisher=native_finisher_state::read(finisherAutomation.recoveryTarget(inputNow));
    const auto& delivered=state->Gamepad;
    const bool padNeutral=!delivered.wButtons&&delivered.bRightTrigger<25&&delivered.bLeftTrigger<25
        &&std::abs(int(delivered.sThumbLX))<3277&&std::abs(int(delivered.sThumbLY))<3277
        &&std::abs(int(delivered.sThumbRX))<3277&&std::abs(int(delivered.sThumbRY))<3277;
    amalur::FinisherObservation finisherObservation;
    finisherObservation.owner=finisher.owner;finisherObservation.target=finisher.target;
    finisherObservation.player=finisher.player;finisherObservation.session=active?motion.session:0;
    finisherObservation.valid=finisher.valid&&finisher.targetKnown;finisherObservation.down=finisher.targetDown;
    finisherObservation.magicResidue=finisher.magicResidue;
    finisherObservation.magicMode=finisher.magicMode;
    finisherObservation.nativeSequence=finisher.nativeSequence;
    finisherObservation.distanceMetres=finisher.distanceMetres;
    finisherObservation.specialBoss=finisher.specialBoss;
    finisherObservation.targetFallback=finisher.usingRecoveryTarget;
    finisherObservation.manualA=((rawButtons|nativeButtons)&XINPUT_GAMEPAD_A)!=0;
    finisherObservation.eligible=active&&inputNow>=motion.tick&&inputNow-motion.tick<100
        &&firstPerson.load()&&headTracking.load()&&!interfaceView.load()&&!dialogueActive.load()
        &&finisher.gameplay&&finisher.finisherReady&&!spellSequence.active();
    finisherObservation.neutral=rawFinisherNeutral&&padNeutral;
    // Walking/turning can continue during modifier cleanup. Face buttons,
    // triggers and grips still cancel before they can combine with owned RT.
    finisherObservation.recoveryNeutral=rawRecoveryNeutral&&!delivered.wButtons
        &&delivered.bRightTrigger<25&&delivered.bLeftTrigger<25;
    const bool manualClear=active&&!(rawButtons&~XINPUT_GAMEPAD_A)
        &&!(delivered.wButtons&~XINPUT_GAMEPAD_A)&&motion.block<.1f
        &&motion.abilities<.1f&&motion.supportGrip<.1f
        &&delivered.bRightTrigger<25&&delivered.bLeftTrigger<25;
    const auto manual=manualFinisher.sample(finisherObservation,manualClear,inputNow);
    auto autoObservation=finisherObservation;
    if(manual.claimed||manualFinisher.blocksAutomatic(inputNow)){autoObservation.eligible=false;autoObservation.neutral=false;autoObservation.recoveryNeutral=false;}
    const auto automatic=finisherAutomation.sample(autoObservation,inputNow);
    if(manual.suppressA)state->Gamepad.wButtons&=~XINPUT_GAMEPAD_A;
    if(manual.x){state->Gamepad.wButtons|=XINPUT_GAMEPAD_X;primaryAttackUntil.store(inputNow+2000);}
    if(manual.a)state->Gamepad.wButtons|=XINPUT_GAMEPAD_A;
    static unsigned previousManualPhase{};
    if(manual.phase!=previousManualPhase){
        log("VR manual finisher tick=%llu owner=%08x target=%08x phase=%u X=%d A=%d residue=%d nativeSequence=%d\n",
            inputNow,finisher.owner,finisher.target,manual.phase,manual.x,manual.a,finisher.magicResidue,finisher.nativeSequence);
        previousManualPhase=manual.phase;
    }
    if(automatic.claimed)explicitSpellUntil.store(inputNow+250);
    if(automatic.rightTrigger)state->Gamepad.bRightTrigger=255;
    if(automatic.a)state->Gamepad.wButtons|=XINPUT_GAMEPAD_A;
    static unsigned previousAutomatic=~0u;static uint64_t lastAutomaticLog{};
    const unsigned automaticState=unsigned(automatic.claimed)|(unsigned(automatic.rightTrigger)<<1)|(unsigned(automatic.a)<<2);
    const unsigned diagnosticState=automaticState|(unsigned(automatic.reason)<<3)
        |(unsigned(finisher.magicMode)<<8)|(unsigned(finisher.magicResidue)<<9)
        |(unsigned(finisher.valid)<<10)|(unsigned(finisher.targetKnown)<<11)
        |(unsigned(finisher.finisherReady)<<12)|(unsigned(finisher.gameplay)<<13)
        |(unsigned(active)<<14)|(unsigned(focused)<<15)|(unsigned(headTracking.load())<<16)
        |(unsigned(interfaceView.load())<<17)|(unsigned(dialogueActive.load())<<18)
        |(unsigned(spellSequence.active())<<19)|(unsigned(rawFinisherNeutral)<<20)
        |(unsigned(padNeutral)<<21)|(unsigned(finisherObservation.eligible)<<22)
        |(unsigned(finisherObservation.recoveryNeutral)<<23);
    if(diagnosticState!=previousAutomatic&&(automatic.claimed||previousAutomatic==~0u||
        (previousAutomatic&1u)||inputNow-lastAutomaticLog>=500)){
        log("VR finisher automation tick=%llu owner=%08x target=%08x currentTarget=%08x fallback=%d output=%u phase=%u reason=%u attempts=%u requests=%u residue=%d nativeSequence=%d distance=%.3f valid=%d known=%d down=%d ready=%d gameplay=%d active=%d focused=%d age=%llu first=%d head=%d interface=%d dialogue=%d spell=%d rawNeutral=%d padNeutral=%d magic=%d manualA=%d recoveryNeutral=%d\n",
            inputNow,finisher.owner,finisher.target,finisher.currentTarget,finisher.usingRecoveryTarget,automaticState,automatic.phase,unsigned(automatic.reason),automatic.attempts,automatic.aRequests,
            finisher.magicResidue,finisher.nativeSequence,finisher.distanceMetres,finisher.valid,finisher.targetKnown,
            finisher.targetDown,finisher.finisherReady,finisher.gameplay,active,focused,active&&inputNow>=motion.tick?inputNow-motion.tick:~uint64_t(0),
            firstPerson.load(),headTracking.load(),interfaceView.load(),dialogueActive.load(),spellSequence.active(),rawFinisherNeutral,padNeutral,
            finisher.magicMode,finisherObservation.manualA,finisherObservation.recoveryNeutral);
        previousAutomatic=diagnosticState;lastAutomaticLog=inputNow;
    }
    static unsigned previousARoute=~0u;
    const unsigned aRoute=((rawButtons&XINPUT_GAMEPAD_A)?1u:0u)
        |((nativeButtons&XINPUT_GAMEPAD_A)?2u:0u)|((state->Gamepad.wButtons&XINPUT_GAMEPAD_A)?4u:0u);
    const unsigned routeState=aRoute|(unsigned(focused)<<3)|(unsigned(active)<<4)
        |(unsigned(state->Gamepad.bRightTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD)<<5)
        |(unsigned(headTracking.load())<<6)|(unsigned(interfaceView.load())<<7)
        |(unsigned(dialogueActive.load())<<8)|(unsigned(spellSequence.active())<<9);
    if(routeState!=previousARoute && (aRoute || (previousARoute&7u))){
        log("VR interaction input-route tick=%llu rawA=%u nativeA=%u deliveredA=%u focused=%d xrActive=%d RT=%u head=%d interface=%d dialogue=%d spell=%d\n",
            inputNow,aRoute&1u,(aRoute>>1)&1u,(aRoute>>2)&1u,focused,active,state->Gamepad.bRightTrigger,
            headTracking.load(),interfaceView.load(),dialogueActive.load(),spellSequence.active());
    }
    previousARoute=routeState;
    nativeAttackInput.observe(true,nativeAttackContext(),state->Gamepad.wButtons,state->Gamepad.bRightTrigger,inputNow);
    // Observe the pad actually returned to the game, including native-controller
    // fallback when XR is stale. Connected path always returns ERROR_SUCCESS.
    interaction_prompt_trace::observe(true,state->Gamepad.wButtons,inputNow,
        state->Gamepad.bRightTrigger,state->Gamepad.sThumbLX,state->Gamepad.sThumbLY);
    staff_aim::observe(state->Gamepad.wButtons,state->Gamepad.bRightTrigger,
        active&&firstPerson.load()&&!interfaceView.load()&&!dialogueActive.load()&&!explicitSpellActive(inputNow),inputNow);
    amalur::locomotionFacing.observe(gameFocused()&&!dialogueActive.load(),state->Gamepad.sThumbLX,
        state->Gamepad.sThumbLY,GetTickCount64());
    // Observe the final merged pad so a physical controller gets the same dodge
    // protection. RT+A is an ability, not a dodge; don't seize native facing.
    const auto interactionTarget=active&&!dialogueActive.load()?nativeInteractionTarget():0;
    amalur::dodgeFacing.observe(gameFocused()&&!dialogueActive.load(),
        (state->Gamepad.wButtons&XINPUT_GAMEPAD_A)!=0,
        state->Gamepad.bRightTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD,GetTickCount64(),interactionTarget!=0);
    static DWORD lastInput=~0u;
    DWORD current=static_cast<DWORD>(state->Gamepad.wButtons)|(static_cast<DWORD>(state->Gamepad.bLeftTrigger)<<16)|(static_cast<DWORD>(state->Gamepad.bRightTrigger)<<24);
    if(current!=lastInput){log("Touch XInput tick=%llu active=%d buttons=%04x LT=%u RT=%u interact=%08x\n",GetTickCount64(),active,state->Gamepad.wButtons,state->Gamepad.bLeftTrigger,state->Gamepad.bRightTrigger,interactionTarget);lastInput=current;}

    const unsigned walkContext=unsigned(firstPerson.load())
        |(unsigned(headTracking.load())<<1)|(unsigned(interfaceView.load())<<2)
        |(unsigned(dialogueActive.load())<<3)|(unsigned(physicalMelee)<<4)
        |(unsigned(spellSequence.active())<<5);
    walk_input_trace::observe(inputNow,active,focused,walkRawX,walkRawY,motion,
        state->Gamepad,finisher,walkContext);
    state->dwPacketNumber=++packetNumber;
    ReleaseSRWLockExclusive(&lock);
    return ERROR_SUCCESS;
}
inline DWORD WINAPI getCapabilities(DWORD index,DWORD flags,XINPUT_CAPABILITIES* caps){
    auto result=originalCapabilities(index,flags,caps);if(index||!caps||result==ERROR_SUCCESS)return result;
    AcquireSRWLockExclusive(&lock);bool connected=channel.open(false);ReleaseSRWLockExclusive(&lock);
    if(!connected)return result;
    *caps={};caps->Type=XINPUT_DEVTYPE_GAMEPAD;caps->SubType=XINPUT_DEVSUBTYPE_GAMEPAD;
    caps->Gamepad.wButtons=0xf3ff;caps->Gamepad.bLeftTrigger=caps->Gamepad.bRightTrigger=255;
    caps->Gamepad.sThumbLX=caps->Gamepad.sThumbLY=32767;
    return ERROR_SUCCESS;
}
inline void install(){
    HMODULE module=GetModuleHandleW(L"xinput1_3.dll");if(!module){log("XInput module unavailable\n");return;}
    auto target=GetProcAddress(module,"XInputGetState");
    if(target)hook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(&getState),reinterpret_cast<void**>(&original),"Touch gamepad state via XInput");
    auto caps=GetProcAddress(module,"XInputGetCapabilities");
    if(caps)hook(reinterpret_cast<void*>(caps),reinterpret_cast<void*>(&getCapabilities),reinterpret_cast<void**>(&originalCapabilities),"Touch gamepad capabilities");
}
}
