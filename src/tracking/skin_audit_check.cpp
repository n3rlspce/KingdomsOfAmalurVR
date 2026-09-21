#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define AMALUR_SKIN_AUDIT_TEST
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <atomic>
#include <array>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include "render_match.hpp"
#include "pose_channel.hpp"
using Microsoft::WRL::ComPtr;
static std::atomic<uint32_t> presents{};
struct PendingMap {ID3D11DeviceContext* context{};ID3D11Resource* resource{};UINT subresource{};void* data{};};
namespace render_pose {
struct Camera {std::array<float,16> vp{};amalur::PosePacket pose;};
inline SRWLOCK lock=SRWLOCK_INIT;inline uint64_t cameraCount{};inline std::array<Camera,128> history;
}
#include "../diagnostic/skin_trace_v4.hpp"
void check(bool pass,const char* message){if(!pass){printf("FAIL: %s\n",message);exit(1);}}
int main(){
    using namespace amalur::skin_audit;
    Coverage c;bool seen[193]{};
    for(unsigned f=0;f<24;++f){unsigned taken=0;for(unsigned i=0;i<193;++i){uint32_t ordinal;
        if(c.select(f,ordinal)){seen[i]=true;++taken;}check(ordinal==i,"ordinal covers entire draw list");}
        check(taken<=24,"bounded window");}
    for(bool s:seen)check(s,"late draws eventually selected despite earlier candidates");
    CopyBudget b;for(unsigned i=0;i<48;++i)check(b.take(1,true),"within upload limit");
    check(!b.take(1,true)&&b.take(1,false)&&b.take(2,true),"budget separated by size and resets per frame");
    skin_trace::ensure();check(skin_trace::memory!=nullptr,"test mapping initialized");
    skin_trace::memory->until=GetTickCount64()+30000;presents=1;
    for(unsigned i=0;i<8;++i)check(skin_trace::reserveCpu(Getter),"bounded getter snapshot admitted");
    check(!skin_trace::reserveCpu(Getter)&&skin_trace::reserveCpu(Remap),"getter flood leaves snapshot capacity for remap");
    CpuRecord cpu;cpu.count=3;cpu.origin=Remap;cpu.trace.root=1;cpu.trace.flags=8;cpu.trace.tick=GetTickCount64();
    for(unsigned object=10;object<15;++object){cpu.trace.object=object;skin_trace::cpu(cpu);}
    for(unsigned repeat=0;repeat<8;++repeat){cpu.trace.object=10;skin_trace::cpu(cpu);}
    cpu.origin=Getter;cpu.trace.object=0;for(unsigned i=0;i<100;++i)skin_trace::cpu(cpu);
    cpu.origin=Remap;cpu.trace.object=10;
    for(unsigned object=10;object<15;++object){bool found=false;
        for(const auto& slot:skin_trace::memory->cpu)for(const auto& r:slot)found|=r.serial&&r.trace.object==object;
        check(found,"hot object does not erase other object histories");}
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"WARP device");
    D3D11_BUFFER_DESC desc{};desc.ByteWidth=256;desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    ComPtr<ID3D11Buffer> world,skin;check(SUCCEEDED(device->CreateBuffer(&desc,nullptr,&world)),"world buffer");
    desc.ByteWidth=3840;check(SUCCEEDED(device->CreateBuffer(&desc,nullptr,&skin)),"live padded skin buffer");
    ID3D11Buffer* bound[]={world.Get(),skin.Get()};context->VSSetConstantBuffers(4,2,bound);
    skin_trace::watch(world.Get(),skin.Get());std::array<float,936> data{};
    // Upload and draw on different threads: shared cache must preserve evidence.
    std::thread uploader([&]{skin_trace::update(world.Get(),data.data());skin_trace::update(skin.Get(),data.data());});uploader.join();
    skin_trace::draw(context.Get(),42);
    check(skin_trace::memory->published==1,"cross-thread upload reaches draw capture");
    auto& record=skin_trace::memory->draws[0];
    check(record.worldSerial&&record.skinSerial&&record.worldBuffer&&record.skinBuffer,"both binding IDs and upload generations present");
    check(record.skinBytes==3840&&(record.flags&SkinCaptured),"padded allocation retains real size and captures bounded shader payload");
    check(record.association==Ambiguous&&record.candidateCount==5,"getter flood cannot starve remaps or become draw candidates");
    // Even one fresh CPU candidate must not be labelled verified.
    memset(skin_trace::recent,0,sizeof(skin_trace::recent));skin_trace::recent[0]=cpu;skin_trace::recent[0].serial=99;
    skin_trace::draw(context.Get(),42);
    check(skin_trace::memory->draws[1].association==Unassociated,"single candidate is still unassociated");
    for(unsigned i=0;i<60;++i)skin_trace::update(skin.Get(),data.data());
    auto before=skin_trace::memory->published;skin_trace::draw(context.Get(),42);
    check(skin_trace::memory->published==before+1&&!(skin_trace::memory->draws[before%128].flags&SkinCaptured)
        &&skin_trace::memory->counters[UploadBudget]>0,"skipped overwrite yields metadata without stale palette");
    // Reused buffers must still be sampled late, not consume all reads on the
    // first actors forever. This exercises the actual writer with WARP bindings.
    for(unsigned f=10;f<28;++f){presents=f;for(unsigned i=0;i<193;++i){
        skin_trace::update(world.Get(),data.data());skin_trace::update(skin.Get(),data.data());skin_trace::draw(context.Get(),42);}}
    bool late=false;for(const auto& d:skin_trace::memory->draws)late|=d.ordinal>100&&d.skinSerial&&d.worldSerial;
    check(late,"reused constant buffer captures late draw uploads within bounded copy budget");
    // Fresh diagnostic epoch, world-only scene: V3's conjunctive watcher made
    // this indistinguishable from broken upload hooks and published nothing.
    skin_trace::memory->until=GetTickCount64()+29000;++presents;skin_trace::coverage={};
    bound[1]=nullptr;context->VSSetConstantBuffers(4,2,bound);
    before=skin_trace::memory->published;skin_trace::draw(context.Get(),7);
    auto& admission=skin_trace::memory->draws[before%128];
    check(admission.worldBytes==256&&admission.skinBytes==0&&!(admission.flags&WorldCaptured),"first draw retains world descriptor without fabricated upload");
    skin_trace::update(world.Get(),data.data());before=skin_trace::memory->published;skin_trace::draw(context.Get(),7);
    auto& worldOnly=skin_trace::memory->draws[before%128];
    check((worldOnly.flags&WorldCaptured)&&!(worldOnly.flags&SkinCaptured)&&worldOnly.worldSerial&&worldOnly.skinSerial==0,"world uploads admitted without a skin binding");
    check(skin_trace::memory->counters[SkinUnbound]>0&&skin_trace::memory->counters[WatchedWorld]>0,"no skin is distinguished from no world watcher");
    // Same 256-byte resource in both slots: keep b4 but reject b5 size.
    bound[1]=world.Get();context->VSSetConstantBuffers(4,2,bound);before=skin_trace::memory->published;skin_trace::draw(context.Get(),7);
    check(skin_trace::memory->draws[before%128].skinBytes==256&&skin_trace::memory->counters[SkinSizeRejected]>0,"unexpected b5 allocation size is visible");
    skin_trace::map(context.Get(),world.Get(),0,data.data());skin_trace::unmap(context.Get(),world.Get(),0);
    check(skin_trace::memory->counters[MapCalls]&&skin_trace::memory->counters[UpdateHookCalls],"map and update hook evidence separate");
    bound[0]=bound[1]=nullptr;context->VSSetConstantBuffers(4,2,bound);before=skin_trace::memory->published;skin_trace::draw(context.Get(),7);
    auto& empty=skin_trace::memory->draws[before%128];
    check(!(empty.flags&(WorldCaptured|SkinCaptured|CameraMatched))&&skin_trace::memory->counters[WorldUnbound]>0,"unbound draw still publishes metadata with no matrices/camera match");
    skin_trace::memory->until=0;++presents;before=skin_trace::memory->published;skin_trace::draw(context.Get(),42);
    check(skin_trace::memory->published==before,"inactive capture publishes nothing");
    printf("PASS: rotating late-draw coverage, bounded reads, per-object retention, cross-thread uploads, explicit ambiguity, overwrite invalidation, opt-in gating; CPU=%zu draw=%zu header=%zu\n",sizeof(CpuRecord),sizeof(DrawRecord),offsetof(Buffer,cpu));
}
