#pragma once
#include <mgs5vr/core.hpp>
#include <cmath>

namespace amalur {
// Local tracking-space tip minus a translation-only body/head origin. Do not
// rotate by head yaw: looking around must not manufacture a blade swing.
inline mgs5vr::Vec3 meleeTipRelative(mgs5vr::Pose grip,mgs5vr::Vec3 origin,
    mgs5vr::Vec3 localTip={0,0,-.2f}){
    return grip.position+mgs5vr::rotate(grip.orientation,localTip)-origin;
}

// Gesture detection only. Eligibility and native hit/damage authority belong
// to the game adapter; this class never decides whether a swing hit anything.
class MeleeSwing {
public:
    void reset(){count_=0;armed_=false;quiet_=fast_=false;speed_=0;hasFire_=false;gate_="warming-up";}
    float speed() const{return speed_;}
    const char* gate() const{return gate_;}
    bool sample(mgs5vr::Vec3 pointRelativeToHead,uint64_t tick,unsigned generation,bool eligible,float threshold=.9f,unsigned sustainedMs=30){
        if(!eligible||!std::isfinite(pointRelativeToHead.x)||!std::isfinite(pointRelativeToHead.y)||!std::isfinite(pointRelativeToHead.z)){
            reset();gate_="ineligible-or-invalid";return false;
        }
        if(count_){
            const auto& last=history_[count_-1];
            if(generation!=generation_||tick<last.tick||tick-last.tick>100||distance(pointRelativeToHead,last.point)>.5f)reset();
            else if(tick==last.tick)return false;
        }
        if(!count_){generation_=generation;history_[count_++]={pointRelativeToHead,tick};return false;}
        // Keep the sample immediately before the 40 ms window boundary.
        while(count_>1&&tick-history_[1].tick>=40){
            for(unsigned i=1;i<count_;++i)history_[i-1]=history_[i];
            --count_;
        }
        if(count_==128){reset();generation_=generation;history_[count_++]={pointRelativeToHead,tick};return false;}
        history_[count_++]={pointRelativeToHead,tick};
        const uint64_t span=tick-history_[0].tick;
        if(span<40)return false;
        const auto displacement=pointRelativeToHead-history_[0].point;
        const auto length=std::sqrt(mgs5vr::dot(displacement,displacement));
        speed_=length*1000.f/static_cast<float>(span);
        const auto direction=length>0?displacement*(1.f/length):mgs5vr::Vec3{};
        if(speed_<.35f){
            if(!quiet_){quiet_=true;quietSince_=tick;}
            if(tick-quietSince_>=80)armed_=true;
        }else quiet_=false;
        if(speed_>=threshold){
            if(!fast_){fast_=true;fastSince_=tick;}
        }else fast_=false;
        // A deliberate return slash can start without an 80ms stationary hold.
        // Keep initial tracking arming and the existing speed/duration/cooldown
        // gates; only a reversal relative to the last fired slash can rearm.
        const bool returnSlash=hasFire_&&mgs5vr::dot(direction,lastDirection_)<-.25f;
        if((armed_||returnSlash)&&fast_&&tick-fastSince_>=sustainedMs&&(!hasFire_||tick-lastFire_>=250)){
            armed_=false;quiet_=false;lastFire_=tick;lastDirection_=direction;hasFire_=true;gate_="accepted";return true;
        }
        gate_=speed_<threshold?"below-speed":!(armed_||returnSlash)?"needs-rearm":!fast_||tick-fastSince_<sustainedMs?"too-brief":"cooldown";
        return false;
    }
private:
    const char* gate_="warming-up";
    struct Point {mgs5vr::Vec3 point;uint64_t tick;};
    Point history_[128]{};
    unsigned count_{},generation_{};
    uint64_t quietSince_{},fastSince_{},lastFire_{};
    bool armed_{},quiet_{},fast_{},hasFire_{};
    mgs5vr::Vec3 lastDirection_{};
    float speed_{};
    static float distance(mgs5vr::Vec3 a,mgs5vr::Vec3 b){const auto d=a-b;return std::sqrt(mgs5vr::dot(d,d));}
};
}
