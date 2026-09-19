#include "render_match.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>

using Matrix=std::array<float,16>;
static void check(bool condition,const char* text){if(!condition){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
static Matrix multiply(const Matrix& a,const Matrix& b){
    Matrix result{};
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){
        double value=0;for(unsigned k=0;k<4;++k)value+=double(a[r*4+k])*b[k*4+c];
        result[r*4+c]=static_cast<float>(value);
    }
    return result;
}
static Matrix identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
static Matrix yaw(float angle){const float c=std::cos(angle),s=std::sin(angle);return {c,0,-s,0,0,1,0,0,s,0,c,0,0,0,0,1};}
static double residual(const Matrix& w,const Matrix& actual,const Matrix& candidate){return amalur::cameraMatrixResidual(w.data(),actual.data(),candidate.data());}
int main(){
    Matrix projection{1.3f,0,0,0,0,1.8f,0,0,.04f,-.02f,1.001f,1,0,0,-.1f,0};
    auto view=yaw(.47f);view[12]=-37;view[13]=12;view[14]=5;
    const auto vp=multiply(view,projection);
    auto world=yaw(-.83f);
    for(unsigned c=0;c<3;++c){world[c]*=2;world[4+c]*=.75f;world[8+c]*=3.5f;}
    world[12]=120;world[13]=-450;world[14]=28;
    const auto wvp=multiply(world,vp);
    check(residual(world,wvp,vp)<1e-6,"rotated translated nonuniformly scaled world matches actual camera");
    auto wrongView=yaw(.53f);wrongView[12]=-37;wrongView[13]=12;wrongView[14]=5;
    check(residual(world,wvp,multiply(wrongView,projection))>.01,"wrong camera yaw rejected by residual");
    auto wrongProjection=projection;wrongProjection[0]*=1.2f;
    check(residual(world,wvp,multiply(view,wrongProjection))>.1,"wrong FOV detected independently of view");
    world[12]=16964.845703125f;world[13]=-31599.17578125f;world[14]=5981.65380859375f;
    auto largeWvp=multiply(world,vp);
    check(residual(world,largeWvp,vp)<1e-6,"large native-world translation remains accurate");
    auto nearEyeView=yaw(.47f);nearEyeView[12]=-world[12];nearEyeView[13]=-world[13];nearEyeView[14]=-world[14];
    auto nearEyeVP=multiply(nearEyeView,projection);
    check(residual(world,multiply(world,nearEyeVP),nearEyeVP)<1e-6,"large opposing translations do not require matrix inversion");
    auto invalid=world;invalid[0]=std::numeric_limits<float>::quiet_NaN();
    check(std::isinf(residual(invalid,largeWvp,vp)),"NaN world rejected");
    invalid=largeWvp;invalid[9]=std::numeric_limits<float>::infinity();
    check(std::isinf(residual(world,invalid,vp)),"nonfinite observed WVP rejected");
    invalid=vp;invalid[2]=std::numeric_limits<float>::quiet_NaN();
    check(std::isinf(residual(world,largeWvp,invalid)),"nonfinite camera candidate rejected");
    invalid=world;for(unsigned c=0;c<3;++c)invalid[4+c]=invalid[c];
    check(std::isinf(residual(invalid,multiply(invalid,vp),vp)),"singular affine world rejected");
    invalid=world;invalid[3]=.01f;
    check(std::isinf(residual(invalid,multiply(invalid,vp),vp)),"non-affine world rejected");
    const auto ortho=identity();
    check(std::isinf(residual(world,multiply(world,ortho),ortho)),"orthographic draw cannot falsely identify a camera");
    check(std::isinf(amalur::cameraMatrixResidual(nullptr,largeWvp.data(),vp.data())),"null data rejected");
    auto smallWorld=identity();smallWorld[0]=smallWorld[5]=smallWorld[10]=.001f;
    check(residual(smallWorld,multiply(smallWorld,vp),vp)<1e-6,"small nonsingular scale is accepted");
    puts("PASS: render camera matching, wrong yaw/FOV discrimination, large-world precision, affine and perspective guards");
}
