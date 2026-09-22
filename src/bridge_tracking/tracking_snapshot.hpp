#pragma once
#include "pose_channel.hpp"

namespace amalur {
// One XR acquisition transaction. ticks are freshness clocks; sequence and
// predictedTime identify the acquisition even when coarse ticks collide.
struct TrackingSnapshot {
    uint32_t version{1}, bytes{sizeof(TrackingSnapshot)};
    uint64_t sequence{}, session{};
    int64_t predictedTime{};
    PosePacket head{}, left{}, right{};
};
class TrackingSnapshotChannel {
    HANDLE mapping_{}, mutex_{};
    void* memory_{};
    bool writer_{};
    TrackingSnapshot cached_{};
    const wchar_t *name_, *mutexName_;
public:
    explicit TrackingSnapshotChannel(const wchar_t* name=L"Local\\AmalurTrackingSnapshotV1",
        const wchar_t* mutexName=L"Local\\AmalurTrackingSnapshotMutexV1") : name_(name),mutexName_(mutexName) {}
    TrackingSnapshotChannel(const TrackingSnapshotChannel&)=delete;
    TrackingSnapshotChannel& operator=(const TrackingSnapshotChannel&)=delete;
    ~TrackingSnapshotChannel(){
        if(writer_)publish({});
        if(memory_)UnmapViewOfFile(memory_);
        if(mapping_)CloseHandle(mapping_);
        if(mutex_)CloseHandle(mutex_);
    }
    bool open(bool writer){
        if(memory_)return true;
        writer_=writer;
        mapping_=writer?CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(TrackingSnapshot),name_)
            :OpenFileMappingW(FILE_MAP_READ,FALSE,name_);
        mutex_=writer?CreateMutexW(nullptr,FALSE,mutexName_):OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,mutexName_);
        if(mapping_&&mutex_)memory_=MapViewOfFile(mapping_,writer?FILE_MAP_WRITE:FILE_MAP_READ,0,0,sizeof(TrackingSnapshot));
        if(!memory_){if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);mapping_=mutex_=nullptr;}
        return memory_!=nullptr;
    }
    bool publish(const TrackingSnapshot& value){
        if(!writer_||!memory_)return false;
        const auto wait=WaitForSingleObject(mutex_,0);
        if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED)return false;
        memcpy(memory_,&value,sizeof(value));ReleaseMutex(mutex_);return true;
    }
    bool read(TrackingSnapshot& value){
        value={};if(!memory_)return false;
        const auto wait=WaitForSingleObject(mutex_,0);
        if(wait==WAIT_OBJECT_0||wait==WAIT_ABANDONED){memcpy(&cached_,memory_,sizeof(cached_));ReleaseMutex(mutex_);}
        else if(wait!=WAIT_TIMEOUT)return false;
        const auto now=GetTickCount64();
        if(cached_.version!=1||cached_.bytes!=sizeof(TrackingSnapshot)||!cached_.sequence||!cached_.session
            ||cached_.predictedTime<=0||cached_.head.version!=3||!cached_.head.valid
            ||cached_.head.tick>now||now-cached_.head.tick>=250)return false;
        value=cached_;
        // Tracking loss of one controller does not invalidate the other or HMD.
        PosePacket* hands[]={&value.left,&value.right};
        for(auto hand:hands){
            if(hand->version!=3||hand->tick!=value.head.tick||hand->recenter!=value.head.recenter)hand->valid=0;
        }
        return true;
    }
};
}
