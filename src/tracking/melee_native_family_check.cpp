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
    check(driver.swing(api,e,1100)&&selectors.size()==beforeHeavy+2,"committed heavy starts native pair and queues supplemental");
    driver.update(api,1,3,9,1,true,1228);
    check(selectors.size()==beforeHeavy+2,"supplemental waits native129ms delay");
    driver.update(api,1,3,9,1,true,1229);
    check(selectors.size()==beforeHeavy+3,"supplemental plays at129ms");
    driver.update(api,1,3,9,1,true,1230);
    check(selectors.size()==beforeHeavy+3,"supplemental only once");
    check(selectors[beforeHeavy]==0x657cab&&selectors[beforeHeavy+1]==0x104d903&&selectors[beforeHeavy+2]==0xb40f89
        &&modes[beforeHeavy+2]==0xffffffff,"heavy keeps ordinary voice and exact supplemental selector/mode");
    check(!driver.swing(api,e,1231)&&selectors.size()==beforeHeavy+3,"contact after committed heavy cannot replay audio");
    ++e.serial;e.tick=1300;e.attackFlags=0;
    check(!driver.swing(api,e,1300)&&selectors.size()==beforeHeavy+3,"wrong heavy flags cannot play supplemental");
    e.attackAsset=50;e.attackFlags=0;e.heavy=false;
    check(driver.swing(api,e,1300)&&selectors.size()==beforeHeavy+5,"ordinary sword retains exactly two sounds");
    check(selectors[beforeHeavy+3]!=0xb40f89&&selectors[beforeHeavy+4]!=0xb40f89,"no heavy supplemental on basic");
    ++e.serial;e.tick=1500;e.attackAsset=78;
    check(!driver.swing(api,e,1500)&&selectors.size()==beforeHeavy+5,"charge preparation never plays heavy release sound");
    driver.update(api,1,3,9,1,false,1600);check(stopped.size()==12,"heavy supplemental follows existing owned cancellation");
    // Delayed supplemental ownership must survive neither cancellation nor a
    // weapon/recenter change; a stalled update must not replay a stale release.
    for(unsigned scenario=0;scenario<4;++scenario){
        const uint64_t start=2000+scenario*1000;
        driver.update(api,1,3,9,1,true,start);
        e={};e.owner=3;e.weapon=9;e.generation=1;e.asset=5457;e.attackAsset=81;e.attackFlags=1;e.serial=10+scenario;e.tick=start+10;
        const auto count=selectors.size();
        check(driver.swing(api,e,start+10)&&selectors.size()==count+2,"pending supplemental setup");
        if(scenario==0)driver.update(api,1,3,9,1,false,start+20);
        if(scenario==1)driver.update(api,1,3,10,1,true,start+20);
        if(scenario==2)driver.update(api,1,3,9,2,true,start+20);
        driver.update(api,1,3,9,1,true,start+400);
        check(selectors.size()==count+2,"canceled or stale supplemental never plays");
        driver.update(api,1,3,9,1,false,start+500);
    }
    driver.update(api,1,3,9,1,true,6500);
    e={};e.owner=3;e.weapon=9;e.generation=1;e.asset=5457;e.attackAsset=81;e.attackFlags=1;e.serial=20;e.tick=6510;
    check(driver.swing(api,e,6510),"new strike cancellation setup");
    e.attackAsset=50;e.attackFlags=0;e.serial=21;e.tick=6610;
    check(driver.swing(api,e,6610),"new same-hand ordinary strike");
    const auto countAfterReplacement=selectors.size();
    driver.update(api,1,3,9,1,true,6639);
    check(selectors.size()==countAfterReplacement,"new same-hand strike cancels prior pending release cue");
    driver.update(api,1,3,9,1,false,6700);
    {
        longsword_audio::Driver resumed;
        MeleeSwingEvent stroke{};stroke.owner=3;stroke.weapon=9;stroke.asset=1520;stroke.attackAsset=199;stroke.generation=1;stroke.serial=77;stroke.tick=8000;
        resumed.update(api,1,3,9,1,true,8000);
        check(resumed.swing(api,stroke,8000),"first event accepted on initial eligible frame");
        stroke.hand=1;check(resumed.swing(api,stroke,8000),"left first event independently accepted");
        const auto played=selectors.size();
        resumed.update(api,1,3,9,1,false,8010);resumed.update(api,1,3,9,1,false,8020);
        check(!resumed.swing(api,stroke,8020),"disabled playback cannot bypass eligibility");
        resumed.update(api,1,3,9,1,true,8030);
        check(!resumed.swing(api,stroke,8030),"left consumed stroke cannot replay after brief focus loss");
        stroke.hand=0;check(!resumed.swing(api,stroke,8030)&&selectors.size()==played,"right consumed stroke cannot replay after brief focus loss");
        ++stroke.serial;stroke.tick=8100;
        check(resumed.swing(api,stroke,8100),"fresh stroke after resume accepted");
        for(unsigned change=0;change<3;++change){
            if(change==0)++stroke.owner;if(change==1)++stroke.weapon;if(change==2)++stroke.generation;
            stroke.tick=8200+change*100;
            resumed.update(api,1,stroke.owner,stroke.weapon,stroke.generation,true,stroke.tick);
            check(resumed.swing(api,stroke,stroke.tick),"new actor weapon or center can accept same serial immediately");
        }
        resumed.reset(api,1,8500);
    }
    struct SoundCase {uint32_t model,attack,flags,motion,voice;};
    const SoundCase soundCases[]{
        {5457,50,0,0x657cab,0x104d903},{5457,5,1,0x657cab,0x104d903},{5457,7,2,0x657cab,0x1edd57b},
        {1520,199,0,0x78678b,0x1c9d985},{1520,200,0,0x78678b,0},
        {1520,201,2,0x78678b,0x1c9d985},{1520,202,0,0x78678b,0x1c9d985},
        {1250,417,0,0x12e49bf,0x104d903},{1250,418,1,0x12e49bf,0x104d903},
        {1250,419,1,0x12e49bf,0x104d903},{1250,420,2,0x12e49bf,0x1edd57b},
        {1323,16,1,0xfca83b,0x104d903},{1323,17,1,0xe72c9a,0x104d903},{1323,18,2,0xe78ed9,0x1edd57b}};
    for(const auto& c:soundCases){const auto sounds=nativeSwingSounds(c.model,c.attack,c.flags);
        check(sounds.motion==c.motion&&sounds.voice==c.voice&&!sounds.supplemental,"exact ordinary family selector catalog");}
    std::puts("PASS native family chain, exact definitions/flags, excluded complex routes, audio selectors, dual-hand dedup and cleanup");
}
