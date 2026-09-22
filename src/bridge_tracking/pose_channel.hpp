#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>

namespace amalur {
struct PosePacket {
    uint32_t version{3}, valid{};
    uint64_t tick{};
    float orientation[4]{0,0,0,1};
    float position[3]{};
    uint32_t gameMode{};
    float projectionX{},projectionY{};
    float depth{20},convergence{100},worldScale{100},horizontalFov{130};
    uint32_t recenter{};
    int32_t stereoStatus{-1};
};
// A mutex protects whole packets across processes; never use torn pose samples.
class PoseChannel {
    HANDLE mapping_{}, mutex_{}; void* memory_{}; bool writer_{};
    PosePacket lastRead_{};
    const wchar_t* mapName_;const wchar_t* mutexName_;
public:
    explicit PoseChannel(bool frame=false):mapName_(frame?L"Local\\AmalurVRFrameV3":L"Local\\AmalurVRPoseV3"),mutexName_(frame?L"Local\\AmalurVRFrameMutexV3":L"Local\\AmalurVRPoseMutexV3"){}
    PoseChannel(const wchar_t* mapName,const wchar_t* mutexName):mapName_(mapName),mutexName_(mutexName){}
    PoseChannel(const PoseChannel&)=delete;
    PoseChannel& operator=(const PoseChannel&)=delete;
    ~PoseChannel(){if(writer_){PosePacket empty;publish(empty);}if(memory_)UnmapViewOfFile(memory_);if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);}
    bool open(bool writer) {
        if(memory_)return true;
        writer_=writer;
        if(writer){mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(PosePacket),mapName_);mutex_=CreateMutexW(nullptr,FALSE,mutexName_);}
        else {mapping_=OpenFileMappingW(FILE_MAP_READ,FALSE,mapName_);mutex_=OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,mutexName_);}
        if(mapping_&&mutex_)memory_=MapViewOfFile(mapping_,writer?FILE_MAP_WRITE:FILE_MAP_READ,0,0,sizeof(PosePacket));
        if(!memory_){if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);mapping_=mutex_=nullptr;}
        return memory_!=nullptr;
    }
    void publish(const PosePacket& packet) {
        if(!memory_||!writer_)return;
        DWORD wait=WaitForSingleObject(mutex_,0);
        if(wait==WAIT_OBJECT_0||wait==WAIT_ABANDONED){memcpy(memory_,&packet,sizeof(packet));ReleaseMutex(mutex_);}
    }
    bool read(PosePacket& packet) {
        if(!memory_)return false;
        DWORD wait=WaitForSingleObject(mutex_,0);
        if(wait==WAIT_OBJECT_0||wait==WAIT_ABANDONED){
            memcpy(&lastRead_,memory_,sizeof(lastRead_));ReleaseMutex(mutex_);
        }else if(wait!=WAIT_TIMEOUT)return false;
        // Brief mutex contention must not alternate the camera between flat and
        // tracked rendering. Reuse only a still-fresh, complete previous packet;
        // explicit invalidation and the existing expiry remain authoritative.
        packet=lastRead_;
        const auto now=GetTickCount64();
        return packet.version==3&&packet.valid&&packet.tick<=now&&now-packet.tick<250;
    }
};
}
