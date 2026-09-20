#pragma once
#include "melee_lifetime.hpp"

namespace amalur {
struct MeleeObservedRuntime {
    uintptr_t address{};
    uint64_t generation{};
    bool valid{};
};

// Observe one explicitly requested factory call. Native reset invalidates a
// token BEFORE asset-release or script callbacks can reenter. Only the first
// constructor in this request can claim the object; nested constructors cannot
// replace its identity. Caller serializes every operation, including finish.
template<size_t Capacity>
class MeleeConstructionObservation {
public:
    MeleeLifetimeRegistry<Capacity> lifetimes;
    bool begin(){
        if(pending_||!lifetimes.healthy())return false;
        pending_=true;entered_=completed_=success_=false;
        address_=0;generation_=0;resets_=0;
        return true;
    }
    bool enter(uintptr_t address){
        if(!pending_||entered_||!address)return false;
        entered_=true;address_=address;return true;
    }
    void reset(uintptr_t address){
        auto previous=lifetimes.current(address);
        if(previous)lifetimes.retire(address,0,previous);
        if(!pending_||!entered_||address!=address_)return;
        // Exactly one reset is expected at be65b0+32. Another reset means the
        // constructor's object was destroyed/reinitialized during callbacks.
        if(resets_<2)++resets_;
        generation_=resets_==1&&!completed_?lifetimes.replace(address):0;
    }
    void complete(bool success){
        if(pending_&&entered_){completed_=true;success_=success;}
    }
    MeleeObservedRuntime finish(bool factorySucceeded){
        if(!pending_)return {};
        bool valid=factorySucceeded&&completed_&&success_&&resets_==1
            &&lifetimes.matches(address_,0,generation_);
        MeleeObservedRuntime result{address_,generation_,valid};
        if(!valid&&generation_)lifetimes.retire(address_,0,generation_);
        pending_=entered_=completed_=success_=false;
        address_=0;generation_=0;resets_=0;
        return result;
    }
private:
    uintptr_t address_{};
    uint64_t generation_{};
    unsigned resets_{};
    bool pending_{},entered_{},completed_{},success_{};
};
}
