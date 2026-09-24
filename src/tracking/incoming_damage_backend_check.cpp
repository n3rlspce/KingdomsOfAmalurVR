#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <cstdio>
inline uintptr_t gameBase{};
inline unsigned calls{},emits{};inline int32_t loss{};
inline unsigned char playerBytes[0x200]{},entityBytes[0x120]{},healthBytes[0x50]{};
namespace player_rig {
 inline std::atomic<void*> player{};
 inline uintptr_t word(uintptr_t a){return *reinterpret_cast<uintptr_t*>(a);}
 inline uintptr_t resolve(uint32_t owner){return owner==0x12345?reinterpret_cast<uintptr_t>(entityBytes):0;}
}
namespace melee_native {
 using Resolve=void(__thiscall*)(void*,uint32_t,void*,const float*,const float*,int32_t,uint32_t);
 struct HitArray {void* data{};uint32_t count{},capacity{};int16_t allocator{},flags{};};
 inline bool ready{};
 inline bool signature(uintptr_t,const unsigned char*,size_t){return false;}
 inline uintptr_t component(uint32_t owner,unsigned slot){return owner==0x12345&&slot==1?reinterpret_cast<uintptr_t>(healthBytes):0;}
}
namespace amalur {enum class ActionFeedbackKind {Damage};}
namespace native_action_haptics {void emit(amalur::ActionFeedbackKind,uint32_t owner,uint64_t id){assert(owner==0x12345&&(id&(uint64_t(1)<<62)));++emits;}}
bool hook(unsigned char*,void*,void**,const char*){return false;}
#include "../diagnostic/incoming_damage_feedback.hpp"
void __fastcall pass(void* self,void*,uint32_t flags,void* hits,const float* from,const float* to,int32_t talent,uint32_t key){
 assert(self==playerBytes&&flags==77&&hits&&from==nullptr&&to==nullptr&&talent==9&&key==88);++calls;
 *reinterpret_cast<int32_t*>(healthBytes+0x48)-=loss;
}
int main(){
 *reinterpret_cast<uintptr_t*>(playerBytes)=0x1359f14;*reinterpret_cast<uint32_t*>(playerBytes+0x1ec)=0x12345;
 *reinterpret_cast<uint32_t*>(entityBytes+0x38)=0x12345;*reinterpret_cast<uint32_t*>(entityBytes+0x10c)=1;
 *reinterpret_cast<int32_t*>(healthBytes+0x48)=100;player_rig::player=playerBytes;
 unsigned char records[0xe0]{};*reinterpret_cast<uint32_t*>(records+0x14)=0x10012345;
 melee_native::HitArray hits{records,1,2};
 incoming_damage_feedback::original=reinterpret_cast<melee_native::Resolve>(&pass);
 auto invoke=[&](){incoming_damage_feedback::resolve(playerBytes,nullptr,77,&hits,nullptr,nullptr,9,88);};
 loss=10;invoke();assert(calls==1&&emits==1);
 loss=0;invoke();assert(calls==2&&emits==1);
 loss=-5;invoke();assert(calls==3&&emits==1);
 *reinterpret_cast<uint32_t*>(records+0x14)=0x30012345;loss=10;invoke();assert(calls==4&&emits==1);
 hits.count=3;invoke();assert(calls==5&&emits==1);
 hits.count=1;*reinterpret_cast<uint32_t*>(records+0x14)=0x10012345;
 player_rig::player=nullptr;invoke();assert(calls==6&&emits==1);
 puts("incoming damage native adapter check PASS");
}
