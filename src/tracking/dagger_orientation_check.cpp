#include "dagger_orientation.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace amalur;
void check(bool value,const char* message){if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
bool near(Vec3 a,Vec3 b){return std::abs(a.x-b.x)<.0001f&&std::abs(a.y-b.y)<.0001f&&std::abs(a.z-b.z)<.0001f;}
int main(){
    const uint32_t ids[]{11436941,11992818,11092278,14407505,10760771,15110326,13087492};
    const int16_t parents[]{-1,0,1,1,0,4,4};
    RigBone before[7]{};
    Pose anchor;check(gripAngleTrim(29,47,-19,anchor),"nonidentity fixture");anchor.position={11,23,37};
    for(unsigned i=0;i<7;++i){before[i].position={float(i),float(i*2),float(i*3)};
        before[i].orientation=nativeQuaternion(anchor.orientation);before[i].positionW=2.f;
        memset(before[i].opaque,0x40+i,sizeof(before[i].opaque));}
    before[1].position=anchor.position;
    before[2].position=mgs5vr::compose(anchor,Pose{{},{2,3,11}}).position;
    before[3].position=mgs5vr::compose(anchor,Pose{{},{-1,4,19}}).position;
    RigBone after[7];memcpy(after,before,sizeof(after));
    check(uprightLeftDagger(after,7,parents,ids),"valid fixture accepted");
    check(!memcmp(&before[1].position,&after[1].position,sizeof(Vec3)),"pivot unchanged exactly");
    check(!memcmp(before,after,sizeof(RigBone))&&!memcmp(before+4,after+4,3*sizeof(RigBone)),"root and right branch byte identical");
    const auto oldAxis=mgs5vr::rotate(anchor.orientation,{0,0,1});
    const auto newAxis=mgs5vr::rotate(bonePose(after[1]).orientation,{0,0,1});
    check(near(newAxis,oldAxis*-1.f),"blade local Z reversed under nonidentity pose");
    for(unsigned i=1;i<4;++i){
        check(before[i].positionW==after[i].positionW&&!memcmp(before[i].opaque,after[i].opaque,16),"opaque bytes preserved");
        const auto oldRelative=mgs5vr::compose(mgs5vr::inverse(anchor),bonePose(before[i]));
        const auto newRelative=mgs5vr::compose(mgs5vr::inverse(bonePose(after[1])),bonePose(after[i]));
        check(near(oldRelative.position,newRelative.position),"child rigid offset preserved");
        check(near(mgs5vr::rotate(oldRelative.orientation,{1,2,3}),mgs5vr::rotate(newRelative.orientation,{1,2,3})),"child rigid orientation preserved");
    }
    HeldWeaponIdentity identity{1,2,3,4,5,6,7,7};
    RigBone current[7];memcpy(current,after,sizeof(current));
    check(restoreDaggerOrientation(current,before,after,identity,identity),"restore accepted");
    for(unsigned i=0;i<7;++i){check(!memcmp(&current[i].position,&after[i].position,sizeof(Vec3)),"orientation restore does not write positions");
        check(!memcmp(&current[i].orientation,&before[i].orientation,sizeof(current[i].orientation)),"orientations restored");}
    // Runtime also restores the branch translation before the next native remap.
    check(restoreDaggerTranslation(current,before,after,identity,identity),"position restoration");
    check(!memcmp(current,before,sizeof(current)),"complete restore exact");
    check(uprightLeftDagger(current,7,parents,ids)&&!memcmp(current,after,sizeof(current)),"restore then apply does not accumulate");
    auto stale=identity;++stale.owner;
    check(!restoreDaggerOrientation(current,before,after,identity,stale)&&!memcmp(current,after,sizeof(current)),"stale identity unchanged");
    current[2].orientation=nativeQuaternion(Pose{}.orientation);const auto overwrite=current[2].orientation;
    check(restoreDaggerOrientation(current,before,after,identity,identity),"partial restore accepted");
    check(!memcmp(&current[2].orientation,&overwrite,sizeof(overwrite)),"native orientation overwrite wins");
    RigBone invalid[7],saved[7];memcpy(invalid,before,sizeof(invalid));
    invalid[3].position.x=std::numeric_limits<float>::quiet_NaN();memcpy(saved,invalid,sizeof(saved));
    check(!uprightLeftDagger(invalid,7,parents,ids)&&!memcmp(invalid,saved,sizeof(saved)),"invalid full candidate cannot partially write");
    memcpy(invalid,before,sizeof(invalid));auto wrongIds=std::array<uint32_t,7>{};memcpy(wrongIds.data(),ids,sizeof(ids));++wrongIds[2];
    check(!uprightLeftDagger(invalid,7,parents,wrongIds.data())&&!memcmp(invalid,before,sizeof(invalid)),"wrong skeleton rejected");
    int16_t wrongParents[7];memcpy(wrongParents,parents,sizeof(parents));wrongParents[3]=4;
    check(!uprightLeftDagger(invalid,7,wrongParents,ids)&&!memcmp(invalid,before,sizeof(invalid)),"wrong branches rejected");
    puts("PASS: dagger local-X flip, pivot, rigid descendants, isolated bytes, guarded restore and invalid-input atomicity");
}
