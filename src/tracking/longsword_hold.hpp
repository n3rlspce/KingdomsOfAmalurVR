#pragma once
#include <mgs5vr/core.hpp>
#include <cmath>
namespace amalur {
// Charge stability uses hand displacement in head-relative metres, not noisy
// blade-tip velocity. Eight centimetres allows normal tremor while held.
class LongswordHold {
public:
    void reset(){*this=LongswordHold{};}
    bool raising()const{return raising_;}
    bool sample(mgs5vr::Vec3 p,uint64_t tick,unsigned generation,bool eligible){
        if(!eligible||!tick||!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)){reset();return false;}
        if(!last_||generation!=generation_||tick<last_||tick-last_>100){
            reset();anchor_=previous_=p;last_=window_=tick;generation_=generation;return false;
        }
        if(tick==last_)return stable_;
        last_=tick;
        if(tick-window_>=100){raising_=p.y-previous_.y>.035f;previous_=p;window_=tick;}
        const auto d=p-anchor_;
        stable_=mgs5vr::dot(d,d)<=.08f*.08f;
        if(!stable_)anchor_=p;
        return stable_;
    }
private:
    mgs5vr::Vec3 anchor_{},previous_{};uint64_t last_{},window_{};unsigned generation_{};bool stable_{},raising_{};
};
}
