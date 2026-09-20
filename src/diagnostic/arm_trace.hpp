#pragma once
#include "../tracking/arm_trace.hpp"
namespace arm_trace {
inline INIT_ONCE once=INIT_ONCE_STATIC_INIT;
inline HANDLE mapping{},mutex{};inline amalur::ArmTraceBuffer* buffer{};
inline BOOL CALLBACK initialize(PINIT_ONCE,PVOID,PVOID*){
    mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(amalur::ArmTraceBuffer),L"Local\\AmalurArmTraceV1");
    mutex=CreateMutexW(nullptr,FALSE,L"Local\\AmalurArmTraceMutexV1");
    if(mapping&&mutex)buffer=static_cast<amalur::ArmTraceBuffer*>(MapViewOfFile(mapping,FILE_MAP_WRITE,0,0,sizeof(amalur::ArmTraceBuffer)));
    if(buffer){buffer->version=1;buffer->pid=GetCurrentProcessId();buffer->capacity=amalur::armTraceCapacity;
        buffer->stride=sizeof(amalur::ArmTraceRecord);buffer->published=buffer->dropped=0;}
    return TRUE;
}
inline void publish(amalur::ArmTraceRecord record){
    InitOnceExecuteOnce(&once,initialize,nullptr,nullptr);if(!buffer)return;
    const auto wait=WaitForSingleObject(mutex,0);
    if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED){InterlockedIncrement(reinterpret_cast<volatile LONG*>(&buffer->dropped));return;}
    record.sequence=buffer->published+1;
    buffer->records[(record.sequence-1)%amalur::armTraceCapacity]=record;
    buffer->published=record.sequence;ReleaseMutex(mutex);
}
}
