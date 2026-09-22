#pragma once
#include <windows.h>
#include <algorithm>
#include <cstdint>
namespace amalur {
// Separate channel keeps the existing motion-input ABI and tools compatible.
class SnapPitchChannel {
    HANDLE mapping_{};volatile LONG64* value_{};
    bool open(){
        if(value_)return true;
        mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,8,L"Local\\AmalurSnapPitchV1");
        if(mapping_)value_=static_cast<volatile LONG64*>(MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,0,0,8));
        if(!value_&&mapping_){CloseHandle(mapping_);mapping_=nullptr;}
        return value_!=nullptr;
    }
public:
    ~SnapPitchChannel(){if(value_)UnmapViewOfFile(const_cast<LONG64*>(value_));if(mapping_)CloseHandle(mapping_);}
    void publish(int32_t steps){if(open())InterlockedExchange64(value_,static_cast<LONG64>((uint64_t(GetCurrentProcessId())<<32)|uint32_t(steps)));}
    bool read(uint32_t& session,int32_t& steps){
        if(!open())return false;
        auto v=static_cast<uint64_t>(InterlockedCompareExchange64(value_,0,0));session=uint32_t(v>>32);steps=int32_t(v);return session!=0;
    }
};
struct SnapPitch {
    uint32_t session{};int32_t previous{};float angle{};bool ready{};
    void reset(){*this={};}
    float sample(uint32_t owner,int32_t steps){
        if(!ready||owner!=session){session=owner;previous=steps;ready=true;return angle;}
        const int64_t delta=int64_t(steps)-previous;previous=steps;
        angle=std::clamp(angle+float(delta)*30.f,-60.f,60.f);return angle;
    }
};
}
