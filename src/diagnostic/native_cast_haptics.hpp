#pragma once
#include "../tracking/native_cast_feedback.hpp"
// Include after melee_owned_source and native_action_haptics. Observation only:
// feedback means accepted native cast/staff-action START, not impact or finish.
namespace native_cast_haptics {
inline SRWLOCK lock=SRWLOCK_INIT;
inline amalur::NativeCastObservation observation;
inline void retired(uintptr_t address){AcquireSRWLockExclusive(&lock);observation.retired(address);ReleaseSRWLockExclusive(&lock);}
inline void created(uintptr_t address,uint32_t asset,uint32_t owner,uint32_t index,int result,bool owned){
 if(owned||result||!address||(asset!=892&&asset!=160&&asset!=161))return;
 __try {
  const auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());
  if(!player||(player_rig::word(player)!=gameBase+0x1359f14&&player_rig::word(player)!=gameBase+0x1359e94)
   ||!owner||player_rig::word(player+0x1ec)!=owner)return;
  const auto entity=player_rig::resolve(owner);
  if(!entity||player_rig::word(entity+0x38)!=owner||!(player_rig::word(entity+0x10c)&1))return;
  if(player_rig::word(address+4)!=asset||player_rig::word(address+0x20)!=index||player_rig::word(address+0x24)!=owner||player_rig::word(address+0x1c)!=1)return;
  const auto definition=melee_owned_source::resident(player_rig::word(gameBase+0x15f4dfc),asset);
  if(!definition||player_rig::word(definition)!=gameBase+0x135807c)return;
  amalur::NativeCastDefinition d;
  d.listeners=player_rig::word(definition+0xc);d.script=player_rig::word(definition+0x94);d.kind=player_rig::word(definition+0x1a0);
  d.field1bc=player_rig::word(definition+0x1bc);d.field1f8=player_rig::word(definition+0x1f8);d.field1fc=player_rig::word(definition+0x1fc);
  d.field200=player_rig::word(definition+0x200);d.field208=player_rig::word(definition+0x208);d.field20c=player_rig::word(definition+0x20c);
  const auto list=player_rig::word(definition+8),listener=list?player_rig::word(list):0;
  if(!listener||player_rig::word(listener+4)!=definition||player_rig::word(player_rig::word(listener)+4)!=gameBase+0xbb4eb0)return;
  const auto script=melee_owned_source::resident(player_rig::word(gameBase+0x15f4d34),d.script);
  if(!script||player_rig::word(script)!=gameBase+0x1331a6c)return;
  d.scriptBytes=player_rig::word(script+0x24);const auto bytes=player_rig::word(script+0x20);
  if(!bytes||(d.scriptBytes!=783&&d.scriptBytes!=1174))return;
  d.scriptHash=2166136261u;for(unsigned i=0;i<d.scriptBytes;++i)d.scriptHash=(d.scriptHash^*reinterpret_cast<const unsigned char*>(bytes+i))*16777619u;
  const auto currentOwner=player_rig::word(player+0x1ec);
  AcquireSRWLockExclusive(&lock);const auto serial=observation.created(address,owner,currentOwner,asset,d,result,owned,true);ReleaseSRWLockExclusive(&lock);
  if(serial)native_action_haptics::emit(amalur::ActionFeedbackKind::Cast,owner,serial);
 }__except(EXCEPTION_EXECUTE_HANDLER){}
}
}
