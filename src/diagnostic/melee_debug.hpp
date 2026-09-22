#pragma once
#include "../tracking/attack_panel_settings.hpp"
#include <d3d11.h>
#include <vector>
#include "../tracking/weapon_contact_profile.hpp"
#include "melee_debug_pipeline.hpp"
#include <d3dcompiler.h>
#include "../tracking/melee_debug_settings.hpp"
#include "../tracking/longsword_trail.hpp"
#include "../tracking/longsword_debug_view.hpp"
namespace melee_debug {
inline constexpr unsigned capacity=32768;
static_assert(capacity>=2*amalur::maxWeaponContactSamples*(3*24*6+6));

struct Sweep {mgs5vr::Vec3 from{},to{};uint64_t tick{};uint32_t owner{};};
inline SRWLOCK lock=SRWLOCK_INIT;
inline Sweep sweeps[2][amalur::maxWeaponContactSamples]{};
inline void record(unsigned hand,unsigned sample,mgs5vr::Vec3 from,mgs5vr::Vec3 to,uint32_t owner=0){
    if(hand>=2||sample>=amalur::maxWeaponContactSamples)return;
    AcquireSRWLockExclusive(&lock);sweeps[hand][sample]={from,to,GetTickCount64(),owner};ReleaseSRWLockExclusive(&lock);
}
struct Vertex {float x,y,z,r,g,b,a;};
inline ComPtr<ID3D11Device> device;
inline ComPtr<ID3D11VertexShader> vs;
inline ComPtr<ID3D11PixelShader> ps;
inline ComPtr<ID3D11InputLayout> layout;
inline ComPtr<ID3D11Buffer> vertices,constants;
inline ComPtr<ID3D11DepthStencilState> depth;
inline ComPtr<ID3D11RasterizerState> raster;
inline ComPtr<ID3D11BlendState> blend;
inline bool initialize(ID3D11Device* d){
    if(device.Get()==d)return vs&&ps&&layout&&vertices&&constants&&depth&&raster&&blend;
    device=d;vs.Reset();ps.Reset();layout.Reset();vertices.Reset();constants.Reset();depth.Reset();raster.Reset();
    const char* shader=R"(
cbuffer Camera : register(b0) {row_major float4x4 vp;};
struct V {float3 p:POSITION;float4 c:COLOR;};
struct O {float4 p:SV_POSITION;float4 c:COLOR;};
// Match native world shaders: geo-11 stereoizes SV_POSITION.
// Do not add a second eye shift to the position here.
O VS(V v){O o;o.p=mul(float4(v.p,1),vp);o.c=v.c;return o;}
float4 PS(O v):SV_TARGET{return v.c;}
)";
    ComPtr<ID3DBlob> v,p,error;
    if(FAILED(D3DCompile(shader,strlen(shader),"melee-debug",nullptr,nullptr,"VS","vs_5_0",0,0,&v,&error))||
       FAILED(D3DCompile(shader,strlen(shader),"melee-debug",nullptr,nullptr,"PS","ps_5_0",0,0,&p,&error)))return false;
    if(FAILED(d->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,&vs))||
       FAILED(d->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,&ps)))return false;
    D3D11_INPUT_ELEMENT_DESC elements[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0}};
    if(FAILED(d->CreateInputLayout(elements,2,v->GetBufferPointer(),v->GetBufferSize(),&layout)))return false;
    D3D11_BUFFER_DESC b{};b.ByteWidth=sizeof(Vertex)*capacity;b.Usage=D3D11_USAGE_DYNAMIC;b.BindFlags=D3D11_BIND_VERTEX_BUFFER;b.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    if(FAILED(d->CreateBuffer(&b,nullptr,&vertices)))return false;
    b.ByteWidth=64;b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;if(FAILED(d->CreateBuffer(&b,nullptr,&constants)))return false;
    D3D11_DEPTH_STENCIL_DESC z{};z.DepthEnable=FALSE;z.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;z.DepthFunc=D3D11_COMPARISON_ALWAYS;
    if(FAILED(d->CreateDepthStencilState(&z,&depth)))return false;
    D3D11_RASTERIZER_DESC r{};r.FillMode=D3D11_FILL_SOLID;r.CullMode=D3D11_CULL_NONE;r.DepthClipEnable=TRUE;
    if(FAILED(d->CreateRasterizerState(&r,&raster)))return false;
    D3D11_BLEND_DESC bd{};auto& rt=bd.RenderTarget[0];rt.BlendEnable=TRUE;
    rt.SrcBlend=D3D11_BLEND_SRC_ALPHA;rt.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOp=D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha=D3D11_BLEND_ONE;rt.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOpAlpha=D3D11_BLEND_OP_ADD;
    rt.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;blend.Reset();if(FAILED(d->CreateBlendState(&bd,&blend)))return false;
    log("Melee collision overlay initialized: cyan right, magenta left; toggle in F11 developer panel\n");return true;
}
inline void draw(IDXGISwapChain* chain,const float* vp,bool valid){
    static amalur::LongswordTrail trail;
    // Build-time emergency switch, independent of the user toggle.
    if constexpr(!amalur::meleeDebugAvailable)return;
    static amalur::MeleeDebugSettings settings;
    const bool enabled=settings.enabled();
    static int loggedToggle=-1;
    if(loggedToggle!=int(enabled)){
        loggedToggle=int(enabled);log("Melee collision overlay toggle=%s\n",enabled?"ON":"OFF");
    }
    auto waiting=[](const char* reason,uint32_t model=0){
        static uint64_t next{};const auto now=GetTickCount64();
        if(now>=next){next=now+3000;log("Melee collision overlay ON waiting=%s model=%u\n",reason,model);}
    };
    if(!valid||!firstPerson.load()||!arm_rig::enabled.load()||interfaceView.load()||!motion_controls::gameFocused()){
        trail.hide();
        waiting("gameplay-camera-or-arms");return;
    }
    mgs5vr::Pose poses[2];uint64_t frame,ticks[2];Sweep recent[2][amalur::maxWeaponContactSamples];uint32_t asset,owner,selection;bool dual;
    float charge;bool chargeReady;
    amalur::MeleeSwingEvent strike;unsigned generation;
    amalur::LongswordDebugStatus chargeDebug;
    AcquireSRWLockShared(&weapon_control::poseLock);
    poses[0]=weapon_control::visualPoses[0];poses[1]=weapon_control::visualPoses[1];frame=weapon_control::visualTick;
    asset=weapon_control::visualAsset;owner=weapon_control::visualWeapon;dual=weapon_control::visualDual;selection=weapon_control::visualSelection;
    ticks[0]=weapon_control::tick;ticks[1]=weapon_control::leftTick;
    charge=weapon_control::longswordCharge;chargeReady=weapon_control::longswordReady;
    strike=weapon_control::swingEvents[0];generation=weapon_control::generation;
    chargeDebug=weapon_control::longswordDebug;
    ReleaseSRWLockShared(&weapon_control::poseLock);
    const auto chargeNow=GetTickCount64();
    const bool chargeFresh=chargeDebug.active&&chargeDebug.tick&&chargeDebug.tick<=chargeNow&&chargeNow-chargeDebug.tick<250;
    const bool selectedPose=selection<=1&&selection==motion_controls::viewControls().selectedWeapon;
    const bool chargeVisible=amalur::knownLongswordModel(asset)&&selectedPose&&chargeFresh&&(chargeDebug.height||charge>0||chargeReady);
    const bool swordVisible=amalur::knownLongswordModel(asset)&&selectedPose&&weapon_control::physicalActor.load()!=0;
    static amalur::AttackPanelSettings attackPanel;
    const bool debugVisible=attackPanel.enabled()&&amalur::knownLongswordModel(asset)&&selectedPose;
    if(!enabled&&!chargeVisible&&!swordVisible&&!debugVisible){trail.hide();return;}
    auto now=GetTickCount64();if(!frame||frame>now||now-frame>=100||selection>1||selection!=motion_controls::viewControls().selectedWeapon){
        trail.hide();
        waiting("tracked-weapon-pose",asset);return;
    }
    const auto* profile=amalur::capturedContactProfile(asset);if(!profile){waiting("uncaptured-model",asset);return;}
    const float radius=profile->radius;
    AcquireSRWLockShared(&lock);memcpy(recent,sweeps,sizeof(recent));ReleaseSRWLockShared(&lock);
    std::vector<Vertex> data(capacity);unsigned count=0;
    unsigned rejected=0;
    auto visible=[&](mgs5vr::Vec3 p){
        double clip[4];for(unsigned j=0;j<4;++j)clip[j]=double(p.x)*vp[j]+double(p.y)*vp[4+j]+double(p.z)*vp[8+j]+vp[12+j];
        for(double v:clip)if(!std::isfinite(v))return false;
        return clip[3]>.1&&clip[2]>=0&&clip[2]<=clip[3]&&std::abs(clip[0])<4*clip[3]&&std::abs(clip[1])<4*clip[3];
    };
    auto line=[&](mgs5vr::Vec3 a,mgs5vr::Vec3 b,unsigned hand,float intensity,int axis=-1,const amalur::LongswordTrailSegment* effect=nullptr){
        if(count+6>data.size())return;
        const auto delta=b-a;
        if(!visible(a)||!visible(b)||mgs5vr::dot(delta,delta)>2500.f){++rejected;return;}
        // Thin camera-facing triangle ribbons avoid geo-11's line primitive path.
        auto side=amalur::cross(delta,mgs5vr::Vec3{vp[3],vp[7],vp[11]});
        if(mgs5vr::dot(side,side)<1e-10f)side=amalur::cross(delta,mgs5vr::Vec3{vp[2],vp[6],vp[10]});
        if(mgs5vr::dot(side,side)<1e-10f)side=amalur::cross(delta,mgs5vr::Vec3{0,0,1});
        const float length=std::sqrt(mgs5vr::dot(side,side));if(!std::isfinite(length)||length<1e-6f)return;
        side=side*((effect?.08f*effect->width:axis>=3?.18f:.08f)/length);
        float red=hand?intensity:0.1f,green=hand?0.15f:intensity;
        float blue=intensity;
        if(axis>=0){red=axis==0?intensity:0.1f;green=axis==1?intensity:0.1f;blue=axis==2?intensity:0.1f;}
        if(axis==3){red=intensity;green=.65f*intensity;blue=.12f;}
        if(axis==4){red=.2f;green=intensity;blue=.35f;}
        if(effect){red=effect->red;green=effect->green;blue=effect->blue;}
        auto vertex=[&](mgs5vr::Vec3 p){data[count++]={p.x,p.y,p.z,red,green,blue,effect?effect->alpha:1};};
        vertex(a-side);vertex(a+side);vertex(b+side);vertex(a-side);vertex(b+side);vertex(b-side);
    };
    if(debugVisible||chargeVisible){
        amalur::DebugProjection projection(vp);mgs5vr::Vec3 nearPoint,midPoint;
        float debugDepth=.98f;
        if(projection.point(0,0,0,nearPoint)&&projection.point(0,0,.5f,midPoint)){
            auto direction=midPoint-nearPoint;if(amalur::normalize(direction)){
                const auto p=nearPoint+direction*60.f;
                const float z=p.x*vp[2]+p.y*vp[6]+p.z*vp[10]+vp[14];
                const float w=p.x*vp[3]+p.y*vp[7]+p.z*vp[11]+vp[15];
                if(w>.1f&&z>0&&z<w)debugDepth=z/w;
            }
        }
        auto quadNdc=[&](float x,float y,float width,float height,float red,float green,float blue,float alpha){
            if(count+6>data.size())return;mgs5vr::Vec3 p[4];
            if(!projection.point(x,y,debugDepth,p[0])||!projection.point(x+width,y,debugDepth,p[1])
                ||!projection.point(x+width,y-height,debugDepth,p[2])||!projection.point(x,y-height,debugDepth,p[3]))return;
            for(auto v:p)if(!visible(v))return;
            for(unsigned i:{0u,1u,2u,0u,2u,3u})data[count++]={p[i].x,p[i].y,p[i].z,red,green,blue,alpha};
        };
        if(chargeVisible){
            // Small view-facing gauge just right of centre; fills bottom-up.
            constexpr float x=.12f,top=.015f,width=.009f,height=.11f,pad=.002f;
            const float progress=std::fmax(0.f,std::fmin(1.f,charge));
            quadNdc(x-pad,top+pad,width+2*pad,height+2*pad,.12f,.15f,.17f,.75f);
            quadNdc(x,top,width,height,.25f,.28f,.29f,.65f);
            const float fill=std::fmax(.003f,height*progress);
            quadNdc(x,top-height+fill,width,fill,chargeReady?.25f:1.f,chargeReady?1.f:.72f,.22f,.95f);
        }
        if(debugVisible){
        auto quad=[&](float x,float y,float width,float height,float red,float green,float blue,float alpha){
            quadNdc((x-.46f)*.25f,(y-.45f)*.25f-.06f,width*.25f,height*.25f,red,green,blue,alpha);
        };
        quad(.10f,.80f,.72f,.70f,.015f,.025f,.035f,.92f);
        auto text=[&](float y,const char* label,bool good){
            float x=.14f;for(unsigned i=0;label[i]&&i<26;++i,x+=.024f){const auto glyph=amalur::debugGlyph(label[i]);
                for(unsigned r=0;r<5;++r)for(unsigned c=0;c<3;++c)if(glyph[r*3+c]=='1')
                    quad(x+c*.006f,y-r*.01f,.0055f,.009f,good?.35f:1.f,good?1.f:.45f,good?.55f:.25f,1.f);
            }
        };
        text(.76f,"LONGSWORD DEBUG",true);
        const bool fresh=chargeDebug.tick&&chargeDebug.tick<=now&&now-chargeDebug.tick<250&&chargeDebug.active;
        text(.68f,fresh?(chargeDebug.gripMode?(chargeDebug.height?"GRIP HELD":"HOLD RIGHT GRIP"):(chargeDebug.height?"HEIGHT OK":"RAISE HAND TO SHOULDER")):"TRACKING OR MODE WAIT",fresh&&chargeDebug.height);
        text(.60f,"ANY BLADE ANGLE",fresh);
        text(.52f,chargeDebug.steady?"STEADY OK":"HOLD HAND STILL",fresh&&chargeDebug.steady);
        char value[64];snprintf(value,sizeof(value),chargeDebug.ready?"READY - STRIKE":"CHARGE %u%%",unsigned(chargeDebug.progress*100));
        text(.44f,value,fresh&&chargeDebug.ready);
        const float progress=fresh?std::fmax(0.f,std::fmin(1.f,chargeDebug.progress)):0;
        quad(.14f,.365f,.62f,.025f,.2f,.2f,.2f,1);if(progress>0)quad(.14f,.365f,.62f*progress,.025f,chargeDebug.ready?.2f:1.f,chargeDebug.ready?1.f:.7f,.2f,1);
        snprintf(value,sizeof(value),"SWING %u SPEED %.1f",chargeDebug.serial,chargeDebug.speed);text(.31f,value,true);
        const char* gate=chargeDebug.preparation?"RAISING - PREPARATION":!strcmp(chargeDebug.gate,"peak")?"STRIKE ACCEPTED":!strcmp(chargeDebug.gate,"stroke")?"CONTACT READY":!strcmp(chargeDebug.gate,"too-short")?"SWING TOO SHORT":!strcmp(chargeDebug.gate,"windup")?"PREPARING":!strcmp(chargeDebug.gate,"recovery")?"RESET OR REVERSE SWING":!strcmp(chargeDebug.gate,"ready")?"READY TO SWING":"WAITING FOR SWING";
        text(.23f,gate,!strcmp(chargeDebug.gate,"peak")||!strcmp(chargeDebug.gate,"stroke"));
        }
    }
    const bool trailEligible=swordVisible&&ticks[0]&&ticks[0]<=now&&now-ticks[0]<100&&mgs5vr::valid(poses[0]);
    if(trailEligible){
        const amalur::LongswordTrailIdentity id{weapon_control::physicalActor.load(),owner,generation,strike.serial};
        if(strike.weapon==owner&&strike.generation==generation&&strike.tick&&strike.tick<=frame&&strike.tick<=now&&now-strike.tick<250)
            trail.begin(id,strike.attackAsset,strike.tick);
        const auto base=amalur::contactCenter(*profile,0,poses[0]);
        const auto tip=amalur::contactCenter(*profile,profile->count-1,poses[0]);
        trail.sample(id,frame,true,base,tip);
        amalur::LongswordTrailSegment segments[amalur::LongswordTrail::maxSegments];
        const auto n=trail.segments(now,segments,amalur::LongswordTrail::maxSegments);
        for(unsigned i=0;i<n;++i)line(segments[i].a,segments[i].b,0,1,-1,&segments[i]);
    }else trail.hide();
    for(unsigned h=0;enabled&&h<(dual?2u:1u);++h){if(!ticks[h]||ticks[h]>now||now-ticks[h]>=100||!mgs5vr::valid(poses[h]))continue;
        if(asset==1689){
            // Solid positive axes; dashed negative axes. Guides only, no hits.
            const mgs5vr::Vec3 axes[]{{30,0,0},{0,30,0},{0,0,30}};
            auto world=[&](mgs5vr::Vec3 p){return mgs5vr::compose(poses[h],mgs5vr::Pose{{},p}).position;};
            for(unsigned axis=0;axis<3;++axis){
                line(world({}),world(axes[axis]),h,1.f,int(axis));
                for(unsigned n=0;n<5;++n)line(world(axes[axis]*(-float(n)/5)),world(axes[axis]*(-float(n+.5f)/5)),h,.65f,int(axis));
            }
        }
        for(unsigned s=0;s<profile->count;++s){auto center=amalur::contactCenter(*profile,s,poses[h]);
            for(unsigned plane=0;plane<3;++plane)for(unsigned n=0;n<24;++n){
                auto point=[&](unsigned j){float a=float(j)*6.28318530718f/24,c=radius*cosf(a),v=radius*sinf(a);
                    return center+mgs5vr::Vec3{plane==0?0:c,plane==1?0:(plane==0?c:v),plane==2?0:v};};
                line(point(n),point(n+1),h,0.7f);
            }
            auto& sweep=recent[h][s];if(asset==1520&&sweep.owner==owner&&sweep.tick&&now>=sweep.tick&&now-sweep.tick<250)line(sweep.from,sweep.to,h,1.f);
        }
    }
    if(!count)return;
    static bool traced=false;const bool trace=!traced;
    auto phase=[&](const char* name){if(trace)log("Melee overlay first draw: %s\n",name);};
    phase("resources");
    static uint64_t lastGeometryLog{};if(enabled&&now-lastGeometryLog>2000){lastGeometryLog=now;
        log("Melee overlay geometry: vertices=%u clippedSegments=%u right=%.2f,%.2f,%.2f left=%.2f,%.2f,%.2f\n",count,rejected,poses[0].position.x,poses[0].position.y,poses[0].position.z,poses[1].position.x,poses[1].position.y,poses[1].position.z);
        FILETIME wall;GetSystemTimeAsFileTime(&wall);
        log("VR capture clock tick=%llu utcFileTime=%llu\n",now,(uint64_t(wall.dwHighDateTime)<<32)|wall.dwLowDateTime);
        for(unsigned h=0;h<(dual?2u:1u);++h){const auto& p=poses[h];
            log("VR collision fit tick=%llu model=%u weapon=%08x selection=%u hand=%u count=%u radius=%.3f pose=%.5f,%.5f,%.5f,%.5f,%.4f,%.4f,%.4f vp=%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                now,asset,owner,selection,h,profile->count,radius,p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w,p.position.x,p.position.y,p.position.z,
                vp[0],vp[1],vp[2],vp[3],vp[4],vp[5],vp[6],vp[7],vp[8],vp[9],vp[10],vp[11],vp[12],vp[13],vp[14],vp[15]);
        }
    }
    ComPtr<ID3D11Device> d;if(FAILED(chain->GetDevice(IID_PPV_ARGS(&d))))return;
    if(!initialize(d.Get())){static bool warned=false;if(!warned){log("Melee overlay unavailable: shader or resources rejected\n");warned=true;}return;}
    ComPtr<ID3D11DeviceContext> context;d->GetImmediateContext(&context);
    auto c=context.Get();if(!simpleOutput(c))return;
    ComPtr<ID3D11Texture2D> back;if(FAILED(chain->GetBuffer(0,IID_PPV_ARGS(&back))))return;
    ComPtr<ID3D11RenderTargetView> target;if(FAILED(d->CreateRenderTargetView(back.Get(),nullptr,&target)))return;
    D3D11_TEXTURE2D_DESC desc{};back->GetDesc(&desc);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(c->Map(vertices.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return;
    memcpy(mapped.pData,data.data(),count*sizeof(Vertex));c->Unmap(vertices.Get(),0);
    if(FAILED(c->Map(constants.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return;
    memcpy(mapped.pData,vp,64);c->Unmap(constants.Get(),0);
    phase("capture wrapped state");
    {
    Pipeline saved(c);
    c->SetPredication(nullptr,FALSE);c->GSSetShader(nullptr,nullptr,0);c->HSSetShader(nullptr,nullptr,0);c->DSSetShader(nullptr,nullptr,0);
    c->OMSetBlendState(blend.Get(),nullptr,0xffffffffu);
    auto rt=target.Get();c->OMSetRenderTargets(1,&rt,nullptr);c->OMSetDepthStencilState(depth.Get(),0);
    c->RSSetState(raster.Get());D3D11_VIEWPORT viewport{0,0,float(desc.Width),float(desc.Height),0,1};c->RSSetViewports(1,&viewport);
    c->IASetInputLayout(layout.Get());c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    auto vb=vertices.Get();UINT stride=sizeof(Vertex),start=0;c->IASetVertexBuffers(0,1,&vb,&stride,&start);
    c->VSSetShader(vs.Get(),nullptr,0);c->PSSetShader(ps.Get(),nullptr,0);auto cb=constants.Get();c->VSSetConstantBuffers(0,1,&cb);
    phase("draw");c->Draw(count,0);phase("restore wrapped state");
    }
    phase("complete");traced=true;
    // Pipeline restores the wrapped state and releases the temporary backbuffer target.

}
}
