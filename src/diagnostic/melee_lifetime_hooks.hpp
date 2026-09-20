#pragma once
#include "melee_owned_calls.hpp"

namespace melee_lifetime_hooks {
using Initialize=int(__thiscall*)(void*,const uint32_t*,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
using Reset=void(__thiscall*)(void*);
inline Initialize originalInitialize{};
inline Reset originalReset{};
inline melee_native::OwnedCalls::EraseFunction originalErase{};
inline amalur::MeleeConstructionObservation<128> observation;
inline amalur::MeleeLifetimeRegistry<128> keys;
inline SRWLOCK lock=SRWLOCK_INIT;
inline DWORD requestThread{};
inline bool installed{};

inline bool begin(){
    AcquireSRWLockExclusive(&lock);
    bool accepted=installed&&observation.begin();
    if(accepted)requestThread=GetCurrentThreadId();
    ReleaseSRWLockExclusive(&lock);
    return accepted;
}
inline amalur::MeleeObservedRuntime finish(bool success){
    AcquireSRWLockExclusive(&lock);
    auto result=requestThread==GetCurrentThreadId()?observation.finish(success):amalur::MeleeObservedRuntime{};
    if(requestThread==GetCurrentThreadId())requestThread=0;
    ReleaseSRWLockExclusive(&lock);
    return result;
}
inline bool matches(uintptr_t address,uint64_t generation){
    AcquireSRWLockShared(&lock);
    bool current=installed&&observation.lifetimes.matches(address,0,generation);
    ReleaseSRWLockShared(&lock);
    return current;
}
inline uint64_t claimKey(uintptr_t part,uint32_t key){
    AcquireSRWLockExclusive(&lock);
    auto generation=installed&&!keys.current(part,key)?keys.replace(part,key):0;
    ReleaseSRWLockExclusive(&lock);return generation;
}
inline bool matchesKey(uintptr_t part,uint32_t key,uint64_t generation){
    AcquireSRWLockShared(&lock);
    bool current=installed&&keys.matches(part,key,generation);
    ReleaseSRWLockShared(&lock);return current;
}
// Lock only registry operations, never the native release or its callbacks.
struct RuntimeRegistry {
    bool matches(uintptr_t address,uint32_t key,uint64_t generation)const{
        return key==0&&melee_lifetime_hooks::matches(address,generation);
    }
    bool retire(uintptr_t address,uint32_t key,uint64_t generation){
        if(key)return false;
        AcquireSRWLockExclusive(&lock);
        bool result=observation.lifetimes.retire(address,0,generation);
        ReleaseSRWLockExclusive(&lock);return result;
    }
};
inline void __fastcall erase(void* self,void*,uint32_t key){
    AcquireSRWLockExclusive(&lock);
    auto part=reinterpret_cast<uintptr_t>(self);
    auto generation=keys.current(part,key);
    if(generation)keys.retire(part,key,generation);
    ReleaseSRWLockExclusive(&lock);
    originalErase(self,key);
}
inline void __fastcall reset(void* self,void*){
    AcquireSRWLockExclusive(&lock);
    observation.reset(reinterpret_cast<uintptr_t>(self));
    ReleaseSRWLockExclusive(&lock);
    // Never hold a lock while native destructors/script callbacks run.
    originalReset(self);
}
inline int __fastcall initialize(void* self,void*,const uint32_t* asset,uint32_t owner,
    uint32_t target,uint32_t related,uint32_t index,uint32_t extra){
    AcquireSRWLockExclusive(&lock);
    bool captured=requestThread==GetCurrentThreadId()&&observation.enter(reinterpret_cast<uintptr_t>(self));
    ReleaseSRWLockExclusive(&lock);
    int result=1;
    __try{
        result=originalInitialize(self,asset,owner,target,related,index,extra);
    }__finally{
        if(captured){
            AcquireSRWLockExclusive(&lock);
            observation.complete(result==0);
            ReleaseSRWLockExclusive(&lock);
        }
    }
    return result;
}
inline bool install(){
    if constexpr(!melee_native::customContactEnabled){return false;}else{
    if(installed)return true;
    // Partial installation remains observational and cannot authorize creation.
    // A failed install is not retried over already-detoured bytes.
    if(originalInitialize||originalReset||originalErase)return false;
    const unsigned char initBytes[]{0x83,0xec,0x10,0x53,0x55,0x56,0x8b,0xf1};
    const unsigned char resetBytes[]{0x56,0x8b,0xf1,0x8b,0x46,0x04,0x57};
    const unsigned char eraseBytes[]{0x53,0x8d,0x59,0x24,0x8b,0x4b,0x04,0x33,0xc0};
    if(!melee_native::signature(0xbe65b0,initBytes,sizeof(initBytes))
        ||!melee_native::signature(0xbbfbf0,resetBytes,sizeof(resetBytes))
        ||!melee_native::signature(0xb80020,eraseBytes,sizeof(eraseBytes)))return false;
    if(!hook(reinterpret_cast<void*>(gameBase+0xbbfbf0),reinterpret_cast<void*>(&reset),
        reinterpret_cast<void**>(&originalReset),"Observe owned melee runtime reset"))return false;
    if(!hook(reinterpret_cast<void*>(gameBase+0xbe65b0),reinterpret_cast<void*>(&initialize),
        reinterpret_cast<void**>(&originalInitialize),"Observe owned melee runtime creation"))return false;
    if(!hook(reinterpret_cast<void*>(gameBase+0xb80020),reinterpret_cast<void*>(&erase),
        reinterpret_cast<void**>(&originalErase),"Observe owned melee key removal"))return false;
    installed=true;return true;
    }
}
inline void connect(melee_native::OwnedCalls& calls){
    calls.lifetimeObserversReady=installed;
    calls.beginObservation=&begin;calls.finishObservation=&finish;
    calls.matchesObservation=&matches;
    calls.claimKey=&claimKey;calls.matchesKey=&matchesKey;calls.eraseTrampoline=originalErase;
}
}
