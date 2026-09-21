#include "support_grip.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
static void check(bool value,const char* message){if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(){
 using namespace amalur;using namespace mgs5vr;
 RigBone bones[6]{};for(unsigned i=0;i<6;++i){bones[i].orientation.w=1;bones[i].position={float(i),2,3};bones[i].positionW=17;memset(bones[i].opaque,0x5a,16);}
 int16_t parents[]{-1,0,1,2,-1,4};uint32_t ids[]{0x00f1076a,0x00d0ea60,0x0087c3ed,123,456,789};RigBone before[6];memcpy(before,bones,sizeof(bones));
 check(attachLeftHand(bones,6,parents,ids,Pose{{},{20,30,40}}),"attach valid hand");
 for(unsigned i:{0u,1u,4u,5u})check(!memcmp(&bones[i],&before[i],sizeof(RigBone)),"shoulder elbow other branches byte-identical");
 check(bones[2].position.x==20&&bones[2].position.y==30&&bones[3].position.x==21,"wrist reaches target, finger follows rigidly");
 check(!memcmp(bones[3].opaque,before[3].opaque,16)&&bones[3].positionW==17,"opaque native data preserved");
 RigBone palm[6];memcpy(palm,before,sizeof(palm));
 const float half=std::sqrt(.5f);
 check(attachLeftHandAtSocket(palm,6,parents,ids,3,Pose{{0,half,0,half},{20,30,40}}),"native grip socket attaches");
 check(std::abs(palm[3].position.x-20)<.001f&&std::abs(palm[3].position.y-30)<.001f
     &&std::abs(palm[3].position.z-40)<.001f,"socket reaches handle under rotated wrist");
 check(std::abs(palm[2].position.z-41)<.001f,"wrist stays offset from handle");
 for(unsigned i:{0u,1u,4u,5u})check(!memcmp(&palm[i],&before[i],sizeof(RigBone)),"socket attachment preserves arm and other branches");
 RigBone savedPalm[6];memcpy(savedPalm,palm,sizeof(palm));
 check(!attachLeftHandAtSocket(palm,6,parents,ids,4,Pose{})&&!memcmp(palm,savedPalm,sizeof(palm)),"socket outside left hand rejected without writes");
 bones[3].position.x=std::numeric_limits<float>::quiet_NaN();memcpy(before,bones,sizeof(bones));
 check(!attachLeftHand(bones,6,parents,ids,Pose{{},{1,2,3}})&&!memcmp(before,bones,sizeof(bones)),"invalid descendant causes no partial writes");
 SupportGrip state;Pose target,handle{},left{{},{0,0,-12}};
 check(!state.update(true,false,1,1,100,handle,left,target),"released grip cannot attach");
 check(state.update(true,true,1,1,100,handle,left,target),"near held grip attaches");
 handle.position={100,0,0};check(state.update(true,true,1,1,100,handle,left,target)&&target.position.x==100,"attached hand follows weapon without moving it");
 check(!state.update(false,true,1,1,100,handle,left,target)&&!state.attached,"tracking or equipment invalidation releases");
 check(!state.update(true,true,2,1,100,handle,left,target),"new distant weapon cannot inherit latch");
 handle={};check(state.update(true,true,2,1,100,handle,left,target),"new nearby weapon can attach");
 check(!state.update(true,false,2,1,100,handle,left,target),"button release detaches");
 // Each captured single-hand profile works in either selected equipment slot.
 for(auto kind:{HeldWeaponKind::Longsword,HeldWeaponKind::Staff,HeldWeaponKind::Greatsword,HeldWeaponKind::Hammer}){
   Vec3 point;check(supportGripPoint(kind,point),"single weapon has support point");
   for(unsigned slot:{0u,1u}){
     SupportGrip candidate;Pose localLeft{{},point*100};
     check(supportGripEligible(kind,slot,true)&&candidate.update(true,true,9,4,100,Pose{},localLeft,target,kind,slot),"both slots attach supported weapon");
     check(std::abs(target.position.z-point.z*100)<.001f,"configured support point used");
     check(!candidate.update(true,true,9,4,100,Pose{},localLeft,target,kind,1-slot)&&!candidate.attached,"slot change releases latch");
     check(!candidate.update(true,true,9,4,100,Pose{},localLeft,target,kind,2)&&!candidate.attached,"invalid slot releases");
     check(!supportGripEligible(kind,slot,false),"occupied offhand blocks support");
   }
 }
 for(auto kind:{HeldWeaponKind::None,HeldWeaponKind::Faeblades}){
   check(!supportGripEligible(kind,0,true)&&!state.update(true,true,2,1,100,Pose{},left,target,kind,0),"unknown and dual weapons excluded");
 }
 Vec3 staffPoint;check(supportGripPoint(HeldWeaponKind::Staff,staffPoint)&&staffPoint.z==-.12f,"staff fit unchanged");
 state.release();handle={};check(state.update(true,true,2,1,100,handle,left,target),"prepare recenter case");
 handle.position={100,0,0};check(!state.update(true,true,2,2,100,handle,left,target)&&!state.attached,"recenter cannot inherit distant latch");
 puts("PASS: hand-only transform, exact arm preservation, invalid data rollback, proximity, follow, release and owner reset");
}
