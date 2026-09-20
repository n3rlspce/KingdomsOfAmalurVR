#pragma once
#include <unordered_map>
#include "../tracking/arm_trace.hpp"
// Opt-in CPU upload/draw audit. No GPU readbacks or rendering-state writes.
namespace skin_trace {
#pragma pack(push,4)
struct Record {
    uint32_t frame{},worldFrame{},skinFrame{},vertexBuffer{},indexBuffer{},stride{},count{},flags{};
    uint64_t tick{},cameraTick{},skinSerial{};
    amalur::ArmTraceRecord cpu;
    float world[64]{},skin[936]{},vp[16]{};
};
struct Buffer {
    uint32_t version{},pid{},stride{},capacity{},published{},dropped{};
    uint64_t until{};
    Record records[128];
};
#pragma pack(pop)
static_assert(sizeof(Record)==4620);
inline INIT_ONCE once=INIT_ONCE_STATIC_INIT;
inline HANDLE mapping{},mutex{};inline Buffer* memory{};
inline std::atomic<bool> ready{false};
inline SRWLOCK cpuLock=SRWLOCK_INIT;
inline amalur::ArmTraceRecord latest{};
inline BOOL CALLBACK initialize(PINIT_ONCE,PVOID,PVOID*){
    mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Buffer),L"Local\\AmalurSkinTraceV1");
    mutex=CreateMutexW(nullptr,FALSE,L"Local\\AmalurSkinTraceMutexV1");
    if(mapping&&mutex)memory=static_cast<Buffer*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Buffer)));
    if(memory){memory->version=1;memory->pid=GetCurrentProcessId();memory->stride=sizeof(Record);memory->capacity=128;
        memory->published=memory->dropped=0;memory->until=0;ready.store(true,std::memory_order_release);}
    return TRUE;
}
inline void remap(const amalur::ArmTraceRecord& record){
    InitOnceExecuteOnce(&once,initialize,nullptr,nullptr);
    AcquireSRWLockExclusive(&cpuLock);latest=record;ReleaseSRWLockExclusive(&cpuLock);
}
inline bool active(){
    if(!ready.load(std::memory_order_acquire))return false;
    // Check the control channel once per render frame per calling thread.
    // Inactive draws must not make one kernel mutex call per draw.
    static thread_local uint32_t checkedFrame=~0u;
    static thread_local bool enabled=false;
    const auto frame=presents.load();
    if(checkedFrame==frame)return enabled;
    checkedFrame=frame;enabled=false;
    const auto wait=WaitForSingleObject(mutex,0);if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED)return false;
    const auto until=memory->until;ReleaseMutex(mutex);
    const auto now=GetTickCount64();enabled=until>now&&until-now<=60000;return enabled;
}
struct Upload {ComPtr<ID3D11Resource> resource;std::array<float,936> data{};uint32_t frame{},bytes{};uint64_t serial{};};
inline thread_local std::unordered_map<ID3D11Resource*,Upload> uploads;
inline thread_local std::unordered_map<ID3D11Resource*,PendingMap> pending;
inline thread_local uint64_t serial{};
inline thread_local uint32_t drawFrame=~0u,drawCount{};
inline UINT size(ID3D11Resource* resource){
    ComPtr<ID3D11Buffer> buffer;if(!resource||FAILED(resource->QueryInterface(IID_PPV_ARGS(&buffer))))return 0;
    D3D11_BUFFER_DESC d{};buffer->GetDesc(&d);
    return (d.BindFlags&D3D11_BIND_CONSTANT_BUFFER)&&(d.ByteWidth==256||d.ByteWidth==3744)?d.ByteWidth:0;
}
inline void update(ID3D11Resource* resource,const void* data){
    if(!active()){uploads.clear();return;}
    const auto bytes=size(resource);if(!bytes||!data)return;
    if(uploads.size()>=64&&!uploads.count(resource))return;
    auto& out=uploads[resource];out.resource=resource;out.bytes=bytes;out.frame=presents.load();out.serial=++serial;
    memcpy(out.data.data(),data,bytes);
}
inline void map(ID3D11DeviceContext* context,ID3D11Resource* resource,UINT subresource,void* data){
    if(!active())return;
    if(data&&size(resource)&&pending.size()<64)pending[resource]={context,resource,subresource,data};
}
inline void unmap(ID3D11DeviceContext* context,ID3D11Resource* resource,UINT subresource){
    const auto found=pending.find(resource);
    if(found!=pending.end()&&found->second.context==context&&found->second.subresource==subresource){
        update(resource,found->second.data);pending.erase(found);
    }
}
inline void draw(ID3D11DeviceContext* context,UINT count){
    if(!active()||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return;
    const auto frame=presents.load();if(drawFrame!=frame){drawFrame=frame;drawCount=0;}
    if(drawCount>=12)return;
    ID3D11Buffer* bound[2]{};context->VSGetConstantBuffers(4,2,bound);
    const auto w=uploads.find(bound[0]),s=uploads.find(bound[1]);
    for(auto buffer:bound)if(buffer)buffer->Release();
    if(w==uploads.end()||s==uploads.end()||w->second.bytes!=256||s->second.bytes!=3744)return;
    Record record;record.frame=frame;record.tick=GetTickCount64();record.count=count;
    AcquireSRWLockShared(&cpuLock);record.cpu=latest;ReleaseSRWLockShared(&cpuLock);
    if(!record.cpu.root||record.tick<record.cpu.tick||record.tick-record.cpu.tick>250)return;
    const auto& matrix=w->second.data;const auto p=record.cpu.rootWorld.position;
    const float dx=matrix[12]-p.x,dy=matrix[13]-p.y,dz=matrix[14]-p.z;
    // Candidate only: nearby actors may share this region. Vertex/index-buffer
    // identities and palette motion allow offline classification; never claim
    // this proximity filter alone proves the draw belongs to the player.
    if(dx*dx+dy*dy+dz*dz>2500)return;
    ++drawCount;record.worldFrame=w->second.frame;record.skinFrame=s->second.frame;record.skinSerial=s->second.serial;
    memcpy(record.world,matrix.data(),256);memcpy(record.skin,s->second.data.data(),3744);
    ID3D11Buffer* vertex{};UINT offset{};context->IAGetVertexBuffers(0,1,&vertex,&record.stride,&offset);
    record.vertexBuffer=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(vertex));if(vertex)vertex->Release();
    ID3D11Buffer* index{};DXGI_FORMAT format{};context->IAGetIndexBuffer(&index,&format,&offset);
    record.indexBuffer=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(index));if(index)index->Release();
    double best=1e-4;
    AcquireSRWLockShared(&render_pose::lock);
    for(uint64_t i=0;i<std::min<uint64_t>(render_pose::cameraCount,render_pose::history.size());++i){
        const auto& camera=render_pose::history[(render_pose::cameraCount-1-i)%render_pose::history.size()];
        const double error=amalur::cameraMatrixResidual(record.world,record.world+32,camera.vp.data());
        if(error<best){best=error;record.cameraTick=camera.pose.tick;memcpy(record.vp,camera.vp.data(),64);record.flags|=1u;}
    }
    ReleaseSRWLockShared(&render_pose::lock);
    const auto wait=WaitForSingleObject(mutex,0);
    if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED){InterlockedIncrement(reinterpret_cast<volatile LONG*>(&memory->dropped));return;}
    memory->records[memory->published%128]=record;++memory->published;ReleaseMutex(mutex);
}
}
