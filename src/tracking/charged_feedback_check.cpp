#include "charged_feedback.hpp"
#include <cstdio>
#include <cstdlib>
static void check(bool ok,const char* label){if(!ok){printf("FAIL: %s\n",label);std::exit(1);}}
int main(){
    using namespace amalur;
    ChargedFeedbackWriter w;ChargedFeedbackReceiver r;
    auto frame=[&](uint64_t now,bool ready,bool eligible=true){w.sample(11,22,3,44,now,eligible,ready);return r.sample(w.packet(),now,eligible,11,22,3);};
    check(!frame(1000,false).kind,"startup baseline silent");
    auto ready=frame(1010,true);check(ready.kind==1&&ready.milliseconds==45&&ready.amplitude==.35f,"ready edge emits light confirmation");
    for(uint64_t now=1020;now<1200;now+=10)check(!frame(now,true).kind,"steady ready and duplicate packets never repeat");
    check(w.committed(44,3,7,1191),"actual committed heavy accepted");
    auto heavy=r.sample(w.packet(),1191,true,11,22,3);
    check(heavy.kind==2&&heavy.amplitude>ready.amplitude&&heavy.milliseconds>ready.milliseconds,"committed heavy pulse distinct from ready");
    check(!w.committed(44,3,7,1192)&&!r.sample(w.packet(),1192,true,11,22,3).kind,"same contact or peak serial never repeats");
    check(!w.committed(99,3,8,1193)&&!w.committed(44,4,8,1193)&&!w.committed(44,3,0,1193),"wrong weapon generation or empty proposal cannot pulse");
    frame(1200,false);check(!frame(1210,true).kind,"rapid ready edge during heavy pulse suppressed");
    check(!frame(1370,true).kind,"expired heartbeat silently rebaselines instead of replay");
    frame(1380,false);check(frame(1390,true).kind==1,"new ready after stale reset works");
    frame(1400,false);check(!frame(1410,true).kind,"ready cooldown discards excessive repeated edges");
    frame(1420,false);w.sample(11,22,3,44,1430,true,true);check(w.committed(44,3,8,1430),"heavy committed alongside readiness");
    check(r.sample(w.packet(),1430,true,11,22,3).kind==2,"heavy wins simultaneous readiness and commit");
    check(w.committed(44,3,9,1440)&&!r.sample(w.packet(),1440,true,11,22,3).kind,"heavy cooldown drops excessive pulse");
    check(!r.sample(w.packet(),1540,true,11,22,3).kind,"rate-limited pulse never queues for later");
    for(unsigned mode=0;mode<7;++mode){
        w={};r.reset();frame(2000,false);w.sample(11,22,3,44,2010,true,true);
        auto p=w.packet();bool eligible=true;uint32_t pid=11,bridge=22,generation=3;uint64_t now=2010;
        if(mode==0)eligible=false; // menu, focus, tracking, third-person all use this gate
        if(mode==1)pid=12;
        if(mode==2)bridge=23;
        if(mode==3)generation=4;
        if(mode==4)now=2160;
        if(mode==5)p.tick=2011;
        if(mode==6)p.version=2;
        check(!r.sample(p,now,eligible,pid,bridge,generation).kind,"invalid context/freshness/version cannot pulse");
        check(!r.sample(w.packet(),2011,true,11,22,3).kind,"resume with pending old edge silently rebaselines");
        frame(2020,false);check(frame(2030,true).kind==1,"fresh edge after resume remains functional");
    }
    w={};r.reset();check(!frame(3000,true).kind&&!frame(3010,true).kind,"already-ready source on startup never pulses");
    frame(3020,false);check(frame(3030,true).kind==1,"startup suppression ends only at fresh edge");
    w.sample(11,22,4,44,3040,true,true);check(!r.sample(w.packet(),3040,true,11,22,4).kind,"recenter ready state is silent");
    w.sample(11,22,4,45,3050,true,true);check(!r.sample(w.packet(),3050,true,11,22,4).kind,"weapon switch ready state is silent");
    w.sample(11,22,4,45,3060,false,false);check(!w.committed(45,4,10,3061),"inactive source cannot commit haptic");
    check(!r.sample(w.packet(),3061,true,11,22,4).kind,"inactive source cancels receiver");
    puts("PASS: charged haptic ready edges, committed serials, distinct pulses, dedup, priority, cooldown, freshness, startup, focus/menu/tracking, session, recenter and weapon resets");
}
