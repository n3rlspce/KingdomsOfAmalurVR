#pragma once
#include <windows.h>
#ifndef AMALUR_DEVELOPER_CAMERA_MAPPING
#define AMALUR_DEVELOPER_CAMERA_MAPPING L"Local\\AmalurThirdPersonCameraV1"
#endif
namespace amalur {
// A session-only switch shared by the bridge panel and the game renderer.
// One metre behind the normal eye; a new session defaults to first person.

class DeveloperCameraSettings {
    HANDLE mapping_{};volatile LONG* value_{};const wchar_t* name_;
public:
    explicit DeveloperCameraSettings(const wchar_t* name=AMALUR_DEVELOPER_CAMERA_MAPPING):name_(name){}
    DeveloperCameraSettings(const DeveloperCameraSettings&)=delete;
    DeveloperCameraSettings& operator=(const DeveloperCameraSettings&)=delete;
    ~DeveloperCameraSettings(){if(value_)UnmapViewOfFile(const_cast<LONG*>(value_));if(mapping_)CloseHandle(mapping_);}
    bool open(){
        if(value_)return true;
        mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(LONG),name_);
        if(mapping_)value_=static_cast<volatile LONG*>(MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,0,0,sizeof(LONG)));
        if(!value_&&mapping_){CloseHandle(mapping_);mapping_=nullptr;}
        return value_!=nullptr;
    }
    bool enabled(){return open()&&InterlockedCompareExchange(value_,0,0)!=0;}
    bool toggle(){
        if(!open())return false;
        LONG previous=InterlockedCompareExchange(value_,0,0);
        for(;;){LONG observed=InterlockedCompareExchange(value_,previous?0:1,previous);if(observed==previous)return true;previous=observed;}
    }
};
}
