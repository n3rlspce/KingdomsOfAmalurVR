#pragma once
#include <Xinput.h>
#include "../tracking/motion_input.hpp"
namespace motion_controls {
using GetState=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
inline GetState original{};
inline amalur::MotionInputChannel channel;
inline SRWLOCK lock=SRWLOCK_INIT;
inline DWORD packetNumber{};
inline bool gameFocused(){DWORD pid{};GetWindowThreadProcessId(GetForegroundWindow(),&pid);return pid==GetCurrentProcessId();}
inline DWORD WINAPI getState(DWORD index,XINPUT_STATE* state){
    DWORD result=original(index,state);
    if(index||!state||!firstPerson.load()||!headTracking.load())return result;
    // A neutral virtual pad stays connected while enabled, including loss of XR
    // focus. No sticky key injection, driver install or writes to player velocity.
    amalur::MotionInputPacket motion;
    AcquireSRWLockExclusive(&lock);
    bool active=gameFocused()&&channel.open(false)&&channel.read(motion);
    if(result!=ERROR_SUCCESS)*state={};
    if(active){state->Gamepad.sThumbLX=static_cast<SHORT>(motion.moveX*32767);state->Gamepad.sThumbLY=static_cast<SHORT>(motion.moveY*32767);}
    else {state->Gamepad.sThumbLX=0;state->Gamepad.sThumbLY=0;}
    state->dwPacketNumber=++packetNumber;
    ReleaseSRWLockExclusive(&lock);
    return ERROR_SUCCESS;
}
inline void install(){
    HMODULE module=GetModuleHandleW(L"xinput1_3.dll");if(!module){log("XInput module unavailable\n");return;}
    auto target=GetProcAddress(module,"XInputGetState");
    if(target)hook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(&getState),reinterpret_cast<void**>(&original),"Touch left stick via XInput");
}
}
