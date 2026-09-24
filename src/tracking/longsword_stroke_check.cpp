#include "longsword_stroke.hpp"
#include "longsword_gesture.hpp"
#include "longsword_contact_policy.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace amalur;
void check(bool ok,const char* name){if(!ok){std::printf("FAIL: %s\n",name);std::exit(1);}}
struct Replay {
    LongswordStroke stroke;LongswordGesture combo;mgs5vr::Vec3 hand{},head{};
    uint64_t tick=100;unsigned generation=1,events{};bool allowed=true;
    void run(unsigned ms,mgs5vr::Vec3 velocity={},bool prep=false,bool walk=false){
        for(unsigned t=0;t<ms;t+=10){tick+=10;hand=hand+velocity*.01f;if(walk)head=head+velocity*.01f;
            combo.sample(1,generation,tick,allowed,false,1,false);
            if(stroke.sample(hand,head,{0,0,-1},tick,generation,allowed,prep)&&stroke.commit(tick)){combo.commit(tick);++events;}
        }
    }
    void contact(){if(stroke.contactReady()&&stroke.commit(tick)){combo.commit(tick);++events;}}
};
int main(){
    Replay resetTrace;resetTrace.run(200);resetTrace.tick+=150;resetTrace.run(10);
    check(resetTrace.stroke.resetInfo().count==1&&std::strcmp(resetTrace.stroke.resetInfo().reason,"tracking-gap")==0
        &&resetTrace.stroke.resetInfo().gap==160,"reset diagnostic preserves tracking discontinuity");
    resetTrace.hand.x+=2;resetTrace.run(10);
    check(resetTrace.stroke.resetInfo().count==2&&std::strcmp(resetTrace.stroke.resetInfo().reason,"hand-jump")==0
        &&resetTrace.stroke.resetInfo().handDelta>1.9f,"reset diagnostic preserves jump magnitude");
    resetTrace.allowed=false;resetTrace.run(100);
    check(resetTrace.stroke.resetInfo().count==3&&std::strcmp(resetTrace.stroke.resetInfo().reason,"ineligible")==0,
        "disabled frames retain one reset edge");
    MeleeSwingEvent proposed{};proposed.serial=1;proposed.weapon=12;proposed.asset=5457;proposed.generation=4;proposed.attackAsset=50;
    auto identity=[&](MeleeSwingEvent live,uint64_t frame=100,unsigned attack=50,unsigned flags=0){return sameLongswordContact(proposed,live,12,5457,4,100,frame,attack,flags);};
    check(identity(proposed),"exact contact identity");
    auto wrong=proposed;wrong.serial=2;check(!identity(wrong),"reject stale serial");wrong=proposed;wrong.generation=5;check(!identity(wrong),"reject recenter snapshot");
    wrong=proposed;wrong.weapon=13;check(!identity(wrong),"reject equipment snapshot");wrong=proposed;wrong.asset=2478;check(!identity(wrong),"reject skin snapshot");
    wrong=proposed;wrong.hand=1;check(!identity(wrong),"reject offhand");check(!identity(proposed,101)&&!identity(proposed,100,5,1),"reject frame and retained recipe mismatch");
    check(longswordContactSpeed({5,0,0},{0,0,1},10,100),"5m/s blade slash");
    check(!longswordContactSpeed({4,0,0},{0,0,1},10,100),"slow blade slash");
    check(longswordContactSpeed({0,0,2},{0,0,1},10,100),"2m/s forward stab");
    check(!longswordContactSpeed({0,0,-2},{0,0,1},10,100),"withdrawal is not stab");
    check(!longswordContactSpeed({5,0,0},{0,0,1},0,100)&&!longswordContactSpeed({5,0,0},{0,0,1},101,100),"invalid contact timing");
    for(auto v:{mgs5vr::Vec3{6,0,0},mgs5vr::Vec3{0,-6,0},mgs5vr::Vec3{0,6,0},mgs5vr::Vec3{0,0,-6}}){
        Replay r;r.run(200);r.run(120,v);check(r.events==0,"no threshold crossing event");r.run(100,v*.3f);check(r.events==1,"horizontal/down/up/stab peak");
        r.run(700,v);r.contact();check(r.events==1,"continued movement single event");
    }
    Replay hit;hit.run(200);hit.run(120,{6,0,0});hit.contact();check(hit.events==1&&hit.combo.step()==1,"hit before peak");hit.run(80,{1,0,0});hit.contact();check(hit.events==1&&hit.combo.step()==1,"peak/contact dedup");
    hit.run(600);hit.run(120,{6,0,0});hit.run(80);check(hit.events==2&&hit.combo.step()==2,"same direction recovery");
    Replay reverse;reverse.run(200);reverse.run(120,{6,0,0});reverse.run(600,{1,0,0});reverse.run(180,{-6,0,0});reverse.run(100);check(reverse.events==2,"reversal");
    Replay back;back.run(200);back.run(150,{0,0,6});back.run(100);check(back.events==0,"backward windup");back.run(180,{0,0,-6});back.run(100);check(back.events==1,"forward after windup");
    Replay prep;prep.run(200);prep.run(150,{0,6,0},true);prep.run(160,{0,-6,0});prep.run(100);check(prep.events==1,"shoulder preparation does not consume stroke");
    Replay latePrep;latePrep.run(200);latePrep.run(80,{0,3,0});latePrep.run(80,{0,3,0},true);latePrep.run(160,{0,-6,0});latePrep.run(100);check(latePrep.events==1,"entering preparation cancels candidate without spending rearm");
    Replay tiny;tiny.run(200);tiny.run(10,{6,0,0});tiny.run(300);check(tiny.events==0&&!tiny.stroke.contactReady(),"tiny flick");
    Replay settling;settling.run(300,{0,0,.2f});settling.run(160,{6,0,0});settling.contact();
    check(settling.events==1,"slow backward settling arms first strike");
    settling.run(650,{0,0,.2f});settling.run(160,{6,0,0});settling.contact();
    check(settling.events==2,"slow backward settling rearms next strike");
    Replay noise;noise.run(200);for(int i=0;i<100;++i){noise.run(10,{3,0,0});noise.run(10,{-3,0,0});}check(noise.events==0,"high speed jitter displacement");
    Replay walk;walk.run(200);walk.run(160,{6,0,0},false,true);walk.run(100);check(walk.events==0&&!walk.stroke.contactReady(),"room walking");
    Replay head;head.run(200);for(int i=0;i<40;++i){head.head.x+=.06f;head.run(10);}check(head.events==0&&!head.stroke.contactReady(),"head alone");
    Replay lost;lost.run(200);lost.allowed=false;lost.run(10);lost.allowed=true;lost.run(140,{6,0,0});lost.run(100);check(lost.events==0,"focus tracking weapon reset requires rearm");
    Replay rec;rec.run(200);++rec.generation;rec.run(140,{6,0,0});rec.run(100);check(rec.events==0,"recenter");
    Replay stale;stale.run(200);stale.tick+=200;stale.run(140,{6,0,0});stale.run(100);check(stale.events==0,"tracking gap");
    Replay continuous;continuous.run(350,{6,0,0});continuous.contact();
    check(continuous.events==0&&!continuous.stroke.contactReady(),"startup continuous first leg harmless");
    continuous.run(160,{-6,0,0});continuous.contact();
    check(continuous.events==1,"deliberate return cut rearms without quiet");
    continuous.run(700,{-6,0,0});continuous.contact();
    check(continuous.events==1,"recovered long sweep does not repeatedly emit");
    Replay jump;jump.run(200);jump.hand.x+=2.f;jump.run(160,{6,0,0});jump.contact();
    check(jump.events==0&&!jump.stroke.contactReady(),"tracking jump plus first leg cannot damage");
    jump.run(160,{-6,0,0});jump.contact();check(jump.events==1,"fresh post-jump reversal recovers");
    Replay recoveryGap;recoveryGap.run(160,{6,0,0});recoveryGap.tick+=200;
    recoveryGap.run(160,{-6,0,0});recoveryGap.contact();
    check(recoveryGap.events==0,"gap erases earlier recovery leg");
    recoveryGap.run(160,{6,0,0});recoveryGap.contact();check(recoveryGap.events==1,"new complete legs after gap recover");
    Replay unarmedJitter;for(unsigned i=0;i<100;++i){unarmedJitter.run(10,{3,0,0});unarmedJitter.run(10,{-3,0,0});}
    unarmedJitter.contact();check(unarmedJitter.events==0,"unarmed jitter cannot supply recovery leg");
    Replay movingBody;movingBody.run(200,{6,0,0},false,true);movingBody.run(200,{-6,0,0},false,true);movingBody.contact();
    check(movingBody.events==0,"whole-body reversal cannot supply recovery leg");
    LongswordSeparation separation;
    auto overlap=[&](uint64_t t){separation.beginFrame(t);const bool allowed=separation.observe(7,t);separation.endFrame(true);return allowed;};
    auto emptyScan=[&](uint64_t t,bool complete=true){separation.beginFrame(t);separation.endFrame(complete);};
    check(overlap(100),"first contact");separation.hit(7,100);
    for(uint64_t t=110;t<3000;t+=10)check(!overlap(t),"embedded no fallback damage");
    check(!overlap(3300),"unavailable250ms is not separation");
    check(!overlap(6000),"unavailable2s does not evict embedded actor");
    for(uint64_t t=6010;t<=6250;t+=10)emptyScan(t);
    check(!overlap(6260),"240ms verified absence insufficient");
    for(uint64_t t=6270;t<=6520;t+=10)emptyScan(t);
    check(overlap(6530),"250ms complete absence rearms");separation.hit(7,6530);
    for(uint64_t t=6540;t<=6740;t+=10)emptyScan(t);
    emptyScan(6750,false);
    for(uint64_t t=6760;t<=6860;t+=10)emptyScan(t);
    check(!overlap(6870),"incomplete frame breaks accumulated absence");
    for(uint64_t t=6880;t<=7080;t+=10)emptyScan(t);
    emptyScan(7400);
    for(uint64_t t=7410;t<=7510;t+=10)emptyScan(t);
    check(!overlap(7520),"unavailable interval breaks partial absence");
    for(uint64_t t=7530;t<=7830;t+=10){
        separation.beginFrame(t);
        // The first sphere is empty; a later sphere overlaps the same actor.
        check(!separation.observe(7,t),"later sphere preserves embedded state");
        check(!separation.observe(7,t),"duplicate sphere does not rearm");
        separation.endFrame(true);
    }
    check(!overlap(7840),"multiple sphere scan remains overlapping");
    separation.beginFrame(7850);check(separation.observe(8,7850),"per actor separation");separation.endFrame(true);
    check(!separation.observe(9,7860),"observation outside scan fails closed");
    LongswordSeparation cuts;
    auto cutScan=[&](uint64_t t,uint64_t serial,bool overlaps,bool complete=true,bool hit=false){
        cuts.beginFrame(t,serial);bool allowed=false;
        if(overlaps)allowed=cuts.observe(7,t);
        if(hit&&allowed)cuts.hit(7,t);
        cuts.endFrame(complete);return allowed;
    };
    check(cutScan(100,1,true,true,true),"return-cut initial hit");
    for(unsigned t=110;t<=400;t+=10)check(!cutScan(t,2,true),"new stroke cannot rearm embedded blade");
    cutScan(410,2,false);
    check(cutScan(420,2,true,true,true),"new stroke after complete exit re-hits before250ms");
    cutScan(430,2,false);
    check(!cutScan(440,2,true),"same stroke return stays blocked");
    check(!cutScan(450,3,true),"same stroke reentry consumes exit proof");
    cutScan(460,3,false,false);
    check(!cutScan(470,3,true),"partial empty scan never proves exit");
    cutScan(480,3,false);
    check(!cutScan(800,3,true),"tracking gap invalidates short exit proof");
    cuts.beginFrame(810,3);check(!cuts.observe(7,810),"late sphere overlap blocks exit");cuts.endFrame(true);
    check(!cutScan(820,4,true),"late sphere overlap plus new stroke remains blocked");
    cutScan(830,4,false);
    check(cutScan(840,4,true,true,true),"fresh observed exit restores quick return after gap");
    LongswordGesture heavy;for(uint64_t t=100;t<=1300;t+=10)heavy.sample(1,1,t,true,true,0,false);
    check(heavy.ready(),"charge ready");auto a=heavy.commit(1310),b=heavy.commit(1320);check(a.heavy&&a.attack==81&&!b.heavy&&b.attack==50,"charge consumed once");
    puts("PASS: stroke peaks, early contact, single combo/event, windup, jitter, walking/head, recovery/reversal, resets, separation, heavy");
}
