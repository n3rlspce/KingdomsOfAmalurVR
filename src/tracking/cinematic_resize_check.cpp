#include "../bridge_tracking/cinematic_resize.hpp"
#include <cstdio>
#include <limits>
int main(){
 using amalur::CinematicResize;
 int failures=0;
 auto check=[&](bool ok){if(!ok)++failures;};
 check(!CinematicResize::ownsInput(true,0,0));
 check(!CinematicResize::ownsInput(true,1,0));
 check(!CinematicResize::ownsInput(true,0,1));
 check(!CinematicResize::ownsInput(false,1,1));
 check(!CinematicResize::ownsInput(true,std::numeric_limits<float>::quiet_NaN(),1));
 check(CinematicResize::ownsInput(true,1,1));
 CinematicResize resize;float size=1;
 for(int i=1;i<=20;++i)resize.update(size,1,0,false,i*20);check(size==1);
 resize.update(size,1,0,true,420);check(size==1);
 resize.update(size,0,0,true,440);resize.update(size,1,0,true,460);check(size>1);
 const float changed=size;check(resize.update(size,1,0,false,480));
 resize.update(size,1,0,false,500);check(size==changed);
 printf("Resize ownership/navigation/neutral entry/release: %s\n",failures?"FAIL":"PASS");return failures?1:0;
}
