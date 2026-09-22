#pragma once
#include <DirectXMath.h>
#include "pose_channel.hpp"
#include <array>
#include <cmath>
namespace amalur {
struct MenuRotation {
    bool active=false;unsigned recenter{};
    std::array<float,4> orientation{};
    std::array<float,12> matrix{1,0,0,0,0,1,0,0,0,0,1,0};
    void reset(){active=false;matrix={1,0,0,0,0,1,0,0,0,0,1,0};}
    void update(const PosePacket& p){
        using namespace DirectX;
        if(!p.valid||!std::isfinite(p.projectionX)||!std::isfinite(p.projectionY)||p.projectionX<=0||p.projectionY<=0)return;
        auto q=XMVectorSet(p.orientation[0],p.orientation[1],p.orientation[2],p.orientation[3]);
        if(!active||recenter!=p.recenter){for(unsigned i=0;i<4;++i)orientation[i]=p.orientation[i];active=true;recenter=p.recenter;}
        auto anchor=XMVectorSet(orientation[0],orientation[1],orientation[2],orientation[3]);
        auto delta=XMQuaternionMultiply(anchor,XMQuaternionInverse(q));
        // Convert from opening clip coordinates to current-camera clip
        // coordinates. OpenXR forward is -Z. Store three homogeneous rows.
        const float px=p.projectionX,py=p.projectionY;
        XMFLOAT3 x,y,z;
        XMStoreFloat3(&x,XMVector3Rotate(XMVectorSet(1/px,0,0,0),delta));
        XMStoreFloat3(&y,XMVector3Rotate(XMVectorSet(0,1/py,0,0),delta));
        XMStoreFloat3(&z,XMVector3Rotate(XMVectorSet(0,0,-1,0),delta));
        matrix={px*x.x,px*y.x,px*z.x,0,py*x.y,py*y.y,py*z.y,0,-x.z,-y.z,-z.z,0};
    }
};
}
