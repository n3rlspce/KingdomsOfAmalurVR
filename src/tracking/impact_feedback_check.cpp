#include "impact_feedback.hpp"
#include <cstdio>
#include <cstdlib>
using namespace amalur;
void ck(bool b,const char* s){if(!b){puts(s);exit(1);}}
int main(){ImpactFeedbackWriter w;ImpactFeedbackReceiver r;
 auto sample=[&](uint64_t n,bool allowed=true){return r.sample(w.packet(),n,allowed,1,2,3);};
 w.sample(1,2,3,4,5,1000,true);ck(!sample(1000).hand[0].milliseconds,"baseline silent");
 ck(w.confirmed(0,7,1010,false),"confirmed right");auto a=sample(1010);ck(a.hand[0].milliseconds==35&&a.hand[0].amplitude==.35f&&!a.hand[1].milliseconds,"right modest pulse");
 ck(!w.confirmed(0,7,1020,false)&&!sample(1020).hand[0].milliseconds,"cleave duplicate no replay");
 ck(w.confirmed(1,7,1020,true),"left independent same serial");a=sample(1020);ck(a.hand[1].milliseconds==45&&!a.hand[0].milliseconds,"left heavy pulse only");
 w.sample(1,2,3,4,5,1030,false);ck(!sample(1030,false).hand[0].milliseconds,"tracking loss silent");
 w.sample(1,2,3,4,5,1040,true);ck(!sample(1040).hand[0].milliseconds&&!w.confirmed(0,7,1041,false),"resume preserves consumed stroke");
 ck(w.confirmed(0,8,1050,false)&&sample(1050).hand[0].milliseconds==35,"new stroke after resume");
 ck(!sample(1300).hand[0].milliseconds,"stale packet rejected");
 w.sample(1,2,3,4,5,1310,true);w.confirmed(0,9,1310,false);ck(!sample(1310).hand[0].milliseconds,"new epoch first event is baseline not replay");
 w.sample(1,2,3,4,6,1320,true);w.confirmed(0,1,1320,false);ck(!sample(1320).hand[0].milliseconds,"new owner silent baseline");
 w.sample(1,2,3,4,6,1330,true);w.confirmed(1,2,1330,false);ck(sample(1330).hand[1].milliseconds==35,"new owner fresh event");
 auto p=w.packet();p.bridge=99;ck(!r.sample(p,1330,true,1,2,3).hand[1].milliseconds,"bridge restart old packet rejected");
 p=w.packet();p.hitTick[0]=1331;ck(!r.sample(p,1330,true,1,2,3).hand[0].milliseconds,"future timestamp rejected");
 w.sample(1,2,3,4,6,1340,true);ck(!w.confirmed(2,9,1340,false),"invalid hand");
 puts("PASS impact hand/stroke dedup, cleave bound, freshness, owner/session transitions and resume/restart no replay");}
