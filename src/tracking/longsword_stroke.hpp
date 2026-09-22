#pragma once
#include <mgs5vr/core.hpp>
#include <cmath>
#include <cstdint>

namespace amalur {
// Original Amalur adapter. Behaviour/default provenance: PLANCK f06fc953,
// include/config.h and SwingHandler::Update; see PLANCK.md. Positions are metres.
class LongswordStroke {
    struct Frame {mgs5vr::Vec3 hand,body;uint64_t tick;};
    Frame frames_[64]{};unsigned count_{},generation_{};
    mgs5vr::Vec3 start_{},direction_{};
    float speed_{},bodySpeed_{},peak_{};uint64_t quietAt_{},committedAt_{},stroke_{};
    bool armed_{},active_{},emitted_{},qualified_{},peakEvent_{};
    const char* gate_="warming-up";
    static float length(mgs5vr::Vec3 p){return std::sqrt(mgs5vr::dot(p,p));}
    static bool finite(mgs5vr::Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
public:
    static constexpr float airSpeed=4.5f,handContactSpeed=1.f;
    void reset(){*this=LongswordStroke{};}
    float speed()const{return speed_;}
    const char* gate()const{return gate_;}
    bool active()const{return active_&&qualified_;}
    bool contactReady()const{return active()&&speed_>=handContactSpeed&&bodySpeed_>=handContactSpeed;}
    bool emitted()const{return emitted_;}
    uint64_t stroke()const{return stroke_;}
    bool commit(uint64_t tick){if(!active()||emitted_)return false;emitted_=true;committedAt_=tick;return true;}
    bool sample(mgs5vr::Vec3 hand,mgs5vr::Vec3 head,mgs5vr::Vec3 forward,uint64_t tick,unsigned generation,bool eligible,bool preparing,unsigned recoveryMs=550){
        peakEvent_=false;
        if(!eligible||!tick||!finite(hand)||!finite(head)||!finite(forward)){reset();return false;}
        const auto body=hand-head;
        if(count_&&(generation!=generation_||tick<frames_[count_-1].tick||tick-frames_[count_-1].tick>100
            ||length(hand-frames_[count_-1].hand)>.5f)){reset();}
        if(count_&&tick==frames_[count_-1].tick)return false;
        if(!count_){generation_=generation;frames_[count_++]={hand,body,tick};start_=body;return false;}
        while(count_>1&&tick-frames_[1].tick>=56){for(unsigned i=1;i<count_;++i)frames_[i-1]=frames_[i];--count_;}
        if(count_==64){reset();return false;}
        frames_[count_++]={hand,body,tick};
        const auto span=tick-frames_[0].tick;if(span<50)return false;
        float distance=0,bodyDistance=0;
        for(unsigned i=1;i<count_;++i){distance+=length(frames_[i].hand-frames_[i-1].hand);bodyDistance+=length(frames_[i].body-frames_[i-1].body);}
        const float previous=speed_;speed_=distance*1000.f/float(span);bodySpeed_=bodyDistance*1000.f/float(span);
        const auto delta=body-frames_[0].body;const float net=length(delta);
        const auto dir=net>.0001f?delta*(1.f/net):mgs5vr::Vec3{};
        const bool backwards=mgs5vr::dot(dir,forward)<-.8f;
        if(preparing||backwards){armed_=armed_||active_;active_=qualified_=false;peak_=0;start_=body;gate_="windup";return false;}
        if(speed_<.35f&&bodySpeed_<.35f){
            if(!quietAt_)quietAt_=tick;
            if(tick-quietAt_>=80){armed_=true;active_=qualified_=false;start_=body;peak_=0;}
        }else quietAt_=0;
        // A direction reversal must build a fresh 12 cm stroke; no periodic
        // retrigger during a long sweep. Quiet recovery handles same-direction cuts.
        if(active_&&mgs5vr::dot(dir,direction_)<-.25f&&net>.025f){active_=qualified_=false;armed_=true;start_=frames_[0].body;peak_=0;}
        if(!active_&&armed_&&speed_>=handContactSpeed&&bodySpeed_>=handContactSpeed){
            active_=true;emitted_=qualified_=false;direction_=dir;peak_=0;stroke_=tick;
        }
        if(active_){
            const auto travel=body-start_;const float displacement=length(travel);
            if(displacement>=.12f&&net>=.025f&&(!committedAt_||tick-committedAt_>=recoveryMs))qualified_=true;
            if(!qualified_)direction_=dir;
            peak_=speed_>peak_?speed_:peak_;
            peakEvent_=qualified_&&!emitted_&&peak_>=airSpeed&&speed_<previous-.03f;
            armed_=false;
        }
        gate_=peakEvent_?"peak":qualified_?"stroke":active_?"too-short":armed_?"ready":"recovery";
        return peakEvent_;
    }
};

// Distinct strokes still need actual separation from an already hit actor.
// No persistent-overlap timeout: embedded blades remain harmless in V1.
struct LongswordSeparation {
    struct Entry {uint32_t actor{};uint64_t seen{},hit{},absentSince{};bool separated{true},observed{};};
    Entry entries[64]{};
    uint64_t frameTick{},lastComplete{};bool frameOpen{};
    void interrupt(){for(auto& e:entries)e.absentSince=0;lastComplete=0;frameOpen=false;}
    // A frame comprises every collision sphere. Missing queries are not evidence
    // that an embedded weapon left an actor; only complete scans prove absence.
    void beginFrame(uint64_t now){
        if(frameOpen||!now||!lastComplete||now<=lastComplete||now-lastComplete>100)interrupt();
        frameTick=now;frameOpen=now!=0;
        for(auto& e:entries)e.observed=false;
    }
    void endFrame(bool completed){
        if(!frameOpen||!completed){interrupt();return;}
        for(auto& e:entries){
            if(!e.actor)continue;
            if(e.observed)e.absentSince=0;
            else if(!e.separated){
                if(!e.absentSince)e.absentSince=frameTick;
                if(frameTick-e.absentSince>=250)e.separated=true;
            }
        }
        lastComplete=frameTick;frameOpen=false;
    }
    bool observe(uint32_t actor,uint64_t now){
        if(!frameOpen||!actor)return false;
        Entry* slot=nullptr;
        for(auto& e:entries)if(e.actor==actor){slot=&e;break;}
        if(!slot)for(auto& e:entries)if(!e.actor||(e.separated&&now>=e.seen&&now-e.seen>2000)){slot=&e;*slot={};slot->actor=actor;break;}
        if(!slot)return false;
        slot->seen=now;slot->observed=true;slot->absentSince=0;return !slot->hit||slot->separated;
    }
    void hit(uint32_t actor,uint64_t now){for(auto& e:entries)if(e.actor==actor){e.hit=now;e.separated=false;e.absentSince=0;return;}}
};
}
