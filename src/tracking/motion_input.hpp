#pragma once
#include <windows.h>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace amalur {
struct MotionInputPacket {
    uint32_t version{1},active{};
    uint64_t tick{};
    float moveX{},moveY{};
};
inline bool validMotionInput(const MotionInputPacket& p,uint64_t now){
    return p.version==1&&p.active==1&&p.tick<=now&&now-p.tick<250
        &&std::isfinite(p.moveX)&&std::isfinite(p.moveY)
        &&std::abs(p.moveX)<=1&&std::abs(p.moveY)<=1;
}
inline void deadzone(float& x,float& y){
    float n=std::sqrt(x*x+y*y);
    if(!std::isfinite(n)||n<=.2f){x=y=0;return;}
    float magnitude=(std::fmin(n,1.f)-.2f)/.8f;
    x=x/n*magnitude;y=y/n*magnitude;
}
class MotionInputChannel {
    HANDLE mapping_{},mutex_{};void* memory_{};bool writer_{};
public:
    ~MotionInputChannel(){if(writer_)publish({});if(memory_)UnmapViewOfFile(memory_);if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);}
    bool open(bool writer){
        if(memory_)return writer==writer_;
        writer_=writer;
        constexpr auto name=L"Local\\AmalurMotionInputV1",lock=L"Local\\AmalurMotionInputMutexV1";
        mapping_=writer?CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(MotionInputPacket),name):OpenFileMappingW(FILE_MAP_READ,FALSE,name);
        mutex_=writer?CreateMutexW(nullptr,FALSE,lock):OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,lock);
        if(mapping_&&mutex_)memory_=MapViewOfFile(mapping_,writer?FILE_MAP_WRITE:FILE_MAP_READ,0,0,sizeof(MotionInputPacket));
        if(!memory_){if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);mapping_=mutex_=nullptr;}
        return memory_!=nullptr;
    }
    void publish(MotionInputPacket p){
        if(!writer_||!memory_)return;
        DWORD w=WaitForSingleObject(mutex_,0);if(w!=WAIT_OBJECT_0&&w!=WAIT_ABANDONED)return;
        p.tick=GetTickCount64();memcpy(memory_,&p,sizeof(p));ReleaseMutex(mutex_);
    }
    bool read(MotionInputPacket& p){
        if(!memory_)return false;
        DWORD w=WaitForSingleObject(mutex_,0);if(w!=WAIT_OBJECT_0&&w!=WAIT_ABANDONED)return false;
        memcpy(&p,memory_,sizeof(p));ReleaseMutex(mutex_);return validMotionInput(p,GetTickCount64());
    }
};
}
