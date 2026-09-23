#pragma once
#include <windows.h>
#include <algorithm>
namespace amalur {
class CameraDepth {
    HANDLE mapping_{};volatile LONG* value_{};
    bool open(){if(value_)return true;
        mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,4,L"Local\\AmalurCameraDepthCmV1");
        if(mapping_)value_=static_cast<volatile LONG*>(MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,0,0,4));
        if(!value_&&mapping_){CloseHandle(mapping_);mapping_=nullptr;}return value_!=nullptr;}
public:
    ~CameraDepth(){if(value_)UnmapViewOfFile(const_cast<LONG*>(value_));if(mapping_)CloseHandle(mapping_);}
    void set(int cm){if(open())InterlockedExchange(value_,std::clamp(cm,-100,100));}
    int get(){return open()?std::clamp(int(InterlockedCompareExchange(value_,0,0)),-100,100):0;}
};
inline CameraDepth cameraDepth;
}
