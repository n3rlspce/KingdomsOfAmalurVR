#pragma once
// Opt-in forensic capture. Records CPU camera and constants actually bound at
// draws; it does not change either. Captures contain process-local addresses.
#include <array>
#include <unordered_map>
namespace camera_audit {
struct Record {
    uint32_t kind{},frame{},thread{},identity{};
    uint64_t tick{},poseTick{};
    float orientation[4]{},values[128]{};
};
static_assert(sizeof(Record)==560,"Audit file layout");
static HANDLE file=INVALID_HANDLE_VALUE;
static SRWLOCK guard=SRWLOCK_INIT;
static std::array<Record,128> records;
static unsigned count{},frames{},draws{};
struct Constants {std::array<float,64> values;};
static std::unordered_map<ID3D11Resource*,Constants> buffers;
static thread_local PendingMap mapped;
static bool enabled(){return file!=INVALID_HANDLE_VALUE;}
static void initialize(){
    wchar_t path[MAX_PATH]{};GetModuleFileNameW(selfModule,path,MAX_PATH);
    auto slash=wcsrchr(path,L'\\');if(!slash)return;slash[1]=0;
    const size_t length=wcslen(path);wcscat_s(path,L"amalur-camera-audit.enable");
    if(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES)return;
    path[length]=0;wcscat_s(path,L"amalur-camera-audit.bin");
    file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
}
static Record make(unsigned kind,uintptr_t identity){
    Record r;r.kind=kind;r.frame=presents.load();r.thread=GetCurrentThreadId();
    r.identity=static_cast<uint32_t>(identity);r.tick=GetTickCount64();return r;
}
static void camera(const unsigned char* core,const amalur::PosePacket& pose){
    if(!enabled())return;
    auto r=make(1,reinterpret_cast<uintptr_t>(core));r.poseTick=pose.tick;
    memcpy(r.orientation,pose.orientation,16);memcpy(r.values,core,sizeof(r.values));
    AcquireSRWLockExclusive(&guard);if(count<records.size())records[count++]=r;ReleaseSRWLockExclusive(&guard);
}
static void update(ID3D11Resource* resource,const void* data){
    if(!enabled()||!data)return;
    ComPtr<ID3D11Buffer> b;if(FAILED(resource->QueryInterface(IID_PPV_ARGS(&b))))return;
    D3D11_BUFFER_DESC d{};b->GetDesc(&d);
    if(d.ByteWidth!=256||!(d.BindFlags&D3D11_BIND_CONSTANT_BUFFER))return;
    Constants value{};memcpy(value.values.data(),data,256);
    AcquireSRWLockExclusive(&guard);
    if(buffers.size()<512||buffers.count(resource))buffers[resource]=value;
    ReleaseSRWLockExclusive(&guard);
}
static void map(ID3D11DeviceContext* context,ID3D11Resource* resource,UINT subresource,void* data){
    if(!enabled())return;
    ComPtr<ID3D11Buffer> b;if(FAILED(resource->QueryInterface(IID_PPV_ARGS(&b))))return;
    D3D11_BUFFER_DESC d{};b->GetDesc(&d);
    if(d.ByteWidth==256&&(d.BindFlags&D3D11_BIND_CONSTANT_BUFFER))mapped={context,resource,subresource,data};
}
static void unmap(ID3D11DeviceContext* context,ID3D11Resource* resource,UINT subresource){
    if(mapped.context==context&&mapped.resource==resource&&mapped.subresource==subresource){update(resource,mapped.data);mapped={};}
}
static void draw(ID3D11DeviceContext* context){
    if(!enabled())return;
    AcquireSRWLockShared(&guard);bool enough=draws>=12;ReleaseSRWLockShared(&guard);if(enough)return;
    ID3D11Buffer* bound{};context->VSGetConstantBuffers(4,1,&bound);
    if(!bound)return;
    AcquireSRWLockExclusive(&guard);
    const auto it=buffers.find(bound);
    if(it!=buffers.end()){
        const auto& v=it->second.values;
        // Skip the early identity-world fullscreen pass. Require an affine
        // placed object and a perspective WVP. Upload versions may share one
        // dynamic buffer, so buffer-address deduplication is deliberately absent.
        const bool placed=v[3]==0&&v[7]==0&&v[11]==0&&v[15]==1&&
            (std::abs(v[12])+std::abs(v[13])+std::abs(v[14])>1000)&&
            (std::abs(v[35])+std::abs(v[39])+std::abs(v[43])>.01f);
        if(placed&&draws<12&&count<records.size()){
            ++draws;auto r=make(2,reinterpret_cast<uintptr_t>(bound));
            memcpy(r.values,v.data(),256);r.values[64]=4;
            r.values[65]=static_cast<float>(context->GetType());
            UINT n=1;D3D11_VIEWPORT viewport{};context->RSGetViewports(&n,&viewport);
            r.values[66]=viewport.Width;r.values[67]=viewport.Height;records[count++]=r;
        }
    }
    ReleaseSRWLockExclusive(&guard);
    bound->Release();
}
static void present(const amalur::PosePacket& pose){
    if(!enabled())return;
    AcquireSRWLockExclusive(&guard);
    auto r=make(3,0);r.poseTick=pose.tick;memcpy(r.orientation,pose.orientation,16);
    if(count<records.size())records[count++]=r;
    DWORD written{};WriteFile(file,records.data(),count*sizeof(Record),&written,nullptr);
    count=draws=0;
    if(pose.valid&&++frames>=2000){CloseHandle(file);file=INVALID_HANDLE_VALUE;}
    ReleaseSRWLockExclusive(&guard);
}
}
