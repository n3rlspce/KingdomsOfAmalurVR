#pragma once
#include "../tracking/rig_status.hpp"
#include "game_pause.hpp"
namespace rig_status {
inline SRWLOCK lock=SRWLOCK_INIT;
inline amalur::RigStatus current;
inline amalur::RigStatusChannel channel;
inline void attachment(void* mapper,uintptr_t slot,uintptr_t source,uintptr_t output,uintptr_t nativeSlot){
    __try {
        auto root=rig_probe::playerRoot();if(!root||source!=root+0x34||output<0x34||slot>=32)return;
        auto object=output-0x34;if(!weapon_control::isSinglePlayerWeapon(object))return;
        auto entry=player_rig::word(reinterpret_cast<uintptr_t>(mapper))+slot*32;
        auto mappings=player_rig::word(entry+4);if(!mappings||mappings>64)return;
        // Last mapping is the held grip for the one-weapon staff layout.
        auto tuple=player_rig::word(entry)+(mappings-1)*12,index=player_rig::word(tuple),destination=player_rig::word(tuple+4);
        auto count=player_rig::word(source+4);if(index>=count||count<=44||destination>=player_rig::word(output+4))return;
        auto bones=reinterpret_cast<const amalur::RigBone*>(player_rig::word(source));
        auto result=reinterpret_cast<const amalur::RigBone*>(player_rig::word(output));
        float wrist[3],socket[3],rendered[3];
        memcpy(wrist,&bones[44].position,12);memcpy(socket,&bones[index].position,12);
        memcpy(rendered,&result[destination].position,12);
        AcquireSRWLockExclusive(&lock);
        ++current.weaponRemaps;current.weaponSlot=static_cast<uint32_t>(slot);current.sourceBone=index;
        current.nativeWeaponSlot=static_cast<uint32_t>(nativeSlot);
        memcpy(current.nativeWrist,wrist,12);memcpy(current.nativeSocket,socket,12);
        memcpy(current.renderedSocket,rendered,12);
        ReleaseSRWLockExclusive(&lock);
    } __except(EXCEPTION_EXECUTE_HANDLER){}
}
inline void publish(){
    AcquireSRWLockExclusive(&lock);
    current.pid=GetCurrentProcessId();current.tick=GetTickCount();current.frames=presents.load();
    current.remaps=arm_rig::samples.load();current.focused=motion_controls::gameFocused();
    current.firstPerson=firstPerson.load();current.tracked=haveCameraForFrame&&cameraForFrame.valid;
    current.paused=game_pause::sample(current.weaponRemaps!=0);
    AcquireSRWLockShared(&weapon_control::poseLock);
    auto now=GetTickCount64();current.handFresh=weapon_control::tick&&weapon_control::tick<=now&&now-weapon_control::tick<150;
    ReleaseSRWLockShared(&weapon_control::poseLock);
    channel.transfer(current,true);ReleaseSRWLockExclusive(&lock);
}
}
