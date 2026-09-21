#pragma once
#include <cstdint>
#include <cstring>
#include <intrin.h>
namespace physical_hitstop {
// All six parameters occupy four-byte x86 stack slots. Preserve their exact
// bits (including float arguments) when forwarding. Native function ret18.
using Add=uint32_t(__thiscall*)(void*,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
inline Add original{};
inline thread_local uint32_t scopedOwner{};
inline thread_local uint64_t suppressedCalls{};
inline uint32_t beginLocalPhysical(uint32_t owner,uint32_t localOwner,bool eligible){
 const auto previous=scopedOwner;scopedOwner=eligible&&owner&&owner==localOwner?owner:0;return previous;
}
inline void endLocalPhysical(uint32_t previous){scopedOwner=previous;}
inline bool suppress(uintptr_t returnRva){return scopedOwner&&(returnRva==0xb9e615||returnRva==0xba17a5);}
inline uint32_t dispatch(uintptr_t returnRva,void* self,uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e,uint32_t f){
 if(suppress(returnRva)){++suppressedCalls;return 0xffffffffu;} // exact native invalid-clock sentinel; resolver ignores it
 return original(self,a,b,c,d,e,f);
}
__declspec(noinline) inline uint32_t __fastcall add(void* self,void*,uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e,uint32_t f){
 const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
 return dispatch(caller-gameBase,self,a,b,c,d,e,f);
}
inline bool callMatches(uintptr_t address,uintptr_t target){
 if(*reinterpret_cast<const unsigned char*>(address)!=0xe8)return false;
 int32_t offset;memcpy(&offset,reinterpret_cast<const void*>(address+1),sizeof(offset));
 return address+5+offset==target;
}
inline bool install(){
 if(original)return true;
 __try{
  const auto target=gameBase+0x679880;
  const unsigned char bytes[]{0x56,0x8b,0xf1,0x8b,0x46,0x04,0x85,0xc0,0x78,0x45,0x3b,0x46,0x0c,0x7d,0x40};
  if(memcmp(reinterpret_cast<const void*>(target),bytes,sizeof(bytes))
   ||!callMatches(gameBase+0xb9e610,target)||!callMatches(gameBase+0xba17a0,target))return false;
  return hook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(&add),reinterpret_cast<void**>(&original),"Scoped local physical hit-stop suppression");
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
