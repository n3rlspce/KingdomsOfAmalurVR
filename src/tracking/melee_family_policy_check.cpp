#include "melee_family_policy.hpp"
#include "longsword_stroke.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace amalur;
static void check(bool ok,const char* message){if(!ok){std::printf("FAIL %s\n",message);std::exit(1);}}
struct Replay {
    LongswordStroke hands[2];mgs5vr::Vec3 positions[2]{};uint64_t tick=100;uint32_t model=1520;
    void run(unsigned ms,mgs5vr::Vec3 right={},mgs5vr::Vec3 left={}){
        for(unsigned i=0;i<ms;i+=10){tick+=10;positions[0]=positions[0]+right*.01f;positions[1]=positions[1]+left*.01f;
            for(unsigned h=0;h<2;++h)hands[h].sample(positions[h],{},{0,0,-1},tick,1,true,false,strokeRecoveryMs(model));
        }
    }
};
int main(){
    check(strokeRecoveryMs(1520)==180&&strokeRecoveryMs(2478)==250&&strokeRecoveryMs(5457)==250
        &&strokeRecoveryMs(1250)==300&&strokeRecoveryMs(1323)==350,"explicit provisional family recovery");
    for(const auto model:{1520u,1250u,1323u}){
        const auto recipe=physicalMeleeRecipe(model);
        for(unsigned hand=0;hand<recipe.hands;++hand){
            MeleeSwingEvent event{};event.asset=model;event.weapon=9;event.serial=7;event.generation=3;event.hand=hand;
            check(assignBasicStrokeRecipe(event),"captured basic recipe");
            auto match=[&](MeleeSwingEvent live,uint64_t frame=100){return sameMeleeContact(event,live,9,model,3,hand,100,frame,recipe.attack,recipe.flags);};
            check(match(event),"exact model/hand recipe accepted");
            auto bad=event;bad.hand=1-hand;check(!match(bad),"cross-hand rejected");
            bad=event;++bad.serial;check(!match(bad),"stale serial rejected");
            bad=event;++bad.generation;check(!match(bad),"recenter rejected");
            bad=event;++bad.weapon;check(!match(bad),"equipment rejected");
            bad=event;++bad.asset;check(!match(bad),"model rejected");
            bad=event;++bad.attackFlags;check(!match(bad),"flags rejected");
            bad=event;++bad.attackAsset;check(!match(bad),"attack rejected");
            bad=event;bad.heavy=true;check(!match(bad),"uncaptured heavy rejected");
            bad=event;bad.chainStep=2;check(!match(bad),"uncaptured combo rejected");
            check(!match(event,101),"pose frame mismatch rejected");
        }
        check(!supportedMeleeHand(model,recipe.hands),"unsupported extra hand rejected");
        check(meleeContactSpeed(model,{5,0,0},{0,0,1},10,100),"common slash speed admitted");
        check(!meleeContactSpeed(model,{4,0,0},{0,0,1},10,100),"slow contact rejected");
        check(meleeContactSpeed(model,{0,0,2},{0,0,3},10,100)==(model!=1323),"normalized stab axis; hammer has no stab discount");
        check(!meleeContactSpeed(model,{0,0,-2},{0,0,1},10,100),"withdrawal not stab");
        check(!meleeContactSpeed(model,{5,0,0},{},10,100),"invalid axis rejected");
        check(!meleeContactSpeed(model,{5,0,0},{0,0,1},0,100),"invalid timing rejected");
        check(!meleeContactSpeed(model,{5,0,0},{0,0,1},10,std::numeric_limits<float>::quiet_NaN()),"invalid scale rejected");
    }
    for(const auto model:{0u,1514u,1689u,1877u,9999u}){
        MeleeSwingEvent event{};event.asset=model;
        check(!assignBasicStrokeRecipe(event)&&!supportedMeleeHand(model,0),"unsupported family closed");
        check(!meleeContactSpeed(model,{50,0,0},{0,0,1},10,100),"unsupported contact closed");
    }
    MeleeSwingEvent sword{};sword.asset=5457;sword.attackAsset=81;sword.attackFlags=1;sword.heavy=true;
    check(!assignBasicStrokeRecipe(sword)&&sword.attackAsset==81&&sword.heavy,"preserve longsword heavy ownership");
    Replay dual;dual.run(200);dual.run(120,{6,0,0},{-6,0,0});
    check(dual.hands[0].contactReady()&&dual.hands[1].contactReady(),"simultaneous dagger candidates");
    check(dual.hands[0].commit(dual.tick)&&dual.hands[1].commit(dual.tick),"both hands independently commit");
    check(!dual.hands[0].commit(dual.tick)&&!dual.hands[1].commit(dual.tick),"no duplicate accounting");
    dual.hands[0].reset();check(dual.hands[1].emitted(),"right tracking reset preserves left stroke");
    LongswordSeparation separation[2];for(auto& s:separation)s.beginFrame(100);
    check(separation[0].observe(22,100)&&separation[1].observe(22,100),"per hand actor observation");
    separation[0].hit(22,100);for(auto& s:separation){s.endFrame(true);s.beginFrame(110);}
    check(!separation[0].observe(22,110)&&separation[1].observe(22,110),"right embedded does not consume left contact");
    for(auto& s:separation)s.endFrame(true);
    for(const auto model:{1520u,2478u,5457u,1250u,1323u}){
        Replay fast;fast.model=model;fast.run(200);fast.run(120,{6,0,0},{-6,0,0});
        const auto first=fast.tick;
        for(unsigned h=0;h<physicalMeleeRecipe(model).hands;++h)check(fast.hands[h].commit(fast.tick),"first family contact");
        // Keep moving in the first direction beyond recovery; time alone must
        // not create another swing even at the new, shorter recovery values.
        fast.run(static_cast<unsigned>(strokeRecoveryMs(model)),{6,0,0},{-6,0,0});
        for(unsigned h=0;h<physicalMeleeRecipe(model).hands;++h)check(!fast.hands[h].commit(fast.tick),"continuous stroke never rearms");
        fast.run(120,{-6,0,0},{6,0,0});
        check(fast.tick-first<550,"family reversal precedes old 550ms recovery");
        for(unsigned h=0;h<physicalMeleeRecipe(model).hands;++h)
            check(fast.hands[h].contactReady()&&fast.hands[h].commit(fast.tick),"fresh family reversal commits independently");
    }
    std::puts("PASS exact family recipes, hand/epoch/frame identity, contact speed units, hammer head gate, unsupported families, dual independence, per-family recovery/reversal and continuous-sweep dedup");
}
