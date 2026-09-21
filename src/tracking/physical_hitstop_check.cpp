#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
uintptr_t gameBase{};
bool hook(void*,void*,void**,const char*){return false;}
#include "../diagnostic/physical_hitstop.hpp"
static unsigned calls;
static uint32_t __fastcall fake(void* self,void*,uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e,uint32_t f){
 if(self!=reinterpret_cast<void*>(123)||a!=0x3f800000||b!=2||c!=3||d!=4||e!=5||f!=1)std::abort();++calls;return 0x12345678;
}
static void check(bool b){if(!b)std::abort();}
static uint32_t invoke(uintptr_t caller){return physical_hitstop::dispatch(caller,reinterpret_cast<void*>(123),0x3f800000,2,3,4,5,1);}
int main(){using namespace physical_hitstop;original=reinterpret_cast<Add>(&fake);
 check(invoke(0xb9e615)==0x12345678&&calls==1);
 auto old=beginLocalPhysical(7,7,true);
 check(invoke(0xb9e615)==0xffffffff&&invoke(0xba17a5)==0xffffffff&&calls==1);
 check(invoke(0xb9e614)==0x12345678&&invoke(0x677c30)==0x12345678&&calls==3);
 auto nested=beginLocalPhysical(8,7,true);check(invoke(0xb9e615)==0x12345678);endLocalPhysical(nested);
 check(invoke(0xb9e615)==0xffffffff);endLocalPhysical(old);check(!scopedOwner);
 old=beginLocalPhysical(7,7,false);check(invoke(0xb9e615)==0x12345678);endLocalPhysical(old);
 // Same finally pattern required at the native resolver integration site.
 old=beginLocalPhysical(7,7,true);__try{__try{RaiseException(0xe0123456,0,0,nullptr);}__finally{endLocalPhysical(old);}}__except(EXCEPTION_EXECUTE_HANDLER){}
 check(!scopedOwner&&invoke(0xba17a5)==0x12345678);
 unsigned char instruction[8]{0xe8};auto address=reinterpret_cast<uintptr_t>(instruction);int32_t delta=19;memcpy(instruction+1,&delta,4);
 check(callMatches(address,address+24));check(!callMatches(address,address+23));instruction[0]=0x90;check(!callMatches(address,address+24));
 puts("PASS: exact hit-stop caller scope, unrelated/foreign forwarding, raw x86 arguments, cleanup and signature checks");
}
