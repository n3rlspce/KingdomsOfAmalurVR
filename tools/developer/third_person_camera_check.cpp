#include "camera_pose.hpp"
#include "camera_inputs.hpp"
#include "movement_basis.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
bool close(float a,float b){return std::abs(a-b)<.001f;}
int main(){
 amalur::CameraPose base{{0,-400,250},{0,0,100},{0,0,1}},out;
 mgs5vr::Pose head{{0,0,0,1},{0,1,0}};
 assert(amalur::trackedThirdPersonCamera(base,head,100,out));
 assert(close(out.eye.x,base.eye.x)&&close(out.eye.y,base.eye.y)&&close(out.eye.z,base.eye.z+100));
 auto native=base.target-base.eye;
 for(int i=0;i<4;++i){
  float r=i*1.57079632679f;
  auto orbit=amalur::orbitCamera(base,{std::cos(r),std::sin(r),0});
  head={{0,0,0,1},{0,0,0}};assert(amalur::trackedThirdPersonCamera(orbit,head,100,out));
  auto forward=out.target-out.eye;
  assert(close(forward.z,native.z));
  amalur::MovementBasis movement;movement.sample(forward,forward,true,10);
  float x=0,y=1;assert(movement.transform(x,y,11));assert(close(x,0)&&close(y,1));
  head={{0,std::sin(r*.5f),0,std::cos(r*.5f)},{0,0,0}};
  assert(amalur::trackedThirdPersonCamera(base,head,100,out));
  assert(close((out.target-out.eye).z,native.z));assert(close(out.up.x,0)&&close(out.up.y,0)&&close(out.up.z,1));
 }
 // Saved camera inputs restore only our own values, retaining engine changes.
 unsigned char bytes[1024]{};float fov=100,vrFov=130;
 std::memcpy(bytes+4,&out.eye,sizeof(out.eye));std::memcpy(bytes+0x14,&out.target,sizeof(out.target));std::memcpy(bytes+0x1c0,&out.up,sizeof(out.up));std::memcpy(bytes+0x2c,&vrFov,4);
 amalur::CameraInputs inputs{bytes,base,out,fov,vrFov};inputs.restore();
 assert(!std::memcmp(bytes+4,&base.eye,sizeof(base.eye))&&!std::memcmp(bytes+0x14,&base.target,sizeof(base.target)));
 puts("PASS: chase pitch does not tilt physical translation/yaw; four snap headings keep forward input; camera input restore");
}
