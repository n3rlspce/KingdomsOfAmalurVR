#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "arm_pose.hpp"
#include "weapon_pose.hpp"
#include "melee_vfx_lifetime.hpp"
inline uintptr_t gameBase{};
template<class... T>void log(const char*,T...){}
namespace player_rig {inline uintptr_t word(uintptr_t p){return *reinterpret_cast<uintptr_t*>(p);}}
namespace rig_probe {inline uintptr_t playerRoot(){return 0;}}
namespace weapon_control {inline SRWLOCK poseLock=SRWLOCK_INIT;inline mgs5vr::Pose desired{},desiredLeft{};inline uint64_t tick{},leftTick{};inline uintptr_t fab(uintptr_t){return 0;}inline bool authoritativeSelectedWeapon(uintptr_t){return false;}}
namespace resolved {
using Resolve=uintptr_t(__thiscall*)(void*,uintptr_t,uint32_t);
using Attach=uintptr_t(__thiscall*)(void*,uintptr_t,uint32_t,uintptr_t,uintptr_t,uint32_t);
using Schedule=uintptr_t(__thiscall*)(void*,uint32_t,uintptr_t,uintptr_t);
inline Resolve originalResolve{};inline Attach originalAttach{};inline Schedule originalSchedule{};
}
inline unsigned hooked{};
bool hook(unsigned char* address,void*,void** out,const char*){++hooked;*out=address;return true;}
#include "../diagnostic/melee_vfx_native.hpp"
void ck(bool value,const char* text){if(!value){std::printf("FAIL %s\n",text);std::exit(1);}}
inline unsigned resets{},releases{};
void __fastcall fakeReset(void*,void*){++resets;}
void __fastcall fakeRelease(uint32_t* handle,void*){++releases;*handle=0;}
void initialize(unsigned char* m){
 auto set=[&](unsigned r,const unsigned char* p,size_t n){memcpy(m+r,p,n);};
 const unsigned char create[]{0xa1,0,0,0,0,0x83,0x78,0x18,0,0x57,0x8b,0xf9};set(0x93cea0,create,sizeof(create));
 const unsigned char release[]{0xa1,0,0,0,0,0x8b,0x50,0x04,0x53,0x56,0x8b,0xd9};set(0x91bce0,release,sizeof(release));
 *reinterpret_cast<uintptr_t*>(m+0x93cea1)=gameBase+0x15fdf60;*reinterpret_cast<uintptr_t*>(m+0x91bce1)=gameBase+0x15fdf60;
 const unsigned char reset[]{0x56,0x8b,0xf1,0x8b,0x46,0x04,0x57};set(0x935460,reset,sizeof(reset));
 const unsigned char pose[]{0x8b,0x49,0x10,0x8b,0x44,0x24,0x04,0x8b,0x54,0x24,0x08};set(0x8b1780,pose,sizeof(pose));
 const unsigned char compose[]{0x83,0xec,0x20,0x53,0x55,0x33,0xd2,0x56,0x8b,0x74,0x24,0x30};set(0x6c5770,compose,sizeof(compose));
 const unsigned char type[]{0xb8,0xb8,0x70,0x26,0,0xc3};set(0x622f90,type,sizeof(type));
 const unsigned char resolve[]{0x83,0xec,0x14,0x8b,0x44,0x24,0x1c};set(0x9cfc80,resolve,sizeof(resolve));
 const unsigned char attach[]{0x53,0x55,0x56,0x57,0x8b,0x7c,0x24,0x14};set(0x90baf0,attach,sizeof(attach));
 const unsigned char schedule[]{0x83,0xec,0x5c,0x56,0x57,0x8b,0xf9};set(0x8f52f0,schedule,sizeof(schedule));
}
int main(){
 using namespace native_vfx;
 auto* image=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x1700000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));ck(image,"image allocation");gameBase=reinterpret_cast<uintptr_t>(image);initialize(image);
 install();ck(ready&&hooked==3,"validated native routes work without capture marker");
 ready=false;hooked=0;image[0x8f52f0]=0xe9;native_vfx::resolve=nullptr;native_vfx::attach=nullptr;native_vfx::schedule=nullptr;
 install();ck(!ready&&!hooked,"unknown hook without verified trampoline rejected");
 resolved::originalResolve=reinterpret_cast<resolved::Resolve>(gameBase+0x100);
 resolved::originalAttach=reinterpret_cast<resolved::Attach>(gameBase+0x200);
 resolved::originalSchedule=reinterpret_cast<resolved::Schedule>(gameBase+0x300);
 install();ck(ready&&native_vfx::schedule==resolved::originalSchedule,"observer trampoline used");
 originalReset=reinterpret_cast<ResetCall>(&fakeReset);owned[0]={77,88,poolEpoch,1,700};const auto oldEpoch=poolEpoch;
 reset(reinterpret_cast<void*>(gameBase+0x400),nullptr);ck(resets==1&&poolEpoch!=oldEpoch&&!lookup(700).group,"same-address manager reset invalidates owned index before native reset");
 originalRelease=reinterpret_cast<GroupCall>(&fakeRelease);owned[1]={99,111,poolEpoch,0,701};uint32_t index=99;
 released(&index,nullptr);ck(releases==1&&!index&&!lookup(701).group,"external native release invalidates reused index");
 owned[1]={99,112,poolEpoch,0,702};
 ck(!lookup(701).group&&lookup(702).group==99,"same native index reused by another hand cannot revive old token");
 uintptr_t definition[1]{gameBase+0x1000};*reinterpret_cast<uintptr_t*>(gameBase+0x101c)=gameBase+0x622f90;
 ck(trailDefinition(reinterpret_cast<uintptr_t>(definition)),"exact FXTrail getter accepted");
 *reinterpret_cast<uintptr_t*>(gameBase+0x101c)=gameBase+0x622f91;ck(!trailDefinition(reinterpret_cast<uintptr_t>(definition)),"other effect class rejected");
 amalur::RigBone b{};float scales[]{1,1,1};memcpy(b.opaque,scales,12);ck(finiteScale(b),"finite native scales accepted");scales[1]=NAN;memcpy(b.opaque,scales,12);ck(!finiteScale(b),"invalid native scale rejected");
 amalur::MeleeSwingEvent right{},left{};right.hand=0;right.asset=1520;right.attackAsset=199;right.owner=1;right.weapon=2;right.generation=3;left=right;left.hand=1;
 poseIdentity[0].committed(right);poseIdentity[1].committed(left);
 auto rp=poseIdentity[0].current(1,2,1520,3,200),lp=poseIdentity[1].current(1,2,1520,3,200);
 ck(rp.hand==0&&lp.hand==1&&amalur::meleeVfxSelection(rp).attachment==0x858053&&amalur::meleeVfxSelection(lp).attachment==0xceac76,"per-hand committed aliases remain independent");
 VirtualFree(image,0,MEM_RELEASE);puts("PASS native VFX binding guards, observer coexistence, in-place reset/reuse and type/scale validation");
}
