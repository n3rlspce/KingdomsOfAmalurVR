#pragma once
#include <cstdint>
namespace amalur {
class WalkTraceSchedule {
    uint64_t lastNow_{},lastMoving_{},next_{};
    bool live_{};
public:
    bool sample(uint64_t now,bool moving){
        if(lastNow_&&now<lastNow_){*this={};}
        lastNow_=now;
        if(moving){lastMoving_=now;live_=true;}
        if(!live_||now<next_)return false;
        next_=now+50;
        if(!moving&&now-lastMoving_>500)live_=false;
        return true; // One final idle edge, then silence until input resumes.
    }
};
}
