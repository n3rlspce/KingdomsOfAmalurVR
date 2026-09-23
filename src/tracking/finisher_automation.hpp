#pragma once
#include <unordered_map>
#include <cmath>
#include <cstdint>
namespace amalur {
// Caller serializes access. Native input only; never changes actor/game state.
struct FinisherObservation {
    uint32_t owner{},target{},session{};
    uintptr_t player{};
    bool valid{},down{},magicResidue{},nativeSequence{},eligible{},neutral{};
    float distanceMetres{};
    bool magicMode{},specialBoss{},manualA{},recoveryNeutral{};
};
enum class FinisherReason : unsigned { Idle, Invalid, NativeSequence, Ineligible, UserInput, Range,
 MagicMode, TargetChanged, PollReset, Cooldown, RetryLimit, CleanupWait, CleanupTimeout, Delivered, Complete, Consumed, AwaitApproach, ARequestLimit, WaitNeutral };
struct FinisherInput {bool rightTrigger{},a{},claimed{};FinisherReason reason{};unsigned phase{},attempts{},aRequests{};};
class FinisherAutomation {
    struct Episode {bool consumed{};unsigned attempts{},aRequests{};uint64_t retryAt{};};
    std::unordered_map<uint32_t,Episode> episodes_;
    uint32_t owner_{},session_{},target_{},pending_{},lastRequest_{};
    uintptr_t player_{};
    uint64_t last_{},phaseAt_{},neutralAt_{};
    bool previousManualA_{},pulseRecorded_{};
    unsigned phase_{}; // 0 idle, 1 RT, 2 release, 3 A, 4 cleaned modifier waiting neutral
    void cancel(){phase_=0;pulseRecorded_=false;target_=0;pending_=0;neutralAt_=0;}
    Episode* episode(uint32_t target,bool down){
        const auto found=episodes_.find(target);
        if(found!=episodes_.end())return &found->second;
        // Ordinary selected actors need no record. Never evict a consumed
        // episode merely because more actors were encountered later.
        return down?&episodes_.try_emplace(target).first->second:nullptr;
    }

    FinisherInput stop(FinisherReason why,uint64_t now){
        unsigned attempts=0;
        if(target_){
            auto found=episodes_.find(target_);
            if(found!=episodes_.end()){
                attempts=found->second.attempts;
                if(phase_&&(phase_<3||phase_==4))found->second.retryAt=now+1000;
            }
        }
        cancel();return {false,false,false,why,0,attempts};
    }
public:
    FinisherInput sample(const FinisherObservation& s,uint64_t now){
        if(s.valid&&s.owner&&(s.owner!=owner_||s.player!=player_)){episodes_.clear();owner_=s.owner;player_=s.player;lastRequest_=0;cancel();}
        const bool manualRise=s.manualA&&!previousManualA_;previousManualA_=s.manualA;
        const bool discontinuity=session_!=s.session||(last_&&(now<last_||now-last_>=250));
        session_=s.session;last_=now;
        if(s.nativeSequence){
            // Native acknowledgement may arrive after the80ms pulse, or after
            // native code clears the combat target. Keep request identity.
            if(s.owner==owner_&&s.player==player_){
                const auto acknowledged=s.target?s.target:lastRequest_;
                auto e=episodes_.find(acknowledged);
                if(e!=episodes_.end())e->second.consumed=true;
            }
            return stop(FinisherReason::NativeSequence,now);
        }
        if(discontinuity&&phase_)return stop(FinisherReason::PollReset,now);
        if(discontinuity)cancel();
        Episode* e=s.valid&&s.owner==owner_&&s.target?episode(s.target,s.down):nullptr;
        if(e&&!s.down)*e=Episode{};
        if(e&&s.down&&s.eligible&&manualRise&&!e->consumed){e->attempts=0;e->retryAt=now+1500;}
        if(!e||!s.down)return stop(FinisherReason::Invalid,now);
        if(!s.eligible)return stop(FinisherReason::Ineligible,now);
        if(!(phase_==1||phase_==2||phase_==4?s.recoveryNeutral:s.neutral))return stop(FinisherReason::UserInput,now);
        if(!std::isfinite(s.distanceMetres)||s.distanceMetres<0)return stop(FinisherReason::Range,now);
        if((phase_==0||phase_==3)&&s.magicMode)return stop(FinisherReason::MagicMode,now);
        if(phase_&&target_!=s.target)return stop(FinisherReason::TargetChanged,now);
        const bool closeForA=s.specialBoss||s.distanceMetres<=3.5f;
        if(!phase_){
            if(e->consumed)return {false,false,false,FinisherReason::Consumed};
            if(now<e->retryAt)return {false,false,false,FinisherReason::Cooldown,0,e->attempts};
            // Adapter policy from observed3.35m success versus4.268/5.15m
            // failures; not a claim about native reach. Modifier cleanup may
            // run farther away, but ordinary autoA waits until approach.
            if(!s.magicResidue&&!closeForA)return {false,false,false,FinisherReason::AwaitApproach};
            if(e->aRequests>=3)return {false,false,false,FinisherReason::ARequestLimit,0,e->attempts,e->aRequests};
            if(s.magicResidue&&e->attempts>=3)return {false,false,false,FinisherReason::RetryLimit,0,e->attempts};
            if(pending_!=s.target){pending_=s.target;neutralAt_=0;}
            if(!neutralAt_){neutralAt_=now;return {};}
            if(now-neutralAt_<100)return {};
            target_=s.target;phaseAt_=now;phase_=s.magicResidue?1:3;
            if(phase_==1)++e->attempts;
        }
        // Match the approximately2s successful physical recovery capture.
        // Native dispatch owns finisher reach; no invented3m cutoff.
        if(phase_==1&&now-phaseAt_>=2000){phase_=2;phaseAt_=now;}
        else if(phase_==2&&now-phaseAt_>=800)return stop(FinisherReason::CleanupTimeout,now);
        else if(phase_==2&&now-phaseAt_>=120&&!s.magicResidue&&!s.magicMode){phase_=4;phaseAt_=now;neutralAt_=0;}
        else if(phase_==3&&now-phaseAt_>=80)return stop(FinisherReason::Complete,now);
        if(phase_==4){
            if(s.magicResidue||s.magicMode)return stop(FinisherReason::CleanupWait,now);
            if(now-phaseAt_>=4000)return stop(FinisherReason::UserInput,now);
            if(!s.neutral||!closeForA)neutralAt_=0;
            else if(!neutralAt_)neutralAt_=now;
            else if(now-neutralAt_>=100){phase_=3;phaseAt_=now;}
        }
        // A delivery is only an attempt. Native acknowledgement alone consumes
        // the episode; bounded retries allow transient native rejection, even
        // at the same position. Never exceed3 automatic A requests per episode.
        if(phase_==3&&!pulseRecorded_){
            pulseRecorded_=true;
            ++e->aRequests;
            e->retryAt=now+1500;lastRequest_=s.target;
        }
        return {phase_==1,phase_==3,true,phase_==2?FinisherReason::CleanupWait:
            phase_==3?FinisherReason::Delivered:phase_==4?(closeForA?FinisherReason::WaitNeutral:FinisherReason::AwaitApproach):FinisherReason::Idle,phase_,e->attempts,e->aRequests};
    }
};
}
