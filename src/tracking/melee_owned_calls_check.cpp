// Real MinHook relocation against a synthetic executable image, never the game.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "../../third_party/minhook/include/MinHook.h"
inline uintptr_t gameBase{};
namespace player_rig {
inline uintptr_t resolve(uint32_t){std::abort();}
inline uint32_t word(uintptr_t){std::abort();}
}
#include "../diagnostic/melee_owned_calls.hpp"
void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
void __fastcall replacement(void*,void*,uint32_t){}
int main(){
    static_assert(melee_native::customContactEnabled);
    gameBase=reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr,0xc00000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    check(gameBase!=0,"allocate synthetic executable image");
    const unsigned char reserve[]{0x53,0x8b,0x5c,0x24,0x08,0x56,0x8b,0xf1};
    const unsigned char bind[]{0x8b,0x51,0x28,0x53,0x33,0xdb,0x33,0xc0};
    const unsigned char create[]{0x8b,0x44,0x24,0x04,0x53,0x8b,0x18,0x55};
    const unsigned char erase[]{0x53,0x8d,0x59,0x24,0x8b,0x4b,0x04,0x33,0xc0};
    std::memcpy(reinterpret_cast<void*>(gameBase+0xb837e0),reserve,sizeof(reserve));
    std::memcpy(reinterpret_cast<void*>(gameBase+0xb41490),bind,sizeof(bind));
    std::memcpy(reinterpret_cast<void*>(gameBase+0xbeb400),create,sizeof(create));
    auto target=reinterpret_cast<void*>(gameBase+0xb80020);std::memcpy(target,erase,sizeof(erase));
    check(MH_Initialize()==MH_OK,"initialize real MinHook");
    melee_native::OwnedCalls calls;
    check(MH_CreateHook(target,reinterpret_cast<void*>(&replacement),reinterpret_cast<void**>(&calls.eraseTrampoline))==MH_OK,"create real relocated trampoline");
    check(MH_EnableHook(target)==MH_OK,"enable synthetic detour");
    check(std::memcmp(reinterpret_cast<void*>(calls.eraseTrampoline),erase,sizeof(erase))!=0,"old nine-byte validation rejects actual trampoline");
    check(calls.eraseTrampolineValid(),"copied instructions and jump-back verified");
    calls.lifetimeObserversReady=calls.onNativeUpdateThread=melee_native::ready=true;
    calls.beginObservation=[](){return true;};calls.finishObservation=[](bool){return amalur::MeleeObservedRuntime{};};
    calls.matchesObservation=[](uintptr_t,uint64_t){return true;};
    calls.claimKey=[](uintptr_t,uint32_t)->uint64_t{return 1;};calls.matchesKey=[](uintptr_t,uint32_t,uint64_t){return true;};
    check(calls.enabled(),"complete native-call gate accepts real trampoline");
    calls.onNativeUpdateThread=false;check(!calls.enabled(),"wrong-thread gate still rejects");calls.onNativeUpdateThread=true;
    unsigned char copy[12];std::memcpy(copy,reinterpret_cast<void*>(calls.eraseTrampoline),sizeof(copy));
    auto real=calls.eraseTrampoline;calls.eraseTrampoline=reinterpret_cast<melee_native::OwnedCalls::EraseFunction>(static_cast<void*>(copy));
    check(!calls.eraseTrampolineValid(),"wrong jump destination rejected");
    calls.eraseTrampoline=real;
    check(MH_DisableHook(target)==MH_OK&&MH_RemoveHook(target)==MH_OK&&MH_Uninitialize()==MH_OK,"remove synthetic hook");
    VirtualFree(reinterpret_cast<void*>(gameBase),0,MEM_RELEASE);
    std::puts("PASS: real MinHook seven-byte relocation, complete enabled gate, wrong thread and wrong continuation");
}
