#pragma once
#include <windows.h>
#include "play_mode.hpp"
#ifndef AMALUR_BODY_DEBUG_MAPPING
#define AMALUR_BODY_DEBUG_MAPPING L"Local\\AmalurBodyAnimationModesV1"
#endif
namespace amalur {
inline constexpr LONG nativeTorso=1,nativeArms=2,nativeCamera=4;
inline constexpr LONG skipRigSockets=8,skipRootSmoothing=16,nativeMeshInput=32;
inline constexpr LONG nativeMeshPositions=64,nativeMeshRotations=128;
inline constexpr LONG rigAblationMask=skipRigSockets|skipRootSmoothing|nativeMeshInput|nativeMeshPositions|nativeMeshRotations;
class BodyDebugSettings {
    HANDLE mapping_{};volatile LONG* value_{};
public:
    ~BodyDebugSettings(){if(value_)UnmapViewOfFile(const_cast<LONG*>(value_));if(mapping_)CloseHandle(mapping_);}
    bool open(){
        if(value_)return true;
        mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(LONG),AMALUR_BODY_DEBUG_MAPPING);
        if(mapping_)value_=static_cast<volatile LONG*>(MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,0,0,sizeof(LONG)));
        if(!value_&&mapping_){CloseHandle(mapping_);mapping_=nullptr;}
        return value_!=nullptr;
    }
    LONG read(){LONG bits=open()?InterlockedCompareExchange(value_,0,0):0;return playMode.normal()?bits|nativeTorso|nativeArms:bits;}
    bool enabled(LONG bit){return (read()&bit)!=0;}
    void resetAblations(){if(open())InterlockedAnd(value_,~rigAblationMask);}
    void toggle(LONG bit){if(open())InterlockedXor(value_,bit);}
};
inline BodyDebugSettings bodyDebug;
}
