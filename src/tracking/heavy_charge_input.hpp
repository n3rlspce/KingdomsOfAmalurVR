#pragma once
#include <windows.h>
#include <cstdint>
#include <cmath>
#ifndef AMALUR_HEAVY_INPUT_MAPPING
#define AMALUR_HEAVY_INPUT_MAPPING L"Local\\AmalurHeavyChargeInputV1"
#define AMALUR_HEAVY_INPUT_MUTEX L"Local\\AmalurHeavyChargeInputMutexV1"
#endif
namespace amalur {
struct HeavyChargePacket {
    uint32_t version{1},mode{};uint64_t tick{};
    uint32_t session{},active{},spell{};float grip{};
};
inline bool validHeavyChargePacket(const HeavyChargePacket& p,uint64_t now){
    return p.version==1&&p.mode<=1&&p.tick&&p.tick<=now&&now-p.tick<250&&p.session
        &&p.active<=1&&p.spell<=1&&std::isfinite(p.grip)&&p.grip>=0&&p.grip<=1;
}
class HeavyChargeChannel {
    HANDLE mapping_{},mutex_{};void* data_{};bool writer_{};
public:
    ~HeavyChargeChannel(){if(data_)UnmapViewOfFile(data_);if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);}
    bool open(bool writer){
        if(data_)return writer_==writer;
        writer_=writer;
        mapping_=writer?CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(HeavyChargePacket),AMALUR_HEAVY_INPUT_MAPPING)
            :OpenFileMappingW(FILE_MAP_READ,FALSE,AMALUR_HEAVY_INPUT_MAPPING);
        mutex_=writer?CreateMutexW(nullptr,FALSE,AMALUR_HEAVY_INPUT_MUTEX):OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,AMALUR_HEAVY_INPUT_MUTEX);
        if(mapping_&&mutex_)data_=MapViewOfFile(mapping_,writer?FILE_MAP_WRITE:FILE_MAP_READ,0,0,sizeof(HeavyChargePacket));
        if(!data_){if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);mapping_=mutex_=nullptr;}
        return data_!=nullptr;
    }
    bool transfer(HeavyChargePacket& p,bool writer){
        if(!open(writer))return false;const auto wait=WaitForSingleObject(mutex_,0);
        if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED)return false;
        if(writer)memcpy(data_,&p,sizeof(p));else memcpy(&p,data_,sizeof(p));
        ReleaseMutex(mutex_);return true;
    }
};
// A held grip on focus regain/mode switch cannot silently start a charge.
class GripChargeGate {
    uint32_t session_{};bool armed_{},held_{};
public:
    void reset(){armed_=held_=false;session_=0;}
    bool sample(const HeavyChargePacket& p,uint64_t now,bool eligible){
        if(!eligible||!validHeavyChargePacket(p,now)||!p.active||p.mode!=1||p.spell){reset();return false;}
        if(session_!=p.session){reset();session_=p.session;}
        if(p.grip<.35f){held_=false;armed_=true;}
        else if(armed_&&p.grip>=.65f)held_=true;
        return held_;
    }
    void consume(){armed_=held_=false;}
};
}
