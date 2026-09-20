#pragma once
#include <windows.h>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace amalur {
// Persistent user wrist trim, transported independently from tracking poses.
struct GripSettingsPacket { uint32_t version{2}; float pitch{},yaw{},roll{}; uint64_t tick{}; float weaponX{},weaponY{},weaponZ{}; };
class GripSettingsChannel {
    HANDLE mapping_{},mutex_{};void* memory_{};bool writer_{};
    const wchar_t* name_;const wchar_t* lock_;
    static bool validAngles(float pitch,float yaw,float roll){return std::isfinite(pitch)&&std::isfinite(yaw)&&std::isfinite(roll)&&std::abs(pitch)<=180&&std::abs(yaw)<=180&&std::abs(roll)<=180;}
public:
    GripSettingsChannel(const wchar_t* name=L"Local\\AmalurGripSettingsV2",const wchar_t* lock=L"Local\\AmalurGripSettingsMutexV2"):name_(name),lock_(lock){}
    GripSettingsChannel(const GripSettingsChannel&)=delete;
    GripSettingsChannel& operator=(const GripSettingsChannel&)=delete;
    ~GripSettingsChannel(){if(memory_)UnmapViewOfFile(memory_);if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);}
    bool open(bool writer){
        if(memory_)return writer==writer_;
        writer_=writer;
        mapping_=writer?CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(GripSettingsPacket),name_):OpenFileMappingW(FILE_MAP_READ,FALSE,name_);
        mutex_=writer?CreateMutexW(nullptr,FALSE,lock_):OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,lock_);
        if(mapping_&&mutex_)memory_=MapViewOfFile(mapping_,writer?FILE_MAP_WRITE:FILE_MAP_READ,0,0,sizeof(GripSettingsPacket));
        if(!memory_){if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);mapping_=mutex_=nullptr;}
        return memory_!=nullptr;
    }
    void publish(float pitch,float yaw,float roll,float weaponX=0,float weaponY=0,float weaponZ=0){
        if(!writer_||!memory_||!validAngles(pitch,yaw,roll)
            ||!validTranslation(weaponX,weaponY,weaponZ))return;
        DWORD wait=WaitForSingleObject(mutex_,0);
        if(wait==WAIT_OBJECT_0||wait==WAIT_ABANDONED){GripSettingsPacket p;p.pitch=pitch;p.yaw=yaw;p.roll=roll;p.weaponX=weaponX;p.weaponY=weaponY;p.weaponZ=weaponZ;p.tick=GetTickCount64();memcpy(memory_,&p,sizeof(p));ReleaseMutex(mutex_);}
    }
    bool read(float& pitch,float& yaw,float& roll){
        float x,y,z;return read(pitch,yaw,roll,x,y,z);
    }
    static bool validTranslation(float x,float y,float z){
        return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z)&&std::abs(x)<=20&&std::abs(y)<=20&&std::abs(z)<=20;
    }
    bool read(float& pitch,float& yaw,float& roll,float& weaponX,float& weaponY,float& weaponZ){
        if(!memory_)return false;
        DWORD wait=WaitForSingleObject(mutex_,0);
        if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED)return false;
        GripSettingsPacket p;memcpy(&p,memory_,sizeof(p));ReleaseMutex(mutex_);
        auto now=GetTickCount64();
        if(p.version!=2||!p.tick||p.tick>now||now-p.tick>1000||!validAngles(p.pitch,p.yaw,p.roll)
            ||!validTranslation(p.weaponX,p.weaponY,p.weaponZ))return false;
        pitch=p.pitch;yaw=p.yaw;roll=p.roll;weaponX=p.weaponX;weaponY=p.weaponY;weaponZ=p.weaponZ;return true;
    }
};
}
