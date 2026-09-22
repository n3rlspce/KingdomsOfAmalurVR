#include "longsword_hold.hpp"
#include "longsword_gesture.hpp"
#include "melee_swing.hpp"
#include "longsword_stroke.hpp"
#include "weapon_contact_profile.hpp"
#include <cstdio>
#include <cstdlib>
void check(bool b,const char* s){if(!b){std::fprintf(stderr,"FAIL %s\n",s);std::exit(1);}}
int main(){
    check(amalur::longswordChargePose({.3f,-.3f,-.3f}),"shoulder in front charges");
    check(amalur::longswordChargePose({.3f,.65f,.4f}),"fully overhead behind shoulder charges");
    check(amalur::longswordChargePose({.3f,-.25f,.5f}),"rear shoulder charges independent of blade pitch");
    check(!amalur::longswordChargePose({.3f,-.5f,.4f}),"low hand cannot charge");
    check(amalur::longswordContact.count==13&&amalur::rustyLongswordContact.count==11,"one added tip sphere per sword");
    check(std::fabs(amalur::longswordContact.centers[11].z-85.8f)<.001f&&std::fabs(amalur::longswordContact.centers[12].z-93.6f)<.001f,"existing fitted spheres unchanged and one interval added");
    check(std::fabs(amalur::rustyLongswordContact.centers[10].z-71.1189f)<.001f&&amalur::rustyLongswordContact.radius==4.f,"rusty tip extended along own axis");
    amalur::LongswordHold hold;amalur::LongswordGesture gesture;uint64_t tick=100;
    for(unsigned i=0;i<130;++i){tick+=10;auto stable=hold.sample({.02f*sinf(float(i)),0,0},tick,1,true);gesture.sample(1,1,tick,true,true,stable?0.f:1.f,false);}
    check(gesture.ready(),"hand jitter permits charge despite noisy instantaneous speed");
    check(!hold.sample({.3f,0,0},tick+10,1,true),"large hand displacement breaks hold");
    check(!hold.sample({.3f,0,0},tick+120,1,true),"tracking gap resets anchor");
    hold.reset();for(unsigned i=0;i<40;++i)hold.sample({0,float(i)*.01f,0},1000+i*10,1,true);
    check(hold.raising(),"upward preparation is detected");
    hold.sample({0,.35f,0},1400,1,true);check(!hold.raising(),"first downstroke sample releases preparation immediately");
    for(unsigned i=0;i<40;++i)hold.sample({0,.4f-float(i)*.01f,0},1400+i*10,1,true);
    check(!hold.raising(),"downstroke releases preparation gate");
    amalur::LongswordStroke stroke;amalur::LongswordGesture overhead;
    mgs5vr::Vec3 high{.3f,.65f,.4f};uint64_t at=3000;
    for(unsigned i=0;i<130;++i){at+=10;overhead.sample(1,1,at,true,amalur::longswordChargePose(high),0,false);stroke.sample(high,{},{0,0,-1},at,1,true,false);}
    check(overhead.ready(),"behind overhead hold readies heavy");
    for(unsigned i=0;i<8;++i){at+=10;high.y-=.06f;stroke.sample(high,{},{0,0,-1},at,1,true,false);overhead.sample(1,1,at,true,amalur::longswordChargePose(high),1,false);}
    check(stroke.contactReady()&&stroke.commit(at),"early overhead contact admitted within80ms");
    check(overhead.commit(at).heavy,"early overhead contact consumes heavy");
    amalur::LongswordHold repositionHold;amalur::LongswordGesture repositionCharge;amalur::LongswordStroke repositionStroke;
    mgs5vr::Vec3 raisedHand{.3f,-.2f,.4f};uint64_t time=6000;unsigned repositionEvents=0;
    auto move=[&](unsigned frames,float dy){for(unsigned i=0;i<frames;++i){time+=10;raisedHand.y+=dy;
        const bool stable=repositionHold.sample(raisedHand,time,1,true);
        const bool preparation=raisedHand.y>-.45f&&(repositionCharge.ready()?repositionHold.risingNow():repositionHold.raising());
        if(repositionStroke.sample(raisedHand,{},{0,0,-1},time,1,true,preparation))++repositionEvents;
        repositionCharge.sample(1,1,time,true,amalur::longswordChargePose(raisedHand),stable?0.f:1.f,false);
    }};
    move(130,0);check(repositionCharge.ready(),"ready before upward reposition");
    move(10,.06f);move(15,0);
    check(repositionCharge.ready()&&!repositionStroke.contactReady()&&!repositionEvents,"fast raising farther overhead preserves charge without a strike");
    move(8,-.06f);check(repositionStroke.contactReady()&&repositionStroke.commit(time)&&repositionCharge.commit(time).heavy,"reposition then immediate downstroke remains heavy");
    amalur::MeleeSwing swing;unsigned fired=0;float x=0;tick=100;
    for(int i=0;i<30;++i)swing.sample({x,0,0},tick+=10,1,true,2.f,55);
    for(int i=0;i<40;++i){x+=.012f;fired+=swing.sample({x,0,0},tick+=10,1,true,2.f,55);}
    check(!fired,"slow1.2m/s positioning rejected by longsword policy");
    for(int i=0;i<30;++i)swing.sample({x,0,0},tick+=10,1,true,2.f,55);
    for(int i=0;i<30;++i){x+=.03f;fired+=swing.sample({x,0,0},tick+=10,1,true,2.f,55);}
    check(fired==1,"deliberate3m/s slash accepted once");
    puts("PASS hand-jitter charging, displacement/gap cancellation, raised preparation and longsword speed policy");
}
