#include "melee_fake_access.hpp"
#include "melee_owned_backend.hpp"
struct Environment {
    using Access=FakeAccess;
    Access calls;bool broken{},released{},expired{},failRelease{};
    bool supported(const amalur::MeleeContextRecipe& r)const{return !broken&&r.owner==7&&r.baseAsset==199&&r.equipmentGeneration==1;}
    uintptr_t part()const{return Access::part;}
    uintptr_t pool()const{return Access::pool;}
    uint32_t target()const{return 0;}
    uint32_t nextKey()const{return 12;}
    bool current(uintptr_t a,uint64_t g)const{return !expired&&a==Access::runtime&&g==1;}
    bool release(uintptr_t a,uint64_t g,uint32_t i,uint32_t asset,uint32_t owner){
        check(a==Access::runtime&&g==1&&i==1&&asset==199&&owner==7,"release original ownership");
        check(calls.memory[Access::records+0x118]==0xffffffffu||!calls.claimedKey,"detach before callbacks");
        if(expired)return true;
        if(failRelease)return false;
        check(!released,"runtime released once");released=true;calls.memory[a+0x1c]=0;return true;
    }
    void fault(const char*){broken=true;}
};
using Backend=amalur::MeleeOwnedBackend<Environment>;
using Scope=amalur::MeleeContextScope<Backend>;
int main(){
    const amalur::MeleeContextRecipe recipe{7,199,199,1};
    for(unsigned i=0;i<10000;++i){Environment e;Backend b(e);Scope s(b);
        check(s.open(recipe)==amalur::MeleeScopeResult::Ready&&s.current(),"complete owned transaction opens");
        check(s.key().key==12&&s.selected().index==1,"owned key and runtime exposed");
        check(s.close()&&s.close()&&e.released&&!e.broken,"idempotent clean retirement");
        check(e.calls.memory[FakeAccess::part+0x28]==1&&e.calls.memory[FakeAccess::records+0x100]==11,"native context preserved");
    }
    {Environment e;e.calls.failCreate=true;Backend b(e);Scope s(b);
        check(s.open(recipe)==amalur::MeleeScopeResult::CreateFailed&&e.broken&&!e.released,"failed creation faults without foreign release");
        check(e.calls.memory[FakeAccess::part+0x28]==1,"failed creation removes empty key");}
    {Environment e;Backend b(e);Scope s(b);check(s.open(recipe)==amalur::MeleeScopeResult::Ready,"expiry setup");
        e.expired=true;check(!s.current()&&s.close()&&!e.released,"expired runtime never freed twice");}
    {Environment e;Backend b(e);Scope s(b);check(s.open(recipe)==amalur::MeleeScopeResult::Ready,"native erase setup");
        e.expired=true;e.calls.claimedKey=0;e.calls.memory[FakeAccess::records+0x118]=88;
        check(s.close()&&!e.released&&e.calls.memory[FakeAccess::records+0x118]==88,"replacement key and runtime preserved");}
    {Environment e;Backend b(e);Scope s(b);check(s.open(recipe)==amalur::MeleeScopeResult::Ready,"release failure setup");
        e.failRelease=true;check(!s.close()&&e.broken&&s.open(recipe)==amalur::MeleeScopeResult::CleanupFailed,"failed cleanup permanently blocks reuse");}
    {Environment e;Backend b(e);Scope s(b);check(s.open(recipe)==amalur::MeleeScopeResult::Ready,"exception setup");
        auto mutations=e.calls.mutations;s.quarantine();check(!s.close()&&e.calls.mutations==mutations&&!e.released,"partial ownership quarantined without unsafe cleanup");}
    std::puts("PASS: 10000 owned transactions, native preservation, expiry, replacement, failure and quarantine");
}
