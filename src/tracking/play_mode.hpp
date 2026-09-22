#pragma once
#include <windows.h>
#include <atomic>
#ifndef AMALUR_PLAY_MODE_MAPPING
#define AMALUR_PLAY_MODE_MAPPING L"Local\\AmalurNormalThirdPersonV1"
#endif
namespace amalur {
inline std::atomic<bool(*)()> nativeBodyProbe{nullptr};
class PlayMode {
    HANDLE mapping_{}; volatile LONG* value_{}; SRWLOCK lock_=SRWLOCK_INIT;
public:
    ~PlayMode(){if(value_)UnmapViewOfFile(const_cast<LONG*>(value_));if(mapping_)CloseHandle(mapping_);}
    bool open(){
        AcquireSRWLockExclusive(&lock_);
        if(value_){ReleaseSRWLockExclusive(&lock_);return true;}
        mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(LONG),AMALUR_PLAY_MODE_MAPPING);
        if(mapping_)value_=static_cast<volatile LONG*>(MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,0,0,sizeof(LONG)));
        if(!value_&&mapping_){CloseHandle(mapping_);mapping_=nullptr;}
        bool result=value_!=nullptr;ReleaseSRWLockExclusive(&lock_);return result;
    }
    bool normal(){return open()&&InterlockedCompareExchange(value_,0,0)!=0;}
    bool nativeBody(){auto probe=nativeBodyProbe.load();return normal()||(probe&&probe());}
    void set(bool normal){if(open())InterlockedExchange(value_,normal?1:0);}
};
inline PlayMode playMode;
// Preserve the first-person preference while temporarily selecting native play.
class FirstPersonPreference {
    std::atomic<bool> requested_{true};
public:
    bool load() const {return requested_.load()&&!playMode.normal();}
    void store(bool value){requested_.store(value);}
};
}
