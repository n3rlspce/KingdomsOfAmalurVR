#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "arm_pose.hpp"
#include "weapon_pose.hpp"
namespace rig_probe {inline uintptr_t playerRoot(){return 0;}}
inline uintptr_t gameBase=0x400000;
inline void log(const char*,...){}
inline bool hook(void*,void*,void**,const char*){return true;}
namespace player_rig {
inline std::atomic<void*> player{};
inline uintptr_t word(uintptr_t p){return *reinterpret_cast<const uint32_t*>(p);}
inline uintptr_t resolve(uint32_t){return 0;}
}
namespace weapon_control {inline mgs5vr::Pose desired{},desiredLeft{};inline uint64_t tick{},leftTick{};inline SRWLOCK poseLock=SRWLOCK_INIT; inline uint64_t visualTick{};inline uint32_t visualWeapon{},visualAsset{};inline uintptr_t fab(uint32_t){return 0;}inline bool authoritativeSelectedWeapon(uintptr_t){return false;}}
namespace motion_controls {struct Controls{unsigned selectedWeapon{};};inline Controls viewControls(){return {};}}
#include "physical_melee_recipe.hpp"
#include "melee_family_policy.hpp"
#include "src/diagnostic/melee_feedback.hpp"
static unsigned calls;
static uintptr_t got[4];
static uintptr_t __fastcall native(void* self,void*,uintptr_t a,uintptr_t b,uintptr_t c){
    ++calls;got[0]=reinterpret_cast<uintptr_t>(self);got[1]=a;got[2]=b;got[3]=c;return 0xabcdef12;
}
static uintptr_t __fastcall native1(void* self,void*,uintptr_t a){++calls;got[0]=reinterpret_cast<uintptr_t>(self);got[1]=a;return 0xabc001;}
static uintptr_t __fastcall native2(void* self,void*,uintptr_t a,uintptr_t b){++calls;got[0]=reinterpret_cast<uintptr_t>(self);got[1]=a;got[2]=b;return 0xabc002;}
static uintptr_t extra[3];
static uintptr_t __fastcall native5(void* self,void*,uintptr_t a,uint32_t b,uintptr_t c,uintptr_t d,uint32_t e){++calls;got[0]=reinterpret_cast<uintptr_t>(self);got[1]=a;got[2]=b;got[3]=c;extra[0]=d;extra[1]=e;return 0xabc005;}
static unsigned voiceAllocations,voiceStops,voicePositions;static uint32_t lastSelector,lastMode;static bool failAllocate;
static uintptr_t __fastcall voiceAllocate(void*,void*,uintptr_t output,uint32_t selector,uint32_t mode,uint32_t a,uint32_t b){
    if(a!=1||b!=0)std::abort();++voiceAllocations;lastSelector=selector;lastMode=mode;
    *reinterpret_cast<uint32_t*>(output)=0x01030000+voiceAllocations%100;return failAllocate?0:0x12340000;
}
static void __fastcall voicePosition(void*,void*,float x,float y,float z){if(x!=1||y!=2||z!=3)std::abort();++voicePositions;}
static void __fastcall voiceStop(void*,void*,uint32_t handle){if(((handle>>16)&255)!=3)std::abort();++voiceStops;}
#define check(b) do { if(!(b)){std::fprintf(stderr,"FAIL line %d\n",__LINE__);std::exit(1);} } while(0)
int main(){
    amalur::MeleeFeedbackInbox box;
    amalur::MeleeSwingEvent e{};e.owner=1;e.weapon=2;e.serial=1;e.tick=1000;
    check(box.accept(e,1000));check(!box.accept(e,1000));
    e.serial=2;e.tick=900;check(!box.accept(e,1000));
    e.tick=1001;check(!box.accept(e,1000));
    e.tick=500;check(!box.accept(e,1000));
    e.tick=1000;e.hand=2;check(!box.accept(e,1000));
    e.hand=1;check(box.accept(e,1000));
    e.hand=0;e.generation=1;e.serial=1;check(box.accept(e,1000));
    e.weapon=3;check(box.accept(e,1000));
    amalur::MeleeFeedbackBudget budget;
    for(unsigned i=0;i<24;++i)check(budget.allow(1000));
    check(!budget.allow(1000));check(budget.dropped==1);
    check(budget.allow(2000)); // Later captures never hit a lifetime cap.
    check(budget.allow(1000)); // Clock discontinuity resets the bucket.
    unsigned char payload[0x40];memset(payload,0xff,sizeof(payload));
    uint32_t fields[4]{};
    melee_feedback::readFields(melee_feedback::Kind::Fx,reinterpret_cast<uintptr_t>(payload),fields);
    check(fields[0]==0xffffffff&&fields[1]==0xffffffff&&fields[2]==0x00ffffff&&fields[3]==0);
    payload[0x28]=1;
    melee_feedback::readFields(melee_feedback::Kind::DerivedSound,reinterpret_cast<uintptr_t>(payload),fields);
    check(fields[2]==1&&fields[0]==0xffffffff&&fields[1]==0xffffffff&&fields[3]==0xffffffff);
    uint32_t event[16]{};
    for(unsigned i=0;i<5;++i)melee_feedback::original[i]=reinterpret_cast<melee_feedback::Callback>(&native);
    using Hook=uintptr_t(__fastcall*)(void*,void*,uintptr_t,uintptr_t,uintptr_t);
    Hook hooks[]{&melee_feedback::dispatch<melee_feedback::Kind::GameSound>,&melee_feedback::dispatch<melee_feedback::Kind::DerivedSound>,
        &melee_feedback::dispatch<melee_feedback::Kind::WeaponFx>,&melee_feedback::dispatch<melee_feedback::Kind::CharacterWeaponFx>,&melee_feedback::dispatch<melee_feedback::Kind::Fx>};
    for(auto h:hooks){check(h(event,nullptr,11,22,33)==0xabcdef12);check(got[0]==reinterpret_cast<uintptr_t>(event)&&got[1]==11&&got[2]==22&&got[3]==33);}
    check(calls==5);
    check(hooks[0](reinterpret_cast<void*>(1),nullptr,11,22,33)==0xabcdef12);
    check(calls==6);check(!melee_feedback::observing.test_and_set());melee_feedback::observing.clear();
    using namespace melee_feedback::resolved;
    scope={};originalSubmit=reinterpret_cast<Audio>(&native1);originalFilter=reinterpret_cast<Audio>(&native1);
    originalStart=reinterpret_cast<Start>(&native2);originalResolve=reinterpret_cast<Resolve>(&native2);
    originalAttach=reinterpret_cast<Attach>(&native5);originalSchedule=reinterpret_cast<Schedule>(&native);
    check(submit(event,nullptr,123)==0xabc001&&got[1]==123);
    check(filter(event,nullptr,124)==0xabc001&&got[1]==124);
    check(start(event,nullptr,125,126)==0xabc002&&got[1]==125&&got[2]==126);
    check(fx(event,nullptr,127,128)==0xabc002&&got[1]==127&&got[2]==128);
    check(attach(event,nullptr,129,130,131,132,133)==0xabc005&&got[1]==129&&got[2]==130&&got[3]==131&&extra[0]==132&&extra[1]==133);
    check(schedule(event,nullptr,134,135,136)==0xabcdef12&&got[1]==134&&got[2]==135&&got[3]==136);
    check(calls==12);
    originalAllocate=reinterpret_cast<Allocate>(&native5);originalInitialize=reinterpret_cast<Initialize>(&native2);
    check(allocate(event,nullptr,140,141,142,143,144)==0xabc005&&got[1]==140&&got[2]==141&&got[3]==142&&extra[0]==143&&extra[1]==144);
    check(initialize(event,nullptr,145,146)==0xabc002&&got[1]==145&&got[2]==146);
    check(calls==14);
    unsigned char pool[0x80]{};uint32_t localPlayer[0x80]{};localPlayer[0x1ec/4]=7;player_rig::player.store(localPlayer);
    const auto identity=reinterpret_cast<uintptr_t>(pool);*reinterpret_cast<uint32_t*>(pool)=99;pool[0x49]=3;
    scope={};scope.trace=123;scope.owner=7;scope.selector=99;scope.kind=melee_feedback::Kind::DerivedSound;
    remember(identity);scope={};check(deferred(identity).trace==123);
    pool[0x49]=4;check(!deferred(identity).trace);pool[0x49]=3;
    localPlayer[0x1ec/4]=8;check(!deferred(identity).trace);localPlayer[0x1ec/4]=7;
    *reinterpret_cast<uint32_t*>(pool)=100;check(!deferred(identity).trace);*reinterpret_cast<uint32_t*>(pool)=99;
    remember(identity);check(!deferred(identity).trace); // Nonlocal reuse invalidates correlation.
    player_rig::player.store(nullptr);scope={};
    const char text[]="weapon_slash";uint32_t record[]={reinterpret_cast<uintptr_t>(text)};uint32_t str[]={reinterpret_cast<uintptr_t>(record),0,12};char copied[512]{};
    check(nameCopy(reinterpret_cast<uintptr_t>(str),copied)&&!strcmp(copied,text));str[2]=512;check(!nameCopy(reinterpret_cast<uintptr_t>(str),copied));
    budgetTick=GetTickCount64();budgetCount=128;budgetDropped=0;check(!allow()&&budgetDropped==1);budgetTick-=1001;check(allow()&&budgetCount==1);
    {
    using namespace melee_feedback::longsword_audio;
    Backend api{reinterpret_cast<Allocate>(&voiceAllocate),reinterpret_cast<Position>(&voicePosition),reinterpret_cast<Stop>(&voiceStop)};
    Driver d;d.update(api,100,7,8,2,true,1000);
    amalur::MeleeSwingEvent a{};a.owner=7;a.weapon=8;a.asset=2478;a.hand=0;a.serial=1;a.generation=2;a.attackAsset=50;a.tick=1000;a.weaponPose.position={1,2,3};
    check(d.swing(api,a,1000)&&voiceAllocations==2&&voicePositions==2&&lastSelector==0x0104d903&&lastMode==0);
    check(!d.swing(api,a,1000));a.serial=2;a.tick=1050;check(!d.swing(api,a,1050));
    a.tick=1200;a.attackAsset=7;a.attackFlags=2;check(d.swing(api,a,1200)&&lastSelector==0x01edd57b);
    d.update(api,100,7,8,2,false,1300);check(voiceStops==4&&!d.eligible&&d.manager==100);
    d.update(api,100,7,8,2,true,2000);a.serial=3;a.tick=2000;a.attackAsset=81;a.attackFlags=1;check(d.swing(api,a,2000));
    d.update(api,100,7,8,2,true,6001);check(voiceStops==6);
    a.serial=4;a.tick=6100;a.asset=1520;check(!d.swing(api,a,6100));a.asset=2478;a.attackAsset=160;check(!d.swing(api,a,6100));
    a.attackAsset=50;a.attackFlags=0;a.tick=5000;check(!d.swing(api,a,6100));a.tick=6100;failAllocate=true;check(!d.swing(api,a,6100));failAllocate=false;
    a.tick=6300;a.serial=5;check(d.swing(api,a,6300));const auto stopped=voiceStops;d.update(api,200,7,8,2,true,6400);check(voiceStops==stopped); // no old-manager writes
    d.update(api,200,7,8,2,false,6500);
    }
    check(!melee_feedback::nativePlaybackAvailable);
    std::puts("PASS longsword owned allocation/position/cancel/watchdog/rejections and feedback freshness/dedup, independent hands, renewing budget, thirteen ABI forwarding wrappers, deferred identity/generation/owner guards, bounded string copy, observation exception forwarding");
}
