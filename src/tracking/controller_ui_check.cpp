#include "controller_ui_policy.hpp"
#include <cstdio>
#include <cstdlib>
void check(bool v,const char* label){if(!v){printf("FAIL: %s\n",label);exit(1);}}
int main(){
    amalur::ControllerUiPolicy p;
    check(p.update(10,3,true)==0,"idle VR selects controller before button press");
    check(p.update(10,0,true)==0,"neutral polls retain preference");
    check(p.update(10,0,false)==3,"expiry or focus loss restores native mode");
    check(p.update(10,2,true)==0,"keyboard mode selects controller");
    check(p.update(10,3,false)==3,"native changes after lease preserved");
    check(p.update(10,0,true)==0&&p.update(10,0,false)==0,"already native controller untouched");
    check(p.update(10,3,true)==0&&p.update(20,0,false)==0,"manager replacement never restores stale state");
    check(p.update(20,2,true)==0&&p.update(0,0,false)==0&&p.update(20,0,false)==0,"destroyed owner loses lease");
    check(p.update(20,1,true)==1&&p.update(20,8,true)==8&&p.update(20,-1,true)==-1,"unknown device modes untouched");
    check(p.update(20,3,false)==3,"desktop without VR unchanged");
    puts("PASS: idle controller preference, focus/expiry restore, native changes, owner lifetime, unsupported modes and desktop fallback");
}
