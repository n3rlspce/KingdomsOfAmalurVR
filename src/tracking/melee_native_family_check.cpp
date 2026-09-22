#include "melee_family_policy.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
#define AMALUR_AUDIO_DRIVER_CHECK
#include "../diagnostic/melee_longsword_audio.hpp"
void check(bool ok,const char* label){if(!ok){std::printf("FAIL %s\n",label);std::exit(1);}}
static std::vector<uint32_t> selectors,modes,stopped;
static uintptr_t __fastcall allocate(void*,void*,uintptr_t out,uint32_t selector,uint32_t mode,uint32_t one,uint32_t zero){
    check(one==1&&!zero,"native one-shot flags");selectors.push_back(selector);modes.push_back(mode);
    *reinterpret_cast<uint32_t*>(out)=0x030000+unsigned(selectors.size()%100);return 1;
}
static void __fastcall position(void*,void*,float,float,float){}
static void __fastcall stop(void*,void*,uint32_t handle){stopped.push_back(handle);}
int main(){
    using namespace amalur;
    for(const auto model:{1520u,1250u,1323u}){
        NativeFamilyChain chain;const auto count=model==1323?3u:4u;
        for(unsigned round=0;round<3;++round)for(unsigned i=1;i<=count;++i){
            const auto now=100+round*count*400+i*400;
            const auto r=chain.preview(9,model,1,now);
            check(r.step==i,"normal native chain order");
            check(chain.preview(9,model,1,now).attack==r.attack,"preview does not advance");
            check(chain.commit(9,model,1,now).attack==r.attack,"commit selected recipe");
            DirectWeaponDefinition d{r.scripted?1u:0u,r.scripted?1077u:1u,1,0,0,0,0,r.field208,0x01000104};
            check(matchesFamilyDefinition(r.attack,d),"captured exact definition");
            d.field208^=0x2000;check(!matchesFamilyDefinition(r.attack,d),"reject wrong direct flags");
            check(supportedNativeFamilyAttack(model,r.attack,r.flags)&&!supportedNativeFamilyAttack(model,r.attack,r.flags^1),"exact resolver flags");
            check(!supportedNativeFamilyAttack(model==1520?1250:1520,r.attack,r.flags),"cross family denied");
        }
        check(chain.preview(9,model,1,10000).step==1,"timeout restarts");
        check(chain.preview(10,model,1,chain.last+10).step==1&&chain.preview(9,model,2,chain.last+10).step==1,"weapon/recenter restart");
    }
    for(auto id:{421u,1519u,437u,1517u,1272u,1320u,1441u,1437u,84u,483u,160u,763u})check(!nativeFamilyAttack(id).attack,"complex or magic runtime excluded");
    longsword_audio::Backend api{reinterpret_cast<longsword_audio::Allocate>(allocate),reinterpret_cast<longsword_audio::Position>(position),reinterpret_cast<longsword_audio::Stop>(stop)};longsword_audio::Driver driver;
    driver.update(api,1,3,9,1,true,100);
    MeleeSwingEvent e{};e.owner=3;e.weapon=9;e.generation=1;e.asset=1520;e.attackAsset=199;e.serial=1;e.tick=200;
    check(driver.swing(api,e,200),"right dagger sound");
    check(selectors.size()==2&&selectors[0]==0x78678b&&selectors[1]==0x1c9d985,"captured dagger selectors");
    e.hand=1;check(driver.swing(api,e,200)&&selectors.size()==3,"left dagger same serial independent slash one voice");
    check(!driver.swing(api,e,201),"same hand serial dedup");
    e.hand=2;++e.serial;e.tick=400;check(!driver.swing(api,e,400),"bad hand rejected without indexing");
    e.hand=0;e.asset=1323;e.attackAsset=16;e.attackFlags=0;check(!driver.swing(api,e,400),"audio exact recipe flags");
    e.attackFlags=1;check(driver.swing(api,e,400)&&selectors[3]==0xfca83b&&selectors[4]==0x104d903,"hammer own motion and voice");
    ++e.serial;e.tick=600;e.asset=1250;e.attackAsset=420;e.attackFlags=2;
    check(driver.swing(api,e,600)&&selectors[5]==0x12e49bf&&selectors[6]==0x1edd57b,"greatsword finisher");
    e.asset=1514;++e.serial;e.tick=800;check(!driver.swing(api,e,800),"magic not assigned melee audio");
    driver.update(api,1,3,9,1,false,900);check(stopped.size()==7,"cancel owned sounds");
    check(modes[0]==0xffffffff&&modes[1]==0,"recorded native modes");
    driver.update(api,1,3,9,1,true,1000);
    e={};e.owner=3;e.weapon=9;e.generation=1;e.asset=5457;e.attackAsset=81;e.attackFlags=1;e.heavy=true;e.serial=1;e.tick=1100;
    const auto beforeHeavy=selectors.size();
    check(driver.swing(api,e,1100)&&selectors.size()==beforeHeavy+3,"committed heavy queues captured extra event");
    check(selectors[beforeHeavy]==0x657cab&&selectors[beforeHeavy+1]==0xb40f89&&selectors[beforeHeavy+2]==0x104d903
        &&modes[beforeHeavy+1]==0xffffffff,"heavy keeps ordinary voice and exact supplemental selector/mode");
    check(!driver.swing(api,e,1101)&&selectors.size()==beforeHeavy+3,"contact after committed heavy cannot replay audio");
    ++e.serial;e.tick=1300;e.attackFlags=0;
    check(!driver.swing(api,e,1300)&&selectors.size()==beforeHeavy+3,"wrong heavy flags cannot play supplemental");
    e.attackAsset=50;e.attackFlags=0;e.heavy=false;
    check(driver.swing(api,e,1300)&&selectors.size()==beforeHeavy+5,"ordinary sword retains exactly two sounds");
    check(selectors[beforeHeavy+3]!=0xb40f89&&selectors[beforeHeavy+4]!=0xb40f89,"no heavy supplemental on basic");
    ++e.serial;e.tick=1500;e.attackAsset=78;
    check(!driver.swing(api,e,1500)&&selectors.size()==beforeHeavy+5,"charge preparation never plays heavy release sound");
    driver.update(api,1,3,9,1,false,1600);check(stopped.size()==12,"heavy supplemental follows existing owned cancellation");
    std::puts("PASS native family chain, exact definitions/flags, excluded complex routes, audio selectors, dual-hand dedup and cleanup");
}
