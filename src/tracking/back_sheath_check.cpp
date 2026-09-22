#include "back_sheath.hpp"
#include <cstdio>
#include <cstdlib>
using namespace amalur;
static void check(bool good,const char* message){if(!good){printf("FAIL: %s\n",message);std::exit(1);}}
struct Replay {
    BackSheathGesture policy;BackSheathInput input;
    Replay(bool sheathed=false){input={{.25f,-.2f,.3f},{0,0,-1},1000,11,22,33,0,true,false,sheathed};policy.sample(input);}
    BackSheathResult sample(uint64_t elapsed,float grip){input.tick=1000+elapsed;input.grip=grip;return policy.sample(input);}
};
int main(){
    check(backSheathRequestCurrent(1000,1099,1000,1000,true),"queued action accepts every timestamp up to ninety-nine milliseconds old");
    check(!backSheathRequestCurrent(1000,1010,1010,1010,false),"tracking input identity or context loss cancels an otherwise fresh queue");
    check(!backSheathRequestCurrent(1000,1100,1100,1100,true),"request expires at one hundred milliseconds even with fresh current input");
    check(!backSheathRequestCurrent(1090,1100,1000,1100,true),"fresh request cannot use stale hand pose");
    check(!backSheathRequestCurrent(1090,1100,1100,1000,true),"fresh request cannot use stale grip input");
    for(unsigned which=0;which<3;++which){
        uint64_t times[]{1000,1000,1000};times[which]=0;
        check(!backSheathRequestCurrent(times[0],1010,times[1],times[2],true),"missing request hand or input timestamp rejected");
        times[which]=1011;
        check(!backSheathRequestCurrent(times[0],1010,times[1],times[2],true),"future request hand or input timestamp rejected");
    }
    check(!backSheathRequestCurrent(1000,999,1000,1000,true),"clock rollback cannot revive a pending action");
    {
        // Model dispatch's consume-before-validation contract: after context
        // loss, restoring the context cannot deliver the discarded request.
        uint64_t queued=1000;const auto taken=queued;queued=0;
        check(!backSheathRequestCurrent(taken,1010,1010,1010,false),"queued gesture rejected after context loss");
        check(!backSheathRequestCurrent(queued,1020,1020,1020,true),"consumed invalid gesture never fires on context recovery");
        check(!backSheathRequestCurrent(1000,1500,1500,1500,true),"expired gesture never fires later with fresh tracking");
    }
    check(backSheathZone({.25f,-.2f,.3f},{0,0,-1}),"neutral back shoulder zone");
    check(backSheathZone({-.3f,-.2f,.25f},{1,0,0}),"zone follows ninety-degree head yaw");
    check(backSheathZone({.25f,-.2f,.3f},{0,.8f,-.6f}),"head pitch does not rotate zone upward");
    check(!backSheathZone({.25f,-.2f,.3f},{0,.999f,-.001f})&&!backSheathZone({.25f,-.2f,.3f},{0,1,0}),"near vertical heading rejected");
    check(!backSheathZone({.25f,-.2f,-.3f},{0,0,-1}),"front of head excluded");
    check(!backSheathZone({.25f,-.2f,.12f},{0,0,-1})&&!backSheathZone({.25f,-.2f,.65f},{0,0,-1}),"strict back depth bounds");
    check(!backSheathZone({-.21f,-.2f,.3f},{0,0,-1})&&!backSheathZone({.76f,-.2f,.3f},{0,0,-1}),"side bounds");
    check(!backSheathZone({.25f,-.76f,.3f},{0,0,-1})&&!backSheathZone({.25f,.36f,.3f},{0,0,-1}),"height bounds");
    check(!backSheathZone({.7f,-.7f,.5f},{0,0,-1}),"overall reach bound");
    {
        Replay r;check(r.sample(10,.65f).claimed,"grip press claims back gesture");
        check(r.sample(60,.5f).claimed,"grip hysteresis preserves claim");
        auto result=r.sample(100,.35f);check(result.action==BackSheathAction::Sheath&&!result.claimed,"short release sheaths exactly once");
        check(r.sample(110,0).action==BackSheathAction::None,"released tap cannot repeat");
    }
    {
        Replay r(true);r.sample(10,1);
        for(uint64_t t=60;t<460;t+=50)check(r.sample(t,1).action==BackSheathAction::None,"hold waits four hundred fifty milliseconds");
        auto result=r.sample(460,1);check(result.action==BackSheathAction::Draw&&result.claimed,"hold draws once at threshold");
        r.input.sheathed=false;
        for(uint64_t t=510;t<=710;t+=50)check(r.sample(t,1).action==BackSheathAction::None,"held draw never toggles repeatedly");
        check(r.sample(750,0).action==BackSheathAction::None,"release after successful draw cannot sheath");
        r.sample(800,1);check(r.sample(850,0).action==BackSheathAction::Sheath,"new press after draw can sheath");
    }
    {
        Replay r;r.sample(10,1);
        for(uint64_t t=60;t<=510;t+=50)check(r.sample(t,1).action==BackSheathAction::None,"hold while drawn has no toggle action");
        check(r.sample(520,0).action==BackSheathAction::None,"long hold drawn cannot become short sheath tap");
        Replay sheathed(true);sheathed.sample(10,1);check(sheathed.sample(100,0).action==BackSheathAction::None,"short tap while sheathed does not draw");
    }
    {
        Replay r(true);r.sample(10,1);r.input.handRelative.z=-.2f;
        check(r.sample(60,1).claimed,"exit keeps claimed grip");r.input.handRelative.z=.3f;
        for(uint64_t t=110;t<=510;t+=50){auto result=r.sample(t,1);check(result.claimed&&result.action==BackSheathAction::None,"reenter cannot revive cancelled hold");}
        check(!r.sample(520,0).claimed,"cancelled grip released");
        Replay tap;tap.sample(10,1);tap.input.handRelative.z=-.2f;tap.sample(60,1);tap.input.handRelative.z=.3f;
        check(tap.sample(100,0).action==BackSheathAction::None,"exit and reenter cannot revive sheath tap");
    }
    {
        Replay r(true);r.input.handRelative.z=-.3f;check(!r.sample(10,1).claimed,"outside grip stays available to other controls");
        r.input.handRelative.z=.3f;
        for(uint64_t t=60;t<=510;t+=50){auto result=r.sample(t,1);check(!result.claimed&&result.action==BackSheathAction::None,"outside press moving inside never claims or draws");}
    }
    for(unsigned kind=0;kind<8;++kind){
        Replay r(true);r.sample(10,1);
        if(kind==0)r.input.eligible=false;
        if(kind==1)r.input.spell=true;
        if(kind==2)++r.input.session;
        if(kind==3)++r.input.generation;
        if(kind==4)++r.input.weapon;
        if(kind==5)r.input.handRelative.x=NAN;
        auto result=r.sample(kind==6?200:kind==7?5:60,1);
        check(result.action==BackSheathAction::None&&!result.claimed,"focus spell session recenter weapon tracking gap clock reset cancels");
        r.input.eligible=true;r.input.spell=false;r.input.handRelative.x=.25f;
        for(uint64_t t=250;t<=800;t+=50)check(!r.sample(t,1).claimed,"held resume remains disarmed");
        r.sample(810,0);check(r.sample(820,1).claimed,"fresh release rearms after reset");
    }
    {
        Replay r;r.sample(10,1);r.input.sheathed=true;
        check(r.sample(60,0).action==BackSheathAction::None,"external native state change cancels action");
        Replay repeated(true);repeated.sample(10,1);
        for(uint64_t t=60;t<460;t+=50)repeated.sample(t,1);
        check(repeated.sample(460,1).action==BackSheathAction::Draw&&repeated.sample(460,1).action==BackSheathAction::None,"same timestamp draw is deduplicated");
        Replay threshold;threshold.sample(10,1);for(uint64_t t=60;t<460;t+=50)threshold.sample(t,1);
        check(threshold.sample(460,0).action==BackSheathAction::Sheath,"exact threshold release while drawn follows inclusive tap rule");
    }
    puts("PASS: back-sheath yaw zone, tap/hold/native state, grip claim, hysteresis, exit/reenter, dedup, focus/spell/tracking/session/recenter/weapon reset and release rearm");
}
