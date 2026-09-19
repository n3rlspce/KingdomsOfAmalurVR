#pragma once
#include <DirectXMath.h>
#include <openxr/openxr.h>

inline XrPosef renderedEyePose(const XrPosef& currentHead,const XrPosef& currentEye,const XrPosef& renderHead){
    using namespace DirectX;
    auto h=currentHead.orientation,e=currentEye.orientation,r=renderHead.orientation;
    auto current=XMVectorSet(h.x,h.y,h.z,h.w);
    auto rendered=XMQuaternionNormalize(XMVectorSet(r.x,r.y,r.z,r.w));
    // DirectX multiply(Q1,Q2) computes Q2*Q1. Apply the change of head
    // frame on the left, retaining the runtime's per-eye cant and offset.
    auto delta=XMQuaternionMultiply(XMQuaternionInverse(current),rendered);
    XMFLOAT4 orientation;XMStoreFloat4(&orientation,XMQuaternionMultiply(XMVectorSet(e.x,e.y,e.z,e.w),delta));
    auto p=currentEye.position,b=currentHead.position;
    XMFLOAT3 offset;XMStoreFloat3(&offset,XMVector3Rotate(XMVectorSet(p.x-b.x,p.y-b.y,p.z-b.z,0),delta));
    return {{orientation.x,orientation.y,orientation.z,orientation.w},
        {renderHead.position.x+offset.x,renderHead.position.y+offset.y,renderHead.position.z+offset.z}};
}
