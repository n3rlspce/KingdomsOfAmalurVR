#include "melee_vfx_lifetime.hpp"
#include <cstdio>
#include <cstdlib>
#include <set>
using namespace amalur;
void ck(bool x,const char* s){if(!x){std::printf("FAIL %s\n",s);std::exit(1);}}
struct Backend {
 std::set<unsigned> live;unsigned next=1,creates{},stops{},poses{};bool attachOK=true,scheduleOK=true,cancelOK=true;
 bool owns(const MeleeVfxEpoch&,unsigned id){return live.count(id)!=0;}
 bool create(const MeleeVfxEpoch&,unsigned& id){id=next++;live.insert(id);++creates;return true;}
 bool attach(const MeleeVfxEpoch&,unsigned,const MeleeSwingEvent&){return attachOK;}
 bool schedule(const MeleeVfxEpoch&,unsigned,const MeleeSwingEvent&,const MeleeVfxRecipe&){return scheduleOK;}
 bool cancel(const MeleeVfxEpoch&,unsigned id){++stops;if(!cancelOK)return false;return live.erase(id)==1;}
 bool pose(const MeleeVfxEpoch&,unsigned,const MeleeSwingEvent&){++poses;return true;}
};
int main(){
 MeleeVfxEpoch ep{1,2,3,4,5,5457,1,1};MeleeVfxRecipe recipe{88,200,true,true,true,true};
 MeleeSwingEvent e{};e.owner=4;e.weapon=5;e.asset=5457;e.generation=1;e.serial=1;e.tick=100;
 Backend b;MeleeVfxLifetime d;d.update(b,ep,true,100);ck(d.start(b,e,recipe,100),"start");
 ck(!d.start(b,e,recipe,100)&&b.creates==1,"dedup");e.hand=1;ck(d.start(b,e,recipe,100),"independent hands");
 e.hand=0;e.serial=2;ck(d.start(b,e,recipe,100)&&b.stops==1&&b.live.size()==2,"cancel before replacement");
 d.update(b,ep,false,110);ck(b.live.empty(),"focus loss cleanup");d.update(b,ep,true,110);e.tick=110;
 ck(!d.start(b,e,recipe,110),"paused serial never replayed");e.serial=3;ck(d.start(b,e,recipe,110),"new swing");
 const auto old=d.slot(0).group;b.live.erase(old);b.live.insert(old);++ep.poolGeneration;
 const auto stops=b.stops;d.update(b,ep,true,120);ck(b.stops==stops&&d.abandoned()==1,"same-address manager reset never cancels recycled index");
 e.tick=120;e.serial=4;ck(d.start(b,e,recipe,120),"new manager epoch");
 b.live.erase(d.slot(0).group);d.update(b,ep,false,121);ck(b.stops==stops&&d.abandoned()==2,"retired group is never stopped again");
 d.update(b,ep,true,130);e.tick=130;e.serial=5;b.scheduleOK=false;ck(!d.start(b,e,recipe,130)&&!d.slot(0).group,"schedule rollback");
 b.scheduleOK=true;e.serial=6;ck(d.start(b,e,recipe,130),"post rollback");d.update(b,ep,true,430);ck(!d.slot(0).group,"finite expiry");
 Backend bad;MeleeVfxLifetime fail;fail.update(bad,ep,true,100);e.tick=100;e.serial=1;bad.attachOK=false;bad.cancelOK=false;
 ck(!fail.start(bad,e,recipe,100)&&fail.faulted(),"failed rollback latches closed");bad.attachOK=bad.cancelOK=true;
 fail.update(bad,ep,true,101);e.serial=2;e.tick=101;ck(!fail.start(bad,e,recipe,101)&&bad.creates==1,"latched cleanup failure cannot allocate");
 std::puts("PASS VFX owned per-hand lifecycle, expiry, reset generation, stale token, rollback and fail-closed cleanup");
}
