#pragma once
#include <mgs5vr/core.hpp>
#include <cstdint>
namespace amalur {
struct MeleeSwingEvent {
    uint32_t owner{},weapon{},asset{};
    unsigned hand{},serial{},chainStep{},generation{};
    uint64_t tick{};
    mgs5vr::Pose weaponPose{};
};
// Gesture sequence only. Native weapon combo/talent selection remains separate.
struct MeleeSwingChain {
    uint32_t weapon{};unsigned generation{},step{};uint64_t last{};
    void reset(){weapon=0;generation=step=0;last=0;}
    unsigned advance(uint32_t current,unsigned center,uint64_t tick){
        if(!current||!tick){reset();return 0;}
        if(current!=weapon||center!=generation||tick<last||tick-last>900)step=0;
        // Simultaneous dual swings share a step instead of skipping one.
        if(current==weapon&&center==generation&&tick==last)return step;
        step=step%3+1;weapon=current;generation=center;last=tick;return step;
    }
};
struct MeleeSwingWindow {
    unsigned serial{};uint64_t until{};
    bool accept(unsigned next,uint64_t started,uint64_t now,bool continuous){
        if(!continuous){serial=next;until=0;return false;}
        if(next==serial)return false;
        serial=next;until=0;
        if(!next||!started||started>now||now-started>300)return false;
        until=started+450;return true;
    }
    bool active(uint64_t now)const{return until&&now<=until;}
};
}
