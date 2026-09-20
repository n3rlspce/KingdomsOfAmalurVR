#include <cstdint>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
static uintptr_t gameBase{};
static void log(const char*,...){}
static void hook(void*,void*,void**,const char*){}
#include "../diagnostic/near_clip.hpp"
static float observed;
static void __fastcall frustum(void* core,void*){observed=*reinterpret_cast<float*>(static_cast<unsigned char*>(core)+0x24);}
static void check(bool ok,const char* message){if(!ok){std::printf("FAIL: %s\n",message);std::exit(1);}}
int main(){
    alignas(16) unsigned char core[0x360]{};
    auto& near=*reinterpret_cast<float*>(core+0x24);auto& far=*reinterpret_cast<float*>(core+0x28);
    near_clip::native=reinterpret_cast<near_clip::BuildFrustum>(&frustum);
    near=20;far=200000;
    {near_clip::Scope scope(core,true);near_clip::onFrustum(core,nullptr);}
    check(observed==2&&near==2&&far==200000,"native frustum receives reduced near; far unchanged");
    near_clip::restore(core);check(near==20&&(core[0x35e]&1),"native near restored and camera marked dirty");
    {near_clip::Scope scope(core,false);near_clip::onFrustum(core,nullptr);}
    check(observed==20,"non-first-person frustum unchanged");
    near=1;{near_clip::Scope scope(core,true);near_clip::onFrustum(core,nullptr);}
    check(observed==1,"already smaller near plane preserved");
    near=20;{near_clip::Scope scope(core,true);near_clip::onFrustum(core,nullptr);}near=12;near_clip::restore(core);
    check(near==12,"new native camera input is not overwritten on restoration");
    std::puts("PASS: scoped near override, native frustum input, restoration and smaller/native-update preservation");
}
