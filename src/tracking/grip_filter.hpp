#pragma once
#include <mgs5vr/core.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace amalur {
// Visual-only filter in tracking-space metres. World movement and virtual
// turns are applied afterwards; combat keeps the original controller samples.
struct GripFilter {
    mgs5vr::Pose previous{},filtered{};
    uint64_t tick{};unsigned generation{};float speed{};bool ready{};
    bool sample(mgs5vr::Pose input,uint64_t time,unsigned center,bool tracked,mgs5vr::Pose& output){
        if(!tracked||!time||!mgs5vr::valid(input)){ready=false;return false;}
        if(!ready||center!=generation||time<tick||time-tick>100){
            previous=filtered=input;tick=time;generation=center;speed=0;ready=true;output=input;return true;
        }
        if(time==tick){output=filtered;return true;}
        const float dt=float(time-tick)*.001f;
        const auto delta=input.position-previous.position;
        const float velocity=std::sqrt(mgs5vr::dot(delta,delta))/dt;
        // Reset across tracking-origin jumps instead of sweeping through them.
        if(mgs5vr::dot(delta,delta)>.25f){ready=false;return sample(input,time,center,true,output);}
        speed+=(velocity-speed)*(1.f-std::exp(-dt/.04f));
        const float cutoff=std::clamp(7.f+12.f*speed,7.f,40.f);
        const float alpha=1.f-std::exp(-6.28318530718f*cutoff*dt);
        filtered.position=filtered.position+(input.position-filtered.position)*alpha;
        auto a=filtered.orientation,b=input.orientation;
        float dot=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
        if(dot<0){b={-b.x,-b.y,-b.z,-b.w};dot=-dot;}
        const float angular=2.f*std::acos(std::clamp(dot,0.f,1.f))/dt;
        const float rotationAlpha=1.f-std::exp(-6.28318530718f*std::clamp(10.f+angular*3.f,10.f,40.f)*dt);
        mgs5vr::Quat q{a.x+(b.x-a.x)*rotationAlpha,a.y+(b.y-a.y)*rotationAlpha,
            a.z+(b.z-a.z)*rotationAlpha,a.w+(b.w-a.w)*rotationAlpha};
        const float length=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
        filtered.orientation={q.x/length,q.y/length,q.z/length,q.w/length};
        previous=input;tick=time;output=filtered;return true;
    }
};
}
