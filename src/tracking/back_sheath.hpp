#pragma once
#include <mgs5vr/core.hpp>
#include <cmath>
#include <cstdint>
namespace amalur {
enum class BackSheathAction {None,Sheath,Draw};
struct BackSheathInput {
    mgs5vr::Vec3 handRelative{},headForward{0,0,-1};
    uint64_t tick{};uint32_t session{},generation{},weapon{};
    float grip{};bool eligible{},spell{},sheathed{};
};
struct BackSheathResult {BackSheathAction action{BackSheathAction::None};bool claimed{};};
// Native dispatch must revalidate the context after taking a queued request.
// The caller supplies current tracking/input/identity eligibility, not the
// eligibility remembered at gesture recognition. Rejected requests are consumed.
inline bool backSheathRequestCurrent(uint64_t requestTick,uint64_t now,uint64_t handTick,
    uint64_t inputTick,bool contextValid){
    return contextValid&&requestTick&&handTick&&inputTick&&requestTick<=now&&handTick<=now&&inputTick<=now
        &&now-requestTick<100&&now-handTick<100&&now-inputTick<100;
}
inline bool backSheathFinite(mgs5vr::Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
// XR coordinates: +Y up, -Z forward. Project head forward onto the horizontal
// plane; pitch must not move the sheath zone into the player's face or chest.
// Reject heading within about 12 degrees of vertical, where yaw is unreliable.
inline bool backSheathZone(mgs5vr::Vec3 p,mgs5vr::Vec3 forward){
    if(!backSheathFinite(p)||!backSheathFinite(forward))return false;
    const float horizontal=forward.x*forward.x+forward.z*forward.z;
    const float total=horizontal+forward.y*forward.y;
    if(total<.000001f||horizontal<.04f*total)return false;
    const float inverse=1.f/std::sqrt(horizontal),fx=forward.x*inverse,fz=forward.z*inverse;
    const float back=-(p.x*fx+p.z*fz),right=-p.x*fz+p.z*fx;
    return back>.12f&&back<.65f&&right>=-.2f&&right<=.75f&&p.y>=-.75f&&p.y<=.35f
        &&p.x*p.x+p.y*p.y+p.z*p.z<.9f*.9f;
}
class BackSheathGesture {
    uint64_t lastTick_{},pressedAt_{};uint32_t session_{},generation_{},weapon_{};
    bool armed_{},held_{},claimed_{},pending_{},beganSheathed_{},fired_{};
public:
    void reset(){*this=BackSheathGesture{};}
    BackSheathResult sample(const BackSheathInput& p){
        if(!p.eligible||p.spell||!p.tick||!p.session||!p.weapon||!std::isfinite(p.grip)
            ||p.grip<0||p.grip>1||!backSheathFinite(p.handRelative)||!backSheathFinite(p.headForward)){
            reset();return {};
        }
        if(!lastTick_||p.session!=session_||p.generation!=generation_||p.weapon!=weapon_
            ||p.tick<lastTick_||p.tick-lastTick_>100){
            reset();session_=p.session;generation_=p.generation;weapon_=p.weapon;
        }
        if(p.tick==lastTick_)return {BackSheathAction::None,claimed_};
        lastTick_=p.tick;
        if(!armed_){if(p.grip<=.35f)armed_=true;return {};}
        const bool inside=backSheathZone(p.handRelative,p.headForward);
        if(!held_){
            if(p.grip<.65f)return {};
            held_=true;claimed_=pending_=inside;beganSheathed_=p.sheathed;pressedAt_=p.tick;fired_=false;
        }
        // A press that starts elsewhere never turns into a back gesture. Leaving
        // the zone cancels the action permanently, but keeps the grip consumed
        // until release so it cannot become a heavy charge midway through.
        if(!inside||(!fired_&&p.sheathed!=beganSheathed_))pending_=false;
        BackSheathAction action=BackSheathAction::None;
        const uint64_t duration=p.tick-pressedAt_;
        if(pending_&&!fired_&&beganSheathed_&&p.sheathed&&duration>=450){
            action=BackSheathAction::Draw;fired_=true;
        }
        if(p.grip<=.35f){
            if(pending_&&!fired_&&!beganSheathed_&&!p.sheathed&&duration<=450)
                action=BackSheathAction::Sheath;
            held_=claimed_=pending_=fired_=false;
        }
        return {action,claimed_};
    }
};
}
