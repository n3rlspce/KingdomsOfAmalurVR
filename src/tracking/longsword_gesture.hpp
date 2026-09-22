#pragma once
#include <cmath>
#include <cstdint>

namespace amalur {
struct LongswordStrike {unsigned step{},attack{},flags{};bool heavy{};};
// VR gesture policy, deliberately separate from native animation inputs.
// Only observed damaging phases participate: 6 and 78 are not contact recipes.
class LongswordGesture {
public:
    void reset(){*this=LongswordGesture{};}
    float progress()const{return progress_;}
    bool ready()const{return ready_;}
    unsigned step()const{return step_;}
    LongswordStrike sample(uint32_t weapon,unsigned generation,uint64_t tick,
        bool eligible,bool raised,float speed,bool accepted){
        if(!eligible||!weapon||!tick||!std::isfinite(speed)||speed<0){reset();return {};}
        if(weapon_!=weapon||generation_!=generation||tick<lastTick_||
            (lastTick_&&tick-lastTick_>100)){
            reset();weapon_=weapon;generation_=generation;
        }
        if(tick==lastTick_)return {};
        lastTick_=tick;
        if(accepted)return commit(tick);
        if(ready_){
            // Allow a brief transition from the raised pose into the strike.
            // Moving farther overhead/behind the shoulder retains a ready
            // charge. Departure starts only after lowering out of the region.
            if(!raised){if(!departed_)departed_=tick;}else departed_=0;
            if(departed_&&tick-departed_>700){ready_=false;progress_=0;held_=departed_=0;}
            return {};
        }
        if(raised&&speed<.25f){
            if(!held_)held_=tick;
            progress_=float(tick-held_)/1000.f;
            if(progress_>=1){progress_=1;ready_=true;step_=0;lastStrike_=0;}
        }else {held_=0;progress_=0;}
        return {};
    }
    LongswordStrike commit(uint64_t tick){
            const bool heavy=ready_&&(!departed_||tick-departed_<=700);
            ready_=false;held_=departed_=0;progress_=0;
            if(heavy){step_=0;lastStrike_=0;return {0,81,1,true};}
            if(!lastStrike_||tick-lastStrike_>1100)step_=0;
            step_=step_%3+1;lastStrike_=tick;
            return {step_,step_==1?50u:step_==2?5u:7u,step_==1?0u:step_==2?1u:2u,false};
        }
private:
    uint32_t weapon_{};unsigned generation_{},step_{};
    uint64_t lastTick_{},lastStrike_{},held_{},departed_{};
    float progress_{};bool ready_{};
};
}
