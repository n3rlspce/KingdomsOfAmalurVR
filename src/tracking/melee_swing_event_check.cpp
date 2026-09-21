#include "melee_swing_event.hpp"
#include <cstdio>
#include <cstdlib>
void check(bool b,const char* why){if(!b){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
int main(){
    amalur::MeleeSwingWindow w;
    check(!w.accept(1,100,100,false)&&!w.active(100),"tracking start does not replay gesture");
    check(!w.accept(1,100,110,true),"seen serial cannot arm");
    check(w.accept(2,200,210,true)&&w.active(650)&&!w.active(651),"fresh gesture opens bounded window");
    check(!w.accept(2,200,700,true)&&!w.active(700),"resting overlap never rearms same swing");
    check(!w.accept(3,200,701,true),"old event rejected");
    check(!w.accept(4,900,800,true),"future event rejected");
    check(w.accept(5,900,900,true),"next fresh swing arms");
    check(!w.accept(5,900,910,false)&&!w.active(910),"tracking gap cancels contact window");
    amalur::MeleeSwingChain chain;
    check(chain.advance(1,1,100)==1&&chain.advance(1,1,100)==1,"simultaneous hands share first step");
    check(chain.advance(1,1,450)==2&&chain.advance(1,1,800)==3,"linked swings reach third step");
    check(chain.advance(1,1,1150)==1,"new chain after third");
    check(chain.advance(1,1,2100)==1,"pause resets chain");
    check(chain.advance(2,1,2400)==1&&chain.advance(2,2,2700)==1,"equipment and recenter reset chain");
    check(chain.advance(2,2,2600)==1&&chain.advance(0,2,2900)==0,"backward time and invalid owner reset");
    puts("PASS: gesture contact windows, no idle rearm, stale/reset guards and linked swing sequence");
}
