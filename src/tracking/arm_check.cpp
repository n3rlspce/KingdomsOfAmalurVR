#include "arm_pose.hpp"
#include "weapon_pose.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include "grip_filter.hpp"
using namespace mgs5vr;
void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
float distance(Vec3 a,Vec3 b){auto d=a-b;return std::sqrt(dot(d,d));}
void wristBasisChecks(){
    for(auto side:{amalur::ArmSide::Right,amalur::ArmSide::Left}){
        auto wrist=amalur::wristFromControllerGrip(side,Pose{});
        check(distance(rotate(wrist.orientation,{1,0,0}),{0,0,-1})<.001f,"native knuckle axis follows controller hand-forward axis");
        check(distance(rotate(wrist.orientation,{0,0,side==amalur::ArmSide::Right?1.f:-1.f}),{0,1,0})<.001f,
            "left and right thumb axes follow grip tube without a ninety-degree wrist bend");
        for(float yaw:{0.f,1.5707963268f,3.1415926536f,-1.5707963268f}){
            Pose grip{{0,0,std::sin(yaw*.5f),std::cos(yaw*.5f)},{20,30,140}};
            auto solved=amalur::wristFromControllerGrip(side,grip);
            check(distance(solved.position,grip.position)==0,"basis correction does not translate the wrist");
            check(distance(rotate(solved.orientation,{1,0,0}),rotate(grip.orientation,{0,0,-1}))<.001f,
                "wrist hand-forward axis remains correct through world turns");
            auto restored=amalur::controllerGripFromWrist(side,solved);
            for(auto axis:{Vec3{1,0,0},Vec3{0,1,0},Vec3{0,0,1}})
                check(distance(rotate(restored.orientation,axis),rotate(grip.orientation,axis))<.001f,"weapon sliders recover the original grip axes");
        }
    }
    // A neutral grip whose hand-forward axis is aligned with the solved forearm
    // must produce a straight visible wrist, independent of the bone's +X axis.
    amalur::RigBone bones[3]{},out[3]{};int16_t parents[]{-1,0,1};
    uint32_t ids[]{0xf21468,0xd1f75e,0x88d0eb};
    bones[0].position={0,20,155};bones[1].position={0,30,125};bones[2].position={0,40,100};
    Pose grip{{},{35,40,140}};
    check(amalur::solveRightArm(bones,out,3,parents,ids,grip,100),"neutral forearm fixture solves");
    auto forward=out[2].position-out[1].position;forward=forward*(1.f/distance(forward,{}));
    auto axis=amalur::cross({0,0,-1},forward);Quat q{axis.x,axis.y,axis.z,1-forward.z};
    float norm=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);grip.orientation={q.x/norm,q.y/norm,q.z/norm,q.w/norm};
    check(amalur::solveRightArm(bones,out,3,parents,ids,amalur::wristFromControllerGrip(amalur::ArmSide::Right,grip),100),"corrected neutral grip solves");
    check(distance(rotate(amalur::bonePose(out[2]).orientation,{1,0,0}),forward)<.001f,"neutral controller yields a straight forearm-to-knuckle direction");
    puts("PASS: anatomical wrist basis, mirrored thumb axes, straight neutral wrist, world turns and unchanged grip translation frame");
}
void weaponTranslationChecks(){
    amalur::RigBone native[7]{},out[7]{},copy[7]{},current[7]{};
    int16_t parents[]{-1,0,1,1,0,4,4};
    uint32_t ids[]{11436941,11992818,11092278,14407505,10760771,15110326,13087492};
    for(unsigned i=0;i<7;++i){native[i].position={float(i),float(i*2),float(i*3)};native[i].positionW=9;memset(native[i].opaque,0xa5,16);}
    memcpy(copy,native,sizeof(copy));
    Pose root{{0,0,.70710678f,.70710678f},{40,80,120}},right{{},{10,20,30}},left{{},{-10,20,30}};
    const Vec3 offset{2,3,4};
    check(amalur::translateDaggerGrip(native,out,7,parents,ids,root,right,left,offset,100),"dagger translation accepted");
    for(unsigned i=1;i<7;++i){
        auto worldDelta=rotate(root.orientation,out[i].position-native[i].position);
        check(distance(worldDelta,{i<4?-2.f:2.f,3,4})<.001f,"world displacement follows wrists and mirrors outward left");
        check(!memcmp(&out[i].orientation,&native[i].orientation,sizeof(Quat))&&out[i].positionW==9
            &&!memcmp(out[i].opaque,native[i].opaque,16),"translation preserves orientation and opaque bytes");
    }
    check(!memcmp(out,native,sizeof(native[0]))&&!memcmp(native,copy,sizeof(copy)),"root and source remain untouched");
    check(amalur::translateDaggerGrip(native,current,7,parents,ids,root,right,left,offset,100,true,false),"right-only position adjustment accepted");
    check(!memcmp(current+1,native+1,3*sizeof(native[0]))&&!memcmp(current+4,out+4,3*sizeof(native[0])),
        "left tracking loss preserves right offset and leaves native left branch untouched");
    check(amalur::translateDaggerGrip(native,current,7,parents,ids,root,right,left,offset,100,false,true),"left-only position adjustment accepted");
    check(!memcmp(current+4,native+4,3*sizeof(native[0]))&&!memcmp(current+1,out+1,3*sizeof(native[0])),
        "right tracking loss preserves left offset and leaves native right branch untouched");
    auto rotated=right;rotated.orientation={0,0,.70710678f,.70710678f};
    check(amalur::translateDaggerGrip(native,current,7,parents,ids,root,rotated,left,offset,200),"rotated wrist and nondefault scale accepted");
    auto rotatedDelta=rotate(root.orientation,current[4].position-native[4].position);
    check(distance(rotatedDelta,{-6,4,8})<.001f,"offset rotates with wrist and scales centimetres to game units");
    amalur::HeldWeaponIdentity identity{100,200,300,4,0x10001,0x10002,5,7};
    memcpy(current,native,sizeof(current));
    for(int frame=0;frame<100;++frame){
        check(amalur::translateDaggerGrip(current,out,7,parents,ids,root,right,left,offset,100),"repeated apply accepted");
        memcpy(current,out,sizeof(current));
        check(amalur::restoreDaggerTranslation(current,native,out,identity,identity),"matching current identity restores");
        check(!memcmp(current,native,sizeof(current)),"reused buffer never accumulates offsets");
    }
    // Restore before a disabled/zero frame, then leave native output untouched.
    memcpy(current,out,sizeof(current));
    check(amalur::restoreDaggerTranslation(current,native,out,identity,identity),"disable restores previous application");
    check(amalur::translateDaggerGrip(current,out,7,parents,ids,root,right,left,{},100)
        &&!memcmp(out,native,sizeof(out)),"zero offsets are byte-exact identity");
    check(amalur::translateDaggerGrip(native,out,7,parents,ids,root,right,left,offset,100),"restore overwrite fixture");
    memcpy(current,out,sizeof(current));current[1].position={100,101,102};current[2].orientation={0,0,1,0};
    check(amalur::restoreDaggerTranslation(current,native,out,identity,identity),"native overwrite restore accepted");
    check(distance(current[1].position,{100,101,102})==0&&current[2].orientation.z==1
        &&distance(current[2].position,native[2].position)==0,"native positions and rotations win independently");
    for(unsigned field=0;field<8;++field){
        auto changed=identity;
        switch(field){case 0:++changed.object;break;case 1:++changed.root;break;case 2:++changed.buffer;break;
            case 3:++changed.index;break;case 4:++changed.owner;break;case 5:++changed.rootOwner;break;
            case 6:++changed.asset;break;case 7:++changed.count;break;}
        memcpy(current,out,sizeof(current));
        check(!amalur::restoreDaggerTranslation(current,native,out,identity,changed)
            &&!memcmp(current,out,sizeof(current)),"replaced or reused object/buffer identity discards stale restore");
    }
    memcpy(current,out,sizeof(current));parents[3]=4;
    check(!amalur::translateDaggerGrip(native,out,7,parents,ids,root,right,left,offset,100)
        &&!memcmp(current,out,sizeof(current)),"unexpected dagger hierarchy rejected without output writes");parents[3]=1;
    check(!amalur::translateDaggerGrip(native,out,7,parents,ids,root,right,left,{21,0,0},100)
        &&!amalur::translateDaggerGrip(native,out,7,parents,ids,root,right,left,{NAN,0,0},100),"invalid offsets rejected");
    ids[2]++;check(!amalur::translateDaggerGrip(native,out,7,parents,ids,root,right,left,offset,100),"other weapon skeleton rejected");
    puts("PASS: weapon-only translation, wrist-relative axes, mirroring, scale, restoration, reused buffers and zero/invalid guards");
}
int main(){
    {
        amalur::GripFilter filter;Pose raw{},out{};uint64_t tick=1000;
        check(filter.sample(raw,tick,1,true,out),"visual grip filter initializes");
        float maximumNoise=0;
        for(unsigned i=0;i<100;++i){tick+=11;raw.position.x=(i%2?1.f:-1.f)*.002f;
            check(filter.sample(raw,tick,1,true,out),"noisy grip accepted");
            if(i>20)maximumNoise=std::max(maximumNoise,std::abs(out.position.x));
        }
        check(maximumNoise<.0011f,"visual filter attenuates alternating two-millimetre tracking noise");
        const auto duplicate=out;filter.sample(raw,tick,1,true,out);
        check(distance(duplicate.position,out.position)==0,"repeated camera callback does not advance filter");
        for(unsigned i=0;i<100;++i){tick+=11;raw.position.x+=.022f;filter.sample(raw,tick,1,true,out);}
        check(distance(raw.position,out.position)<.012f,"fast two-metre-per-second motion retains low position lag");
        for(unsigned i=0;i<30;++i){tick+=11;filter.sample(raw,tick,1,true,out);}
        check(distance(raw.position,out.position)<.0001f,"stopped controller converges without deadzone or reach loss");
        raw.orientation={0,0,0,-1};tick+=11;filter.sample(raw,tick,1,true,out);
        check(valid(out)&&std::abs(out.orientation.w)>.999f,"quaternion sign change does not spin visual hand");
        check(!filter.sample(raw,tick,1,false,out),"tracking loss resets visual filter");
        raw.position.y=2;tick+=11;filter.sample(raw,tick,1,true,out);
        check(distance(raw.position,out.position)==0,"reacquisition snaps directly to fresh tracking");
        raw.position.x+=.2f;tick+=11;filter.sample(raw,tick,2,true,out);
        check(distance(raw.position,out.position)==0,"recenter resets visual filter");
        raw.position.x+=.2f;tick+=150;filter.sample(raw,tick,2,true,out);
        check(distance(raw.position,out.position)==0,"long gap resets visual filter");
        puts("PASS: visual tracking noise attenuation, bounded fast-motion lag, full reach, duplicate samples and reset guards");
    }
    wristBasisChecks();
    weaponTranslationChecks();
    // The same freshness predicate gates arm solving and the dual-blade slot.
    // Verify independent hands across attack/sheath slots and the expiry edge.
    const uint64_t now=1000;
    const Pose validHand{};
    check(amalur::freshHeldDaggers(10,10,now,now,validHand,now,validHand,now),"recent held dagger remap permits idle-hide guard");
    check(!amalur::freshHeldDaggers(11,10,now,now,validHand,now,validHand,now),"equipment owner change revokes visibility override");
    check(!amalur::freshHeldDaggers(10,10,now-250,now,validHand,now,validHand,now),"old held remap cannot keep an unrelated native state visible");
    check(amalur::freshHeldDaggers(10,10,now,now,validHand,now,validHand,now-250),"one lost hand cannot revoke the other dagger visibility");
    check(!amalur::freshHeldDaggers(10,10,now,now,validHand,0,validHand,0),"both lost hands revoke visibility override");
    check(!amalur::freshHeldDaggers(10,10,0,now,validHand,now,validHand,now),"unseen held mapping cannot suppress hiding");
    auto daggerSlot=[&](uintptr_t slot,uint64_t rightTick,uint64_t leftTick){
        return amalur::trackedDaggerSlot(slot,amalur::freshWeaponPose(validHand,rightTick,now),
            amalur::freshWeaponPose(validHand,leftTick,now));
    };
    for(uintptr_t slot:{9u,11u}){
        check(daggerSlot(slot,now,now)==7,"both fresh hands force held daggers");
        check(daggerSlot(slot,now,0)==7,"right-only tracking keeps the right dagger held");
        check(daggerSlot(slot,0,now)==7,"left-only tracking keeps the left dagger held");
        check(daggerSlot(slot,now,now-250)==7,"stale left hand cannot sheath the right dagger");
        check(daggerSlot(slot,now-250,now)==7,"stale right hand cannot sheath the left dagger");
        check(daggerSlot(slot,now-250,now-250)==slot,"both stale hands preserve native dagger slot");
        check(daggerSlot(slot,now-249,now-249)==7,"both hands inside freshness boundary remain held");
        check(daggerSlot(slot,now+1,now)==7&&daggerSlot(slot,now,now+1)==7,"future timestamp on one hand cannot sheath the other valid hand");
        check(daggerSlot(slot,now+1,now+1)==slot,"both future timestamps retain native fallback");
        check(daggerSlot(slot,now,now)==7,"tracking recovery restores held slot");
    }
    for(uintptr_t slot:{5u,7u,8u,10u})check(daggerSlot(slot,now,now)==slot,"other native slots unchanged");
    auto invalidHand=validHand;invalidHand.position.x=std::numeric_limits<float>::quiet_NaN();
    check(!amalur::freshWeaponPose(invalidHand,now,now),"fresh timestamp cannot validate a nonfinite hand pose");
    Pose trim;
    check(amalur::gripAngleTrim(0,0,0,trim)&&distance(rotate(trim.orientation,{0,1,0}),{0,1,0})<.001f,"neutral grip trim is deterministic identity");
    check(amalur::gripAngleTrim(90,0,0,trim)&&distance(rotate(trim.orientation,{0,1,0}),{0,0,1})<.001f,"grip pitch rotates forward upward");
    check(amalur::gripAngleTrim(0,90,0,trim)&&distance(rotate(trim.orientation,{0,1,0}),{-1,0,0})<.001f,"grip yaw rotates in local horizontal plane");
    check(amalur::gripAngleTrim(0,0,90,trim)&&distance(rotate(trim.orientation,{0,0,1}),{1,0,0})<.001f,"grip roll rotates around forward axis");
    check(!amalur::gripAngleTrim(NAN,0,0,trim)&&!amalur::gripAngleTrim(0,181,0,trim),"invalid grip angles rejected");
    check(amalur::gripAngleTrim(30,-45,20,trim),"combined trim valid");
    Pose grip{{0,0,.70710678f,.70710678f},{12,34,56}};
    const auto adjusted=compose(grip,trim);
    check(distance(adjusted.position,grip.position)<.001f,"trim changes wrist angle without moving controller anchor");
    check(distance(rotate(adjusted.orientation,{0,1,0}),rotate(grip.orientation,rotate(trim.orientation,{0,1,0})))<.001f,"trim is controller-local independent of world heading");
    amalur::RigBone bones[6]{},out[6]{};
    int16_t parents[]{-1,0,1,2,3,0};uint32_t ids[]{1,0xf21468,0xd1f75e,0x88d0eb,2,3};
    bones[1].position={0,20,155};bones[2].position={0,30,125};bones[3].position={0,40,100};
    bones[4].position={2,40,96};bones[5].position={-10,-30,110};
    for(auto& b:bones){b.positionW=7;for(unsigned i=0;i<16;++i)b.opaque[i]=static_cast<unsigned char>(0x80+i);}
    amalur::RigBone original[6];std::memcpy(original,bones,sizeof(bones));
    const Pose target{{0,0,.38268343f,.92387953f},{35,40,140}};
    check(amalur::solveRightArm(bones,out,6,parents,ids,target,100),"reachable target");
    check(distance(out[3].position,target.position)<.001f,"wrist reaches controller");
    auto wristDirection=rotate(amalur::bonePose(out[3]).orientation,{1,0,0});
    auto targetDirection=rotate(target.orientation,{1,0,0});
    check(distance(wristDirection,targetDirection)<.001f,"solved wrist orientation survives native quaternion storage");
    check(std::abs(distance(out[1].position,out[2].position)-distance(bones[1].position,bones[2].position))<.001f,"upper arm length preserved");
    check(std::abs(distance(out[2].position,out[3].position)-distance(bones[2].position,bones[3].position))<.001f,"forearm length preserved");
    check(std::abs(distance(out[4].position,out[3].position)-distance(bones[4].position,bones[3].position))<.001f,"finger relative distance preserved");
    check(!std::memcmp(bones,original,sizeof(bones)),"native animation not mutated");
    check(!std::memcmp(out+5,bones+5,sizeof(bones[5])),"unrelated limb untouched");
    for(unsigned i=0;i<6;++i)check(out[i].positionW==7&&!std::memcmp(out[i].opaque,bones[i].opaque,16),"opaque bone bytes preserved");
    amalur::ArmReference reference;const Vec3 anchor{0,0,170};
    check(amalur::captureRightArmReference(bones,6,parents,ids,anchor,reference),"neutral arm reference captured");
    amalur::RigBone stable[6],attack[6],attackOriginal[6];
    check(amalur::solveRightArm(bones,stable,6,parents,ids,target,100,&reference,anchor),"reference arm solved");
    std::memcpy(attack,bones,sizeof(attack));
    // Model a spell animation which pulls the shoulder forward and sweeps the
    // whole arm upward. It must not change the controller-driven visual arm.
    for(unsigned i=1;i<=4;++i){attack[i].position=attack[i].position+Vec3{28,-15,20};attack[i].orientation={0,0,.70710678f,.70710678f};}
    attack[4].position=attack[4].position+Vec3{10,5,-8}; // Animated finger/socket moves independently.
    std::memcpy(attackOriginal,attack,sizeof(attack));
    check(amalur::solveRightArm(attack,out,6,parents,ids,target,100,&reference,anchor),"cast pose solved against neutral reference");
    for(unsigned i=1;i<=3;++i){
        check(distance(out[i].position,stable[i].position)<.001f,"cast animation cannot move controlled arm joints");
        check(distance(rotate(amalur::bonePose(out[i]).orientation,{1,0,0}),rotate(amalur::bonePose(stable[i]).orientation,{1,0,0}))<.001f,"cast animation cannot rotate controlled arm joints");
    }
    check(distance(out[4].position,stable[4].position)<.001f,"cast finger animation cannot move weapon socket");
    check(distance(rotate(amalur::bonePose(out[4]).orientation,{0,1,0}),rotate(amalur::bonePose(stable[4]).orientation,{0,1,0}))<.001f,"cast finger animation cannot rotate weapon socket");
    check(!std::memcmp(attack,attackOriginal,sizeof(attack)),"native attack pose remains untouched");
    check(!std::memcmp(out+5,attack+5,sizeof(attack[5])),"reference leaves other limbs animated");
    const Vec3 movedAnchor=anchor+Vec3{12,-23,7};auto movedTarget=target;movedTarget.position=movedTarget.position+(movedAnchor-anchor);
    check(amalur::solveRightArm(attack,out,6,parents,ids,movedTarget,100,&reference,movedAnchor),"reference follows body anchor");
    for(unsigned i=1;i<=3;++i)check(distance(out[i].position,stable[i].position+(movedAnchor-anchor))<.001f,"anchor shift translates solved arm coherently");
    amalur::ArmReference missing;
    check(!amalur::solveRightArm(bones,out,6,parents,ids,target,100,&missing,anchor),"uncaptured reference rejected");
    check(amalur::solveRightArm(bones,out,6,parents,ids,{{},{1000,40,150}},100),"distant target clamps");
    check(distance(out[1].position,out[3].position)<2*(distance(bones[1].position,bones[2].position)+distance(bones[2].position,bones[3].position)),"implausible target retains bounded extended reach");
    check(amalur::solveRightArm(bones,out,6,parents,ids,{{},bones[1].position},100),"coincident shoulder target stays finite");
    check(!amalur::solveRightArm(bones,out,6,parents,ids,target,0),"invalid scale rejected");
    auto invalid=target;invalid.position.x=std::numeric_limits<float>::quiet_NaN();
    check(!amalur::solveRightArm(bones,out,6,parents,ids,invalid,100),"NaN target rejected");
    parents[2]=2;check(!amalur::solveRightArm(bones,out,6,parents,ids,target,100),"cyclic hierarchy rejected");parents[2]=1;
    ids[4]=ids[3];check(!amalur::solveRightArm(bones,out,6,parents,ids,target,100),"ambiguous wrist ID rejected");
    // Two independent arm chains from the captured player skeleton IDs.
    amalur::RigBone both[9]{},rightOnly[9]{},dual[9]{},reverse[9]{},leftOnly[9]{};
    int16_t dualParents[]{-1,0,1,2,3,0,5,6,7};
    uint32_t dualIds[]{1,0xf21468,0xd1f75e,0x88d0eb,2,0xf1076a,0xd0ea60,0x87c3ed,3};
    for(unsigned i=1;i<=4;++i){both[i]=original[i];both[i+4]=original[i];both[i+4].position.y=-both[i].position.y;}
    amalur::ArmReference rightRef,leftRef;
    check(amalur::captureRightArmReference(both,9,dualParents,dualIds,anchor,rightRef)&&
        amalur::captureLeftArmReference(both,9,dualParents,dualIds,anchor,leftRef),"capture independent left and right references");
    Pose rightTarget{{},{35,40,140}},leftTarget{{},{35,-40,140}};
    check(amalur::solveRightArm(both,rightOnly,9,dualParents,dualIds,rightTarget,100,&rightRef,anchor)&&
        amalur::solveLeftArm(rightOnly,dual,9,dualParents,dualIds,leftTarget,100,&leftRef,anchor),"solve both hands sequentially");
    check(distance(dual[3].position,rightTarget.position)<.001f&&distance(dual[7].position,leftTarget.position)<.001f,"both wrists reach independent targets");
    check(!std::memcmp(dual+1,rightOnly+1,4*sizeof(both[0])),"left solve preserves previously solved right arm byte-for-byte");
    check(distance(dual[6].position,{dual[2].position.x,-dual[2].position.y,dual[2].position.z})<.001f,"left elbow pole mirrors right elbow across body");
    check(amalur::solveLeftArm(both,leftOnly,9,dualParents,dualIds,leftTarget,100,&leftRef,anchor)&&
        amalur::solveRightArm(leftOnly,reverse,9,dualParents,dualIds,rightTarget,100,&rightRef,anchor),"solve hands in reverse order");
    check(!std::memcmp(dual,reverse,sizeof(dual)),"arm solve order does not change either hand");
    check(!amalur::solveLeftArm(both,dual,9,dualParents,dualIds,leftTarget,100,&rightRef,anchor),"right calibration rejected for left hand");
    auto leftRotated=leftTarget;leftRotated.orientation={.38268343f,0,0,.92387953f};
    check(amalur::solveLeftArm(both,dual,9,dualParents,dualIds,leftRotated,100,&leftRef,anchor),"left wrist orientation follows controller");
    check(distance(rotate(amalur::bonePose(dual[7]).orientation,{0,1,0}),rotate(leftRotated.orientation,{0,1,0}))<.001f,"left native quaternion storage preserves controller orientation");
    check(distance(dual[8].position,compose(amalur::bonePose(dual[7]),leftRef.handRelative[8]).position)<.001f,"left finger attachment follows wrist");
    for(auto side:{amalur::ArmSide::Right,amalur::ArmSide::Left}){
        const unsigned shoulder=side==amalur::ArmSide::Right?1:5,elbow=shoulder+1,wrist=shoulder+2;
        const auto& ref=side==amalur::ArmSide::Right?rightRef:leftRef;
        const float upper=distance(both[shoulder].position,both[elbow].position);
        const float lower=distance(both[elbow].position,both[wrist].position);
        // Sweep through the old limit in both directions: no endpoint plateau
        // or accumulated stretch, and hand/socket geometry stays rigid.
        for(float reach:{40.f,57.f,59.f,65.f,80.f,95.f,80.f,59.f,57.f,40.f}){
            Pose extended{{0,0,.38268343f,.92387953f},both[shoulder].position+Vec3{reach,0,0}};
            check(amalur::solveArm(side,both,dual,9,dualParents,dualIds,extended,100,&ref,anchor),"extended tracked arm solves");
            check(distance(dual[wrist].position,extended.position)<.002f,"wrist follows controller beyond authored reach");
            const float u=distance(dual[shoulder].position,dual[elbow].position),l=distance(dual[elbow].position,dual[wrist].position);
            check(std::abs(u/l-upper/lower)<.001f,"extension preserves upper-to-forearm proportions");
            if(reach<upper+lower-.1f)check(std::abs(u-upper)<.001f&&std::abs(l-lower)<.001f,"returning inside reach restores native segment lengths");
            check(distance(dual[shoulder].position,both[shoulder].position)<.001f,"extension keeps shoulder anchored");
            check(distance(rotate(amalur::bonePose(dual[wrist]).orientation,{1,0,0}),rotate(extended.orientation,{1,0,0}))<.001f,"extension preserves corrected wrist orientation");
            check(distance(dual[wrist+1].position,compose(amalur::bonePose(dual[wrist]),ref.handRelative[wrist+1]).position)<.001f,"extension carries rigid hand and weapon socket with wrist");
        }
    }
    puts("PASS: bilateral controller reach beyond native length, proportional extension, anchored shoulders, rigid sockets and contraction without drift");
    dualIds[8]=dualIds[7];check(!amalur::solveLeftArm(both,dual,9,dualParents,dualIds,leftTarget,100),"ambiguous left wrist rejected");
    std::puts("PASS: bilateral arm reach, independent mirrored elbow poles, native quaternion storage, animation-independent reference, moving body anchor, immutable source and input guards");
}
