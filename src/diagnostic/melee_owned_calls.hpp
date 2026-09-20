#pragma once
#include "melee_native.hpp"
#include "../tracking/melee_creation.hpp"
#include "../tracking/melee_observation.hpp"

namespace melee_native {
// Low-level owned-context calls only. This does not authorize a contact or
// establish ownership. A backend still needs retained source assets, equipment
// validation and native lifetime observation before invoking this interface.
struct OwnedCalls {
    using EraseFunction=void(__thiscall*)(void*,uint32_t);
    bool lifetimeObserversReady{};
    bool onNativeUpdateThread{};
    bool (*beginObservation)(){};
    amalur::MeleeObservedRuntime (*finishObservation)(bool){};
    bool (*matchesObservation)(uintptr_t,uint64_t){};
    amalur::MeleeObservedRuntime lastCreation{};
    uint64_t (*claimKey)(uintptr_t,uint32_t){};
    bool (*matchesKey)(uintptr_t,uint32_t,uint64_t){};
    EraseFunction eraseTrampoline{};
    uintptr_t keyPart{};
    uint32_t ownedKey{};
    uint64_t keyGeneration{};
    bool eraseTrampolineValid()const{
        // MinHook copies whole instructions through byte 7, then jumps back.
        // Bytes 7..8 of the original function are NOT present in the trampoline.
        if(!eraseTrampoline)return false;
        auto bytes=reinterpret_cast<const unsigned char*>(eraseTrampoline);
        const unsigned char prefix[]{0x53,0x8d,0x59,0x24,0x8b,0x4b,0x04};
        if(std::memcmp(bytes,prefix,sizeof(prefix))||bytes[7]!=0xe9)return false;
        int32_t displacement{};std::memcpy(&displacement,bytes+8,sizeof(displacement));
        return reinterpret_cast<uintptr_t>(bytes)+12+static_cast<uintptr_t>(displacement)==gameBase+0xb80027;
    }
    bool enabled()const{
        if constexpr(!customContactEnabled)return false;
        if(!ready||!lifetimeObserversReady||!onNativeUpdateThread
            ||!beginObservation||!finishObservation||!matchesObservation
            ||!claimKey||!matchesKey||!eraseTrampoline)return false;
        const unsigned char reserveBytes[]{0x53,0x8b,0x5c,0x24,0x08,0x56,0x8b,0xf1};
        const unsigned char bindBytes[]{0x8b,0x51,0x28,0x53,0x33,0xdb,0x33,0xc0};
        const unsigned char createBytes[]{0x8b,0x44,0x24,0x04,0x53,0x8b,0x18,0x55};
        return signature(0xb837e0,reserveBytes,sizeof(reserveBytes))
            &&signature(0xb41490,bindBytes,sizeof(bindBytes))
            &&eraseTrampolineValid()
            &&signature(0xbeb400,createBytes,sizeof(createBytes));
    }
    uint32_t read(uintptr_t address)const{return player_rig::word(address);}
    bool claim(uintptr_t part,uint32_t key){
        if(!enabled()||keyGeneration)return false;
        auto generation=claimKey(part,key);
        if(!generation)return false;
        keyPart=part;ownedKey=key;keyGeneration=generation;return true;
    }
    bool owns(uintptr_t part,uint32_t key)const{
        return part==keyPart&&key==ownedKey&&keyGeneration&&matchesKey
            &&matchesKey(part,key,keyGeneration);
    }
    uint64_t keySerial(uintptr_t part,uint32_t key)const{return owns(part,key)?keyGeneration:0;}
    uint64_t observedSerial(uintptr_t address)const{return observed(address)?lastCreation.generation:0;}
    bool observed(uintptr_t address)const{
        return lastCreation.valid&&lastCreation.address==address&&matchesObservation
            &&matchesObservation(address,lastCreation.generation);
    }
    void write(uintptr_t address,uint32_t value){if(enabled())*reinterpret_cast<uint32_t*>(address)=value;}
    void reserve(uintptr_t part,uint32_t key){
        if(!enabled())return;
        using Fn=int32_t(__thiscall*)(void*,const uint32_t*);
        reinterpret_cast<Fn>(gameBase+0xb837e0)(reinterpret_cast<void*>(part+0x24),&key);
    }
    void bind(uintptr_t part,uint32_t key,uint32_t index){
        if(!enabled())return;
        using Fn=void(__thiscall*)(void*,uint32_t,uint32_t);
        reinterpret_cast<Fn>(gameBase+0xb41490)(reinterpret_cast<void*>(part),key,index);
    }
    void erase(uintptr_t part,uint32_t key){
        if(!enabled())return;
        using Fn=void(__thiscall*)(void*,uint32_t);
        reinterpret_cast<Fn>(gameBase+0xb80020)(reinterpret_cast<void*>(part),key);
    }
    uint32_t create(uintptr_t pool,uint32_t asset,uint32_t owner,uint32_t target){
        lastCreation={};
        if(!enabled())return 0;
        if(!beginObservation())return 0;
        using Fn=uint32_t(__thiscall*)(void*,const uint32_t*,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
        // Matches b877b3-b877c7: asset ref, source, target, source, 0, 0, 0.
        uint32_t result=0;
        __try{
            result=reinterpret_cast<Fn>(gameBase+0xbeb400)(reinterpret_cast<void*>(pool),&asset,owner,target,owner,0,0,0);
        }__finally{
            lastCreation=finishObservation(result!=0);
        }
        // Preserve the returned slot even if observation failed: the caller
        // must quarantine it, never infer ownership from its index alone.
        return result;
    }
};
}
