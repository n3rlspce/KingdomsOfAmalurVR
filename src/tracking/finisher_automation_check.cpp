#include "finisher_automation.hpp"
#include <cassert>
using namespace amalur;
static FinisherObservation ready(){FinisherObservation s{1,2,3,123,true,true,false,false,true,true,3.35f};s.recoveryNeutral=true;return s;}
static void polls(FinisherAutomation& p,FinisherObservation& s,uint64_t from,uint64_t to){for(auto n=from;n<=to;n+=50)p.sample(s,n);}
int main(){
 // Far5.15m waits. Stationary unacknowledged A retries after1500ms
 // plus neutral preparation, but never more than3 automatic requests.
 FinisherAutomation p;auto s=ready();s.distanceMetres=5.15f;
 assert(p.sample(s,900).reason==FinisherReason::AwaitApproach);s.distanceMetres=3.35f;
 p.sample(s,1000);auto r=p.sample(s,1100);assert(r.a&&r.aRequests==1);
 assert(p.sample(s,1100).aRequests==1);p.sample(s,1180);polls(p,s,1200,2550);
 assert(p.sample(s,2551).reason==FinisherReason::Cooldown);
 p.sample(s,2600);r=p.sample(s,2700);assert(r.a&&r.aRequests==2);p.sample(s,2780);
 polls(p,s,2800,4150);p.sample(s,4200);r=p.sample(s,4300);assert(r.a&&r.aRequests==3);p.sample(s,4380);
 polls(p,s,4400,5900);assert(p.sample(s,5950).reason==FinisherReason::ARequestLimit);
 // Native ack after the pulse and targetclear consumes the pending episode.
 s.target=0;s.valid=false;s.nativeSequence=true;p.sample(s,6000);
 s.target=2;s.valid=true;s.nativeSequence=false;assert(p.sample(s,6050).reason==FinisherReason::Consumed);
 // Manual A passes unchanged but cannot remove the automatic3-request cap.
 s.manualA=true;s.neutral=false;s.recoveryNeutral=false;p.sample(s,6100);
 s.manualA=false;s.neutral=true;s.recoveryNeutral=true;assert(p.sample(s,6200).reason==FinisherReason::Consumed);
 FinisherAutomation cap;auto t=ready();uint64_t tick=1000;
 for(unsigned i=0;i<3;++i){cap.sample(t,tick);assert(cap.sample(t,tick+100).a);cap.sample(t,tick+180);polls(cap,t,tick+200,tick+1550);tick+=1600;}
 t.manualA=true;t.neutral=false;t.recoveryNeutral=false;cap.sample(t,tick);
 t.manualA=false;t.neutral=true;t.recoveryNeutral=true;polls(cap,t,tick+50,tick+1500);
 assert(cap.sample(t,tick+1550).reason==FinisherReason::ARequestLimit);
 // Sticks can move during owned modifier recovery, but A waits neutral100ms.
 FinisherAutomation moving;auto u=ready();u.magicResidue=true;moving.sample(u,1000);assert(moving.sample(u,1100).rightTrigger);
 u.neutral=false;u.magicMode=true;for(uint64_t n=1150;n<3100;n+=50)assert(moving.sample(u,n).rightTrigger);
 assert(!moving.sample(u,3100).rightTrigger);u.magicMode=false;u.magicResidue=false;
 assert(moving.sample(u,3220).reason==FinisherReason::WaitNeutral);polls(moving,u,3250,4000);
 assert(!moving.sample(u,4050).a);u.neutral=true;moving.sample(u,4100);assert(moving.sample(u,4200).a);
 // Cleanup may run far away; neither phase4 nor direct startup fires far A.
 FinisherAutomation approach;auto far=ready();far.distanceMetres=12;far.magicResidue=true;
 approach.sample(far,1000);assert(approach.sample(far,1100).rightTrigger);polls(approach,far,1150,3100);
 far.magicResidue=false;assert(approach.sample(far,3220).reason==FinisherReason::AwaitApproach);
 polls(approach,far,3250,4000);far.distanceMetres=3.35f;approach.sample(far,4050);assert(approach.sample(far,4150).a);
 // Proven paired special-boss readiness keeps native-owned range.
 FinisherAutomation special;far=ready();far.specialBoss=true;far.distanceMetres=30;special.sample(far,1000);assert(special.sample(far,1100).a);
 // Conflicting control still cancels immediately and pre-A retry stays bounded.
 FinisherAutomation cancel;u=ready();u.magicResidue=true;tick=1000;
 for(unsigned i=0;i<3;++i){cancel.sample(u,tick);assert(cancel.sample(u,tick+100).rightTrigger);
  u.recoveryNeutral=false;assert(cancel.sample(u,tick+150).reason==FinisherReason::UserInput);u.recoveryNeutral=true;
  polls(cancel,u,tick+200,tick+1100);tick+=1200;}
 assert(cancel.sample(u,tick).reason==FinisherReason::RetryLimit);
 // Cleanup timeout emits no A, owner change resets history, target loss does not.
 FinisherAutomation timeout;u=ready();u.magicResidue=true;timeout.sample(u,1000);timeout.sample(u,1100);polls(timeout,u,1150,3850);
 assert(timeout.sample(u,3900).reason==FinisherReason::CleanupTimeout);
 s.valid=false;p.sample(s,6300);s.valid=true;assert(p.sample(s,6400).reason==FinisherReason::Consumed);
 s.owner=9;p.sample(s,6500);assert(p.sample(s,6600).a);
 // More than64 targets remain tracked; each receives at most3 requests.
 FinisherAutomation many;auto m=ready();tick=1000;
 for(uint32_t actor=1;actor<=200;++actor){m.target=actor;many.sample(m,tick);assert(many.sample(m,tick+100).a);many.sample(m,tick+180);tick+=200;}
 for(uint32_t actor=1;actor<=200;++actor){m.target=actor;assert(!many.sample(m,tick++).a);}
 return 0;
}
