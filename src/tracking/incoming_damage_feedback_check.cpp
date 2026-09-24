#include "incoming_damage_feedback.hpp"
#include "melee_hit_identity.hpp"
#include <cassert>
#include <cstdio>
int main(){
 using namespace amalur;
 IncomingDamageSnapshot a{1,2,3,0x12345,100},b=a;
 b.health=80;assert(confirmedIncomingDamage(a,b,true));
 assert(!confirmedIncomingDamage(a,b,false)); // unrelated health cost is not a hit
 b.health=100;assert(!confirmedIncomingDamage(a,b,true)); // shield/no-op
 b.health=110;assert(!confirmedIncomingDamage(a,b,true)); // heal
 b.health=0;assert(confirmedIncomingDamage(a,b,true)); // lethal hit
 b.health=-1;assert(!confirmedIncomingDamage(a,b,true));
 b=a;b.health=80;b.owner++;assert(!confirmedIncomingDamage(a,b,true));
 b=a;b.health=80;b.entity++;assert(!confirmedIncomingDamage(a,b,true));
 b=a;b.health=80;b.healthPart++;assert(!confirmedIncomingDamage(a,b,true));
 b=a;b.health=80;b.player++;assert(!confirmedIncomingDamage(a,b,true));
 a.health=0;b=a;assert(!confirmedIncomingDamage(a,b,true));
 assert(meleeHitActor(0x10012345)==0x12345&&meleeHitActor(0x20012345)==0x12345);
 assert(!meleeHitActor(0x30012345));
 puts("incoming damage feedback check PASS");
}
