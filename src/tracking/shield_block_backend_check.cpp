#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <cstdio>
inline uintptr_t gameBase{};
inline unsigned calls{},emits{};
namespace player_rig {
 inline std::atomic<void*> player{};
 inline uintptr_t entity{};
 inline uintptr_t word(uintptr_t a){return *reinterpret_cast<uintptr_t*>(a);}
 inline uintptr_t resolve(uint32_t){return entity;}
}
namespace amalur {enum class ActionFeedbackKind {Block};}
namespace native_action_haptics {void emit(amalur::ActionFeedbackKind,uint32_t,uint32_t){++emits;}}
bool hook(unsigned char*,void*,void**,const char*){return false;}
#include "../diagnostic/shield_block_feedback.hpp"
uintptr_t __fastcall pass(void*,void*,const uint32_t*,const void*){++calls;return 0x1234;}
int main(){
 alignas(8) unsigned char storage[128]{},value[40]{};uint32_t keys[]{amalur::attacksBlockedHash};
 auto part=reinterpret_cast<uintptr_t>(storage);
 *reinterpret_cast<uintptr_t*>(storage+0x30)=reinterpret_cast<uintptr_t>(keys);
 *reinterpret_cast<uint32_t*>(storage+0x34)=1;
 *reinterpret_cast<uintptr_t*>(storage+0x40)=reinterpret_cast<uintptr_t>(value);
 *reinterpret_cast<uint32_t*>(value+0x20)=6;*reinterpret_cast<int32_t*>(value+0x18)=5;
 uint32_t n{};assert(shield_block_feedback::counter(part,n)&&n==5);
 *reinterpret_cast<uint32_t*>(storage+0x34)=0;assert(shield_block_feedback::counter(part,n)&&n==0);
 *reinterpret_cast<uint32_t*>(storage+0x34)=4097;assert(!shield_block_feedback::counter(part,n));
 shield_block_feedback::original=reinterpret_cast<shield_block_feedback::SetVariable>(&pass);
 assert(shield_block_feedback::setVariable(storage,nullptr,keys,value)==0x1234&&calls==1&&emits==0);
 assert(shield_block_feedback::setVariable(storage,nullptr,nullptr,nullptr)==0x1234&&calls==2&&emits==0);
 puts("PASS native block table parsing, missing-key zero, corrupt bound rejection, non-script pass-through exactly once");
}
