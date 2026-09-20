// Offline x86 adapter checks: fake memory only, no game process or native calls.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
inline uintptr_t gameBase{};
namespace player_rig {
inline std::unordered_map<uintptr_t,uint32_t> memory;
inline unsigned reads{};
inline uintptr_t resolve(uint32_t){std::abort();}
inline uint32_t word(uintptr_t address){
    ++reads;
    auto found=memory.find(address);
    if(found==memory.end()){std::fprintf(stderr,"Unexpected read: %zx\n",size_t(address));std::abort();}
    return found->second;
}
}
#include "../diagnostic/melee_native.hpp"
#include "../diagnostic/melee_owned_calls.hpp"
#include "melee_lifetime.hpp"
void check(bool pass,const char* message){if(!pass){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(){
    static_assert(!melee_native::customContactEnabled,"Default build must keep owned contact disabled");
    constexpr uintptr_t table=0x1000,runtime=0x2000;
    auto valid=[&](int32_t index=1,uint32_t count=2){return melee_native::runtimeCurrent(table,count,index,runtime,42,7);};
    check(!valid(-1)&&!valid(0)&&!valid(2)&&!valid(1,0)&&!valid(1,1048577),"invalid bounds reject before any table access");
    check(player_rig::reads==0,"invalid bounds do not access memory");
    player_rig::memory={{table+4,runtime},{runtime+0x1c,1},{runtime+0x20,1},{runtime+0x24,7},{runtime+4,42}};
    check(valid(),"active matching runtime accepted");
    player_rig::memory[runtime+0x1c]=0;
    check(!valid(),"inactive slot rejected even when pointer, asset and owner match");
    player_rig::memory[runtime+0x1c]=1;player_rig::memory[runtime+0x24]=8;
    check(!valid(),"same pointer and asset reused by another actor rejected");
    player_rig::memory[runtime+0x24]=7;player_rig::memory[runtime+0x20]=3;
    check(!valid(),"runtime index identity mismatch rejected");
    player_rig::memory[runtime+0x20]=1;player_rig::memory[runtime+4]=43;
    check(!valid(),"same pointer reused for a different asset rejected");
    player_rig::memory[table+4]=0x3000;
    check(!valid(),"changed table pointer rejected without reading replacement");
    player_rig::memory.clear();player_rig::reads=0;
    melee_native::ready=true;
    melee_native::Context context;context.owner=7;context.baseTalent=context.selectedTalent=1;
    float from[3]{},to[3]{1,0,0};
    check(melee_native::contact(1,context,from,to,4.f)==0,"disabled adapter returns no contacts");
    melee_native::HitArray hits;
    check(!melee_native::gather(1,7,from,to,4.f,hits),"disabled query makes no native calls");
    melee_native::resolveHits(1,context,hits,from,to);
    amalur::MeleeLifetimeRegistry<2> lifetimes;
    auto generation=lifetimes.replace(runtime);
    check(melee_native::retireOwnedRuntime(lifetimes,runtime,generation,1,42,7)
        ==melee_native::RetirementResult::Disabled,"disabled adapter cannot retire live runtimes");
    check(lifetimes.matches(runtime,0,generation),"disabled retirement preserves ownership token");
    melee_native::OwnedCalls nativeCalls;
    nativeCalls.lifetimeObserversReady=nativeCalls.onNativeUpdateThread=true;
    check(!nativeCalls.enabled(),"owned native calls remain disabled even with readiness flags set");
    nativeCalls.reserve(1,1);nativeCalls.bind(1,1,1);nativeCalls.erase(1,1);nativeCalls.write(1,1);
    check(nativeCalls.create(1,199,7,0)==0,"disabled native constructor makes no call");
    check(player_rig::reads==0,"disabled adapter makes no context reads or native calls");
    std::puts("PASS: native runtime bounds, inactive/recycled identity rejection and disabled contact guard");
}
