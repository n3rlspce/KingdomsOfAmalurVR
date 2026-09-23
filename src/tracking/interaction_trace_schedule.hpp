#pragma once
#include <cstdint>
namespace amalur {
struct InteractionTraceEvents {unsigned edge{};bool down{},release{},post100{},post500{},exhausted{},rearmed{};};
class InteractionTraceSchedule {
    bool down_{},pending_{},post100_{},post500_{},notified_{};
    unsigned count_{},windowCount_{};uint64_t start_{},last_{},windowStart_{};bool windowSet_{};
public:
    InteractionTraceEvents sample(bool active,bool a,uint64_t now){
        InteractionTraceEvents out;out.edge=count_;
        if(!active||(last_&&now<last_)){down_=pending_=false;last_=now;return out;}
        last_=now;
        if(!windowSet_||now<windowStart_||now-windowStart_>=60000){
            out.rearmed=windowSet_;windowSet_=true;windowStart_=now;windowCount_=0;notified_=false;
        }
        if(a&&!down_){
            if(windowCount_>=128){if(!notified_){out.exhausted=true;notified_=true;}}
            else {++windowCount_;out.edge=++count_;out.down=true;pending_=true;post100_=post500_=false;start_=now;}
        }
        if(!a&&down_&&pending_)out.release=true;
        down_=a;
        if(pending_&&now>=start_){
            if(!post100_&&now-start_>=100){out.post100=true;post100_=true;}
            if(!post500_&&now-start_>=500){out.post500=true;post500_=true;pending_=false;}
        }
        return out;
    }
};
}
