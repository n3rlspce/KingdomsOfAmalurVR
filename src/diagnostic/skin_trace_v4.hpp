#pragma once
#include <unordered_map>
#include "../tracking/skin_trace_protocol.hpp"
// Opt-in bounded CPU evidence only. No native pose or rendering state writes.
namespace skin_trace {
using namespace amalur::skin_audit;
inline INIT_ONCE once=INIT_ONCE_STATIC_INIT;
inline HANDLE mapping{},mutex{};inline Buffer* memory{};
inline std::atomic<bool> ready{false};
inline std::atomic<uint64_t> captureDeadline{0},solveSerial{0};
inline BOOL CALLBACK initialize(PINIT_ONCE,PVOID,PVOID*){
#ifdef AMALUR_SKIN_AUDIT_TEST
    const auto mapName=L"Local\\AmalurSkinTraceV4Test",mutexName=L"Local\\AmalurSkinTraceMutexV4Test";
#else
    const auto mapName=L"Local\\AmalurSkinTraceV4",mutexName=L"Local\\AmalurSkinTraceMutexV4";
#endif
    mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Buffer),mapName);
    mutex=CreateMutexW(nullptr,FALSE,mutexName);
    if(mapping&&mutex)memory=static_cast<Buffer*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Buffer)));
    if(memory){memset(memory,0,sizeof(Buffer));memory->version=version;memory->pid=GetCurrentProcessId();
        memory->drawStride=sizeof(DrawRecord);memory->drawCapacity=drawCapacity;
        memory->cpuStride=sizeof(CpuRecord);memory->cpuSlots=objectSlots;memory->cpuDepth=historyDepth;
        ready.store(true,std::memory_order_release);}
    return TRUE;
}
inline void ensure(){InitOnceExecuteOnce(&once,initialize,nullptr,nullptr);}
inline bool active(){
    if(!ready.load(std::memory_order_acquire))return false;
    static thread_local uint32_t checkedFrame=~0u;
    static thread_local bool enabled=false;
    const auto frame=presents.load();
    if(checkedFrame==frame)return enabled&&GetTickCount64()<captureDeadline.load();
    checkedFrame=frame;enabled=false;
    const auto wait=WaitForSingleObject(mutex,0);if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED)return false;
    const auto until=memory->until;ReleaseMutex(mutex);
    const auto now=GetTickCount64();enabled=until>now&&until-now<=60000;
    if(enabled)captureDeadline.store(until);return enabled;
}
inline void count(Counter c){InterlockedIncrement(reinterpret_cast<volatile LONG*>(&memory->counters[c]));}
inline SRWLOCK cpuLock=SRWLOCK_INIT;
inline uint32_t objectKeys[objectSlots]{},originKeys[objectSlots]{},objectCounts[objectSlots]{};
inline uint32_t objectFrames[objectSlots]{},frameCounts[objectSlots]{};
inline CpuRecord recent[objectSlots]{};
inline uint64_t cpuSerial{},cpuDeadline{};
inline uint32_t cpuFrame=~0u,cpuCount{},getterCount{};
inline uint32_t snapshotFrame=~0u,snapshotCount{},snapshotGetters{};
inline bool reserveCpu(uint32_t origin){
    if(!active())return false;
    AcquireSRWLockExclusive(&cpuLock);
    const auto frame=presents.load();if(snapshotFrame!=frame){snapshotFrame=frame;snapshotCount=snapshotGetters=0;}
    const bool allowed=snapshotCount<64&&(origin!=Getter||snapshotGetters<8);
    if(allowed){++snapshotCount;if(origin==Getter)++snapshotGetters;}
    ReleaseSRWLockExclusive(&cpuLock);if(!allowed)count(CpuDropped);return allowed;
}
inline void cpu(CpuRecord& record){
    if(!record.count||!active())return;
    AcquireSRWLockExclusive(&cpuLock);
    const auto frame=presents.load();if(cpuFrame!=frame){cpuFrame=frame;cpuCount=getterCount=0;}
    if(cpuCount>=64||(record.origin==Getter&&getterCount>=8)){ReleaseSRWLockExclusive(&cpuLock);count(CpuDropped);return;}
    if(cpuDeadline!=captureDeadline.load()){
        memset(objectKeys,0,sizeof(objectKeys));memset(objectCounts,0,sizeof(objectCounts));
        memset(recent,0,sizeof(recent));cpuDeadline=captureDeadline.load();
    }
    const auto key=record.trace.object?record.trace.object:record.trace.root;
    uint32_t slot=objectSlots;
    for(uint32_t i=0;i<objectSlots;++i)if(objectKeys[i]==key&&originKeys[i]==record.origin){slot=i;break;}
    if(slot==objectSlots){
        slot=0;for(uint32_t i=0;i<objectSlots;++i)if(recent[i].serial<recent[slot].serial)slot=i;
        if(objectKeys[slot])count(CpuEvicted);
        objectKeys[slot]=key;originKeys[slot]=record.origin;objectCounts[slot]=0;frameCounts[slot]=0;recent[slot]={};
    }
    if(objectFrames[slot]!=frame){objectFrames[slot]=frame;frameCounts[slot]=0;}
    if(frameCounts[slot]>=4){ReleaseSRWLockExclusive(&cpuLock);count(CpuDropped);return;}
    ++frameCounts[slot];++cpuCount;if(record.origin==Getter)++getterCount;
    record.serial=++cpuSerial;
    const auto wait=WaitForSingleObject(mutex,0);
    if(wait==WAIT_OBJECT_0||wait==WAIT_ABANDONED){
        memory->cpu[slot][objectCounts[slot]++%historyDepth]=record;++memory->cpuPublished;recent[slot]=record;
        ReleaseMutex(mutex);count(CpuPublished);
    }else count(CpuDropped);
    ReleaseSRWLockExclusive(&cpuLock);
}
struct Upload {ComPtr<ID3D11Resource> resource;std::array<float,936> data{};
    uint32_t frame{},bytes{};uint64_t serial{},use{};};
inline SRWLOCK uploadLock=SRWLOCK_INIT;
inline std::unordered_map<ID3D11Resource*,Upload> uploads;
inline thread_local std::unordered_map<ID3D11Resource*,PendingMap> pending;
inline uint64_t serial{},cacheDeadline{},useClock{};
inline CopyBudget budget; // uploadLock protects shared per-Present copy budget
inline thread_local Coverage coverage;
inline bool describe(ID3D11Resource* resource,bool skin,D3D11_BUFFER_DESC& d){
    if(!resource){count(skin?SkinUnbound:WorldUnbound);return false;}
    ComPtr<ID3D11Buffer> buffer;
    if(FAILED(resource->QueryInterface(IID_PPV_ARGS(&buffer)))){count(skin?SkinQueryFailed:WorldQueryFailed);return false;}
    buffer->GetDesc(&d);
    if(!(d.BindFlags&D3D11_BIND_CONSTANT_BUFFER)){count(skin?SkinTypeRejected:WorldTypeRejected);return false;}
    // Live b5 allocations are 3840 bytes; reflected weightMatrices use the
    // first 3744 (78 float3x4 matrices). Keep copying only that bounded prefix.
    const bool sizeMatches=skin?(d.ByteWidth==3744u||d.ByteWidth==3840u):d.ByteWidth==256u;
    if(!sizeMatches){count(skin?SkinSizeRejected:WorldSizeRejected);return false;}
    return true;
}
inline void watch(ID3D11Resource* world,ID3D11Resource* skin,DrawRecord* record=nullptr){
    D3D11_BUFFER_DESC wd{},sd{};const bool haveWorld=describe(world,false,wd),haveSkin=describe(skin,true,sd);
    if(record){record->worldBytes=wd.ByteWidth;record->skinBytes=sd.ByteWidth;
        record->worldBindFlags=wd.BindFlags;record->skinBindFlags=sd.BindFlags;}
    // Each slot has an independent admission path. Scenery/no b5 must not
    // suppress valid b4 evidence or conflate absent skin with missing uploads.
    if(!haveWorld&&!haveSkin)return;
    AcquireSRWLockExclusive(&uploadLock);
    const auto deadline=captureDeadline.load();
    if(cacheDeadline!=deadline){uploads.clear();budget={};cacheDeadline=deadline;}
    for(unsigned side=0;side<2;++side){
        if(side?!haveSkin:!haveWorld)continue;
        auto resource=side?skin:world;
        if(auto it=uploads.find(resource);it!=uploads.end()){it->second.use=++useClock;continue;}
        if(uploads.size()>=128){auto oldest=uploads.begin();
            for(auto it=uploads.begin();it!=uploads.end();++it)if(it->second.use<oldest->second.use)oldest=it;
            uploads.erase(oldest);count(Evicted);}
        auto& entry=uploads[resource];entry.resource=resource;entry.bytes=resource==world?256:3744;entry.use=++useClock;
        count(resource==world?WatchedWorld:WatchedSkin);
    }
    ReleaseSRWLockExclusive(&uploadLock);
}
inline void update(ID3D11Resource* resource,const void* data,bool mapped=false){
    if(!active()||!data)return;
    count(CopyCalls);if(!mapped)count(UpdateHookCalls);
    AcquireSRWLockExclusive(&uploadLock);
    const auto found=uploads.find(resource);
    if(cacheDeadline!=captureDeadline.load()||found==uploads.end()){ReleaseSRWLockExclusive(&uploadLock);count(CopyUnwatched);return;}
    auto& out=found->second;const auto bytes=out.bytes;
    // Never let a skipped overwrite masquerade as the contents now bound.
    out.serial=0;
    // On the draw thread, follow the next selected ordinal even if the same
    // constant buffer is rewritten for hundreds of earlier actors. A separate
    // uploader without draw history uses the shared hard copy budget instead.
    if(coverage.ready&&!coverage.wantsNext(presents.load())){ReleaseSRWLockExclusive(&uploadLock);count(UploadOutsideWindow);return;}
    if(!budget.take(presents.load(),bytes==3744)){ReleaseSRWLockExclusive(&uploadLock);count(UploadBudget);return;}
    out.frame=presents.load();out.serial=++serial;memcpy(out.data.data(),data,bytes);
    ReleaseSRWLockExclusive(&uploadLock);count(bytes==256?WorldUploads:SkinUploads);
}
inline void map(ID3D11DeviceContext* context,ID3D11Resource* resource,UINT subresource,void* data){
    if(!active()||!data)return;
    count(MapCalls);
    AcquireSRWLockExclusive(&uploadLock);
    const auto it=uploads.find(resource);const bool watched=cacheDeadline==captureDeadline.load()&&it!=uploads.end();
    if(watched)it->second.serial=0;
    ReleaseSRWLockExclusive(&uploadLock);
    if(watched){if(pending.size()<64)pending[resource]={context,resource,subresource,data};else count(PendingFull);}
    else count(MapUnwatched);
}
inline void unmap(ID3D11DeviceContext* context,ID3D11Resource* resource,UINT subresource){
    const auto it=pending.find(resource);
    if(it!=pending.end()&&it->second.context==context&&it->second.subresource==subresource){update(resource,it->second.data,true);pending.erase(it);}
    else if(active())count(UnmapMiss);
}
inline void draw(ID3D11DeviceContext* context,UINT count){
    // Render discovery must work before the first tracked-body solve, including
    // sessions that start with the headset disconnected. Capture stays opt-in.
    ensure();
    if(!active())return;
    skin_trace::count(Draws);
    if(context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE){skin_trace::count(Deferred);return;}
    const auto frame=presents.load();uint32_t ordinal{};
    if(!coverage.select(frame,ordinal)){skin_trace::count(NotSelected);return;}
    skin_trace::count(Selected);
    ID3D11Buffer* bound[2]{};context->VSGetConstantBuffers(4,2,bound);
    DrawRecord record;record.frame=frame;record.tick=GetTickCount64();record.count=count;
    watch(bound[0],bound[1],&record);
    record.ordinal=ordinal;record.thread=GetCurrentThreadId();
    record.worldBuffer=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(bound[0]));
    record.skinBuffer=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(bound[1]));
    AcquireSRWLockShared(&uploadLock);
    const auto w=uploads.find(bound[0]),s=uploads.find(bound[1]);
    const bool current=cacheDeadline==captureDeadline.load();
    const bool haveWorld=current&&w!=uploads.end()&&w->second.bytes==256&&w->second.serial;
    const bool haveSkin=current&&s!=uploads.end()&&s->second.bytes==3744&&s->second.serial;
    if(haveWorld){record.flags|=WorldCaptured;record.worldFrame=w->second.frame;record.worldSerial=w->second.serial;memcpy(record.world,w->second.data.data(),256);}
    if(haveSkin){record.flags|=SkinCaptured;record.skinFrame=s->second.frame;record.skinSerial=s->second.serial;memcpy(record.skin,s->second.data.data(),3744);}
    ReleaseSRWLockShared(&uploadLock);for(auto buffer:bound)if(buffer)buffer->Release();
    if(!haveWorld)skin_trace::count(MissingWorld);if(!haveSkin)skin_trace::count(MissingSkin);
    AcquireSRWLockShared(&cpuLock);
    if(cpuDeadline==captureDeadline.load())for(const auto& c:recent){
        if(c.serial&&c.origin==Remap&&(c.trace.flags&8)&&record.tick>=c.trace.tick&&record.tick-c.trace.tick<=250)
            record.cpuSerials[record.candidateCount++]=c.serial;
    }
    ReleaseSRWLockShared(&cpuLock);
    record.association=record.candidateCount>1?Ambiguous:Unassociated;
    ID3D11Buffer* vertex{};UINT offset{};context->IAGetVertexBuffers(0,1,&vertex,&record.stride,&offset);
    record.vertexBuffer=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(vertex));if(vertex)vertex->Release();
    ID3D11Buffer* index{};DXGI_FORMAT format{};context->IAGetIndexBuffer(&index,&format,&offset);
    record.indexBuffer=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(index));if(index)index->Release();
    ID3D11VertexShader* vs{};ID3D11PixelShader* ps{};context->VSGetShader(&vs,nullptr,nullptr);context->PSGetShader(&ps,nullptr,nullptr);
    record.vertexShader=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(vs));if(vs)vs->Release();
    record.pixelShader=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ps));if(ps)ps->Release();
    double best=1e-4;AcquireSRWLockShared(&render_pose::lock);
    for(uint64_t i=0;haveWorld&&i<std::min<uint64_t>(render_pose::cameraCount,render_pose::history.size());++i){
        const auto& c=render_pose::history[(render_pose::cameraCount-1-i)%render_pose::history.size()];
        const double error=amalur::cameraMatrixResidual(record.world,record.world+32,c.vp.data());
        if(error<best){best=error;record.cameraTick=c.pose.tick;memcpy(record.vp,c.vp.data(),64);record.flags|=1u;}}
    ReleaseSRWLockShared(&render_pose::lock);
    const auto wait=WaitForSingleObject(mutex,0);
    if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED){InterlockedIncrement(reinterpret_cast<volatile LONG*>(&memory->dropped));return;}
    memory->draws[memory->published%drawCapacity]=record;++memory->published;ReleaseMutex(mutex);skin_trace::count(Published);
}
}
