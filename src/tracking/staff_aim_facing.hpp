#pragma once
#include <cstdint>
namespace amalur {
// Runtime caller validates the selected weapon as a staff before publishing.
struct StaffAimSnapshot {
    uint32_t owner{},weapon{},selection{},generation{};
    uint64_t tick{};
    bool valid{};
};
using StaffAimAttack=StaffAimSnapshot;
inline bool validStaffAimIdentity(const StaffAimSnapshot& s){
    return s.valid&&s.owner&&s.weapon&&s.selection<=1;
}
inline bool sameStaffAimIdentity(const StaffAimSnapshot& a,const StaffAimSnapshot& b){
    return a.owner==b.owner&&a.weapon==b.weapon&&a.selection==b.selection&&a.generation==b.generation;
}
inline bool freshStaffAimPose(const StaffAimSnapshot& s,uint64_t now){
    return validStaffAimIdentity(s)&&s.tick&&s.tick<=now&&now-s.tick<100;
}
inline bool staffFacingAllowed(const StaffAimSnapshot& pose,const StaffAimAttack& attack,uint64_t now){
    return freshStaffAimPose(pose,now)&&validStaffAimIdentity(attack)&&sameStaffAimIdentity(pose,attack)
        &&attack.tick&&attack.tick<=now&&now-attack.tick<500;
}
// Serial policy: the runtime wrapper must protect all access with its lock.
// This leases facing only. It neither emits attack input nor changes movement.
class StaffAimFacingLease {
    StaffAimAttack attack_{};
public:
    void reset(){attack_={};}
    void observe(const StaffAimSnapshot& pose,uint16_t buttons,uint8_t nativeRT,bool contextValid,uint64_t now){
        if(!contextValid||nativeRT>=30||!freshStaffAimPose(pose,now)){
            reset();return;
        }
        if(!sameStaffAimIdentity(pose,attack_)||(attack_.tick&&now<attack_.tick))reset();
        const uint16_t selectedButton=pose.selection?0x8000:0x4000;
        if(buttons&selectedButton){attack_=pose;attack_.tick=now;}
    }
    bool allowed(const StaffAimSnapshot& pose,bool contextValid,uint64_t now)const{
        return contextValid&&staffFacingAllowed(pose,attack_,now);
    }
};
}
