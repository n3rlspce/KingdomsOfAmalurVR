#pragma once
#include "../tracking/staff_aim_facing.hpp"
namespace staff_aim {
inline SRWLOCK lock=SRWLOCK_INIT;
inline amalur::StaffAimSnapshot pose;
inline amalur::StaffAimFacingLease lease;
inline void publish(const amalur::StaffAimSnapshot& next){
    AcquireSRWLockExclusive(&lock);
    if(!amalur::validStaffAimIdentity(next)||!amalur::sameStaffAimIdentity(pose,next))lease.reset();
    pose=next;ReleaseSRWLockExclusive(&lock);
}
inline void observe(uint16_t buttons,uint8_t nativeRT,bool active,uint64_t now){
    AcquireSRWLockExclusive(&lock);lease.observe(pose,buttons,nativeRT,active,now);ReleaseSRWLockExclusive(&lock);
}
inline bool allowed(uint32_t owner,uint64_t now){
    AcquireSRWLockShared(&lock);const bool result=lease.allowed(pose,owner&&pose.owner==owner,now);ReleaseSRWLockShared(&lock);return result;
}
}
