#define NOMINMAX
#include "motion_input.hpp"
#include <cstdio>
#include <cstdlib>
static void check(bool ok,const char* what){if(!ok){std::printf("FAIL: %s\n",what);std::exit(1);}}
int main(){
    amalur::TouchMapper mapper;amalur::TouchInput touch;
    mapper.map(touch,true,true,1000);
    touch.rightTrigger=1;
    auto attack=mapper.map(touch,true,true,1010);
    check(attack.buttons==XINPUT_GAMEPAD_X&&attack.abilities==0,"RT alone attacks");
    touch.leftTrigger=1;
    auto chord=mapper.map(touch,true,true,1020);
    check(chord.buttons==0&&chord.block==1&&chord.abilities==1,"LT joins held RT as native Reckoning");
    XINPUT_GAMEPAD pad{};amalur::mergeMotion(pad,chord);
    check(pad.bLeftTrigger==255&&pad.bRightTrigger==255&&pad.wButtons==0,"native XInput receives LT+RT");
    touch.leftTrigger=0;
    auto released=mapper.map(touch,true,true,1030);
    check(released.buttons==0&&released.abilities==0,"releasing LT cannot resume attack or spell");
    touch.rightTrigger=0;mapper.map(touch,true,true,1040);
    touch.rightTrigger=1;
    check(mapper.map(touch,true,true,1050).buttons==XINPUT_GAMEPAD_X,"fresh RT rearms attack");
    touch={};mapper.map(touch,true,true,1060);
    touch.leftTrigger=1;touch.rightTrigger=1;touch.rightGrip=1;
    chord=mapper.map(touch,true,true,1070);
    check(chord.buttons==0&&chord.block==1&&chord.abilities==1,"LT+RT works with weapon grip held");
    amalur::TouchMapper menuMapper;menuMapper.map({},true,false,2000);
    touch={};touch.leftTrigger=1;touch.rightTrigger=1;
    auto menu=menuMapper.map(touch,true,false,2010);
    check(menu.buttons==0&&menu.block==1&&menu.abilities==1,"menus keep native triggers");
    std::puts("PASS: native Reckoning, attack rearm, grip held, menu triggers");
}
