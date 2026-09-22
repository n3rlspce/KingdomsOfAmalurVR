#pragma once
#include <windows.h>
#ifndef AMALUR_CINEMATIC_MODE_MAPPING
#define AMALUR_CINEMATIC_MODE_MAPPING L"Local\\AmalurCinematicWindowV1"
#endif
namespace amalur {
class CinematicMode {
 HANDLE mapping_{};volatile LONG* value_{};SRWLOCK lock_=SRWLOCK_INIT;
 bool open(){AcquireSRWLockExclusive(&lock_);if(!value_){mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(LONG),AMALUR_CINEMATIC_MODE_MAPPING);if(mapping_)value_=static_cast<volatile LONG*>(MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,0,0,sizeof(LONG)));if(!value_&&mapping_){CloseHandle(mapping_);mapping_=nullptr;}}bool ok=value_!=nullptr;ReleaseSRWLockExclusive(&lock_);return ok;}
public:
 ~CinematicMode(){if(value_)UnmapViewOfFile(const_cast<LONG*>(value_));if(mapping_)CloseHandle(mapping_);}
 bool fullVR(){return !open()||InterlockedCompareExchange(value_,0,0)==0;}
 void setFullVR(bool full){if(open())InterlockedExchange(value_,full?0:1);}
};
inline CinematicMode cinematicMode;
}
