#include "melee_fake_access.hpp"
int main(){
    FakeAccess a;amalur::MeleeCreation<FakeAccess> calls(a);
    check(!calls.reserve(a.part,7,10)&&a.mutations==0,"existing native key never reserved");
    check(!calls.bind(a.part,7,12,1)&&a.mutations==0,"missing key never reaches unsafe native bind");
    check(calls.reserve(a.part,7,12),"fresh key created after vector relocation");
    auto candidate=calls.create(a.pool,7,199,0);
    check(candidate.valid&&candidate.index==1&&candidate.address==a.runtime,"new active runtime identity verified");
    check(calls.bind(a.part,7,12,candidate.index),"fresh base bound to new key");
    auto before=a.mutations;
    check(!calls.bind(a.part,7,12,2)&&!calls.erase(a.part,7,12)&&a.mutations==before,"bound context never overwritten or erased");
    check(!calls.unbind(a.part,7,12,2)&&a.mutations==before,"foreign runtime index never detached");
    check(calls.unbind(a.part,7,12,1)&&calls.erase(a.part,7,12),"owned record detached then erased");
    check(a.memory[a.records+0x100]==11&&a.memory[a.keys]==10,"native attack survives scoped binding");
    a.failCreate=true;a.reads=0;
    check(calls.create(a.pool,7,199,0).index==0&&a.reads==0,"zero creation failure never indexes pool");
    a.failCreate=false;a.reuseRuntime=true;
    candidate=calls.create(a.pool,7,199,0);
    check(!candidate.valid&&candidate.index==1&&candidate.address==a.runtime,"failed identity preserves candidate for quarantine");
    a.reuseRuntime=false;a.expiredObservation=true;
    candidate=calls.create(a.pool,7,199,0);
    check(!candidate.valid&&candidate.index==1,"matching fields cannot replace expired lifetime observation");
    a.allowed=false;a.reads=0;before=a.mutations;
    check(!calls.reserve(a.part,7,99)&&!calls.bind(a.part,7,99,1)
        &&!calls.unbind(a.part,7,99,1)&&!calls.erase(a.part,7,99)
        &&!calls.create(a.pool,7,199,0).valid,"disabled creation rejects all operations");
    check(a.reads==0&&a.mutations==before,"disabled creation performs no memory access");
    std::puts("PASS: creation, reservation, binding, relocation, identity failure, cleanup and disabled guard");
}
