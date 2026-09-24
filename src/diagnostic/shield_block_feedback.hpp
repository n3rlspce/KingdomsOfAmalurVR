#pragma once
#include <intrin.h>
#include "../tracking/shield_block_feedback.hpp"
// Include at global scope after player_rig and native_action_haptics.
namespace shield_block_feedback {
using SetVariable=uintptr_t(__thiscall*)(void*,const uint32_t*,const void*);
inline SetVariable original{};
inline bool installed{};
inline bool local(uintptr_t part,uint32_t& owner){
 const auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());
 if(!player||(player_rig::word(player)!=gameBase+0x1359f14&&player_rig::word(player)!=gameBase+0x1359e94))return false;
 owner=uint32_t(player_rig::word(player+0x1ec));const auto entity=player_rig::resolve(owner);
 return owner&&entity&&player_rig::word(entity+0x38)==owner&&(player_rig::word(entity+0x10c)&1)
  &&player_rig::word(entity+0x3c+35*4)==part&&player_rig::word(part+0x18)==owner
  &&player_rig::word(part+0x1c)==35&&(player_rig::word(part+0x20)&1);
}
inline bool counter(uintptr_t part,uint32_t& value){
 const auto count=player_rig::word(part+0x34),keys=player_rig::word(part+0x30),values=player_rig::word(part+0x40);
 if(count>4096||(count&&(!keys||!values)))return false;
 bool found=false;
 for(unsigned i=0;i<count;++i)if(player_rig::word(keys+i*4)==amalur::attacksBlockedHash){
  if(found)return false;found=true;const auto v=values+i*40;
  if(!amalur::blockCounterValue(uint32_t(player_rig::word(v+0x20)),reinterpret_cast<void*>(v+0x18),value))return false;
 }
 // ACTOR.get_variable A3BFD6 -> 6A18E0(0) returns integer zero on a missing key.
 if(!found)value=0;
 return player_rig::word(part+0x34)==count&&player_rig::word(part+0x30)==keys&&player_rig::word(part+0x40)==values;
}
inline uintptr_t __fastcall setVariable(void* self,void*,const uint32_t* key,const void* variant){
 const bool scriptSetter=reinterpret_cast<uintptr_t>(_ReturnAddress())==gameBase+0xa3beb7;
 uint32_t owner{},before{},requested{};bool observe=false;
 __try {
  if(scriptSetter&&key&&*key==amalur::attacksBlockedHash&&variant&&local(reinterpret_cast<uintptr_t>(self),owner)){
   const auto v=reinterpret_cast<uintptr_t>(variant);
   observe=counter(reinterpret_cast<uintptr_t>(self),before)
    &&amalur::blockCounterValue(uint32_t(player_rig::word(v+0x20)),reinterpret_cast<void*>(v+0x18),requested);
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){observe=false;}
 // Observe an existing native write. Always execute it exactly once, unchanged.
 const auto result=original(self,key,variant);
 __try {
  uint32_t currentOwner{},after{};
  if(observe&&local(reinterpret_cast<uintptr_t>(self),currentOwner)&&currentOwner==owner
   &&counter(reinterpret_cast<uintptr_t>(self),after)
   &&amalur::confirmedShieldBlock(before,requested,after,true,scriptSetter))
    native_action_haptics::emit(amalur::ActionFeedbackKind::Block,owner,after);
 }__except(EXCEPTION_EXECUTE_HANDLER){}
 return result;
}
inline void install(){
 if(installed)return;
 __try {
  constexpr unsigned char prefix[]{0x53,0x55,0x8b,0x6c,0x24,0x0c,0x56,0x57,0x8b,0xf9};
  const auto call=gameBase+0xa3beb2;
  if(memcmp(reinterpret_cast<void*>(gameBase+0xb27a50),prefix,sizeof(prefix))
   ||*reinterpret_cast<unsigned char*>(call)!=0xe8
   ||call+5+*reinterpret_cast<int32_t*>(call+1)!=gameBase+0xb27a50)return;
  installed=hook(reinterpret_cast<unsigned char*>(gameBase+0xb27a50),reinterpret_cast<void*>(&setVariable),
    reinterpret_cast<void**>(&original),"Confirmed local shield block statistic");
 }__except(EXCEPTION_EXECUTE_HANDLER){}
}
}
