#pragma once
#include <DirectXMath.h>
#include <openxr/openxr.h>
#include <algorithm>
#include <cmath>
#include "../bridge_tracking/pose_channel.hpp"
struct WristHud {
    float alpha=0;uint64_t lastTick=0;
    static XrPosef pose(const amalur::PosePacket& hand,float along,float height){
        using namespace DirectX;
        auto q=XMQuaternionNormalize(XMVectorSet(hand.orientation[0],hand.orientation[1],hand.orientation[2],hand.orientation[3]));
        XMFLOAT3 offset;XMStoreFloat3(&offset,XMVector3Rotate(XMVectorSet(0,height,along,0),q));
        XMFLOAT4 orientation;XMStoreFloat4(&orientation,XMQuaternionMultiply(XMQuaternionRotationAxis(XMVectorSet(1,0,0,0),-XM_PIDIV2),q));
        return {{orientation.x,orientation.y,orientation.z,orientation.w},{hand.position[0]+offset.x,hand.position[1]+offset.y,hand.position[2]+offset.z}};
    }
    static bool valid(const amalur::PosePacket& hand,uint64_t now){
        if(!hand.valid||hand.tick>now||now-hand.tick>100)return false;
        float length=0;for(float v:hand.orientation){if(!std::isfinite(v))return false;length+=v*v;}
        for(float v:hand.position)if(!std::isfinite(v))return false;
        return length>.5f&&length<1.5f;
    }
    float update(const amalur::PosePacket& hand,const XrPosef& head,bool active,uint64_t now){
        using namespace DirectX;
        const float dt=lastTick&&now>=lastTick?std::min(.05f,float(now-lastTick)*.001f):0.f;lastTick=now;
        if(!active||!valid(hand,now)){alpha=0;return alpha;}
        auto q=XMQuaternionNormalize(XMVectorSet(hand.orientation[0],hand.orientation[1],hand.orientation[2],hand.orientation[3]));
        auto delta=XMVectorSet(head.position.x-hand.position[0],head.position.y-hand.position[1],head.position.z-hand.position[2],0);
        float distance=XMVectorGetX(XMVector3Length(delta));
        float facing=XMVectorGetX(XMVector3Dot(XMVector3Rotate(XMVectorSet(0,1,0,0),q),XMVector3Normalize(delta)));
        // Hysteresis avoids flicker on the edge of the inspection gesture.
        bool shown=distance>.18f&&distance<1.1f&&hand.position[1]>head.position.y-(alpha>.5f?.65f:.55f)&&facing>(alpha>.5f?.20f:.35f);
        alpha=std::clamp(alpha+(shown?1.f:-1.f)*dt*6.f,0.f,1.f);return alpha;
    }
};
