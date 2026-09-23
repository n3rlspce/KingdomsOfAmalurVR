#include "melee_query_recovery.hpp"
#include <cassert>
int main(){
    amalur::MeleeQueryRecovery r;r.suspend(100);
    for(unsigned t=110;t<1100;t+=10)assert(!r.claim(1,2,3,t,true));
    assert(r.claim(1,2,3,1100,true));
    r.suspend(1200);
    for(unsigned t=1210;t<=2100;t+=10)assert(!r.claim(1,2,3,t,true));
    assert(!r.claim(1,2,3,2200,false));
    for(unsigned t=2210;t<2710;t+=10)assert(!r.claim(1,2,3,t,true));
    assert(r.claim(1,2,3,2710,true));
    r.suspend(2800);
    for(unsigned t=2810;t<=3790;t+=10)assert(!r.claim(1,2,3,t,true));
    assert(!r.claim(1,2,4,3800,true)); // new identity needs its own stable run
    for(unsigned t=3810;t<4300;t+=10)assert(!r.claim(1,2,4,t,true));
    assert(r.claim(1,2,4,4300,true));
    r.suspend(4400);
    for(unsigned t=4410;t<9000;t+=10)assert(!r.claim(1,2,4,t,true));
    amalur::MeleeQueryRecovery gap;gap.suspend(100);
    assert(!gap.claim(1,2,3,1000,true));assert(!gap.claim(1,2,3,2000,true));
    for(unsigned t=2010;t<2500;t+=10)assert(!gap.claim(1,2,3,t,true));
    assert(gap.claim(1,2,3,2500,true));
    // Missing native query world is a wait state, not a recovery attempt.
    // Loading can last many seconds without burning the three-attempt budget.
    amalur::MeleeQueryRecovery loading;loading.suspend(100);
    for(unsigned t=110;t<15000;t+=10)assert(!loading.claim(1,2,3,t,false));
    for(unsigned t=15000;t<15500;t+=10)assert(!loading.claim(1,2,3,t,true));
    assert(loading.claim(1,2,3,15500,true));
}
