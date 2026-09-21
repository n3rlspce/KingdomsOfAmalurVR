#pragma once
#include <mgs5vr/core.hpp>
#include <cmath>
#include <cstdint>
namespace amalur {
// Custom V1 visual only: no game FX resources, bindings or damage authority.
struct LongswordTrailIdentity {
    uint32_t owner{},weapon{},generation{},serial{};
    bool sameWeapon(const LongswordTrailIdentity& b)const{return owner==b.owner&&weapon==b.weapon&&generation==b.generation;}
};
struct LongswordTrailSegment {
    mgs5vr::Vec3 a{},b{};float red{},green{},blue{},alpha{},width{};
};
class LongswordTrail {
public:
    static constexpr unsigned capacity=32,maxSegments=(capacity-1)*3;
    static constexpr uint64_t durationMs=250,maxGapMs=100;
    bool begin(LongswordTrailIdentity id,unsigned attack,uint64_t tick){
        if(!id.owner||!id.weapon||!id.serial||!tick||!supported(attack))return false;
        if(lastAccepted_.sameWeapon(id)&&lastAccepted_.serial==id.serial)return false;
        lastAccepted_=identity_=id;attack_=attack;start_=tick;count_=0;active_=true;return true;
    }
    void hide(){active_=false;count_=0;}
    bool sample(LongswordTrailIdentity id,uint64_t tick,bool eligible,mgs5vr::Vec3 base,mgs5vr::Vec3 tip){
        if(!active_||!eligible||!identity_.sameWeapon(id)||identity_.serial!=id.serial
            ||tick<start_||tick-start_>=durationMs||!finite(base)||!finite(tip)){hide();return false;}
        if(count_){const auto& last=points_[count_-1];
            if(tick<last.tick||tick-last.tick>maxGapMs||squared(base-last.base)>2500.f||squared(tip-last.tip)>2500.f){hide();return false;}
            if(tick==last.tick)return true;
        }
        if(count_==capacity){for(unsigned i=1;i<count_;++i)points_[i-1]=points_[i];--count_;}
        points_[count_++]={base,tip,tick};return true;
    }
    unsigned segments(uint64_t tick,LongswordTrailSegment* output,unsigned outputCapacity){
        if(!active_||tick<start_||tick-start_>=durationMs||!count_||tick<points_[count_-1].tick||tick-points_[count_-1].tick>maxGapMs){hide();return 0;}
        if(!output)return 0;
        const bool heavy=attack_==81,finisher=attack_==7;
        const float red=heavy?1.f:finisher?.8f:.45f,green=heavy?.72f:1.f,blue=heavy?.2f:1.f;
        const float remaining=1.f-float(tick-start_)/float(durationMs);
        unsigned n=0;
        for(unsigned i=1;i<count_;++i){
            const auto& a=points_[i-1];const auto& b=points_[i];
            const float age=1.f-float(tick-a.tick)/float(durationMs);
            const float opacity=remaining*age*(finisher||heavy?.9f:.65f);
            auto emit=[&](mgs5vr::Vec3 from,mgs5vr::Vec3 to,float fade){if(n<outputCapacity&&squared(to-from)>.0001f)output[n++]={from,to,red,green,blue,opacity*fade,heavy?2.5f:finisher?2.f:1.5f};};
            emit(a.tip,b.tip,1.f);emit(a.base,b.base,.3f);emit(b.base,b.tip,.12f);
        }
        return n;
    }
    bool active()const{return active_;}
    unsigned size()const{return count_;}
private:
    struct Point {mgs5vr::Vec3 base,tip;uint64_t tick;};
    Point points_[capacity]{};unsigned count_{},attack_{};uint64_t start_{};bool active_{};
    LongswordTrailIdentity identity_{},lastAccepted_{};
    static bool supported(unsigned a){return a==50||a==5||a==7||a==81;}
    static bool finite(mgs5vr::Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
    static float squared(mgs5vr::Vec3 p){return mgs5vr::dot(p,p);}
};
}
