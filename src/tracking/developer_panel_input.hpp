#pragma once
#include <algorithm>
#include "motion_input.hpp"

namespace amalur {
struct DeveloperPanelEvents {
    bool toggle{}, close{}, activate{}, capture{};
    int row{}, destination{};
};
// A near-center double click opens immediately. Grips are not panel controls;
// holding them must not swallow the first click or prevent panel navigation.
class DeveloperPanelInput {
    bool ready_{}, chordSeen_{}, chordCaptured_{}, release_{}, seenVisible_{}, stickArmed_{true};
    bool trigger_{}, confirm_{}, back_{};
    static bool neutral(const TouchInput& t) {
        return std::abs(t.leftX)<.25f&&std::abs(t.leftY)<.25f&&
            std::abs(t.rightX)<.25f&&std::abs(t.rightY)<.25f&&
            t.rightTrigger<.25f&&!t.a&&!t.b&&!t.leftClick&&!t.rightClick;
    }
public:
    DeveloperPanelEvents update(const TouchInput& t,bool active,bool visible,bool busy,uint64_t) {
        DeveloperPanelEvents e;
        if(!active){*this={};e.capture=true;return e;}
        if(!t.leftClick&&!t.rightClick){chordSeen_=chordCaptured_=false;}
        if(t.leftClick&&t.rightClick&&!chordSeen_){
            chordSeen_=true;
            const bool centered=std::abs(t.leftX)<.6f&&std::abs(t.leftY)<.6f&&
                std::abs(t.rightX)<.6f&&std::abs(t.rightY)<.6f;
            if(centered){
                chordCaptured_=true;ready_=true;release_=true;seenVisible_=!visible;
                e.toggle=e.capture=true;return e;
            }
        }
        if(chordCaptured_){e.capture=true;return e;}
        if(!ready_){ready_=neutral(t);e.capture=true;return e;}
        if(visible!=seenVisible_){release_=true;seenVisible_=visible;}
        if(release_){
            e.capture=true;
            if(neutral(t)){release_=false;trigger_=confirm_=back_=false;stickArmed_=true;}
            return e;
        }
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
