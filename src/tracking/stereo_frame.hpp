#pragma once
#include "pose_channel.hpp"
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace amalur {
struct StereoFrame {
    uint32_t version{1}, producer{};
    uint64_t generation{},sequence{},published{};
    ULONG_PTR texture{};
    PosePacket pose{};
};
// CPU metadata and GPU ownership travel together. GPU keys: 0 producer,
// 1 consumer. Both sides use zero-timeout acquisition and skip on contention.
class StereoMailbox {
    HANDLE mapping_{},mutex_{};StereoFrame* data_{};
    const wchar_t* name_;const wchar_t* mutexName_;
public:
    StereoMailbox(const wchar_t* name=L"Local\\AmalurStereoFrameV1",const wchar_t* mutexName=L"Local\\AmalurStereoFrameMutexV1"):name_(name),mutexName_(mutexName){}
    ~StereoMailbox(){if(data_)UnmapViewOfFile(data_);if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);}
    bool open(bool create){
        if(data_)return true;
        mapping_=create?CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(StereoFrame),name_):OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name_);
        mutex_=create?CreateMutexW(nullptr,FALSE,mutexName_):OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,mutexName_);
        if(mapping_&&mutex_)data_=static_cast<StereoFrame*>(MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,0,0,sizeof(StereoFrame)));
        if(!data_){if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);mapping_=mutex_=nullptr;}
        return data_!=nullptr;
    }
    StereoFrame* lock(){
        if(!data_)return nullptr;
        DWORD status=WaitForSingleObject(mutex_,0);
        if(status!=WAIT_OBJECT_0&&status!=WAIT_ABANDONED)return nullptr;
        if(status==WAIT_ABANDONED)*data_=StereoFrame{};
        return data_;
    }
    void unlock(){ReleaseMutex(mutex_);}
};
struct MailboxLock {
    StereoMailbox& box;StereoFrame* frame;
    explicit MailboxLock(StereoMailbox& b):box(b),frame(b.lock()){}
    ~MailboxLock(){if(frame)box.unlock();}
};

class StereoPublisher {
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
    StereoMailbox box_;
    Ptr<ID3D11Texture2D> texture_;Ptr<IDXGIKeyedMutex> keyed_;
    uint64_t generation_{},sequence_{},lastPublish_{};ULONG_PTR handle_{};
public:
    StereoPublisher()=default;
    StereoPublisher(const wchar_t* map,const wchar_t* mutex):box_(map,mutex){}
    bool publish(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* source,const PosePacket& pose){
        if(!box_.open(true))return false;MailboxLock lock(box_);if(!lock.frame)return false;
        D3D11_TEXTURE2D_DESC desc{},old{};source->GetDesc(&desc);if(texture_)texture_->GetDesc(&old);
        auto now=GetTickCount64();
        // Recover a stopped/crashed consumer without blocking the game, and
        // replace the resource on resize. Generation prevents recycled handles.
        if(!texture_||desc.Width!=old.Width||desc.Height!=old.Height||desc.Format!=old.Format||now-lastPublish_>1000){
            *lock.frame=StereoFrame{};keyed_.Reset();texture_.Reset();
            desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=0;
            desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
            desc.MiscFlags=D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
            if(FAILED(device->CreateTexture2D(&desc,nullptr,&texture_))||FAILED(texture_.As(&keyed_)))return false;
            Ptr<IDXGIResource> resource;HANDLE handle{};
            if(FAILED(texture_.As(&resource))||FAILED(resource->GetSharedHandle(&handle)))return false;
            handle_=reinterpret_cast<ULONG_PTR>(handle);++generation_;lastPublish_=now;
        }
        HRESULT acquired=keyed_->AcquireSync(0,0);
        if(acquired!=S_OK){if(acquired!=WAIT_TIMEOUT){keyed_.Reset();texture_.Reset();*lock.frame=StereoFrame{};}return false;}
        context->CopyResource(texture_.Get(),source);
        context->Flush();
        if(keyed_->ReleaseSync(1)!=S_OK){texture_.Reset();keyed_.Reset();*lock.frame=StereoFrame{};return false;}
        StereoFrame frame;frame.producer=GetCurrentProcessId();frame.generation=generation_;
        frame.sequence=++sequence_;frame.published=now;frame.texture=handle_;frame.pose=pose;
        *lock.frame=frame;lastPublish_=now;return true;
    }
};
}
