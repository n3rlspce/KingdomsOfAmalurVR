#pragma once
#include <atomic>
#include <cstdint>

namespace amalur {
// The native dodge owns facing while its directional animation runs. This is a
// provisional bounded lease until the native dodge-state transition is located;
// it is not a measured animation duration. Head/camera tracking is unaffected.
class DodgeFacingLease {
    bool wasA_{}; // observe is serialized by the XInput hook's lock.
    std::atomic<uint64_t> until_{0};
public:
    static constexpr uint64_t durationMs=700;
    void observe(bool active,bool a,bool abilityModifier,uint64_t now,bool interactionTarget=false){
        if(!active){wasA_=false;until_.store(0);return;}
        if(a&&!wasA_&&!abilityModifier)until_.store(interactionTarget?0:now+durationMs);
        // Releasing the ability modifier while A is held is not a new dodge.
        wasA_=a;
    }
    bool suppress(uint64_t now)const{return now<until_.load();}
};
inline DodgeFacingLease dodgeFacing;

// set_facing writes the same PartMotion rotation requests used by ordinary
// locomotion. Yield that simulation ownership for non-neutral analog movement,
// including a short release grace for the game's deceleration transition.
class LocomotionFacingLease {
    std::atomic<uint64_t> until_{0};
public:
    static constexpr uint64_t releaseGraceMs=250;
    void observe(bool active,int16_t x,int16_t y,uint64_t now){
        if(!active){until_.store(0);return;}
        if(x||y)until_.store(now+releaseGraceMs);
    }
    bool suppress(uint64_t now)const{return now<until_.load();}
};
inline LocomotionFacingLease locomotionFacing;
}
