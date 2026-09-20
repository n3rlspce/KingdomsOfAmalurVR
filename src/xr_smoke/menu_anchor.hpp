#pragma once
#include <DirectXMath.h>
#include <openxr/openxr.h>
#include <cmath>

// A fullscreen panel belongs to LOCAL space. Capture its pose on opening,
// retain it across temporary tracking/source loss, and re-anchor on recenter.
struct MenuAnchor {
    XrPosef pose{{0,0,0,1},{0,0,-2}};
    bool active=false;
    unsigned generation=0;
    void close(){active=false;}
    void update(const XrPosef& head,unsigned recenter){
        if(active&&generation==recenter)return;
        using namespace DirectX;
        auto q=head.orientation;
        auto headRotation=XMQuaternionNormalize(XMVectorSet(q.x,q.y,q.z,q.w));
        XMFLOAT3 forward;XMStoreFloat3(&forward,XMVector3Rotate(XMVectorSet(0,0,-1,0),headRotation));
        // The panel must not capture head roll/pitch. Keep its up axis aligned
        // with LOCAL-space gravity and place its centre at opening eye height.
        // Looking straight up/down has no heading: retain the last panel yaw.
        const float yaw=forward.x*forward.x+forward.z*forward.z>1e-6f
            ?std::atan2(-forward.x,-forward.z):2.f*std::atan2(pose.orientation.y,pose.orientation.w);
        auto rotation=XMQuaternionRotationAxis(XMVectorSet(0,1,0,0),yaw);
        XMFLOAT3 offset;XMStoreFloat3(&offset,XMVector3Rotate(XMVectorSet(0,0,-2,0),rotation));
        XMFLOAT4 normalized;XMStoreFloat4(&normalized,rotation);
        pose={{normalized.x,normalized.y,normalized.z,normalized.w},
            {head.position.x+offset.x,head.position.y+offset.y,head.position.z+offset.z}};
        active=true;generation=recenter;
    }
};
