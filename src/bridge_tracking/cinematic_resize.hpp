#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace amalur {
// Dimensions relative to the original cinematic screen. Neutral entry avoids
// carrying movement into a cutscene. Time-based growth feels equal at any FPS.
class CinematicResize {
    bool armed_{},dirty_{};
    uint64_t last_{};
public:
    bool update(float& scale,float left,float right,bool active,uint64_t now){
        const float dt=last_&&now>=last_?std::min(float(now-last_)*.001f,.05f):0.f;
        last_=now;
        if(!active||!std::isfinite(left)||!std::isfinite(right)){
            armed_=false;const bool save=dirty_;dirty_=false;return save;
        }
        const bool neutral=std::abs(left)<.2f&&std::abs(right)<.2f;
        if(!armed_){armed_=neutral;return false;}
        auto axis=[](float v){return std::copysign(std::clamp((std::abs(v)-.2f)/.8f,0.f,1.f),v);};
        const float input=std::clamp(axis(left)+axis(right),-1.f,1.f);
        if(input!=0){
            const float next=std::clamp(scale*std::exp(input*.6f*dt),.5f,3.f);
            dirty_=dirty_||next!=scale;scale=next;
        }
        if(neutral&&dirty_){dirty_=false;return true;}
        return false;
    }
};
}
