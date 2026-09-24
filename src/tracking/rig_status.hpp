#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
namespace amalur {
// Fixed-width, process-independent diagnostics. Contains no native pointers.
struct RigStatus {
    uint32_t version{1}, pid{}, tick{}, frames{}, remaps{}, weaponRemaps{};
    uint32_t focused{}, tracked{}, firstPerson{}, handFresh{}, weaponSlot{}, sourceBone{};
    int32_t paused{-1};
    uint32_t nativeWeaponSlot{};
    float nativeWrist[3]{}, nativeSocket[3]{}, renderedSocket[3]{};
};
class RigStatusChannel {
    HANDLE mapping_{}, mutex_{};void* memory_{};
public:
    ~RigStatusChannel(){if(memory_)UnmapViewOfFile(memory_);if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);}
    bool open(bool writer){
        if(memory_)return true;
        const auto name=L"Local\\AmalurRigStatusV1",lock=L"Local\\AmalurRigStatusMutexV1";
        mapping_=writer?CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(RigStatus),name):OpenFileMappingW(FILE_MAP_READ,FALSE,name);
        mutex_=writer?CreateMutexW(nullptr,FALSE,lock):OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,lock);
        if(mapping_&&mutex_)memory_=MapViewOfFile(mapping_,writer?FILE_MAP_WRITE:FILE_MAP_READ,0,0,sizeof(RigStatus));
        if(!memory_){if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);mapping_=mutex_=nullptr;}
        return memory_!=nullptr;
    }
    bool transfer(RigStatus& value,bool write){
        if(!open(write))return false;
        auto wait=WaitForSingleObject(mutex_,0);if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED)return false;
        if(write)memcpy(memory_,&value,sizeof(value));else memcpy(&value,memory_,sizeof(value));
        ReleaseMutex(mutex_);return true;
    }
};
}
