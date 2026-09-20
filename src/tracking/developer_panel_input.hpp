#pragma once
#include <algorithm>
#include "motion_input.hpp"

namespace amalur {
struct DeveloperPanelEvents {
    bool toggle{}, close{}, activate{}, capture{};
    int row{}, destination{};
};
// Neutral hold avoids stealing the existing two-stick D-pad chord. Every
// activation requires release; focus changes and panel exit require neutral.
class DeveloperPanelInput {
    bool ready_{}, holding_{}, release_{}, seenVisible_{}, stickArmed_{true};
    bool trigger_{}, confirm_{}, back_{};
    uint64_t holdStart_{};
    static bool neutral(const TouchInput& t) {
        return std::abs(t.leftX)<.25f&&std::abs(t.leftY)<.25f&&
            std::abs(t.rightX)<.25f&&std::abs(t.rightY)<.25f&&
            t.leftTrigger<.25f&&t.rightTrigger<.25f&&t.leftGrip<.25f&&t.rightGrip<.25f&&
            !t.a&&!t.b&&!t.x&&!t.y&&!t.menu&&!t.leftClick&&!t.rightClick;
    }
public:
    DeveloperPanelEvents update(const TouchInput& t,bool active,bool visible,bool busy,uint64_t now) {
        DeveloperPanelEvents e;
        if(!active){*this={};e.capture=true;return e;}
        if(!ready_){ready_=neutral(t);e.capture=true;return e;}
        if(visible!=seenVisible_){release_=true;seenVisible_=visible;}
        if(release_){
            e.capture=true;
            if(neutral(t)){release_=false;holding_=false;trigger_=confirm_=back_=false;stickArmed_=true;}
            return e;
        }
        const bool centered=std::abs(t.leftX)<.25f&&std::abs(t.leftY)<.25f&&
            std::abs(t.rightX)<.25f&&std::abs(t.rightY)<.25f;
        const bool chord=t.leftClick&&t.rightClick&&centered;
        if(chord){
            e.capture=true;
            if(!holding_){holding_=true;holdStart_=now;}
            if(now-holdStart_>=650){e.toggle=true;release_=true;}
            return e;
        }
        holding_=false;
        e.capture=visible;
        if(!visible)return e;
        bool trigger=t.rightTrigger>(trigger_?.35f:.7f);
        bool confirm=t.a||trigger;
        e.close=t.b&&!back_;
        e.activate=confirm&&!confirm_&&!busy&&!e.close;
        trigger_=trigger;confirm_=confirm;back_=t.b;
        if(e.close){release_=true;return e;}
        if(std::abs(t.leftX)<.25f&&std::abs(t.leftY)<.25f)stickArmed_=true;
        if(stickArmed_&&std::max(std::abs(t.leftX),std::abs(t.leftY))>.65f){
            stickArmed_=false;
            if(std::abs(t.leftY)>=std::abs(t.leftX))e.row=t.leftY>0?-1:1;
            else e.destination=t.leftX>0?1:-1;
            e.activate=false; // A simultaneous navigation gesture never runs the new row.
        }
        return e;
    }
};
}
