#pragma once
#include <cmath>
#include <cstdint>
namespace amalur {
// Input adapter for a native TOGGLE action, not an authoritative sneak-state
// reader. Manual input takes ownership. Never add a camera-height offset here.
class PhysicalCrouch {
    bool calibrated_{},low_{},owned_{},manualPrevious_{},manualSneak_{},blocked_{};
    float standing_{};
    unsigned center_{},session_{};
    uint64_t candidateSince_{},pulseUntil_{},last_{};
public:
    bool update(float height,bool tracked,bool gameplay,bool enabled,unsigned center,
                unsigned session,bool manual,uint64_t now){
        if(session_!=session){*this={};session_=session;}
        if(now<last_){candidateSince_=pulseUntil_=0;calibrated_=false;}
        const bool gap=last_&&now-last_>250;last_=now;
        if(gap)candidateSince_=pulseUntil_=0;
        if(!gameplay||!tracked||!std::isfinite(height)){
            candidateSince_=pulseUntil_=0;manualPrevious_=manual;return false;
        }
        if(manual&&!manualPrevious_){
            // The manual edge itself goes through unchanged. Release physical
            // ownership, and wait for standing before detecting another crouch.
            manualSneak_=!(owned_||manualSneak_);owned_=false;blocked_=true;
            candidateSince_=pulseUntil_=0;
        }
        manualPrevious_=manual;
        if(!calibrated_||center!=center_){
            standing_=height;center_=center;calibrated_=true;low_=false;
            candidateSince_=pulseUntil_=0;
            // Recenter defines a new upright height; restore an owned toggle
            // only in active gameplay, with no simultaneous manual action.
            if(owned_&&!manual){owned_=false;pulseUntil_=now+120;}
            return !manual&&now<pulseUntil_;
        }
        if(!enabled){
            candidateSince_=0;low_=false;
            if(owned_&&!manual){owned_=false;pulseUntil_=now+120;}
            return !manual&&now<pulseUntil_;
        }
        const float drop=standing_-height;
        if(drop<=.15f)blocked_=false;
        if(manual){candidateSince_=0;return false;}
        const bool desired=low_?drop>.15f:drop>=.30f;
        if(desired!=low_){
            if(!candidateSince_)candidateSince_=now;
            if(now-candidateSince_>=150){
                low_=desired;candidateSince_=0;
                if(low_&&!blocked_&&!manualSneak_){owned_=true;pulseUntil_=now+120;}
                else if(!low_&&owned_){owned_=false;pulseUntil_=now+120;}
            }
        }else candidateSince_=0;
        return now<pulseUntil_;
    }
};
}
