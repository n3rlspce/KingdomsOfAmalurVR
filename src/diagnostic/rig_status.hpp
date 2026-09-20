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
        auto object=output-0x34;bool daggers=weapon_control::isPlayerDaggers(object);
        if(daggers)motion_controls::daggerSeen.store(GetTickCount64());
        if(!daggers&&!weapon_control::isSinglePlayerWeapon(object))return;
        // Bounded discovery of a newly equipped weapon's attachment maps.
        // This observes the native table; it does not select an unverified slot.
        static uintptr_t observedObject{};static uint32_t observedOwner{};
        auto owner=player_rig::word(object+0xf8);
        if(observedObject!=object||observedOwner!=owner){
            observedObject=object;observedOwner=owner;
            auto table=player_rig::word(reinterpret_cast<uintptr_t>(mapper));
            log("Weapon attachment census bones=%u nativeSlot=%u\n",player_rig::word(output+4),unsigned(nativeSlot));
            for(unsigned s=0;s<9;++s){
                auto e=table+s*32,n=player_rig::word(e+4),p=player_rig::word(e);
                if(n>8)continue;
                for(unsigned j=0;j<n;++j)log("Weapon map slot=%u tuple=%u src=%u dst=%u flags=%u\n",s,j,
                    player_rig::word(p+j*12),player_rig::word(p+j*12+4),player_rig::word(p+j*12+8));
            }
        }
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
    current.firstPerson=firstPerson.load();current.tracked=trackedCameraAvailable.load();
    current.paused=game_pause::sample(current.weaponRemaps!=0);
    AcquireSRWLockShared(&weapon_control::poseLock);
    auto now=GetTickCount64();current.handFresh=weapon_control::tick&&weapon_control::tick<=now&&now-weapon_control::tick<150;
    ReleaseSRWLockShared(&weapon_control::poseLock);
    channel.transfer(current,true);ReleaseSRWLockExclusive(&lock);
}
}
