#pragma once
#include "finisher_automation.hpp"
namespace amalur {
struct ManualFinisherInput {bool claimed{},suppressA{},x{},a{};unsigned phase{};};
// One user A edge supplies the observed native X lead-in, then X+A.
// No actor writes, automatic retries, or synthetic QTE input.
class ManualFinisher {
    uint32_t owner_{},target_{},session_{};uintptr_t player_{};
    uint64_t last_{},at_{},automaticAfter_{};unsigned phase_{};bool previousA_{};
public:
    bool blocksAutomatic(uint64_t now)const{return now<automaticAfter_;}
    ManualFinisherInput sample(const FinisherObservation& s,bool clearOtherInput,uint64_t now){
        const bool rise=s.manualA&&!previousA_;previousA_=s.manualA;
        const bool gap=last_&&(now<last_||now-last_>=250);last_=now;
        const bool eligible=s.valid&&s.owner&&s.target&&s.player&&s.eligible&&s.down
            &&!s.nativeSequence&&!s.targetFallback&&clearOtherInput;
        if(!eligible||gap||(phase_&&(s.owner!=owner_||s.player!=player_||s.target!=target_||s.session!=session_))){
            phase_=0;return {};
        }
        if(!phase_&&rise){owner_=s.owner;player_=s.player;target_=s.target;session_=s.session;at_=now;phase_=1;}
        if(phase_==1&&now-at_>=150){phase_=2;at_=now;automaticAfter_=now+1500;}
        else if(phase_==2&&now-at_>=80)phase_=3;
        if(phase_==3){if(!s.manualA){phase_=0;return {};}
            return {true,true,false,false,3};}
        return phase_?ManualFinisherInput{true,true,true,phase_==2,phase_}:ManualFinisherInput{};
    }
};
}
