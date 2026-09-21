#include "longsword_gesture.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
void check(bool v,const char* why){if(!v){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
struct Replay {
    amalur::LongswordGesture g;uint64_t tick=100;uint32_t weapon=1;unsigned gen=1;
    amalur::LongswordStrike frame(bool raised=false,float speed=0,bool swing=false,bool eligible=true){tick+=10;return g.sample(weapon,gen,tick,eligible,raised,speed,swing);}
    void hold(unsigned frames=110){for(unsigned i=0;i<frames;++i)frame(true);}
};
int main(){
    Replay a;a.hold();check(a.g.ready(),"one second raised quiet arms heavy");
    for(int i=0;i<20;++i)a.frame(false,1);
    auto h=a.frame(false,2,true);check(h.heavy&&h.attack==81&&h.flags==1,"ready survives windup and consumes heavy");
    check(!a.g.ready()&&a.g.progress()==0,"heavy consumed once");
    auto first=a.frame(false,2,true);check(first.attack==50&&first.step==1,"next swing returns to first normal");
    check(a.frame(false,2,true).attack==5,"second contact recipe");
    check(a.frame(false,2,true).attack==7,"third finisher skips noncontact prep6");
    check(a.frame(false,2,true).attack==50,"chain wraps");
    for(int i=0;i<111;++i)a.frame();check(a.frame(false,2,true).step==1,"chain timeout");
    Replay low;for(int i=0;i<130;++i)low.frame(false);check(!low.g.ready(),"idle lowered cannot charge");
    Replay moving;for(int i=0;i<130;++i)moving.frame(true,.4f);check(!moving.g.ready(),"raised moving cannot charge");
    Replay partial;partial.hold(60);partial.frame(false);partial.hold(60);check(!partial.g.ready(),"partial holds do not accumulate");
    Replay cancel;cancel.hold();for(int i=0;i<72;++i)cancel.frame(false,1);check(!cancel.frame(false,2,true).heavy,"lowering expires charge");
    Replay switched;switched.hold();++switched.weapon;check(!switched.frame(false,2,true).heavy,"weapon switch clears charge");
    Replay recentered;recentered.hold();++recentered.gen;check(!recentered.frame(false,2,true).heavy,"recenter clears charge");
    Replay lost;lost.hold();lost.frame(true,0,false,false);check(!lost.frame(false,2,true).heavy,"eligibility loss clears charge");
    Replay stale;stale.hold();stale.tick+=101;check(!stale.frame(false,2,true).heavy,"stale tracking clears charge");
    Replay invalid;invalid.hold();invalid.frame(true,std::numeric_limits<float>::quiet_NaN());check(!invalid.g.ready(),"invalid sample cancels");
    Replay dup;auto e=dup.frame(false,2,true);check(dup.g.sample(1,1,dup.tick,true,false,2,true).attack==0,"same packet cannot double attack");
    std::puts("longsword gesture policy checks passed");
}
