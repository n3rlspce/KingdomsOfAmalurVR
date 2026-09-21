#include "weapon_contact_profile.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
void check(bool value,const char* message){if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
bool near(float a,float b){return std::abs(a-b)<.001f;}
int main(){
 using namespace amalur;
 const uint32_t assets[]{1520,2478,1514,1250,1323,1689,5457};
 const unsigned counts[]{6,12,12,16,1,0,10};
 for(unsigned a=0;a<7;++a){const auto* p=capturedContactProfile(assets[a]);
  check(p&&p->count==counts[a]&&p->count<=maxWeaponContactSamples,"bounded per-asset profiles");
  check((!p->count||p->radius>0)&&std::isfinite(p->radius),"finite positive radius");
  for(unsigned i=0;i<p->count;++i){
   const auto point=contactCenter(*p,i,{{},{11,22,33}});
   check(near(point.x,p->centers[i].x+11)&&near(point.y,p->centers[i].y+22)&&near(point.z,p->centers[i].z+33),"model to world translation");
   if(i){const auto d=p->centers[i]-p->centers[i-1];check(mgs5vr::dot(d,d)<=4*p->radius*p->radius,"adjacent spheres have no gap along landmark line");}
  }
 }
 check(!capturedContactProfile(1877)&&!capturedContactProfile(0)&&!capturedContactProfile(1249),"thrown and uncaptured models rejected");
 check(prototypeDaggers.centers[0].z-prototypeDaggers.radius>=4&&prototypeDaggers.centers[5].z+prototypeDaggers.radius>=50,"dagger excludes grip and covers screenshot tip estimate");
 check(near(longswordContact.centers[11].z,85.8f)&&near(staffContact.centers[11].z,81.89f),"longsword visual fit and unchanged staff endpoint");
 check(near(greatswordContact.centers[0].z,18.79f)&&near(greatswordContact.centers[15].z,136),"greatsword marker interval excludes proximal grip");
 check(near(rustyLongswordContact.centers[9].z,64.007f)&&capturedContactProfile(5457)!=capturedContactProfile(2478),"rusty geometry stays separate");
 check(hammerContact.count==1&&near(hammerContact.centers[0].z,92.33f),"hammer head only");
 check(faebladesContact.count==0,"disproved faeblade hand-axis volume disabled pending axis fit");
 const float q=std::sqrt(.5f);const auto rotated=contactCenter(prototypeDaggers,5,{{0,q,0,q},{11,22,33}});
 check(near(rotated.x,57)&&near(rotated.z,33),"rotation follows corrected weapon pose");
 check(contactLine({},{},0,4).count==0&&contactLine({},{},17,4).count==0,"invalid generation counts bounded");
 puts("PASS: visual fit extents, contiguous envelopes, dagger grip exclusion, coordinate transforms and unsupported models");
}
