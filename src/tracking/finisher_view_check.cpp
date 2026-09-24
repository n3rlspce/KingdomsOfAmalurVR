#include "finisher_view.hpp"
#include "finisher_sequence.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
static void ck(bool b,const char* s){if(!b){std::fprintf(stderr,"FAIL %s\n",s);std::exit(1);}}
static bool near(amalur::Vec3 a,amalur::Vec3 b){auto d=a-b;return mgs5vr::dot(d,d)<1e-5f;}
int main(){
 using namespace amalur;
 // Native lifecycle, independent of input-mode swaps or QTE subphases.
 ck(!nativeFinisherSequence(false),"ordinary Reckoning without Fate_Shift is not a sequence");
 ck(nativeFinisherSequence(true),"Fate_Shift_Start owns camera after native entry");
 FinisherView sequenceView;Pose sequenceHead{};CameraPose sequenceCamera{};
 for(unsigned observedMode:{83u,84u,48u,66u}){
     (void)observedMode; // Deliberately not an input to ownership.
     ck(nativeFinisherSequence(true)&&sequenceView.apply(10,0,0,{0,0,195},{1,0,0},sequenceHead,100,sequenceCamera),"native mode changes retain active script camera");
 }
 if(!nativeFinisherSequence(false))sequenceView.reset();
 ck(!sequenceView.active,"native script completion releases camera");
 RigBone world{},joint{};world.orientation.w=joint.orientation.w=1;
 world.position={1000,2000,3000};joint.position={20,30,154};
 float inactive[]{0,0,NAN};memcpy(world.opaque,inactive,12);memcpy(joint.opaque,inactive,12);
 Vec3 eye{};
 ck(finisherHeadPosition(world,joint,world.position,eye)&&near(eye,{1020,2030,3154}),"opaque native tail must not reject animated head");
 joint.position.x+=100;ck(finisherHeadPosition(world,joint,world.position,eye)&&near(eye,{1120,2030,3154}),"animated model-space head moves camera while body location stays fixed");
 joint.position.x=100000;ck(!finisherHeadPosition(world,joint,world.position,eye),"stale distant head rejected");
 joint.position.x=NAN;ck(!finisherHeadPosition(world,joint,world.position,eye),"invalid head rejected");
 FinisherView v;Pose h{};CameraPose a{},b{};
 ck(v.apply(10,0,0,{10,20,195},{1,0,0},h,100,a),"entry");
 ck(near(a.eye,{10,20,195})&&near(a.target-a.eye,{200,0,0}),"first-person eye and entry heading");
 ck(v.apply(10,0,0,{200,300,290},{-1,0,0},h,100,b),"animated head travel");
 ck(near(b.eye,{200,300,290})&&near(b.target-b.eye,a.target-a.eye),"head follows animation without rotating view to native cut");
 h.position.x=.15f;ck(v.apply(10,0,0,{200,300,290},{0,1,0},h,100,b)&&near(b.eye,{200,315,290}),"HMD translation preserved");
 h.orientation={0,std::sin(.3f),0,std::cos(.3f)};
 ck(v.apply(10,0,0,{200,300,290},{0,1,0},h,100,b)&&!near(b.target-b.eye,a.target-a.eye),"HMD free look");
 ck(v.apply(10,1,0,{200,300,290},{0,1,0},h,100,b)&&near(b.eye,{200,300,290})&&near(b.target-b.eye,{200,0,0}),"recenter retains virtual heading");
 ck(v.apply(11,1,0,{0,0,195},{0,1,0},h,100,b)&&near(b.target-b.eye,{0,200,0}),"new actor rebases");
 v.reset();ck(v.apply(11,1,0,{0,0,195},{-1,0,0},h,100,b)&&near(b.target-b.eye,{-200,0,0}),"new sequence same actor rebases");
 ck(!v.apply(0,1,0,{},{1,0,0},h,100,b),"missing actor rejected");
 ck(!v.apply(11,1,0,{},{1,0,0},h,0,b),"invalid scale rejected");
 ck(!v.apply(11,1,0,{NAN,0,0},{1,0,0},h,100,b),"invalid eye rejected");
 puts("PASS first-person finisher animated-eye following, free HMD look, cut isolation, recenter and identity reset");
}
