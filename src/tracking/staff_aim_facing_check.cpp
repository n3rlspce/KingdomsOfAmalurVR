#include "staff_aim_facing.hpp"
#include <cassert>
int main(){
    using namespace amalur;
    StaffAimSnapshot pose{7,9,0,1,1000,true};
    StaffAimFacingLease lease;
    assert(!lease.allowed(pose,true,1000));
    lease.observe(pose,0x8000,0,true,1000);assert(!lease.allowed(pose,true,1000));
    lease.observe(pose,0x4000,0,true,1000);assert(lease.allowed(pose,true,1000));
    assert(lease.allowed(pose,true,1099));
    assert(!lease.allowed(pose,true,1100));assert(!lease.allowed(pose,true,999));
    pose.tick=1499;lease.observe(pose,0,0,true,1499);assert(lease.allowed(pose,true,1499));
    pose.tick=1500;assert(!lease.allowed(pose,true,1500));
    pose.tick=1600;pose.selection=1;
    lease.observe(pose,0x4000,0,true,1600);assert(!lease.allowed(pose,true,1600));
    lease.observe(pose,0x8000,29,true,1600);assert(lease.allowed(pose,true,1600));
    lease.observe(pose,0x8000,30,true,1600);assert(!lease.allowed(pose,true,1600));
    lease.observe(pose,0x8000,0,true,1600);assert(!lease.allowed(pose,false,1600));
    lease.observe(pose,0,0,false,1600);assert(!lease.allowed(pose,true,1600));
    lease.observe(pose,0x8000,0,true,1600);
    auto changed=pose;changed.owner++;assert(!lease.allowed(changed,true,1600));
    changed=pose;changed.weapon++;assert(!lease.allowed(changed,true,1600));
    changed=pose;changed.generation++;assert(!lease.allowed(changed,true,1600));
    changed=pose;changed.valid=false;assert(!lease.allowed(changed,true,1600));
    changed=pose;changed.selection=0;assert(!lease.allowed(changed,true,1600));
    // Observing an identity change cancels the previous lease permanently.
    lease.observe(changed,0,0,true,1600);assert(!lease.allowed(pose,true,1600));
    lease.observe(pose,0x8000,0,true,1600);
    lease.observe(pose,0,0,true,1700);pose.tick=1700;assert(!lease.allowed(pose,true,1700));
    pose.tick=1800;lease.observe(pose,0x8000,0,true,1800);
    pose.tick=1799;lease.observe(pose,0,0,true,1799);assert(!lease.allowed(pose,true,1799));
    StaffAimAttack attack=pose;assert(staffFacingAllowed(pose,attack,1799));
    attack.tick=1800;assert(!staffFacingAllowed(pose,attack,1799));
    attack=pose;attack.generation=0;assert(!staffFacingAllowed(pose,attack,1799));
    pose.generation=0;assert(staffFacingAllowed(pose,attack,1799));
}
