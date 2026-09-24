#include "finisher_manual.hpp"
#include <cassert>
#include <cstdio>
using namespace amalur;
FinisherObservation ready(){FinisherObservation s{};s.owner=1;s.target=2;s.player=3;s.session=4;s.valid=s.down=s.eligible=true;s.magicResidue=true;return s;}
int main(){
 auto s=ready();ManualFinisher f;assert(!f.sample(s,true,100).claimed);
 s.manualA=true;auto o=f.sample(s,true,110);assert(o.x&&!o.a&&o.suppressA);
 s.manualA=false;o=f.sample(s,true,200);assert(o.x&&!o.a); // tap need not be held
 o=f.sample(s,true,260);assert(o.x&&o.a);o=f.sample(s,true,339);assert(o.x&&o.a);
 assert(!f.sample(s,true,340).claimed);assert(!f.sample(s,true,400).claimed);
 assert(f.blocksAutomatic(400)&&f.blocksAutomatic(1759)&&!f.blocksAutomatic(1760));
 // Native acknowledgement can lag the chord without automatic RT interfering.
 FinisherAutomation automatic;auto pending=s;pending.neutral=pending.recoveryNeutral=true;
 for(uint64_t t=400;t<=600;t+=50){pending.eligible=!f.blocksAutomatic(t);auto a=automatic.sample(pending,t);assert(!a.rightTrigger&&!a.a);}
 s.manualA=true;assert(f.sample(s,true,410).x);assert(f.sample(s,true,560).a);
 o=f.sample(s,true,640);assert(o.claimed&&o.suppressA&&!o.x&&!o.a);
 o=f.sample(s,true,700);assert(o.suppressA&&!o.x); // held A cannot repeat
 s.manualA=false;assert(!f.sample(s,true,710).claimed);
 s.manualA=true;assert(f.sample(s,true,720).x);s.nativeSequence=true;
 assert(!f.sample(s,true,730).claimed); // native QTE passes immediately
 for(int cause=0;cause<8;++cause){ManualFinisher g;auto q=ready();q.manualA=true;assert(g.sample(q,true,100).x);
  bool clear=true;switch(cause){case 0:q.target=9;break;case 1:q.owner=9;break;case 2:q.player=9;break;case 3:q.session=9;break;case 4:q.eligible=false;break;case 5:q.targetFallback=true;break;case 6:clear=false;break;case 7:q.down=false;break;}
  assert(!g.sample(q,clear,150).claimed);
 }
 ManualFinisher gap;auto q=ready();q.manualA=true;assert(gap.sample(q,true,100).x);assert(!gap.sample(q,true,400).claimed);
 ManualFinisher ordinary;q=ready();q.down=false;q.manualA=true;assert(!ordinary.sample(q,true,100).claimed);
 q=ready();q.manualA=true;assert(!ordinary.sample(q,true,110).claimed); // walking into target while holding A does not start
 q.manualA=false;ordinary.sample(q,true,120);q.manualA=true;assert(ordinary.sample(q,true,130).x);
 puts("PASS manual A: native X lead-in/chord, tap/hold/retry, no auto initiation, QTE passthrough, target/session/focus/conflict cancellation");
}
