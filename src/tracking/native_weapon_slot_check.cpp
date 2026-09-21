#include "native_weapon_slot.hpp"
#include <cstdlib>
#include <cstdio>
void check(bool v){if(!v)std::exit(1);}
int main(){using namespace amalur;NativeWeaponSlot e{1,2,3,4,100,0,true};
 check(freshBackSocket(e,1,3,2,4,0,199));check(!freshBackSocket(e,1,3,2,4,0,200));
 check(!freshBackSocket(e,1,3,2,4,0,99));check(!freshBackSocket(e,1,3,2,4,1,101));
 check(!freshBackSocket(e,1,8,2,4,0,101));check(!freshBackSocket(e,1,3,2,8,0,101));
 check(!freshBackSocket(e,8,3,2,4,0,101));check(!freshBackSocket(e,1,3,8,4,0,101));
 e.backSocket=false;check(!freshBackSocket(e,1,3,2,4,0,101));e.backSocket=true;e.tick=0;check(!freshBackSocket(e,1,3,2,4,0,1));
 e.tick=100;e.selection=1;check(freshBackSocket(e,1,3,2,4,1,101));e.selection=2;check(!freshBackSocket(e,1,3,2,4,2,101));
 puts("PASS: current stowed sibling allowed; active, stale, future, selection, root and owner changes rejected");}
