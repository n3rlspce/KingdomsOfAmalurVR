#include "held_visibility.hpp"
#include <cstdio>
#include <cstdlib>
void check(bool b){if(!b)std::abort();}
int main(){
    amalur::HeldVisibilityIdentity captured{1,2,3,4,5,6,7,5457,0};
    check(amalur::sameHeldVisibilityIdentity(captured,captured));
    auto current=captured;current.owner=8;check(!amalur::sameHeldVisibilityIdentity(captured,current));
    current=captured;current.rootOwner=8;check(!amalur::sameHeldVisibilityIdentity(captured,current));
    current=captured;current.buffer=8;check(!amalur::sameHeldVisibilityIdentity(captured,current));
    current=captured;current.table=8;check(!amalur::sameHeldVisibilityIdentity(captured,current));
    current=captured;current.asset=1423;check(!amalur::sameHeldVisibilityIdentity(captured,current));
    current=captured;current.selection=1;check(!amalur::sameHeldVisibilityIdentity(captured,current));
    captured.table=0;check(!amalur::sameHeldVisibilityIdentity(captured,captured));
    std::puts("held visibility identity checks passed");
}
