#include "dodge_facing.hpp"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
void check(bool pass,const char* message){if(!pass){std::printf("FAIL: %s\n",message);std::exit(1);}}
int main(){
    amalur::DodgeFacingLease lease;
    lease.observe(true,false,false,1000);
    check(!lease.suppress(1000),"idle leaves native head facing enabled");
    lease.observe(true,true,false,1010);
    check(lease.suppress(1010)&&lease.suppress(1709),"A edge reserves facing through dodge window");
    lease.observe(true,true,false,1600);
    check(!lease.suppress(1710),"held A cannot extend lease indefinitely");
    lease.observe(true,false,false,1800);
    lease.observe(true,true,true,1810);
    check(!lease.suppress(1810),"ability plus A does not reserve facing");
    lease.observe(true,true,false,1820);
    check(!lease.suppress(1820),"modifier release while A held is not a dodge edge");
    lease.observe(true,false,false,1830);
    lease.observe(true,true,false,1840);
    check(lease.suppress(1840),"new A edge restarts lease");
    lease.observe(false,true,false,1850);
    check(!lease.suppress(1850),"focus loss clears pending lease");
    amalur::LocomotionFacingLease movement;
    for(auto axis: {int16_t(-32767),int16_t(1),int16_t(32767)}){
        movement.observe(true,axis,0,2000);
        check(movement.suppress(2000),"horizontal analog direction yields native facing");
        movement.observe(true,0,axis,2100);
        check(movement.suppress(2100),"vertical analog direction yields native facing");
        movement.observe(true,0,0,2150);
        check(movement.suppress(2349)&&!movement.suppress(2350),"neutral allows deceleration then restores head facing");
    }
    movement.observe(true,20000,-20000,3000);
    movement.observe(true,20000,-20000,3200);
    check(movement.suppress(3400),"sustained diagonal movement renews ownership");
    movement.observe(false,20000,0,3410);
    check(!movement.suppress(3410),"focus loss clears movement lease");
    std::puts("dodge_check PASS");
}
