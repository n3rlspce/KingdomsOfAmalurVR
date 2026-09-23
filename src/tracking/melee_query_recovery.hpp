#pragma once
#include <cstdint>
namespace amalur {
// Only for exceptions before context creation after query storage cleaned up.
// Ownership/retirement faults never enter this recovery path.
class MeleeQueryRecovery {
    uint64_t since_{},last_{},failed_{};uintptr_t physics_{};uint32_t owner_{},weapon_{};
    unsigned retries_{};
public:
    void suspend(uint64_t now){failed_=now;since_=last_=0;}
    bool claim(uintptr_t physics,uint32_t owner,uint32_t weapon,uint64_t now,bool valid){
        if(!valid||!physics||!owner||!weapon||!now){since_=last_=0;return false;}
        if(physics!=physics_||owner!=owner_||weapon!=weapon_||!last_||now<last_||now-last_>100){
            physics_=physics;owner_=owner;weapon_=weapon;since_=now;
        }
        last_=now;
        if(retries_>=3||now<failed_||now-failed_<1000||now-since_<500)return false;
        ++retries_;since_=last_=0;return true;
    }
};
}
