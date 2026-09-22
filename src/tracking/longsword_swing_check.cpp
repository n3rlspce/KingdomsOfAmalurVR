#include "longsword_swing.hpp"
#include <cstdio>
#include <cstdlib>
using namespace amalur;
void check(bool b){if(!b)std::abort();}
struct Replay {MeleeSwing detector; mgs5vr::Vec3 point{};uint64_t tick=100;unsigned fired{};
 void run(unsigned ms,float vy,bool preparing=false){for(unsigned t=0;t<ms;t+=10){tick+=10;point.y+=vy*.01f;if(sampleLongswordSwing(detector,point,tick,1,true,preparing))++fired;}}
};
int main(){
 Replay a;a.run(200,0);a.run(100,2,true);check(a.fired==0);a.run(140,-1.4f);check(a.fired==1); // upward preparation must not spend250ms cooldown
 a.run(160,0);a.run(110,2,true);a.run(140,-1.4f);check(a.fired==2); // repeat same downward direction after quiet
 Replay slow;slow.run(200,0);slow.run(160,1.2f);check(slow.fired==1); // below former2m/s threshold
 slow.run(400,1.2f);check(slow.fired==1); // no periodic hits from continuing one motion
 Replay noise;noise.run(200,0);for(unsigned i=0;i<100;++i){noise.run(20,.3f);noise.run(20,-.3f);}check(noise.fired==0);
 Replay brief;brief.run(200,0);brief.run(10,2);brief.run(200,0);check(brief.fired==0);
 Replay prep;prep.run(200,0);prep.run(300,2,true);prep.run(20,-1.2f);check(prep.fired==0);prep.run(100,-1.2f);check(prep.fired==1);
 Replay invalid;invalid.run(200,0);invalid.detector.sample(invalid.point,invalid.tick+10,1,false);invalid.run(120,1.4f);check(invalid.fired==0);
 // Default detector semantics remain untouched for daggers:0.4m/s does not rearm.
 MeleeSwing dagger;mgs5vr::Vec3 q{};unsigned hits=0;uint64_t tick=100;
 auto feed=[&](int ms,float speed){for(int i=0;i<ms;i+=10){tick+=10;q.y+=speed*.01f;if(dagger.sample(q,tick,1,true))++hits;}};
 feed(200,0);feed(140,1.4f);check(hits==1);feed(300,.4f);feed(140,1.4f);check(hits==1);
 puts("PASS: preparation admission, slow strike, same-direction repeats, noise/brief rejection and unchanged dagger defaults");
}
