// Run the actual hook wrappers against local fake native functions. No process
// access, injection or hook installation. Build/run as Win32 to check the ABI.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
inline uintptr_t gameBase{};
namespace player_rig {
inline uintptr_t resolve(uint32_t){std::abort();}
inline uint32_t word(uintptr_t){std::abort();}
}
inline bool hook(void*,void*,void**,const char*){std::abort();}
#include "../diagnostic/melee_lifetime_hooks.hpp"
void check(bool value,const char* message){if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
namespace test {
unsigned resetCalls{},initCalls{},eraseCalls{};
bool doubleReset{},throwInit{};
void __fastcall reset(void*,void*){++resetCalls;}
void __fastcall erase(void*,void*,uint32_t key){check(key==99,"erase ABI preserves key");++eraseCalls;}
int __fastcall initialize(void* self,void*,const uint32_t* asset,uint32_t owner,
    uint32_t target,uint32_t related,uint32_t index,uint32_t extra){
    ++initCalls;
    check(*asset==199&&owner==7&&target==0&&related==7&&index==1&&extra==0,"constructor ABI preserves six arguments");
    melee_lifetime_hooks::reset(self,nullptr);
    if(doubleReset)melee_lifetime_hooks::reset(self,nullptr);
    if(throwInit)RaiseException(0xe0001234,0,0,nullptr);
    return 0;
}
}
bool exceptionalConstructor(const uint32_t* asset){
    __try{
        melee_lifetime_hooks::initialize(reinterpret_cast<void*>(0x1000),nullptr,asset,7,0,7,1,0);
    }__except(GetExceptionCode()==0xe0001234?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){
        return true;
    }
    return false;
}
int main(){
    namespace h=melee_lifetime_hooks;
    check(!h::install(),"disabled gate prevents real hook installation");
    h::originalInitialize=reinterpret_cast<h::Initialize>(&test::initialize);
    h::originalReset=reinterpret_cast<h::Reset>(&test::reset);
    h::originalErase=reinterpret_cast<melee_native::OwnedCalls::EraseFunction>(&test::erase);
    h::installed=true; // Local test fixture only; no patching of an executable.
    uint32_t asset=199;
    void* runtime=reinterpret_cast<void*>(0x1000);
    check(h::begin(),"begin requested constructor");
    std::thread unrelated([&]{h::initialize(reinterpret_cast<void*>(0x2000),nullptr,&asset,7,0,7,1,0);});
    unrelated.join();
    h::initialize(runtime,nullptr,&asset,7,0,7,1,0);
    auto token=h::finish(true);
    check(token.valid&&token.address==0x1000&&h::matches(token.address,token.generation),"other thread cannot claim pending construction");
    h::reset(runtime,nullptr);
    check(!h::matches(token.address,token.generation),"actual reset wrapper invalidates issued token");
    test::doubleReset=true;
    check(h::begin(),"begin reentrant reset");
    h::initialize(runtime,nullptr,&asset,7,0,7,1,0);
    check(!h::finish(true).valid,"actual wrappers reject double reset");
    test::doubleReset=false;test::throwInit=true;
    check(h::begin()&&exceptionalConstructor(&asset),"native constructor exception reaches caller");
    check(!h::finish(false).valid,"exception path issues no valid token");
    test::throwInit=false;
    check(h::begin(),"exception leaves observation ready for explicit next request");
    h::initialize(runtime,nullptr,&asset,7,0,7,1,0);
    check(h::finish(true).valid,"constructor wrapper remains functional after exception");
    auto keyGeneration=h::claimKey(0x3000,99);
    check(keyGeneration&&h::matchesKey(0x3000,99,keyGeneration),"owned key claimed");
    check(!h::claimKey(0x3000,99),"existing key token never overwritten");
    h::erase(reinterpret_cast<void*>(0x3000),nullptr,99);
    check(!h::matchesKey(0x3000,99,keyGeneration)&&test::eraseCalls==1,"erase wrapper expires key before native release");
    auto replacement=h::claimKey(0x3000,99);
    check(replacement!=keyGeneration&&!h::matchesKey(0x3000,99,keyGeneration),"reused key cannot resurrect old token");
    check(test::initCalls==5&&test::resetCalls==7,"native calls execute once per hook invocation");
    std::puts("PASS: Win32 hook ABI, other-thread construction, runtime/key invalidation and reentrant reset");
}
