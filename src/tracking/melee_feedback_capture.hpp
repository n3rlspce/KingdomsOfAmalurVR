#pragma once
#include "melee_swing_event.hpp"

namespace amalur {
// A renewing budget, not a session-wide cap: later weapon tests remain visible.
struct MeleeFeedbackBudget {
    uint64_t epoch{};
    unsigned emitted{},dropped{};
    bool allow(uint64_t now) {
        if(!epoch||now<epoch||now-epoch>=1000){epoch=now;emitted=0;}
        if(emitted>=24){++dropped;return false;}
        ++emitted;return true;
    }
};
struct MeleeFeedbackInbox {
    MeleeSwingEvent latest[2]{};
    bool accept(const MeleeSwingEvent& event,uint64_t now) {
        if(event.hand>=2||!event.owner||!event.weapon||!event.serial||!event.tick
            ||event.tick>now||now-event.tick>300||!mgs5vr::valid(event.weaponPose))return false;
        auto& old=latest[event.hand];
        if(old.owner==event.owner&&old.weapon==event.weapon&&old.generation==event.generation
            &&(event.tick<old.tick||event.serial<=old.serial))return false;
        old=event;return true;
    }
};
}
