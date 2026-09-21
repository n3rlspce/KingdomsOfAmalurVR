#include "longsword_hold.hpp"
#include "longsword_gesture.hpp"
#include "melee_swing.hpp"
#include <cstdio>
#include <cstdlib>
void check(bool b,const char* s){if(!b){std::fprintf(stderr,"FAIL %s\n",s);std::exit(1);}}
int main(){
    amalur::LongswordHold hold;amalur::LongswordGesture gesture;uint64_t tick=100;
    for(unsigned i=0;i<130;++i){tick+=10;auto stable=hold.sample({.02f*sinf(float(i)),0,0},tick,1,true);gesture.sample(1,1,tick,true,true,stable?0.f:1.f,false);}
    check(gesture.ready(),"hand jitter permits charge despite noisy instantaneous speed");
    check(!hold.sample({.3f,0,0},tick+10,1,true),"large hand displacement breaks hold");
    check(!hold.sample({.3f,0,0},tick+120,1,true),"tracking gap resets anchor");
    hold.reset();for(unsigned i=0;i<40;++i)hold.sample({0,float(i)*.01f,0},1000+i*10,1,true);
    check(hold.raising(),"upward preparation is detected");
    for(unsigned i=0;i<40;++i)hold.sample({0,.4f-float(i)*.01f,0},1400+i*10,1,true);
    check(!hold.raising(),"downstroke releases preparation gate");
    amalur::MeleeSwing swing;unsigned fired=0;float x=0;tick=100;
    for(int i=0;i<30;++i)swing.sample({x,0,0},tick+=10,1,true,2.f,55);
    for(int i=0;i<40;++i){x+=.012f;fired+=swing.sample({x,0,0},tick+=10,1,true,2.f,55);}
    check(!fired,"slow1.2m/s positioning rejected by longsword policy");
    for(int i=0;i<30;++i)swing.sample({x,0,0},tick+=10,1,true,2.f,55);
    for(int i=0;i<30;++i){x+=.03f;fired+=swing.sample({x,0,0},tick+=10,1,true,2.f,55);}
    check(fired==1,"deliberate3m/s slash accepted once");
    puts("PASS hand-jitter charging, displacement/gap cancellation, raised preparation and longsword speed policy");
}
