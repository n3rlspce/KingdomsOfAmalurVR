#pragma once
#include <array>
#include "../tracking/locomotion_frame.hpp"
#include "../tracking/render_match.hpp"
// Bind the source pose to the world matrix that actually reaches a draw. The
// engine updates cameras ahead of rendering, on a different thread. Present
// must never read the latest simulation camera and call it the rendered pose.
namespace render_pose {
static std::atomic<uint64_t> descriptorTicks{},scanTicks{},descriptorCalls{},scanCalls{},comparisons{},matches{};
static uint64_t counter(){LARGE_INTEGER t;QueryPerformanceCounter(&t);return static_cast<uint64_t>(t.QuadPart);}
struct Timer {std::atomic<uint64_t>& total;uint64_t start{counter()};~Timer(){total.fetch_add(counter()-start,std::memory_order_relaxed);}};
struct Performance {double descriptorMs{},scanMs{};uint64_t descriptors{},scans{},candidates{},matched{};};
static Performance performance(){
    LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);const double ms=1000.0/frequency.QuadPart;
    return {descriptorTicks.exchange(0)*ms,scanTicks.exchange(0)*ms,descriptorCalls.exchange(0),scanCalls.exchange(0),comparisons.exchange(0),matches.exchange(0)};
}
struct Camera {std::array<float,16> vp{};amalur::PosePacket pose;amalur::LocomotionFrame locomotion;};
static SRWLOCK lock=SRWLOCK_INIT;
static std::array<Camera,128> history;
static uint64_t cameraCount{};
static amalur::PosePacket drawn;
static bool haveDrawn{};
static std::array<float,16> drawnVP{};
static std::atomic<uint32_t> epoch{1};
struct Upload {
    ID3D11DeviceContext* context{};ID3D11Resource* resource{};
    std::array<float,64> values{};uint64_t serial{};
};
static thread_local Upload upload;
static thread_local PendingMap mapping;
static thread_local uint32_t acceptedEpoch{};
static thread_local uint32_t testedEpoch{};
static thread_local uint64_t testedSerial{};
static void camera(const unsigned char* core,const amalur::PosePacket& pose,const amalur::LocomotionFrame& locomotion={}){
    Camera sample;memcpy(sample.vp.data(),core+0x104,64);sample.pose=pose;sample.locomotion=locomotion;
    AcquireSRWLockExclusive(&lock);history[cameraCount++%history.size()]=sample;ReleaseSRWLockExclusive(&lock);
}
static bool locomotionForVP(const float* vp,amalur::LocomotionFrame& frame){
    if(!vp)return false;bool found=false;
    AcquireSRWLockShared(&lock);
    for(uint64_t i=0;i<std::min<uint64_t>(cameraCount,history.size());++i){
        const auto& c=history[(cameraCount-1-i)%history.size()];
        if(c.pose.valid&&c.locomotion.valid&&!memcmp(vp,c.vp.data(),64)){
            if(!found){frame=c.locomotion;found=true;}
            else if(frame.owner!=c.locomotion.owner||frame.center!=c.locomotion.center
                ||memcmp(&frame.pose,&c.locomotion.pose,sizeof(frame.pose))){found=false;break;}
        }
    }
    ReleaseSRWLockShared(&lock);return found;
}
static bool candidate(ID3D11Resource* resource){
    Timer timer{descriptorTicks};++descriptorCalls;
    ComPtr<ID3D11Buffer> b;if(FAILED(resource->QueryInterface(IID_PPV_ARGS(&b))))return false;
    D3D11_BUFFER_DESC d{};b->GetDesc(&d);
    return d.ByteWidth==256&&(d.BindFlags&D3D11_BIND_CONSTANT_BUFFER);
}
static void update(ID3D11DeviceContext* context,ID3D11Resource* resource,const void* data){
    // A rewritten buffer invalidates its cached contents even when this frame
    // already has a pose. Never carry a stale upload into the next epoch.
    if(upload.resource==resource)upload.resource=nullptr;
    if(acceptedEpoch==epoch.load())return;
    if(!data||!candidate(resource))return;
    upload.context=context;upload.resource=resource;memcpy(upload.values.data(),data,256);++upload.serial;
}
static void map(ID3D11DeviceContext* context,ID3D11Resource* resource,UINT subresource,void* data){
    if(upload.resource==resource)upload.resource=nullptr;
    if(acceptedEpoch==epoch.load())return;
    if(data&&candidate(resource))mapping={context,resource,subresource,data};
}
static void unmap(ID3D11DeviceContext* context,ID3D11Resource* resource,UINT subresource){
    if(mapping.context==context&&mapping.resource==resource&&mapping.subresource==subresource){
        // WRITE_DISCARD memory is write-combined. Reading every upload here
        // stalls the CPU badly; copy only until this frame's pose is identified.
        if(acceptedEpoch==epoch.load()){mapping={};return;}
        upload.context=context;upload.resource=resource;memcpy(upload.values.data(),mapping.data,256);++upload.serial;mapping={};
    }
}
static void draw(ID3D11DeviceContext* context){
    const auto current=epoch.load();
    if(acceptedEpoch==current||upload.context!=context||!upload.resource||
        (testedEpoch==current&&testedSerial==upload.serial))return;
    // Amalur's verified world shaders bind g_dynamicbuffer at VS b4. Deferred
    // command lists would need execution-bound metadata; this game uses the
    // immediate context, confirmed by the draw audit. Fail closed otherwise.
    if(context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return;
    ID3D11Buffer* bound{};context->VSGetConstantBuffers(4,1,&bound);
    const bool same=bound==upload.resource;if(bound)bound->Release();if(!same)return;
    testedSerial=upload.serial;testedEpoch=current;
    Timer timer{scanTicks};++scanCalls;
    const float* w=upload.values.data();const float* wvp=w+32;
    double best=1e-4;const Camera* match=nullptr;
    AcquireSRWLockExclusive(&lock);
    for(uint64_t i=0;i<std::min<uint64_t>(cameraCount,history.size());++i){
        ++comparisons;
        const auto& c=history[(cameraCount-1-i)%history.size()];
        const double error=amalur::cameraMatrixResidual(w,wvp,c.vp.data());
        if(error<best){best=error;match=&c;}
    }
    if(match&&epoch.load()==current){drawn=match->pose;drawnVP=match->vp;haveDrawn=true;acceptedEpoch=current;++matches;}
    ReleaseSRWLockExclusive(&lock);
}
static amalur::PosePacket currentDrawn(){
    AcquireSRWLockShared(&lock);auto pose=haveDrawn?drawn:amalur::PosePacket{};ReleaseSRWLockShared(&lock);return pose;
}
static amalur::PosePacket beginPresent(float* vp=nullptr){
    AcquireSRWLockExclusive(&lock);
    auto pose=haveDrawn?drawn:amalur::PosePacket{};
    if(vp){if(haveDrawn)memcpy(vp,drawnVP.data(),64);else memset(vp,0,64);}
    haveDrawn=false;epoch.fetch_add(1);
    ReleaseSRWLockExclusive(&lock);return pose;
}
}
