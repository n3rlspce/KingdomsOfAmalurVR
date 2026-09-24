#pragma once
#include "../tracking/native_arrow_feedback.hpp"
// AF4520 is the native on_animation_begin_callback wrapper. It passes self+38
// and the uint32 argument to script callback index11. Player dispatches exact
// hash005F7BF4 to player_ammo_manager.arrow_fired(), which consumes Arrow01.
// This observes release, never bow charge construction or controller input.
namespace native_arrow_haptics {
using Begin=uintptr_t(__thiscall*)(void*,const uint32_t*);
inline Begin original{};
inline SRWLOCK lock=SRWLOCK_INIT;
inline amalur::NativeArrowObservation observation;
inline amalur::NativeArrowIdentity identity(uintptr_t entity){
 amalur::NativeArrowIdentity out;
 __try{
  const auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());
  if(!entity||!player||(player_rig::word(player)!=gameBase+0x1359f14&&player_rig::word(player)!=gameBase+0x1359e94))return out;
  const auto owner=player_rig::word(player+0x1ec);
  if(!owner||player_rig::resolve(owner)!=entity||player_rig::word(entity+0x38)!=owner||!(player_rig::word(entity+0x10c)&1))return out;
  out={entity,player,owner,true};
 }__except(EXCEPTION_EXECUTE_HANDLER){}
 return out;
}
inline uint32_t callback(const uint32_t* argument){uint32_t hash{};__try{if(argument)hash=*argument;}__except(EXCEPTION_EXECUTE_HANDLER){}return hash;}
inline uintptr_t __fastcall begin(void* self,void*,const uint32_t* argument){
 const auto hash=callback(argument);const auto entity=reinterpret_cast<uintptr_t>(self);
 const auto before=hash==amalur::nativeArrowFiredHash?identity(entity):amalur::NativeArrowIdentity{};
 // Native exceptions propagate, and no event is published if original fails.
 return amalur::observeNativeArrowCall([&]{return original(self,argument);},[&]{return before.valid?identity(entity):amalur::NativeArrowIdentity{};},
  [&](const amalur::NativeArrowIdentity& after){
   AcquireSRWLockExclusive(&lock);const auto serial=observation.completed(hash,before,after);ReleaseSRWLockExclusive(&lock);
   if(serial)native_action_haptics::emit(amalur::ActionFeedbackKind::Arrow,before.owner,serial);
  });
}
inline bool install(){
 if(original)return true;
 __try {
  constexpr unsigned char prefix[]{0x81,0xec,0xe0,0x01,0,0,0x8b,0x84,0x24,0xe4,0x01,0,0};
  constexpr unsigned char end[]{0xc2,0x04,0x00};
  if(memcmp(reinterpret_cast<void*>(gameBase+0xaf4520),prefix,sizeof(prefix))
    ||memcmp(reinterpret_cast<void*>(gameBase+0xaf482f),end,sizeof(end))
    ||*reinterpret_cast<const uint16_t*>(gameBase+0xaf46c5)!=0x0b6a)return false;
  return hook(reinterpret_cast<void*>(gameBase+0xaf4520),reinterpret_cast<void*>(&begin),reinterpret_cast<void**>(&original),"Native arrow fired haptic observation");
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
