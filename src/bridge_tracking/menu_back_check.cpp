#define NOMINMAX
#include "motion_input.hpp"
#include <cstdio>
#include <cstdlib>
static void check(bool value,const char* message){if(!value){std::printf("FAIL: %s\n",message);std::exit(1);}}
int main(){
    amalur::TouchMapper mapper;amalur::TouchInput touch;
    mapper.map(touch,true,true,1000);
    touch.leftGrip=1;touch.leftY=1;
    mapper.map(touch,true,false,1010);
    touch.leftClick=true;
    auto back=mapper.map(touch,true,false,1020);
    check(back.active&&(back.buttons&XINPUT_GAMEPAD_BACK),"BACK works through paused-menu rearm");
    touch.leftClick=false;
    auto released=mapper.map(touch,true,false,1150);
    check(!(released.buttons&XINPUT_GAMEPAD_BACK),"menu click does not pulse again on release");
    touch.leftGrip=touch.leftY=0;mapper.map(touch,true,false,1160);
    touch.leftClick=true;
    back=mapper.map(touch,true,false,1200);
    check(back.buttons&XINPUT_GAMEPAD_BACK,"ready menu sends BACK on press");
    touch.leftClick=false;
    released=mapper.map(touch,true,false,1350);
    check(!(released.buttons&XINPUT_GAMEPAD_BACK),"ready menu does not toggle again on release");
    std::puts("PASS: BACK in paused UI and during mode rearm");
}
