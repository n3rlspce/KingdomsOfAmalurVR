#include "motion_input.hpp"
#include <cassert>
int main(){
 using namespace amalur;TouchMapper mapper;TouchInput t;
 mapper.map(t,true,true,1000);t.leftGrip=t.rightGrip=1;t.leftClick=true;
 mapper.map(t,true,true,1010);
 assert(mapper.map(t,true,true,1360).buttons&XINPUT_GAMEPAD_LEFT_SHOULDER);
 t.leftY=1;
 auto p=mapper.map(t,true,false,1370);
 assert((p.buttons&XINPUT_GAMEPAD_LEFT_SHOULDER)&&p.moveY==1);
 t.leftClick=false;t.leftY=0;
 p=mapper.map(t,true,false,1380);assert(!(p.buttons&(XINPUT_GAMEPAD_LEFT_SHOULDER|XINPUT_GAMEPAD_BACK)));
 mapper.map(t,true,true,1390); // game resumes, grips remain held
 t.leftY=1;p=mapper.map(t,true,true,1400);assert(p.moveY==1&&!p.buttons);
 mapper.map(t,false,true,1410);p=mapper.map(t,true,true,1420);assert(p.moveY==0);
 t.leftY=0;mapper.map(t,true,true,1430);t.leftY=1;
 assert(mapper.map(t,true,true,1440).moveY==0); // focus regain still needs grip neutral
 t={};mapper.map(t,true,true,1450);t.leftY=1;assert(mapper.map(t,true,true,1460).moveY==1);
 // Mode transition without a held wheel still clears actions and requires
 // neutral buttons/sticks; only passive grip is excluded from rearming.
 TouchMapper second;t={};second.map(t,true,true,2000);t.rightGrip=1;t.a=true;
 assert(!second.map(t,true,false,2010).buttons);
 t.a=false;second.map(t,true,false,2020);t.leftX=1;
 assert(second.map(t,true,false,2030).moveX==1);
 t.leftX=0;t.a=true;
 p=second.map(t,true,false,2040);assert((p.buttons&XINPUT_GAMEPAD_A)&&p.abilities==0); // boss/menu A no longer lost to ready gate

}
