#include "src/tracking/fine_facing.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
void check(bool ok,const char* s){if(!ok){std::printf("FAIL %s\n",s);std::exit(1);}}
uint32_t angle(double degrees){uint32_t d;amalur::fineFacingDelta(std::cos(degrees*3.141592653589793/180),std::sin(degrees*3.141592653589793/180),0,d);return d;}
double signedDegrees(uint32_t v){return (v<=0x7fffffffu?double(v):double(v)-4294967296.0)*(360.0/4294967296.0);}
int main(){
 uint32_t delta=99;check(!amalur::fineFacingDelta(1,0,0,delta)&&delta==0,"zero yaw does not enqueue a rotation");
 uint32_t current=angle(10.0);
 for(unsigned i=1;i<=1000;++i){double requested=10+i*.001;uint32_t target=angle(requested);delta=target-current;check(delta>0&&std::abs(signedDegrees(delta)-.001)<1e-7,"sub-degree ramp has no 1-degree steps or deadband");current+=delta;check(current==target,"native modular accumulator reaches requested angle");}
 check(std::abs(signedDegrees(angle(.1)-angle(359.9))-.2)<1e-7,"positive wrap uses short delta");
 check(std::abs(signedDegrees(angle(359.9)-angle(.1))+.2)<1e-7,"negative wrap uses short delta");
 check(angle(90)==0x40000000u&&angle(180)==0x80000000u&&angle(270)==0xc0000000u,"native full-turn angle units");
 check(!amalur::fineFacingDelta(0,0,0,delta),"degenerate direction rejected");
 check(!amalur::fineFacingDelta(std::numeric_limits<double>::quiet_NaN(),0,0,delta),"NaN rejected");
 check(!amalur::fineFacingDelta(0,std::numeric_limits<double>::infinity(),0,delta),"infinity rejected");
 check(angle(-.000000001)==0,"near-turn rounding wraps without undefined uint conversion");
 std::puts("PASS: fine-angle ramp, native angle units, bidirectional wrap, modular accumulation and input rejection");
}
