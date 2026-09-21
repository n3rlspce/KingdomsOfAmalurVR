#pragma once
#include "../tracking/weapon_family.hpp"
#include "../tracking/melee_swing_event.hpp"
#include "../tracking/melee_feedback_capture.hpp"
#include <cmath>

// Include after player_rig and weapon_control. This prerequisite observes the
// actual native audio/FX callbacks, but NEVER replays them or borrows pointers.
// onSwing is called on the native update thread by the physical-swing owner.
namespace melee_feedback {
inline constexpr bool nativePlaybackAvailable=false;
inline SRWLOCK stateLock=SRWLOCK_INIT;
inline amalur::MeleeFeedbackInbox inbox;
inline amalur::MeleeFeedbackBudget swingBudget,nativeBudget;
inline std::atomic_flag observing=ATOMIC_FLAG_INIT;

inline void onSwing(const amalur::MeleeSwingEvent& event);

enum class Kind:unsigned {GameSound,DerivedSound,WeaponFx,CharacterWeaponFx,Fx};
inline constexpr uintptr_t vtables[]{0x132a46c,0x132aadc,0x1329cdc,0x132a564,0x1322634};
inline constexpr uintptr_t callbacks[]{0x986960,0x9e5b80,0x9eee70,0x993610,0x90bef0};
inline constexpr const char* names[]{"game-sound","derived-sound","weapon-fx","character-weapon-fx","fx"};
inline constexpr const char* meanings[]{"gameplay-noise","derived-audio","dynamic-weapon-effect","character-weapon-effect","effect"};
// Payload widths from the retail serializers/readers, not padded object size.
inline void readFields(Kind kind,uintptr_t event,uint32_t (&fields)[4]){
    for(auto& field:fields)field=0;
    if(kind==Kind::GameSound)fields[0]=*reinterpret_cast<const uint16_t*>(event+0x14);
    else if(kind==Kind::DerivedSound){
        fields[0]=player_rig::word(event+0x20);
        fields[1]=player_rig::word(event+0x24); // Unknown field; not a sound ID.
        fields[2]=*reinterpret_cast<const unsigned char*>(event+0x28);
        fields[3]=player_rig::word(event+0x2c);
    }else if(kind==Kind::WeaponFx){
        fields[0]=player_rig::word(event+0x14);fields[1]=player_rig::word(event+0x18);
        fields[2]=*reinterpret_cast<const unsigned char*>(event+0x1c);
    }else if(kind==Kind::Fx){
        // FXEvent serializer6193f0 reads11bytes at+14. Byte+1f is padding.
        memcpy(fields,reinterpret_cast<const void*>(event+0x14),11);
    }else for(unsigned i=0;i<3;++i)fields[i]=player_rig::word(event+0x14+i*4);
}
// All five retail virtual+44 handlers use ECX=self, three stack arguments and
// ret 0xc. Preserve EAX as well as every argument; no native exception is caught.
using Callback=uintptr_t(__thiscall*)(void*,uintptr_t,uintptr_t,uintptr_t);
inline Callback original[5]{};

inline void inspect(Kind kind,uintptr_t event,uintptr_t record,uintptr_t index,uintptr_t times){
    const auto k=static_cast<unsigned>(kind);
    if(player_rig::word(event)!=gameBase+vtables[k])return;
    const auto context=player_rig::word(record);
    if(!context)return;
    const auto owner=player_rig::word(context+0x44);
    const auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());
    if(!player||(player_rig::word(player)!=gameBase+0x1359f14&&player_rig::word(player)!=gameBase+0x1359e94)
        ||player_rig::word(player+0x1ec)!=owner)return;
    const auto entity=player_rig::resolve(owner);
    if(!entity||player_rig::word(entity+0x38)!=owner||!(player_rig::word(entity+0x10c)&1))return;

    // Copy only fixed, statically observed event fields. Values remain raw until
    // their meanings/asset namespaces are verified. In particular a sound hash
    // is not assumed to be a playable resource ID or a character voice ID.
    uint32_t fields[4]{};
    readFields(kind,event,fields);
    const auto contextTag=player_rig::word(context+0x3c),fabIndex=player_rig::word(context+0x48);
    const auto object=weapon_control::fab(fabIndex);
    uint32_t model=0,visibility=0;
    if(object&&player_rig::word(object+0x194)==fabIndex){model=player_rig::word(object+0xf0);visibility=player_rig::word(object+0x1d0);}
    const auto start=player_rig::word(times),end=player_rig::word(times+4);
    const auto recordStart=player_rig::word(record+8),recordEnd=player_rig::word(record+0xc);
    const auto physics=player_rig::word(entity+0x3c+15*4);
    unsigned windows=0;
    if(physics&&player_rig::word(physics+0x18)==owner&&player_rig::word(physics+0x1c)==15)
        windows=player_rig::word(physics+0x2e4);
    if(windows>32)return;
    if(player_rig::word(record)!=context||player_rig::word(context+0x44)!=owner
        ||player_rig::word(event)!=gameBase+vtables[k]||player_rig::word(player+0x1ec)!=owner)return;
    const auto now=GetTickCount64();
    unsigned serial[2]{},chain[2]{},dropped=0;
    AcquireSRWLockExclusive(&stateLock);
    const bool report=nativeBudget.allow(now);
    if(report){
        dropped=nativeBudget.dropped;nativeBudget.dropped=0;
        for(unsigned i=0;i<2;++i){const auto& swing=inbox.latest[i];
            if(swing.owner==owner&&swing.tick<=now&&now-swing.tick<=900){serial[i]=swing.serial;chain[i]=swing.chainStep;}}
    }
    ReleaseSRWLockExclusive(&stateLock);
    if(report)log("VR melee native feedback tick=%llu kind=%s meaning=%s owner=%08x event=%08x contextTag=%u model=%u fab=%u visibility=%08x index=%u fields=%08x,%08x,%08x,%08x times=%u,%u recordTimes=%u,%u nativeWindows=%u swing=%u,%u chain=%u,%u dropped=%u\n",
        now,names[k],meanings[k],owner,unsigned(event),contextTag,model,fabIndex,visibility,unsigned(index),fields[0],fields[1],fields[2],fields[3],
        start,end,recordStart,recordEnd,windows,serial[0],serial[1],chain[0],chain[1],dropped);
}
#include "melee_feedback_resolved.hpp"
#include "melee_longsword_audio.hpp"
inline void onSwing(const amalur::MeleeSwingEvent& event){
    const auto now=GetTickCount64();
    AcquireSRWLockExclusive(&stateLock);
    const bool accepted=inbox.accept(event,now);
    const bool report=accepted&&swingBudget.allow(now);
    ReleaseSRWLockExclusive(&stateLock);
    if(accepted)longsword_audio::onSwing(event);
    if(report)log("VR melee feedback swing tick=%llu owner=%08x weapon=%08x asset=%u hand=%u serial=%u chain=%u generation=%u nativePlayback=%s\n",event.tick,event.owner,event.weapon,event.asset,event.hand,event.serial,event.chainStep,event.generation,amalur::knownLongswordModel(event.asset)?"longsword-audio-pilot":"unavailable");
}

inline void observe(Kind kind,uintptr_t event,uintptr_t record,uintptr_t index,uintptr_t times){
    if(observing.test_and_set())return;
    __try{inspect(kind,event,record,index,times);}
    __except(EXCEPTION_EXECUTE_HANDLER){} // Observation failure never skips native work.
    observing.clear();
}
template<Kind K> inline uintptr_t __fastcall dispatch(void* self,void*,uintptr_t record,uintptr_t index,uintptr_t times){
    observe(K,reinterpret_cast<uintptr_t>(self),record,index,times);
    const auto previous=resolved::scope;
    resolved::scope=resolved::begin(K,reinterpret_cast<uintptr_t>(self),record);
    uintptr_t result=0;
    __try{result=original[static_cast<unsigned>(K)](self,record,index,times);}
    __finally{resolved::finish();resolved::scope=previous;}
    return result;
}
inline void install(){
    if(GetFileAttributesW(L"amalur-melee-effects-probe.enable")==INVALID_FILE_ATTRIBUTES)return;
    // Exact retail instruction prefixes and RTTI-derived virtual dispatch table
    // slots must agree. CharacterWeaponFx contains a relocated cookie address.
    const unsigned char expected[5][8]{
        {0x8b,0x44,0x24,0x04,0x83,0xec,0x14,0x56},
        {0x81,0xec,0xc8,0,0,0,0xf6,0x05},
        {0x8b,0x44,0x24,0x0c,0x8b,0x50,0x04,0x2b},
        {0x83,0xec,0x74,0xa1,0,0,0,0},
        {0x83,0xec,0x34,0x56,0x8b,0xf1,0x8b,0x46}};
    void* replacements[]{reinterpret_cast<void*>(&dispatch<Kind::GameSound>),reinterpret_cast<void*>(&dispatch<Kind::DerivedSound>),
        reinterpret_cast<void*>(&dispatch<Kind::WeaponFx>),reinterpret_cast<void*>(&dispatch<Kind::CharacterWeaponFx>),reinterpret_cast<void*>(&dispatch<Kind::Fx>)};
    for(unsigned i=0;i<5;++i){
        auto target=reinterpret_cast<unsigned char*>(gameBase+callbacks[i]);
        if(player_rig::word(gameBase+vtables[i]+0x44)!=reinterpret_cast<uintptr_t>(target)
            ||memcmp(target,expected[i],i==3?4:8)
            ||(i==3&&player_rig::word(reinterpret_cast<uintptr_t>(target)+4)!=gameBase+0x157713c)){
            log("VR melee native feedback signature mismatch kind=%s; observer skipped\n",names[i]);continue;
        }
        hook(target,replacements[i],reinterpret_cast<void**>(&original[i]),names[i]);
    }
    resolved::install();
    longsword_audio::install();
    log("VR melee native feedback observation enabled; longsword audio pilot installed when signatures validate\n");
}
}
