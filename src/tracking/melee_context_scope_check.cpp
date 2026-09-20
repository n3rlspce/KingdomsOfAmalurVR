#include "melee_context_scope.hpp"
#include "melee_lifetime.hpp"
#include "melee_retirement.hpp"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <utility>
using namespace amalur;
void check(bool value,const char* message){if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
struct FakeNative {
    struct Runtime {uint64_t generation;uint32_t owner,asset;};
    std::map<uint32_t,Runtime> pool;
    std::map<uint32_t,uint64_t> keys{{10,1}}; // Unrelated native attack.
    uint64_t generation=2,equipment=1;
    unsigned creates{},failCreate{},released{},erased{};
    bool reject{},failReserve{},failBind{},expireOnBind{},failUnbind{},attached{},failRelease{},failErase{};
    bool supported(const MeleeContextRecipe& r)const{return !reject&&r.equipmentGeneration==equipment;}
    MeleeKeyToken reserve(uint32_t){
        if(failReserve)return {};
        uint32_t key=10;while(keys.count(key))++key;
        keys[key]=generation++;
        return {key,keys[key]};
    }
    MeleeRuntimeToken create(uint32_t owner,uint32_t asset){
        if(++creates==failCreate)return {};
        uint32_t index=1;while(pool.count(index))++index;
        pool[index]={generation++,owner,asset};return {index,pool[index].generation};
    }
    bool bind(uint32_t,MeleeKeyToken,MeleeRuntimeToken,MeleeRuntimeToken){
        if(expireOnBind)++equipment;
        attached=!failBind;
        return !failBind;
    }
    bool unbind(uint32_t,MeleeKeyToken,MeleeRuntimeToken,MeleeRuntimeToken){
        if(failUnbind)return false;
        attached=false;return true;
    }
    bool matches(uint32_t owner,MeleeKeyToken k,MeleeRuntimeToken b,MeleeRuntimeToken s)const{
        auto key=keys.find(k.key);
        if(key==keys.end()||key->second!=k.generation)return false;
        for(auto token:{b,s}){
            auto item=pool.find(token.index);
            if(item==pool.end()||item->second.generation!=token.generation||item->second.owner!=owner)return false;
        }
        return true;
    }
    bool erase(uint32_t,MeleeKeyToken key){
        if(failErase)return false;
        auto item=keys.find(key.key);
        if(item!=keys.end()&&item->second==key.generation){keys.erase(item);++erased;}
        return true;
    }
    bool release(MeleeRuntimeToken token){
        check(!attached,"references detached before effect-complete callbacks can run");
        if(failRelease)return false;
        auto item=pool.find(token.index);
        if(item!=pool.end()&&item->second.generation==token.generation){pool.erase(item);++released;}
        return true;
    }
    bool clean()const{return pool.empty()&&keys.size()==1&&keys.at(10)==1;}
};
int main(){
    MeleeNotification notifications[]{{0,1,7,12},{0,1,7,13},{0,1,8,14}};
    auto plan=planMeleeRetirement(7,12,13,notifications,3);
    check(plan.valid&&plan.count==2&&plan.ids[0]==12&&plan.ids[1]==13,"retire both owned notifications");
    plan=planMeleeRetirement(7,12,12,notifications,3);
    check(plan.valid&&plan.count==1,"aliased notification cancelled once");
    plan=planMeleeRetirement(7,99,0xffffffffu,notifications,3);
    check(plan.valid&&plan.count==0,"missing or invalid notification requires no cancellation");
    check(!planMeleeRetirement(7,12,14,notifications,3).valid,"foreign second ID rejects before cancelling first");
    notifications[2]={0,1,7,12};
    check(!planMeleeRetirement(7,12,13,notifications,3).valid,"duplicate native ID rejects ambiguous ownership");
    check(!planMeleeRetirement(0,12,13,notifications,3).valid,"reserved runtime zero rejected");
    check(!planMeleeRetirement(7,12,13,nullptr,3).valid,"missing notification vector rejected");
    MeleeLifetimeRegistry<2> reusable;
    auto retired=reusable.replace(0x1000,10);
    auto collision=reusable.replace(0x1000,12);
    check(reusable.retire(0x1000,10,retired),"retire current lifetime");
    check(!reusable.matches(0x1000,10,retired),"retired token immediately invalid");
    check(reusable.matches(0x1000,12,collision),"tombstone does not break collision chain");
    auto next=reusable.replace(0x1000,12);
    check(next!=collision&&reusable.retire(0x1000,12,next),"replace finds existing entry beyond tombstone");
    for(unsigned i=0;i<10000;++i){
        auto token=reusable.replace(0x1000,i+20);
        check(token&&reusable.retire(0x1000,i+20,token),"retired keys do not exhaust registry over time");
    }
    auto again=reusable.replace(0x1000,10);
    check(again!=retired&&!reusable.retire(0x1000,10,retired),"late cleanup cannot retire new generation");
    check(reusable.matches(0x1000,10,again),"new generation survives stale cleanup");
    MeleeLifetimeRegistry<2> lifetimes;
    const auto first=lifetimes.replace(0x1000,10);
    check(first&&lifetimes.matches(0x1000,10,first),"observed lifetime has a valid token");
    const auto replacement=lifetimes.replace(0x1000,10);
    check(replacement!=first&&!lifetimes.matches(0x1000,10,first),"same address and key never resurrect old token");
    check(lifetimes.replace(0x1000,12)!=0,"hash collision preserves distinct key identity");
    check(lifetimes.matches(0x1000,10,replacement),"collision does not overwrite prior entry");
    check(lifetimes.replace(0x2000,0)==0&&!lifetimes.healthy(),"registry exhaustion disables ownership claims");
    check(!lifetimes.matches(0x1000,10,replacement),"exhaustion rejects all previously issued tokens");
    MeleeContextRecipe recipe{7,42,43,1};
    for(unsigned fail=0;fail<6;++fail){
        FakeNative native;
        native.reject=fail==0;native.failReserve=fail==1;
        native.failCreate=fail==2?1:fail==3?2:0;native.failBind=fail==4;native.expireOnBind=fail==5;
        MeleeContextScope<FakeNative> scope(native);
        check(scope.open(recipe)!=MeleeScopeResult::Ready,"injected failure prevents damage readiness");
        check(!scope.current()&&!scope.key()&&!scope.selected(),"failure exposes no usable context");
        scope.close();scope.close();
        check(native.clean(),"each failure unwinds owned resources and preserves native attack");
    }
    FakeNative native;
    {
        MeleeContextScope<FakeNative> scope(native);
        check(scope.open(recipe)==MeleeScopeResult::Ready,"fresh independent context ready");
        check(scope.key().key!=10&&native.keys.at(10)==1,"existing native key is never overwritten");
        const auto selected=scope.selected();const auto key=scope.key();
        // Worst-case ABA: same index, owner and asset, but a new lifetime.
        ++native.pool.at(selected.index).generation;
        ++native.keys.at(key.key);
        check(!scope.current(),"ABA reuse rejects despite matching owner and asset");
        scope.close();
        check(native.pool.count(selected.index)==1&&native.keys.count(key.key)==1,"cleanup never frees a replacement lifetime");
        check(native.released==1,"still-owned base released once");
    }
    FakeNative sameAsset;
    {
        MeleeContextScope<FakeNative> scope(sameAsset);
        auto same=recipe;same.selectedAsset=same.baseAsset;
        check(scope.open(same)==MeleeScopeResult::Ready&&sameAsset.creates==1,"base-selected alias creates one runtime");
        ++sameAsset.equipment;
        check(!scope.current(),"equipment change invalidates readiness");
    }
    check(sameAsset.clean()&&sameAsset.released==1,"destructor balances alias once");
    FakeNative uncertain;
    {
        MeleeContextScope<FakeNative> scope(uncertain);
        check(scope.open(recipe)==MeleeScopeResult::Ready,"cleanup-failure fixture ready");
        uncertain.failUnbind=true;
        check(!scope.close(),"unsafe detachment reports cleanup failure");
        check(uncertain.released==0&&uncertain.erased==0,"failed detachment never frees referenced runtimes");
        check(scope.open(recipe)==MeleeScopeResult::CleanupFailed,"cleanup failure prevents further allocations");
        check(uncertain.creates==2,"failed scope cannot silently accumulate more runtimes");
    }
    FakeNative repeated;
    for(bool failRelease:{false,true}){
        FakeNative failure;
        MeleeContextScope<FakeNative> scope(failure);
        check(scope.open(recipe)==MeleeScopeResult::Ready,"late cleanup-failure fixture ready");
        failure.failRelease=failRelease;failure.failErase=!failRelease;
        check(!scope.close(),"release or key erasure failure is reported");
        check(scope.open(recipe)==MeleeScopeResult::CleanupFailed,"late cleanup failure prevents new damage contexts");
    }
    for(unsigned i=0;i<1000;++i){
        MeleeContextScope<FakeNative> scope(repeated);
        check(scope.open(recipe)==MeleeScopeResult::Ready,"repeated scoped creation succeeds");
    }
    check(repeated.clean()&&repeated.released==2000&&repeated.erased==1000,"repeated contacts do not accumulate contexts");
    std::puts("PASS: scoped contexts, failure rollback, key collision, lifetime ABA, equipment invalidation and repeated cleanup");
}
