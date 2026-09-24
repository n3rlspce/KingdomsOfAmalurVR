#include "walk_trace_schedule.hpp"
#include <cassert>
#include <cstdio>
int main(){
    amalur::WalkTraceSchedule s;
    for(unsigned t=1;t<100;++t)assert(!s.sample(t,false));
    unsigned n=0;for(unsigned t=100;t<1100;++t)n+=s.sample(t,true);
    assert(n==20);
    n=0;for(unsigned t=1100;t<3000;++t)n+=s.sample(t,false);
    assert(n==11); // 500ms tail and exactly one final idle edge.
    assert(s.sample(3000,true));assert(!s.sample(3001,true));
    assert(s.sample(10,true)); // Clock rollback starts a fresh observation epoch.
    std::puts("PASS: movement trace 20Hz cap, bounded stop tail, idle silence, restart and clock reset");
}
