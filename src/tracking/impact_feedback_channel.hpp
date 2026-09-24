#pragma once
#include "impact_feedback.hpp"
#include <windows.h>
#include <cstring>
#ifndef AMALUR_IMPACT_FEEDBACK_MAPPING
#define AMALUR_IMPACT_FEEDBACK_MAPPING L"Local\\AmalurImpactFeedbackV1"
#define AMALUR_IMPACT_FEEDBACK_MUTEX L"Local\\AmalurImpactFeedbackMutexV1"
#endif
namespace amalur {
class ImpactFeedbackChannel {
    HANDLE mapping_{},mutex_{};void* data_{};bool writer_{};
public:
    ImpactFeedbackChannel()=default;
    ImpactFeedbackChannel(const ImpactFeedbackChannel&)=delete;
    ImpactFeedbackChannel& operator=(const ImpactFeedbackChannel&)=delete;
    ~ImpactFeedbackChannel(){if(data_)UnmapViewOfFile(data_);if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);}
    bool open(bool writer){
        if(data_)return writer_==writer;
        writer_=writer;
        mapping_=writer?CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(ImpactFeedbackPacket),AMALUR_IMPACT_FEEDBACK_MAPPING)
            :OpenFileMappingW(FILE_MAP_READ,FALSE,AMALUR_IMPACT_FEEDBACK_MAPPING);
        mutex_=writer?CreateMutexW(nullptr,FALSE,AMALUR_IMPACT_FEEDBACK_MUTEX)
            :OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,AMALUR_IMPACT_FEEDBACK_MUTEX);
        if(mapping_&&mutex_)data_=MapViewOfFile(mapping_,writer?FILE_MAP_WRITE:FILE_MAP_READ,0,0,sizeof(ImpactFeedbackPacket));
        if(!data_){if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);mapping_=mutex_=nullptr;}
        return data_!=nullptr;
    }
    bool transfer(ImpactFeedbackPacket& p,bool writer){
        if(!open(writer))return false;const auto wait=WaitForSingleObject(mutex_,0);
        if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED)return false;
        if(writer)memcpy(data_,&p,sizeof(p));else memcpy(&p,data_,sizeof(p));
        ReleaseMutex(mutex_);return true;
    }
};
}
