#include "movement_basis.hpp"
#include <cstdio>
#include <cstdlib>
using mgs5vr::Vec3;
void check(bool pass,const char* message){if(!pass){std::printf("FAIL: %s\n",message);std::exit(1);}}
bool near(float a,float b){return std::abs(a-b)<.0001f;}
int main(){
    amalur::MovementBasis basis;
    for(int n=-180;n<=180;n+=30)for(int h=-180;h<=180;h+=30){
        const float nr=n*.01745329252f,hr=h*.01745329252f;
        Vec3 native{std::cos(nr),std::sin(nr),.4f},head{std::cos(hr),std::sin(hr),-.9f};
        basis.sample(native,head,true,1000);
        for(int axis=0;axis<4;++axis){
            const float ox=axis==0?1.f:axis==1?-1.f:0.f,oy=axis==2?1.f:axis==3?-1.f:0.f;
            float x=ox,y=oy;check(basis.transform(x,y,1001),"fresh basis applies");
            const Vec3 actual{-native.y*x+native.x*y,native.x*x+native.y*y,0};
            const Vec3 expected{-head.y*ox+head.x*oy,head.x*ox+head.y*oy,0};
            check(near(actual.x,expected.x)&&near(actual.y,expected.y),"all stick directions preserve head-relative world travel across headings");
        }
    }
    float x=.4f,y=-.8f;check(!basis.transform(x,y,1250)&&near(x,.4f)&&near(y,-.8f),"expired basis leaves menu/input unchanged");
    basis.sample({1,0,0},{0,1,0},false,2000);
    check(!basis.transform(x,y,2001),"disabled gameplay has no remapping");
    basis.sample({0,0,1},{0,1,0},true,2000);
    check(!basis.transform(x,y,2001),"degenerate native heading rejected");
    std::puts("movement_check PASS");
}
