#pragma once
#include "melee_creation.hpp"
#include "melee_context_scope.hpp"

namespace amalur {
// Transactional single-base backend shared by the native integration and tests.
// Environment supplies a retained source, current equipment, observed native
// calls, synchronized runtime release and a persistent fault latch.
template<class Environment>
class MeleeOwnedBackend {
    using Access=typename Environment::Access;
public:
    explicit MeleeOwnedBackend(Environment& environment):env_(environment),creation_(env_.calls){}
    bool supported(const MeleeContextRecipe& recipe)const{
        return !unsafe_&&recipe.baseAsset==recipe.selectedAsset&&env_.supported(recipe);
    }
    MeleeKeyToken reserve(uint32_t owner){
        part_=env_.part();owner_=owner;
        for(unsigned i=0;i<4097;++i){
            auto key=env_.nextKey();if(!key)break;
            auto before=creation_.record(part_,key);
            if(!before.tableValid)break;
            if(before.address)continue;
            if(creation_.reserve(part_,owner,key)){
                key_={key,env_.calls.keySerial(part_,key)};
                if(key_)return key_;
            }
            // A native allocation may have completed before post-validation
            // failed. Only an empty, observed record can be rolled back.
            auto after=creation_.record(part_,key);
            if(after.tableValid&&creation_.empty(after.address)){
                if(env_.calls.owns(part_,key)||env_.calls.claim(part_,key))
                    creation_.erase(part_,owner,key);
            }
            break;
        }
        env_.fault("key reservation failed");return {};
    }
    MeleeRuntimeToken create(uint32_t owner,uint32_t asset){
        if(runtime_||owner!=owner_)return {};
        candidate_=creation_.create(env_.pool(),owner,asset,env_.target());
        if(!candidate_.valid){
            env_.fault("runtime creation/observation failed");return {};
        }
        asset_=asset;
        runtime_={candidate_.index,env_.calls.observedSerial(candidate_.address)};
        if(!runtime_)env_.fault("runtime lifetime vanished after creation");
        return runtime_;
    }
    bool bind(uint32_t owner,MeleeKeyToken key,MeleeRuntimeToken base,MeleeRuntimeToken selected){
        if(owner!=owner_||!sameKey(key)||!sameRuntime(base)||!sameRuntime(selected)||!runtimeCurrent())return false;
        if(creation_.bind(part_,owner,key.key,base.index))return true;
        auto record=creation_.record(part_,key.key);
        bool detached=record.tableValid&&(!record.address||creation_.empty(record.address));
        if(!detached)detached=creation_.unbind(part_,owner,key.key,base.index);
        unsafe_=!detached;
        env_.fault("runtime binding failed");return false;
    }
    bool matches(uint32_t owner,MeleeKeyToken key,MeleeRuntimeToken base,MeleeRuntimeToken selected)const{
        if(owner!=owner_||!sameKey(key)||!sameRuntime(base)||!sameRuntime(selected)
            ||!env_.calls.owns(part_,key.key)||!runtimeCurrent())return false;
        auto record=creation_.record(part_,key.key);
        return record.tableValid&&record.address&&env_.calls.read(record.address)==base.index
            &&env_.calls.read(record.address+4)==0&&env_.calls.read(record.address+12)==0;
    }
    bool unbind(uint32_t owner,MeleeKeyToken key,MeleeRuntimeToken base,MeleeRuntimeToken selected){
        if(owner!=owner_||!sameKey(key)||!sameRuntime(base)||!sameRuntime(selected))return false;
        // Native removal invalidates the key before callbacks and may already
        // have retired its runtime. Never modify its replacement key.
        if(!env_.calls.owns(part_,key.key))return true;
        bool result=creation_.unbind(part_,owner,key.key,base.index);
        if(!result){unsafe_=true;env_.fault("owned key detachment failed");}
        return result;
    }
    bool release(MeleeRuntimeToken token){
        if(!sameRuntime(token)||unsafe_)return false;
        bool result=env_.release(candidate_.address,token.generation,token.index,asset_,owner_);
        runtime_={};
        if(!result)env_.fault("owned runtime retirement failed");
        return result;
    }
    bool erase(uint32_t owner,MeleeKeyToken key){
        if(owner!=owner_||!sameKey(key)||unsafe_)return false;
        bool result=!env_.calls.owns(part_,key.key)||creation_.erase(part_,owner,key.key);
        key_={};if(!result)env_.fault("owned key erasure failed");return result;
    }
    uintptr_t runtimeAddress()const{return candidate_.address;}
private:
    bool sameKey(MeleeKeyToken value)const{return value.key==key_.key&&value.generation==key_.generation&&bool(value);}
    bool sameRuntime(MeleeRuntimeToken value)const{return value.index==runtime_.index&&value.generation==runtime_.generation&&bool(value);}
    bool runtimeCurrent()const{
        if(!runtime_||!env_.current(candidate_.address,runtime_.generation))return false;
        auto pool=env_.pool(),table=env_.calls.read(pool+4),count=env_.calls.read(pool+8);
        return count<=1048576&&runtime_.index<count&&table
            &&env_.calls.read(table+runtime_.index*4)==candidate_.address
            &&(env_.calls.read(candidate_.address+0x1c)&1)
            &&env_.calls.read(candidate_.address+0x20)==runtime_.index
            &&env_.calls.read(candidate_.address+0x24)==owner_
            &&env_.calls.read(candidate_.address+4)==asset_;
    }
    Environment& env_;
    MeleeCreation<Access> creation_;
    typename MeleeCreation<Access>::Candidate candidate_{};
    uintptr_t part_{};
    uint32_t owner_{},asset_{};
    MeleeKeyToken key_{};
    MeleeRuntimeToken runtime_{};
    bool unsafe_{};
};
}
