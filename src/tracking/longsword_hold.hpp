#pragma once
#include <mgs5vr/core.hpp>
#include <cmath>
namespace amalur {
// No forward boundary or sword pitch requirement: behind-shoulder/overhead
// holds with the blade tilted backwards or down are intentional charge poses.
inline bool longswordChargePose(mgs5vr::Vec3 relativeHand){
    return std::isfinite(relativeHand.x)&&std::isfinite(relativeHand.y)&&std::isfinite(relativeHand.z)
        &&relativeHand.y>=-.35f;
}
// Charge stability uses hand displacement in head-relative metres, not noisy
// blade-tip velocity. Eight centimetres allows normal tremor while held.
class LongswordHold {
public:
    void reset(){*this=LongswordHold{};}
    bool raising()const{return raising_;}
    bool risingNow()const{return risingNow_;}
    bool sample(mgs5vr::Vec3 p,uint64_t tick,unsigned generation,bool eligible){
        if(!eligible||!tick||!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)){reset();return false;}
        if(!last_||generation!=generation_||tick<last_||tick-last_>100){
            reset();anchor_=previous_=lastPosition_=p;last_=window_=tick;generation_=generation;return false;
        }
        if(tick==last_)return stable_;
        const auto dy=p.y-lastPosition_.y;const auto elapsed=tick-last_;
        risingNow_=elapsed&&dy*1000.f/float(elapsed)>.25f;
        const bool lowering=elapsed&&dy*1000.f/float(elapsed)<-.25f;
        last_=tick;lastPosition_=p;
        if(tick-window_>=100){raising_=p.y-previous_.y>.035f;previous_=p;window_=tick;}
        if(lowering)raising_=false; // release immediately, not after100ms
        const auto d=p-anchor_;
        stable_=mgs5vr::dot(d,d)<=.08f*.08f;
        if(!stable_)anchor_=p;
        return stable_;
    }
private:
    mgs5vr::Vec3 anchor_{},previous_{},lastPosition_{};uint64_t last_{},window_{};unsigned generation_{};bool stable_{},raising_{},risingNow_{};
};
}
