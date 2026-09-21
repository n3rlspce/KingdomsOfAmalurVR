#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
inline uintptr_t gameBase=0x400000;
inline void log(const char*,...){}
inline bool hook(void*,void*,void**,const char*){return true;}
namespace player_rig {
inline std::atomic<void*> player{};
inline uintptr_t word(uintptr_t p){return *reinterpret_cast<const uint32_t*>(p);}
inline uintptr_t resolve(uint32_t){return 0;}
}
namespace weapon_control {inline SRWLOCK poseLock=SRWLOCK_INIT; inline uint64_t visualTick{};inline uint32_t visualWeapon{},visualAsset{};inline uintptr_t fab(uint32_t){return 0;}}
namespace motion_controls {struct Controls{unsigned selectedWeapon{};};inline Controls viewControls(){return {};}}
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
static void check(bool b){if(!b)std::abort();}
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
    const char text[]="weapon_slash";uint32_t record[]={reinterpret_cast<uintptr_t>(text)};uint32_t str[]={reinterpret_cast<uintptr_t>(record),0,12};char copied[512]{};
    check(nameCopy(reinterpret_cast<uintptr_t>(str),copied)&&!strcmp(copied,text));str[2]=512;check(!nameCopy(reinterpret_cast<uintptr_t>(str),copied));
    budgetTick=GetTickCount64();budgetCount=128;budgetDropped=0;check(!allow()&&budgetDropped==1);budgetTick-=1001;check(allow()&&budgetCount==1);
    check(!melee_feedback::nativePlaybackAvailable);
    std::puts("PASS feedback freshness/dedup, independent hands, renewing budget, eleven ABI forwarding wrappers, bounded string copy, observation exception forwarding");
}
