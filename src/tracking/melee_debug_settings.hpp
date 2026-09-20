#pragma once
#include <windows.h>
namespace amalur {
// A session-only switch shared by the bridge panel and the game renderer.
// Zero-filled new mappings mean ON, matching the collision preview default.
class MeleeDebugSettings {
    HANDLE mapping_{};volatile LONG* value_{};const wchar_t* name_;
public:
    explicit MeleeDebugSettings(const wchar_t* name=L"Local\\AmalurMeleeDebugSwitchV1"):name_(name){}
    MeleeDebugSettings(const MeleeDebugSettings&)=delete;
    MeleeDebugSettings& operator=(const MeleeDebugSettings&)=delete;
    ~MeleeDebugSettings(){if(value_)UnmapViewOfFile(const_cast<LONG*>(value_));if(mapping_)CloseHandle(mapping_);}
    bool open(){
        if(value_)return true;
        mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(LONG),name_);
        if(mapping_)value_=static_cast<volatile LONG*>(MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,0,0,sizeof(LONG)));
        if(!value_&&mapping_){CloseHandle(mapping_);mapping_=nullptr;}
        return value_!=nullptr;
    }
    bool enabled(){return !open()||InterlockedCompareExchange(value_,0,0)==0;}
    bool toggle(){
        if(!open())return false;
        LONG previous=InterlockedCompareExchange(value_,0,0);
        for(;;){LONG observed=InterlockedCompareExchange(value_,previous?0:1,previous);if(observed==previous)return true;previous=observed;}
    }
};
}
