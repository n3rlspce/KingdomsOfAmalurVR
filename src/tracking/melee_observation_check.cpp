#include "melee_observation.hpp"
#include <cstdio>
#include <cstdlib>
void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(){
    amalur::MeleeConstructionObservation<2> observed;
    constexpr uintptr_t runtime=0x1000,other=0x2000;
    auto create=[&](){
        check(observed.begin()&&observed.enter(runtime),"begin owned construction");
        observed.reset(runtime);observed.complete(true);return observed.finish(true);
    };
    auto first=create();
    check(first.valid&&observed.lifetimes.matches(runtime,0,first.generation),"successful constructor yields current token");
    observed.reset(runtime);
    check(!observed.lifetimes.matches(runtime,0,first.generation),"native destruction invalidates before reuse");
    auto second=create();
    check(second.valid&&second.generation!=first.generation,"identical address receives distinct lifetime");
    check(observed.begin()&&!observed.begin(),"nested owned requests cannot overwrite outer request");
    check(observed.enter(runtime)&&!observed.enter(other),"nested native constructor cannot steal ownership");
    observed.reset(runtime);observed.reset(other);observed.complete(true);
    check(observed.finish(true).valid,"unrelated nested reset leaves owned runtime intact");
    check(observed.begin()&&observed.enter(runtime),"start reentrant reset case");
    observed.reset(runtime);observed.reset(runtime);observed.complete(true);
    check(!observed.finish(true).valid,"same-address reinitialization during constructor rejects token");
    check(observed.begin()&&observed.enter(runtime),"start retirement-after-constructor case");
    observed.reset(runtime);observed.complete(true);observed.reset(runtime);
    check(!observed.finish(true).valid,"retirement before factory returns rejects token");
    check(observed.begin()&&observed.enter(runtime),"start missing reset case");
    observed.complete(true);
    check(!observed.finish(true).valid,"missing reset hook prevents ownership claim");
    for(bool factorySuccess:{false,true}){
        check(observed.begin()&&observed.enter(runtime),"start failed constructor");
        observed.reset(runtime);observed.complete(!factorySuccess);
        check(!observed.finish(factorySuccess).valid&&!observed.lifetimes.current(runtime),"constructor or factory failure discards token");
    }
    for(unsigned i=0;i<10000;++i){
        check(create().valid,"repeated owned construction remains observable");
        observed.reset(runtime);
    }
    check(observed.lifetimes.healthy(),"observing repeated reuse does not exhaust capacity");
    check(!observed.finish(true).valid,"finish without request exposes no token");
    std::puts("PASS: observed construction, native reset, same-address reuse, nested callbacks and failure paths");
}
