#pragma once
#include "charged_feedback.hpp"
#include <windows.h>
#include <cstring>
#ifndef AMALUR_CHARGED_FEEDBACK_MAPPING
#define AMALUR_CHARGED_FEEDBACK_MAPPING L"Local\\AmalurChargedFeedbackV1"
#define AMALUR_CHARGED_FEEDBACK_MUTEX L"Local\\AmalurChargedFeedbackMutexV1"
#endif
namespace amalur {
class ChargedFeedbackChannel {
    HANDLE mapping_{},mutex_{};void* data_{};bool writer_{};
public:
    ChargedFeedbackChannel()=default;
    ChargedFeedbackChannel(const ChargedFeedbackChannel&)=delete;
    ChargedFeedbackChannel& operator=(const ChargedFeedbackChannel&)=delete;
    ~ChargedFeedbackChannel(){if(data_)UnmapViewOfFile(data_);if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);}
    bool open(bool writer){
        if(data_)return writer_==writer;
        writer_=writer;
        mapping_=writer?CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(ChargedFeedbackPacket),AMALUR_CHARGED_FEEDBACK_MAPPING)
            :OpenFileMappingW(FILE_MAP_READ,FALSE,AMALUR_CHARGED_FEEDBACK_MAPPING);
        mutex_=writer?CreateMutexW(nullptr,FALSE,AMALUR_CHARGED_FEEDBACK_MUTEX)
            :OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,AMALUR_CHARGED_FEEDBACK_MUTEX);
        if(mapping_&&mutex_)data_=MapViewOfFile(mapping_,writer?FILE_MAP_WRITE:FILE_MAP_READ,0,0,sizeof(ChargedFeedbackPacket));
        if(!data_){if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);mapping_=mutex_=nullptr;}
        return data_!=nullptr;
    }
    bool transfer(ChargedFeedbackPacket& p,bool writer){
        if(!open(writer))return false;const auto wait=WaitForSingleObject(mutex_,0);
        if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED)return false;
        if(writer)memcpy(data_,&p,sizeof(p));else memcpy(&p,data_,sizeof(p));
        ReleaseMutex(mutex_);return true;
    }
};
}
