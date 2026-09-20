#include "melee_swing.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
struct Replay {
    amalur::MeleeSwing detector;uint64_t tick{};unsigned generation{1};mgs5vr::Vec3 point{};unsigned fires{};
    bool step(float velocity=0,bool eligible=true,unsigned dt=10){tick+=dt;point.x+=velocity*dt*.001f;bool hit=detector.sample(point,tick,generation,eligible);fires+=hit;return hit;}
    void run(unsigned frames,float velocity=0){for(unsigned i=0;i<frames;++i)step(velocity);}
};
int main(){
    Replay slow;slow.run(20);slow.run(100,.3f);check(slow.fires==0,"slow pose movement does not trigger attack");
    Replay initial;initial.run(50,2);check(initial.fires==0,"new tracking requires quiet arming before first swing");
    Replay swing;swing.run(20);swing.run(60,2);check(swing.fires==1,"sustained swing fires exactly once");
    check(!swing.detector.sample(swing.point,swing.tick,swing.generation,true),"same timestamp never repeats");
    swing.run(20);swing.run(20,-2);check(swing.fires==2,"quiet rearm permits a return swing");
    Replay alternating;alternating.run(20);
    for(unsigned stroke=0;stroke<10;++stroke)alternating.run(35,stroke%2?-2.f:2.f);
    check(alternating.fires==10,"continuous alternating slashes each fire without a stationary hold");
    Replay twitch;twitch.run(20);twitch.run(35,2);twitch.run(2,-2);twitch.run(25,2);
    check(twitch.fires==1,"brief reversal noise cannot rearm a continuing slash");
    Replay cancelled;cancelled.run(20);while(!cancelled.step(2)){}
    cancelled.run(8,-2);cancelled.run(30,2);
    check(cancelled.fires==1,"reversal abandoned during cooldown cannot arm later forward motion");
    Replay cooldown;cooldown.run(20);while(!cooldown.step(2)){}
    const auto firstFire=cooldown.tick;cooldown.run(12);
    while(cooldown.tick-firstFire<240)cooldown.step(-2);
    check(cooldown.fires==1,"rearmed rapid follow-up cannot bypass 250 ms cooldown");
    cooldown.run(10,-2);check(cooldown.fires==2,"sustained rearmed follow-up fires after cooldown");
    Replay shortSwing;shortSwing.run(20);shortSwing.run(2,2);shortSwing.run(20);check(shortSwing.fires==0,"brief velocity spike fails sustained swing requirement");
    Replay lost;lost.run(20);lost.step(0,false);lost.run(30,2);check(lost.fires==0,"tracking or eligibility loss disarms");
    lost.run(20);lost.run(20,2);check(lost.fires==1,"reacquired tracking rearms only after quiet hold");
    Replay recenter;recenter.run(20);++recenter.generation;recenter.run(30,2);check(recenter.fires==0,"new session or recenter invalidates armed state");
    Replay gap;gap.run(20);gap.step(0,true,101);gap.run(30,2);check(gap.fires==0,"stale pose gap disarms");
    Replay teleport;teleport.run(20);teleport.point.x+=1;teleport.run(30,2);check(teleport.fires==0,"tracking teleport disarms");
    Replay invalid;invalid.run(20);invalid.detector.sample({std::numeric_limits<float>::quiet_NaN(),0,0},invalid.tick+1,1,true);invalid.run(30,2);check(invalid.fires==0,"nonfinite pose disarms");
    Replay backwards;backwards.run(20);backwards.tick=0;backwards.run(30,2);check(backwards.fires==0,"clock rollback disarms");
    amalur::MeleeSwing translated;unsigned translationFires=0;
    for(unsigned i=0;i<100;++i){mgs5vr::Vec3 origin{float(i)*.03f,0,0};mgs5vr::Pose grip{{},origin+mgs5vr::Vec3{.3f,-.2f,-.3f}};translationFires+=translated.sample(amalur::meleeTipRelative(grip,origin),10*i,1,true);}
    check(translationFires==0,"common player translation does not manufacture swing velocity");
    const auto tip=amalur::meleeTipRelative({{0,.70710678f,0,.70710678f},{1,2,3}},{1,2,3});
    check(std::abs(tip.x+.2f)<.0001f&&std::abs(tip.z)<.0001f,"grip rotation moves configurable blade tip");
    std::puts("PASS: melee swing gesture replay, hysteresis, tracking gates, translation cancellation and blade-tip orientation");
}
