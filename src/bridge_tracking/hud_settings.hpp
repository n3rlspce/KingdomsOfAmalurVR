#pragma once
#include <windows.h>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace amalur {
// Independent of head tracking: HUD settings also work while tracking is off.
struct HudSettingsPacket { uint32_t version{1}; float size{.8f}; uint64_t tick{}; };
class HudSettingsChannel {
    HANDLE mapping_{},mutex_{};void* memory_{};bool writer_{};
    const wchar_t* name_;const wchar_t* lock_;
public:
    HudSettingsChannel(const wchar_t* name=L"Local\\AmalurHudSettingsV1",const wchar_t* lock=L"Local\\AmalurHudSettingsMutexV1"):name_(name),lock_(lock){}
    HudSettingsChannel(const HudSettingsChannel&)=delete;
    HudSettingsChannel& operator=(const HudSettingsChannel&)=delete;
    ~HudSettingsChannel(){if(memory_)UnmapViewOfFile(memory_);if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);}
    bool open(bool writer){
        if(memory_)return writer==writer_;
        writer_=writer;
        mapping_=writer?CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(HudSettingsPacket),name_):OpenFileMappingW(FILE_MAP_READ,FALSE,name_);
        mutex_=writer?CreateMutexW(nullptr,FALSE,lock_):OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,lock_);
        if(mapping_&&mutex_)memory_=MapViewOfFile(mapping_,writer?FILE_MAP_WRITE:FILE_MAP_READ,0,0,sizeof(HudSettingsPacket));
        if(!memory_){if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);mapping_=mutex_=nullptr;}
        return memory_!=nullptr;
    }
    void publish(float size){
        if(!writer_||!memory_||!std::isfinite(size)||size<.4f||size>1.2f)return;
        DWORD wait=WaitForSingleObject(mutex_,0);
        if(wait==WAIT_OBJECT_0||wait==WAIT_ABANDONED){HudSettingsPacket p;p.size=size;p.tick=GetTickCount64();memcpy(memory_,&p,sizeof(p));ReleaseMutex(mutex_);}
    }
    bool read(float& size){
        if(!memory_)return false;
        DWORD wait=WaitForSingleObject(mutex_,0);
        if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED)return false;
        HudSettingsPacket p;memcpy(&p,memory_,sizeof(p));ReleaseMutex(mutex_);
        auto now=GetTickCount64();
        if(p.version!=1||p.tick>now||now-p.tick>1000||!std::isfinite(p.size)||p.size<.4f||p.size>1.2f)return false;
        size=p.size;return true;
    }
};
}
