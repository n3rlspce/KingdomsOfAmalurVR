#pragma once
#include "pose_channel.hpp"
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <array>

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
    struct Slot {Ptr<ID3D11Texture2D> texture;Ptr<IDXGIKeyedMutex> keyed;ULONG_PTR handle{};};
    std::array<Slot,3> slots_{};D3D11_TEXTURE2D_DESC description_{};unsigned next_{};
    uint64_t generation_{},sequence_{},lastPublish_{};
    void reset(StereoFrame& frame){slots_={};description_={};next_=0;frame=StereoFrame{};}
public:
    StereoPublisher()=default;
    StereoPublisher(const wchar_t* map,const wchar_t* mutex):box_(map,mutex){}
    bool publish(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* source,const PosePacket& pose){
        if(!box_.open(true))return false;MailboxLock lock(box_);if(!lock.frame)return false;
        D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);
        auto now=GetTickCount64();
        // Recover a stopped/crashed consumer without blocking the game, and
        // replace the resource on resize. Generation prevents recycled handles.
        if(!slots_[0].texture||desc.Width!=description_.Width||desc.Height!=description_.Height||desc.Format!=description_.Format
            ||desc.MipLevels!=description_.MipLevels||desc.ArraySize!=description_.ArraySize||now-lastPublish_>1000){
            reset(*lock.frame);
            desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=0;
            desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
            desc.MiscFlags=D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
            for(auto& slot:slots_){
                Ptr<IDXGIResource> resource;HANDLE handle{};
                if(FAILED(device->CreateTexture2D(&desc,nullptr,&slot.texture))||FAILED(slot.texture.As(&slot.keyed))
                    ||FAILED(slot.texture.As(&resource))||FAILED(resource->GetSharedHandle(&handle))){reset(*lock.frame);return false;}
                slot.handle=reinterpret_cast<ULONG_PTR>(handle);
            }
            description_=desc;++generation_;lastPublish_=now;
        }
        Slot* selected=nullptr;
        // Prefer consumer-returned/free slots. If all are pending, an unconsumed
        // key-1 slot can be reclaimed while the CPU mailbox lock excludes the
        // consumer. A GPU-leased slot cannot be acquired by either key.
        for(unsigned key=0;key<=1&&!selected;++key)for(unsigned n=0;n<slots_.size();++n){
            auto index=(next_+n)%static_cast<unsigned>(slots_.size());auto& slot=slots_[index];
            HRESULT acquired=slot.keyed->AcquireSync(key,0);
            if(acquired==S_OK){selected=&slot;next_=(index+1)%static_cast<unsigned>(slots_.size());break;}
            if(acquired!=WAIT_TIMEOUT){reset(*lock.frame);return false;}
        }
        if(!selected)return false;
        context->CopyResource(selected->texture.Get(),source);
        context->Flush();
        if(selected->keyed->ReleaseSync(1)!=S_OK){reset(*lock.frame);return false;}
        StereoFrame frame;frame.producer=GetCurrentProcessId();frame.generation=generation_;
        frame.sequence=++sequence_;frame.published=now;frame.texture=selected->handle;frame.pose=pose;
        *lock.frame=frame;lastPublish_=now;return true;
    }
};
}
