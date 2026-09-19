#include "mgs5vr/arm_ik.hpp"
#include <algorithm>
#include <cmath>
namespace mgs5vr {
namespace {
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float length(Vec3 v){return std::sqrt(dot(v,v));}
Vec3 unit(Vec3 v){return v*(1/length(v));}
Quat normalize(Quat q){const float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);return {q.x/n,q.y/n,q.z/n,q.w/n};}
Quat swing(Vec3 a,Vec3 b){
    a=unit(a);b=unit(b);const float d=std::clamp(dot(a,b),-1.0f,1.0f);
    if(d<-0.9999f){const auto axis=unit(cross(a,std::abs(a.x)<0.8f?Vec3{1,0,0}:Vec3{0,1,0}));return {axis.x,axis.y,axis.z,0};}
    const auto axis=cross(a,b);return normalize({axis.x,axis.y,axis.z,1+d});
}
Quat turn(Quat delta,Quat q){return normalize(compose(Pose{delta,{}},Pose{q,{}}).orientation);}
std::optional<Quat> basisRotation(Vec3 localAxis,Vec3 localUp,Vec3 axis,Vec3 up){
    const auto frame=[](Vec3 x,Vec3 y)->std::optional<Pose>{
        if(length(x)<0.001f)return {};
        x=unit(x);y=y-x*dot(x,y);
        if(length(y)<0.001f)return {};
        y=unit(y);const auto z=cross(x,y);
        return nativeAffinePose({x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,0,0,0,1});
    };
    const auto local=frame(localAxis,localUp),world=frame(axis,up);
    if(!local||!world)return {};
    return compose(*world,inverse(*local)).orientation;
}
}
std::optional<Vec3> outsideArmSurface(Vec3 point,const ArmSurface& surface){
    if(!valid(Pose{{},point})||!valid(Pose{{},surface.point})||!valid(Pose{{},surface.normal})
       ||std::abs(dot(surface.normal,surface.normal)-1.f)>.003f
       ||!std::isfinite(surface.clearance)||surface.clearance<0||surface.clearance>.15f)return {};
    return point+surface.normal*std::max(0.f,surface.clearance-dot(point-surface.point,surface.normal));
}
std::optional<ArmSolution> solveArm(const ArmPose& a,Pose target,Vec3 hint,const ArmBasis* basis,const ArmSurface* surface){
    if(!valid(a.shoulder)||!valid(a.elbow)||!valid(a.wrist)||!valid(target)||!valid(Pose{{},hint}))return {};
    const auto upper=a.elbow.position-a.shoulder.position,lower=a.wrist.position-a.elbow.position;
    const float u=length(upper),l=length(lower);
    if(u<0.05f||l<0.05f||u>0.7f||l>0.7f)return {};
    auto ray=target.position-a.shoulder.position;float requested=length(ray);
    if(requested<0.0001f)ray=a.wrist.position-a.shoulder.position;
    if(length(ray)<0.0001f)return {};
    const auto forward=unit(ray);
    const float reach=std::clamp(requested,std::abs(u-l)+0.001f,u+l-0.001f);
    const float along=(u*u-l*l+reach*reach)/(2*reach);
    const float away=std::sqrt(std::max(0.0f,u*u-along*along));
    auto bend=hint-forward*dot(hint,forward);
    if(length(bend)<0.001f)bend=upper-forward*dot(upper,forward);
    if(length(bend)<0.001f)bend=cross(forward,std::abs(forward.y)<0.8f?Vec3{0,1,0}:Vec3{1,0,0});
    const auto center=a.shoulder.position+forward*along;
    bend=unit(bend);
    if(surface){
        if(!outsideArmSurface(center,*surface))return {};
        // Keep both bone lengths by choosing a feasible point on the elbow
        // circle. Translating a solved elbow would stretch the arm instead.
        const auto inCircle=surface->normal-forward*dot(surface->normal,forward);
        const float span=length(inCircle)*away;
        const float required=surface->clearance-dot(center-surface->point,surface->normal);
        if(required>span+.0001f)return {};
        if(span>.0001f&&required>dot(bend,surface->normal)*away){
            const auto towardSurface=unit(inCircle);
            const float cosine=std::clamp(required/span,-1.f,1.f);
            auto tangent=bend-towardSurface*dot(bend,towardSurface);
            if(length(tangent)<.0001f)tangent=cross(forward,towardSurface);
            bend=towardSurface*cosine+unit(tangent)*std::sqrt(std::max(0.f,1.f-cosine*cosine));
        }
    }
    const auto elbow=center+bend*away;
    const auto wrist=a.shoulder.position+forward*reach;
    ArmSolution out{a,std::abs(requested-reach)>0.0001f};
    out.pose.shoulder.orientation=turn(swing(upper,elbow-a.shoulder.position),a.shoulder.orientation);
    auto forearmRotation=turn(swing(lower,wrist-elbow),a.elbow.orientation);
    if(basis){
        // The native elbow is a hinge. Pronation belongs to its downstream
        // twist helpers; rotating the elbow itself knots the upper sleeve.
        const auto upperRotation=basisRotation(basis->upperAxis,basis->elbowBend,elbow-a.shoulder.position,wrist-elbow);
        if(!upperRotation)return {};
        out.pose.shoulder.orientation=*upperRotation;
        const auto axis=unit(wrist-elbow);
        const auto up=rotate(*upperRotation,basis->wristUp);
        const auto lowerRotation=basisRotation(basis->forearmAxis,basis->wristUp,axis,up);
        if(!lowerRotation)return {};
        forearmRotation=*lowerRotation;
    }
    out.pose.elbow={forearmRotation,elbow};
    out.pose.wrist={normalize(target.orientation),wrist};
    return out;
}
std::optional<Pose> nativeAffinePose(const std::array<float,16>& m){
    for(float f:m)if(!std::isfinite(f))return {};
    if(std::abs(m[3])+std::abs(m[7])+std::abs(m[11])+std::abs(m[15]-1)>0.001f)return {};
    const Vec3 x{m[0],m[1],m[2]},y{m[4],m[5],m[6]},z{m[8],m[9],m[10]};
    if(std::abs(dot(x,x)-1)>0.003f||std::abs(dot(y,y)-1)>0.003f||std::abs(dot(z,z)-1)>0.003f
       ||std::abs(dot(x,y))>0.003f||std::abs(dot(x,z))>0.003f||std::abs(dot(y,z))>0.003f||dot(cross(x,y),z)<0.997f)return {};
    Quat q;const float trace=m[0]+m[5]+m[10];
    if(trace>0){const float s=std::sqrt(trace+1)*2;q={(m[6]-m[9])/s,(m[8]-m[2])/s,(m[1]-m[4])/s,s/4};}
    else if(m[0]>m[5]&&m[0]>m[10]){const float s=std::sqrt(1+m[0]-m[5]-m[10])*2;q={s/4,(m[4]+m[1])/s,(m[8]+m[2])/s,(m[6]-m[9])/s};}
    else if(m[5]>m[10]){const float s=std::sqrt(1+m[5]-m[0]-m[10])*2;q={(m[4]+m[1])/s,s/4,(m[9]+m[6])/s,(m[8]-m[2])/s};}
    else {const float s=std::sqrt(1+m[10]-m[0]-m[5])*2;q={(m[8]+m[2])/s,(m[9]+m[6])/s,s/4,(m[1]-m[4])/s};}
    return Pose{normalize(q),{m[12],m[13],m[14]}};
}
}
