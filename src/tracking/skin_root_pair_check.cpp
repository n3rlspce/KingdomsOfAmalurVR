#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
std::atomic<bool> headTracking{true},interfaceView{false},firstPerson{true};
std::atomic<unsigned> recenterGeneration{};
uintptr_t gameBase{},testRoot{};
namespace arm_rig {inline std::atomic<bool> enabled{true};}
namespace rig_probe {inline uintptr_t playerRoot(){return testRoot;}}
namespace weapon_control {inline uintptr_t fab(uintptr_t p){return p;}}
namespace player_rig {inline uintptr_t word(uintptr_t p){return *reinterpret_cast<uint32_t*>(p);}}
namespace amalur {struct Debug {unsigned value{};unsigned read(){return value;}};inline Debug bodyDebug;}
void log(const char*,...){}
unsigned hooked{},rigidHooked{};
void hook(void*,void*,void**,const char* label){++hooked;if(!strcmp(label,"Rigid weapon packet provenance"))++rigidHooked;}
#include "src/tracking/locomotion_frame.hpp"
namespace render_pose {inline const float* receivedVP{};inline amalur::LocomotionFrame drawn{};inline bool locomotionForVP(const float* vp,amalur::LocomotionFrame& out){receivedVP=vp;out=drawn;return drawn.valid;}}
#include "play_mode.hpp"
#include "src/diagnostic/skin_root_pair.hpp"
void check(bool b,const char* s){if(!b){printf("FAIL %s\n",s);exit(1);}}
void put(uintptr_t p,uint32_t x){memcpy(reinterpret_cast<void*>(p),&x,4);}
unsigned char object[512]{},rootBytes[512]{},entry[20]{},boneBytes[96]{},inst[128]{},gpu[96]{},packet[0x58+96]{},manager[0x500]{},header[16]{};
uint32_t children[1],table[2048],generations[2048];
unsigned char asset[128]{},model[0x5c]{},mapping[0x34]{};
uintptr_t __fastcall fakeAllocate(void*,void*,unsigned){return uintptr_t(packet);}
unsigned char rigidPacket[0x54]{};
uintptr_t __fastcall fakeRigidAllocate(void*,void*,unsigned){return uintptr_t(rigidPacket);}
void __fastcall fakeRigidPublish(void*,void*,uintptr_t,unsigned,uintptr_t,uintptr_t,float){
 auto p=skin_root_pair::allocateRigid(nullptr,nullptr,0x54);
 *reinterpret_cast<uint16_t*>(p)=0x1c;*reinterpret_cast<uint16_t*>(p+2)=0x54;put(p+4,player_rig::word(uintptr_t(entry)+8));
 memcpy(reinterpret_cast<void*>(p+8),inst,45);
}
void __fastcall fakePublish(void*,void*,uintptr_t,unsigned,uintptr_t,uintptr_t,float){
 auto p=skin_root_pair::allocate(nullptr,nullptr,sizeof(packet));
 *reinterpret_cast<uint16_t*>(p)=0x20;*reinterpret_cast<uint16_t*>(p+2)=sizeof(packet);
 put(p+4,player_rig::word(uintptr_t(entry)+8));put(p+0x54,2);memcpy(reinterpret_cast<void*>(p+0x58),gpu,96);
}
uintptr_t expectedClone{},originalInstance{};bool checkedThunk{};
void __cdecl verify(uintptr_t context,uintptr_t instance,unsigned a,unsigned b,unsigned c,unsigned d,unsigned e){
 check(context==123&&a==1&&b==2&&c==3&&d==4&&e==5,"native register/stack arguments");
 check(instance!=originalInstance&&!memcmp(reinterpret_cast<void*>(instance),rootBytes+0x124,48),"draw clone root");
 check(!memcmp(reinterpret_cast<void*>(instance+48),inst+48,80),"instance payload preserved");checkedThunk=true;
}
__declspec(naked) void fakeWorld(){__asm {
 push ebp
 mov ebp,esp
 push [ebp+24]
 push [ebp+20]
 push [ebp+16]
 push [ebp+12]
 push [ebp+8]
 push edx
 push ecx
 call verify
 add esp,28
 mov eax,456
 pop ebp
 ret
}}
int main(){
 using namespace skin_root_pair;
 gameBase=uintptr_t(VirtualAlloc(nullptr,0x1600000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));check(gameBase!=0,"allocation");
 testRoot=uintptr_t(rootBytes);children[0]=uintptr_t(object);put(testRoot+0x24,uintptr_t(children));put(testRoot+0x28,1);put(testRoot+0xf8,99);
 auto ob=uintptr_t(object);put(ob+0xf8,88);put(ob+0x14,uintptr_t(entry));put(ob+0x18,1);put(ob+0x34,uintptr_t(boneBytes));put(ob+0x38,2);put(uintptr_t(entry)+8,0x10000);
 put(gameBase+0x15fdfb4,uintptr_t(manager));put(uintptr_t(manager)+0x4a0,uintptr_t(table));put(uintptr_t(manager)+0x4ac,1);put(uintptr_t(manager)+0x450,uintptr_t(generations));put(uintptr_t(manager)+0x458,2048);generations[0]=0x10000;table[0]=uintptr_t(header);put(uintptr_t(header)+8,uintptr_t(inst));
 put(uintptr_t(inst)+0x30,uintptr_t(gpu));put(uintptr_t(inst)+0x34,2);memset(rootBytes+0x124,7,48);memset(gpu,42,96);
 put(uintptr_t(asset)+0x4c,uintptr_t(model));put(uintptr_t(model)+0x20,1);put(uintptr_t(model)+0x24,uintptr_t(mapping));model[0x50]=4;
 originalAllocate=reinterpret_cast<Allocate>(&fakeAllocate);originalPublish=reinterpret_cast<Publish>(&fakePublish);originalWorld=&fakeWorld;
 remember(ob,testRoot,true);check(bones[0].tick!=0,"remap snapshot captured");
 remember(1234,0,false);check(bones[0].object==ob&&bones[0].tick,"unrelated native remap preserves player snapshot");
 Bones copy{};
 boneBytes[48]=1;check(prepare(uintptr_t(entry),0,uintptr_t(asset),ob+0x34,copy),"unreferenced helper bone changes allowed");
 boneBytes[0]=1;check(!prepare(uintptr_t(entry),0,uintptr_t(asset),ob+0x34,copy),"referenced bone changes rejected");boneBytes[0]=0;
 put(uintptr_t(mapping)+0x30,2);check(!prepare(uintptr_t(entry),0,uintptr_t(asset),ob+0x34,copy),"out of range mapping rejected");put(uintptr_t(mapping)+0x30,0);
 publish(entry,nullptr,testRoot+0x124,0,uintptr_t(asset),ob+0x34,0);check(captured==1,"publication captures palette/root provenance");
 unsigned char out[128];check(select(uintptr_t(inst),out)==uintptr_t(out),"matching palette selected");
 originalInstance=uintptr_t(inst);unsigned result=0;
 __asm {
 push 5
 push 4
 push 3
 push 2
 push 1
 mov ecx,123
 mov edx,originalInstance
 call world
 add esp,20
 mov result,eax
 }
 check(render_pose::receivedVP==reinterpret_cast<const float*>(1),"native VP argument forwarded to pairing");
 check(checkedThunk&&result==456,"thunk return/stack intact");check(inst[0]==0,"original root not mutated");
 gpu[0]^=1;check(select(uintptr_t(inst),out)==uintptr_t(inst),"changed palette rejected");gpu[0]^=1;
 ++recenterGeneration;check(select(uintptr_t(inst),out)==uintptr_t(inst),"recenter invalidates pair");--recenterGeneration;
 put(ob+0xf8,89);check(select(uintptr_t(inst),out)==uintptr_t(inst),"reused owner rejected");put(ob+0xf8,88);
 palettes[1]=palettes[0];palettes[1].world[0]^=1;check(select(uintptr_t(inst),out)==uintptr_t(inst),"ambiguous identical palettes rejected");palettes[1]={};
 palettes[0].tick=GetTickCount64()-251;check(select(uintptr_t(inst),out)==uintptr_t(inst),"stale pair rejected");
 // Rigid weapon: publication root x=20, solved root x=10, attachment x=3.
 // Camera locomotion advances 5. Final weapon x must be 18, not 15 or 28.
 memset(palettes,0,sizeof(palettes));cursor=0;memset(inst,0,sizeof(inst));
 amalur::RigBone source{};source.orientation.w=1;source.positionW=1;source.position.x=10;
 float scale[3]={.9f,.9f,.9f};memcpy(source.opaque,scale,12);source.opaque[12]=0x1e;
 memcpy(rootBytes+0x124,&source,48);
 auto published=source;published.position.x=20;auto attached=source;attached.position.x=23;
 memcpy(inst,&attached,48);model[0x50]=1;put(uintptr_t(model)+0x34,0);
 amalur::LocomotionFrame sourceFrame{};sourceFrame.valid=true;sourceFrame.owner=99;sourceFrame.tick=GetTickCount64();
 sourceFrame.pose.orientation.w=1;sourceFrame.pose.position.x=10;
 remember(ob,testRoot,true,sourceFrame);
 check(prepare(uintptr_t(entry),0,uintptr_t(asset),ob+0x34,copy),"rigid referenced bone accepted");
 boneBytes[0]^=1;check(!prepare(uintptr_t(entry),0,uintptr_t(asset),ob+0x34,copy),"rigid changed bone rejected");boneBytes[0]^=1;
 put(uintptr_t(model)+0x34,2);check(!prepare(uintptr_t(entry),0,uintptr_t(asset),ob+0x34,copy),"rigid out of range bone rejected");put(uintptr_t(model)+0x34,0);
 originalRigidAllocate=reinterpret_cast<Allocate>(&fakeRigidAllocate);originalPublish=reinterpret_cast<Publish>(&fakeRigidPublish);
 publish(entry,nullptr,uintptr_t(&published),0,uintptr_t(asset),ob+0x34,0);
 check(rigidCaptured==1,"rigid native command captured");
 render_pose::drawn=sourceFrame;render_pose::drawn.pose.position.x=15;
 check(select(uintptr_t(inst),out)==uintptr_t(out),"rigid transform matched");
 amalur::RigBone corrected;memcpy(&corrected,out,48);
 check(std::abs(corrected.position.x-18)<.0001f,"weapon offset retained through root pairing and locomotion");
 check(!memcmp(corrected.opaque,scale,12)&&!memcmp(inst,&attached,48),"rigid scale and original instance preserved");
 check(rigidMatched==1&&rigidAdvanced==1,"rigid draw counters");
 inst[47]^=0xff;inst[44]^=0x40;check(select(uintptr_t(inst),out)==uintptr_t(out),"native padding and cached flags do not invalidate rigid pair");
 inst[0]^=1;check(select(uintptr_t(inst),out)==uintptr_t(inst),"different rigid transform rejected");inst[0]^=1;
 palettes[1]=palettes[0];palettes[1].locomotion.pose.position.x+=1;
 check(select(uintptr_t(inst),out)==uintptr_t(inst),"ambiguous rigid locomotion rejected");palettes[1]={};
 // Equipment replacement can use valid sparse indices above both the live
 // allocation count and the unrelated +4ac field (128 in the game capture).
 memset(palettes,0,sizeof(palettes));cursor=0;
 put(uintptr_t(manager)+0x4ac,128);put(uintptr_t(manager)+0x454,100);
 constexpr uint32_t highHandle=0x00400452;constexpr unsigned highIndex=1106;
 table[highIndex]=uintptr_t(header);generations[highIndex]=highHandle;put(uintptr_t(entry)+8,highHandle);
 publish(entry,nullptr,uintptr_t(&published),0,uintptr_t(asset),ob+0x34,0);
 check(select(uintptr_t(inst),out)==uintptr_t(out),"sparse weapon handle above unrelated/live count accepted");
 ++generations[highIndex];
 check(select(uintptr_t(inst),out)==uintptr_t(inst),"recycled renderer handle generation rejected");
 --generations[highIndex];put(uintptr_t(manager)+0x458,highIndex);
 check(renderInstance(uintptr_t(manager),highHandle)==0,"actual capacity boundary rejected");
 put(uintptr_t(manager)+0x458,2048);
 puts("PASS: sparse render pool capacity and generation identity");
 auto turned=source;turned.position={0,0,0};turned.orientation=amalur::nativeQuaternion({0,0,.70710678f,.70710678f});
 auto origin=source;origin.position={0,0,0};auto offset=origin;offset.position.x=3;
 check(rebaseRigid(reinterpret_cast<unsigned char*>(&offset),reinterpret_cast<unsigned char*>(&origin),reinterpret_cast<unsigned char*>(&turned)),"rigid rotating parent accepted");
 check(std::abs(offset.position.x)<.001f&&std::abs(offset.position.y-3)<.001f,"rigid attachment rotates with paired parent");
 puts("PASS: provenance, exact palette matching, native x86 ABI, private clone, owner/recenter/stale/ambiguity rejection");
 puts("PASS: rigid publication, attachment preservation, locomotion, rotation, scale, padding and ambiguity");
 // Check installation independently from the drawing tests.
 const unsigned char pubSig[]={0x81,0xec,0xd8,0,0,0,0xa1},allocSig[]={0x8b,0x89,0xa8,1,0,0},drawSig[]={0x81,0xec,0x0c,1,0,0,0xa1};
 auto pub=reinterpret_cast<unsigned char*>(gameBase+0x8ae460);
 auto alloc=reinterpret_cast<unsigned char*>(gameBase+0x880a90);
 auto rigid=reinterpret_cast<unsigned char*>(gameBase+0x880af0);
 auto draw=reinterpret_cast<unsigned char*>(gameBase+0x892150);
 memcpy(pub,pubSig,sizeof(pubSig));memcpy(alloc,allocSig,sizeof(allocSig));memcpy(draw,drawSig,sizeof(drawSig));
 put(uintptr_t(pub)+7,gameBase+0x157713c);put(uintptr_t(draw)+7,gameBase+0x157713c);
 pub[0x40d]=0xc2;pub[0x40e]=0x14;draw[0x393]=0xc3;
 install();check(hooked==3&&rigidHooked==0,"invalid rigid signature preserves all body hooks");
 memcpy(rigid,allocSig,sizeof(allocSig));rigid[0x52]=0xb9;put(uintptr_t(rigid)+0x53,0x1c);rigid[0x5c]=0xc2;rigid[0x5d]=4;
 hooked=rigidHooked=0;install();check(hooked==4&&rigidHooked==1,"verified rigid allocator installs alongside body hooks");
 puts("PASS: independent rigid signature guard and body fallback installation");
 VirtualFree(reinterpret_cast<void*>(gameBase),0,MEM_RELEASE);
 return 0;
}
