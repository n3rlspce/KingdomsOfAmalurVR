#pragma once
#include <Xinput.h>
#include "../tracking/motion_input.hpp"
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
inline DWORD WINAPI getState(DWORD index,XINPUT_STATE* state){
    DWORD result=original(index,state);
    if(index||!state)return result;
    // XR buttons also work in menus. Focus/overlay/expiry gate the complete pad,
    // not just locomotion, so released or disconnected triggers cannot stick.
    amalur::MotionInputPacket motion;
    AcquireSRWLockExclusive(&lock);
    bool connected=channel.open(false);
    if(!connected){ReleaseSRWLockExclusive(&lock);return result;}
    bool active=gameFocused()&&channel.read(motion);
    if(result!=ERROR_SUCCESS)*state={};
    if(active){
        // Item radial directions belong to its screen-space selector, not the
        // world. Rotate virtual locomotion only; keep physical pads untouched.
        if(!dialogueActive.load()&&!(motion.buttons&XINPUT_GAMEPAD_LEFT_SHOULDER))
            movementBasis.transform(motion.moveX,motion.moveY,GetTickCount64());
        amalur::mergeMotion(state->Gamepad,motion);
        if(!dialogueActive.load()&&contactEnabled.load()&&(state->Gamepad.wButtons&XINPUT_GAMEPAD_X)&&state->Gamepad.bRightTrigger<XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
            primaryAttackUntil.store(GetTickCount64()+2000);
    }
    amalur::locomotionFacing.observe(gameFocused()&&!dialogueActive.load(),state->Gamepad.sThumbLX,
        state->Gamepad.sThumbLY,GetTickCount64());
    // Observe the final merged pad so a physical controller gets the same dodge
    // protection. RT+A is an ability, not a dodge; don't seize native facing.
    amalur::dodgeFacing.observe(gameFocused()&&!dialogueActive.load(),
        (state->Gamepad.wButtons&XINPUT_GAMEPAD_A)!=0,
        state->Gamepad.bRightTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD,GetTickCount64());
    static DWORD lastInput=~0u;
    DWORD current=static_cast<DWORD>(state->Gamepad.wButtons)|(static_cast<DWORD>(state->Gamepad.bLeftTrigger)<<16)|(static_cast<DWORD>(state->Gamepad.bRightTrigger)<<24);
    if(current!=lastInput){log("Touch XInput tick=%llu active=%d buttons=%04x LT=%u RT=%u\n",GetTickCount64(),active,state->Gamepad.wButtons,state->Gamepad.bLeftTrigger,state->Gamepad.bRightTrigger);lastInput=current;}

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
