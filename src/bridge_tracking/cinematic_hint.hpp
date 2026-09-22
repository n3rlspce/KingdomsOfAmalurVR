#pragma once
#include <cstdint>
namespace amalur {
class CinematicHint {
    bool started_{};uint64_t start_{},lastCinematic_{};
public:
    float update(bool cinematic,bool visible,uint64_t now){
        if(!cinematic){if(started_&&now-lastCinematic_>=1000)started_=false;return 0;}
        lastCinematic_=now;
        if(!visible)return 0;
        if(!started_){started_=true;start_=now;}
        const auto age=now-start_;
        return age<3500?1.f:age<4000?float(4000-age)/500.f:0.f;
    }
};
}
