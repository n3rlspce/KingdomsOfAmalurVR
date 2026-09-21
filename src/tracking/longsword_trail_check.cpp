#include "longsword_trail.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace amalur;
void check(bool b,const char* s){if(!b){printf("FAIL %s\n",s);exit(1);}}
int main(){
 LongswordTrail t;LongswordTrailIdentity id{1,2,3,1};LongswordTrailSegment out[LongswordTrail::maxSegments]{};
 check(t.begin(id,50,1000),"accepted swing starts");
 check(t.sample(id,1000,true,{0,0,0},{0,0,78}),"first pose");
 check(t.sample(id,1010,true,{1,0,0},{1,0,78}),"second pose");
 auto n=t.segments(1010,out,LongswordTrail::maxSegments);check(n==3&&out[0].a.x==0&&out[0].b.x==1&&out[0].blue==1,"world geometry and cyan");
 const auto alpha=out[0].alpha;t.segments(1050,out,LongswordTrail::maxSegments);check(out[0].alpha<alpha,"fades");
 check(t.segments(1250,out,LongswordTrail::maxSegments)==0&&!t.active(),"expires at250ms");
 check(!t.begin(id,50,1300),"duplicate cannot restart expired trail");
 ++id.serial;check(t.begin(id,81,1400),"heavy starts");t.sample(id,1400,true,{0,0,0},{0,0,78});t.sample(id,1410,true,{1,0,0},{1,0,78});
 t.segments(1410,out,1);check(out[0].red==1&&out[0].blue==.2f&&out[0].width==2.5f,"heavy gold and output capacity");
 check(!t.sample(id,1420,true,{60,0,0},{60,0,78})&&!t.active(),"teleport suppresses entire trail");
 ++id.serial;t.begin(id,7,1500);t.sample(id,1500,true,{0,0,0},{0,0,78});check(!t.sample(id,1601,true,{1,0,0},{1,0,78}),"gap rejects");
 ++id.serial;t.begin(id,50,1700);auto wrong=id;++wrong.generation;check(!t.sample(wrong,1701,true,{},{0,0,78}),"generation rejects");
 ++id.serial;t.begin(id,50,1800);check(!t.sample(id,1801,false,{},{0,0,78}),"eligibility hides");
 ++id.serial;t.begin(id,50,1900);check(!t.sample(id,1901,true,{},{std::numeric_limits<float>::quiet_NaN(),0,0}),"invalid pose rejected");
 ++id.serial;t.begin(id,50,2000);for(unsigned i=0;i<200;++i)t.sample(id,2000+i,true,{float(i)*.02f,0,0},{float(i)*.02f,0,78});
 check(t.size()==32&&t.segments(2199,out,LongswordTrail::maxSegments)==93,"bounded rolling history");
 check(t.segments(2300,out,LongswordTrail::maxSegments)==0,"no updates hide");
 check(!t.begin(id,999,2400),"unsupported recipe rejects");
 puts("PASS custom V1 trail: expiry, fading, identity, teleport, gap, eligibility, invalid inputs and bounds");
}
