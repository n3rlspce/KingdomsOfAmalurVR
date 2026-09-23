#include "interaction_trace_schedule.hpp"
#include <cassert>
int main(){using namespace amalur;InteractionTraceSchedule s;
 assert(!s.sample(true,false,1).down);
 auto e=s.sample(true,true,10);assert(e.down&&e.edge==1&&!e.post100);
 assert(!s.sample(true,true,20).down);
 e=s.sample(true,false,40);assert(e.release&&!e.post100);
 assert(!s.sample(true,false,109).post100);
 assert(s.sample(true,false,110).post100);
 assert(!s.sample(true,false,111).post100);
 assert(s.sample(true,false,510).post500);
 assert(!s.sample(true,false,511).post500);
 assert(s.sample(true,true,600).down);
 s.sample(false,false,601);assert(!s.sample(true,false,1200).post500);
 assert(s.sample(true,true,1300).down);
 assert(!s.sample(true,true,1200).post100); // rollback cancels
 s.sample(true,false,1400);
 for(unsigned n=3;n<128;++n){assert(s.sample(true,true,1500+n*2).down);s.sample(true,false,1501+n*2);}
 e=s.sample(true,true,2000);assert(!e.down&&e.exhausted);
 s.sample(true,false,2001);assert(!s.sample(true,true,2002).exhausted);
 s.sample(true,false,62000);
 e=s.sample(true,true,62001);assert(e.down&&e.edge==129);
 InteractionTraceSchedule delayed;delayed.sample(true,true,10);
 e=delayed.sample(true,false,700);assert(e.release&&e.post100&&e.post500);
}
