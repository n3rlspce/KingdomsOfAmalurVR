#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "ui_capture.hpp"
#include "wrist_clip.hpp"
#include "ui_shader.hpp"
#include <cstdio>
#include <cmath>
#include <stdexcept>
#include <string>
using Microsoft::WRL::ComPtr;
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
void hr(HRESULT result){require(SUCCEEDED(result),"D3D call failed");}
ComPtr<ID3DBlob> compile(const char* code,const char* entry,const char* profile){
    ComPtr<ID3DBlob> result,error;auto h=D3DCompile(code,strlen(code),nullptr,nullptr,nullptr,entry,profile,0,0,&result,&error);
    if(FAILED(h)&&error)puts(static_cast<const char*>(error->GetBufferPointer()));hr(h);return result;
}
int main(){try{
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;
    hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context));
    auto texture=[&](unsigned w,unsigned h){D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;ComPtr<ID3D11Texture2D> t;hr(device->CreateTexture2D(&d,nullptr,&t));return t;};
    auto left=texture(64,48),right=texture(64,48),smallTexture=texture(16,16);
    ComPtr<ID3D11RenderTargetView> leftRt,rightRt,smallRt;
    hr(device->CreateRenderTargetView(left.Get(),nullptr,&leftRt));hr(device->CreateRenderTargetView(right.Get(),nullptr,&rightRt));hr(device->CreateRenderTargetView(smallTexture.Get(),nullptr,&smallRt));
    auto bind=[&](ID3D11RenderTargetView* target){context->OMSetRenderTargets(1,&target,nullptr);};bind(leftRt.Get());
    D3D11_DEPTH_STENCIL_DESC ds{};ds.DepthEnable=false;ds.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;ds.DepthFunc=D3D11_COMPARISON_ALWAYS;
    ComPtr<ID3D11DepthStencilState> depth;hr(device->CreateDepthStencilState(&ds,&depth));context->OMSetDepthStencilState(depth.Get(),9);
    D3D11_BLEND_DESC b{};auto& rt=b.RenderTarget[0];rt.BlendEnable=true;rt.SrcBlend=D3D11_BLEND_SRC_ALPHA;rt.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOp=rt.BlendOpAlpha=D3D11_BLEND_OP_ADD;rt.SrcBlendAlpha=D3D11_BLEND_ZERO;rt.DestBlendAlpha=D3D11_BLEND_ONE;rt.RenderTargetWriteMask=15;
    ComPtr<ID3D11BlendState> blend;hr(device->CreateBlendState(&b,&blend));const float factors[]{.2f,.3f,.4f,.5f};context->OMSetBlendState(blend.Get(),factors,0xffffffff);
    D3D11_RASTERIZER_DESC rs{};rs.FillMode=D3D11_FILL_SOLID;rs.CullMode=D3D11_CULL_NONE;rs.ScissorEnable=true;rs.DepthClipEnable=true;
    ComPtr<ID3D11RasterizerState> raster;hr(device->CreateRasterizerState(&rs,&raster));context->RSSetState(raster.Get());
    D3D11_VIEWPORT vp{0,0,64,48,0,1};context->RSSetViewports(1,&vp);D3D11_RECT clip{8,3,56,45};context->RSSetScissorRects(1,&clip);
    auto vsCode=compile("float4 vs(uint i:SV_VertexID):SV_POSITION {float2 uv=float2((i<<1)&2,i&2);return float4(uv*float2(2,-2)+float2(-1,1),0,1);}","vs","vs_4_0");
    auto psCode=compile("float4 ps():SV_TARGET{return float4(1,0,0,.5);}","ps","ps_4_0");
    ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;hr(device->CreateVertexShader(vsCode->GetBufferPointer(),vsCode->GetBufferSize(),nullptr,&vs));hr(device->CreatePixelShader(psCode->GetBufferPointer(),psCode->GetBufferSize(),nullptr,&ps));
    context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const float background[]{0,1,0,1};context->ClearRenderTargetView(leftRt.Get(),background);context->ClearRenderTargetView(rightRt.Get(),background);
    amalur::UiCapture capture;capture.output(left.Get());capture.beginFrame();
    {amalur::UiCapture::Binding layer(capture,context.Get(),true);require(bool(layer),"left capture eligible");context->Draw(3,0);}
    bind(rightRt.Get());{amalur::UiCapture::Binding layer(capture,context.Get(),true);require(bool(layer),"right capture eligible");context->Draw(3,0);}
    auto pixel=[&](ID3D11Texture2D* t,UINT x,UINT y){D3D11_TEXTURE2D_DESC d{};t->GetDesc(&d);d.Usage=D3D11_USAGE_STAGING;d.BindFlags=d.MiscFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> staging;hr(device->CreateTexture2D(&d,nullptr,&staging));context->CopyResource(staging.Get(),t);D3D11_MAPPED_SUBRESOURCE m{};hr(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&m));auto p=static_cast<unsigned char*>(m.pData)+y*m.RowPitch+x*4;std::array<int,4> value{p[0],p[1],p[2],p[3]};context->Unmap(staging.Get(),0);return value;};
    require(pixel(left.Get(),32,24)==std::array<int,4>{0,255,0,255},"left world must contain no captured UI");
    require(pixel(right.Get(),32,24)==std::array<int,4>{0,255,0,255},"right world must contain no captured UI");
    auto p=pixel(capture.texture(),32,24);require(p[0]==128&&p[1]==0&&p[3]==128,"coverage is half alpha, not accumulated twice");
    require(pixel(capture.texture(),4,24)==std::array<int,4>{0,0,0,0},"outside scissor transparent");
    require(pixel(capture.texture(),32,4)[3]==128&&pixel(capture.texture(),32,44)[3]==128,"top and bottom remain visible");
    ComPtr<ID3D11RenderTargetView> restored;context->OMGetRenderTargets(1,&restored,nullptr);require(restored==rightRt,"restore native target");
    ComPtr<ID3D11BlendState> restoredBlend;float restoredFactors[4];UINT mask;context->OMGetBlendState(&restoredBlend,restoredFactors,&mask);require(restoredBlend==blend&&mask==0xffffffff&&restoredFactors[1]==factors[1],"restore blend and constants");
    UINT count=1;D3D11_RECT actual;context->RSGetScissorRects(&count,&actual);require(actual.top==3&&actual.bottom==45,"preserve native clipping");
    {amalur::UiCapture::Binding layer(capture,context.Get(),false);require(!bool(layer),"non-UI draw rejected");}
    auto sceneImage=texture(64,48);ComPtr<ID3D11ShaderResourceView> sceneSrv;hr(device->CreateShaderResourceView(sceneImage.Get(),nullptr,&sceneSrv));auto sceneRaw=sceneSrv.Get();context->PSSetShaderResources(0,1,&sceneRaw);
    {amalur::UiCapture::Binding layer(capture,context.Get(),true);require(!bool(layer)&&layer.nativeBackdrop(),"sampled full-resolution room stays out of UI layer");}
    sceneRaw=nullptr;context->PSSetShaderResources(0,1,&sceneRaw);
    bind(smallRt.Get());{amalur::UiCapture::Binding layer(capture,context.Get(),true);require(!bool(layer),"small intermediate target rejected");}
    bind(leftRt.Get());capture.beginFrame();clip={16,16,48,32};context->RSSetScissorRects(1,&clip);
    {amalur::UiCapture::Binding layer(capture,context.Get(),true);require(bool(layer),"next frame capture");context->Draw(3,0);}
    require(pixel(capture.texture(),32,4)[3]==0,"old UI cleared next frame");
    capture.beginFrame();require(capture.draws()==0,"close emits no UI");capture.releaseOutput();
    {amalur::UiCapture::Binding layer(capture,context.Get(),true);require(!bool(layer),"resize invalidates target");}
    const char* ui="void vs(float2 p:POSITION,float4 c:COLOR,float2 t:TEXCOORD,out float4 o:SV_POSITION,out float4 oc:COLOR,out float2 ot:TEXCOORD){o=float4(p,0,1);oc=c;ot=t;}";
    auto native=compile(ui,"vs","vs_4_0");require(amalur::nativeUiShader(native->GetBufferPointer(),native->GetBufferSize()),"native textured UI recognised");
    auto font=compile("cbuffer g_instanceDatabuffer:register(b0){float4x4 mvp;} void vs(float3 p:POSITION,float4 c:COLOR,float2 t:TEXCOORD0,float q:TEXCOORD1,out float4 o:SV_POSITION,out float4 oc:COLOR,out float2 ot:TEXCOORD0,out float oq:TEXCOORD1){o=mul(mvp,float4(p,1));oc=c;ot=t;oq=q;}","vs","vs_4_0");require(amalur::nativeUiShader(font->GetBufferPointer(),font->GetBufferSize()),"native font recognised");
    auto minimap=compile("cbuffer g_constantDatabuffer:register(b0){float4x4 mvp;float4 u,v;} void vs(float3 p:POSITION,out float4 o:SV_POSITION,out float2 t:TEXCOORD0,out float q:TEXCOORD1){o=mul(mvp,float4(p,1));t=float2(dot(u,float4(p,1)),dot(v,float4(p,1)));q=p.z;}","vs","vs_4_0");require(amalur::nativeUiShader(minimap->GetBufferPointer(),minimap->GetBufferSize()),"native minimap recognised");
    auto solid=compile("void vs(float2 p:POSITION,float4 c:COLOR,out float4 o:SV_POSITION,out float4 oc:COLOR){o=float4(p,0,1);oc=c;}","vs","vs_4_0");require(amalur::nativeUiShader(solid->GetBufferPointer(),solid->GetBufferSize()),"native solid UI recognised");
    require(!amalur::nativeUiShader(vsCode->GetBufferPointer(),vsCode->GetBufferSize()),"fullscreen postprocess not UI");
    auto material=compile("cbuffer material:register(b0){float4x4 m;} float4 vs(float3 p:POSITION):SV_POSITION{return mul(m,float4(p,1));}","vs","vs_4_0");require(!amalur::nativeUiShader(material->GetBufferPointer(),material->GetBufferSize()),"world material not UI");
    // A single triangle spans both moved widgets AND the remaining HUD.
    // Verify partitioning happens before the ordinary projection and leaves
    // both native eyes unchanged in the extracted corners.
    amalur::WristClip wrist;require(wrist.initialize(context.Get()),"wrist clip texture");
    auto wristCode=compile(R"(
Texture1D<float4> clip:register(t117);
void vs(uint i:SV_VertexID,out float4 p:SV_POSITION,out float4 d:SV_ClipDistance1){
 float2 uv=float2((i<<1)&2,i&2);p=float4(uv*float2(2,-2)+float2(-1,1),0,1);
 float4 r=clip.Load(int2(0,0));d=clip.Load(int2(1,0)).x>.5?float4(p.x-r.x,r.y-p.x,p.y-r.z,r.w-p.y):1;
 p.xy*=.75;
})","vs","vs_5_0");
    ComPtr<ID3D11VertexShader> wristVs;hr(device->CreateVertexShader(wristCode->GetBufferPointer(),wristCode->GetBufferSize(),nullptr,&wristVs));
    context->VSSetShader(wristVs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);
    clip={0,0,64,48};context->RSSetScissorRects(1,&clip);
    for(auto target:{leftRt.Get(),rightRt.Get()}){
        bind(target);context->ClearRenderTargetView(target,background);
        for(int r=0;r<2;++r){amalur::WristClip::Binding bounds(wrist,context.Get(),r);context->Draw(3,0);}
    }
    for(auto image:{left.Get(),right.Get()}){
        require(pixel(image,13,10)==std::array<int,4>{0,255,0,255},"left HUD corner removed without world changes");
        require(pixel(image,51,10)==std::array<int,4>{0,255,0,255},"right HUD corner removed without world changes");
        require(pixel(image,23,10)==std::array<int,4>{0,255,0,255},"status right edge removed from head HUD");
        require(pixel(image,35,10)==std::array<int,4>{0,255,0,255},"boss left edge removed from head HUD");
        require(pixel(image,32,10)[0]==128&&pixel(image,32,35)[0]==128,"rest of HUD preserved once, without alpha doubling");
    }
    ComPtr<ID3D11ShaderResourceView> restoredClip;context->VSGetShaderResources(117,1,&restoredClip);
    require(!restoredClip,"clip slot restored after both passes");
    // geo11 can expose two eye views of the SAME array resource. Resource
    // identity alone is insufficient, and the capture must be publishable as
    // a single-slice mono image rather than rejecting all array-backed HUDs.
    D3D11_TEXTURE2D_DESC stereoDesc{};left->GetDesc(&stereoDesc);stereoDesc.ArraySize=2;
    ComPtr<ID3D11Texture2D> stereoArray;hr(device->CreateTexture2D(&stereoDesc,nullptr,&stereoArray));
    ComPtr<ID3D11RenderTargetView> slices[2];
    for(UINT i=0;i<2;++i){D3D11_RENDER_TARGET_VIEW_DESC r{};r.Format=stereoDesc.Format;r.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2DARRAY;r.Texture2DArray.ArraySize=1;r.Texture2DArray.FirstArraySlice=i;hr(device->CreateRenderTargetView(stereoArray.Get(),&r,&slices[i]));}
    context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);
    context->OMSetDepthStencilState(depth.Get(),9);context->OMSetBlendState(blend.Get(),factors,0xffffffff);
    clip={0,0,64,48};context->RSSetScissorRects(1,&clip);
    capture.output(left.Get());capture.beginFrame();
    for(UINT i=0;i<2;++i){bind(slices[i].Get());
        {amalur::UiCapture::Binding layer(capture,context.Get(),true);require(bool(layer),"single eye of stereo array accepted");context->Draw(3,0);}
        ComPtr<ID3D11RenderTargetView> native;context->OMGetRenderTargets(1,&native,nullptr);require(native==slices[i],"original stereo slice restored");
    }
    require(pixel(capture.texture(),32,24)[3]==128,"array eyes must not double alpha");
    D3D11_TEXTURE2D_DESC capturedDesc{};capture.texture()->GetDesc(&capturedDesc);require(capturedDesc.ArraySize==1,"published wrist image is mono");
    D3D11_RENDER_TARGET_VIEW_DESC both{};both.Format=stereoDesc.Format;both.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2DARRAY;both.Texture2DArray.ArraySize=2;
    ComPtr<ID3D11RenderTargetView> layered;hr(device->CreateRenderTargetView(stereoArray.Get(),&both,&layered));bind(layered.Get());
    auto gsCode=compile(R"(
struct O{float4 p:SV_POSITION;uint eye:SV_RenderTargetArrayIndex;float4 c:COLOR;};
[maxvertexcount(6)] void gs(triangle float4 p[3]:SV_POSITION,inout TriangleStream<O> stream){
 for(uint eye=0;eye<2;++eye){for(uint i=0;i<3;++i){O o;o.p=p[i];o.eye=eye;o.c=eye?float4(0,0,1,.5):float4(1,0,0,.5);stream.Append(o);}stream.RestartStrip();}
})","gs","gs_5_0");
    auto eyePsCode=compile("float4 ps(float4 p:SV_POSITION,uint eye:SV_RenderTargetArrayIndex,float4 c:COLOR):SV_TARGET{return c;}","ps","ps_5_0");
    ComPtr<ID3D11GeometryShader> gs;ComPtr<ID3D11PixelShader> eyePs;
    hr(device->CreateGeometryShader(gsCode->GetBufferPointer(),gsCode->GetBufferSize(),nullptr,&gs));
    hr(device->CreatePixelShader(eyePsCode->GetBufferPointer(),eyePsCode->GetBufferSize(),nullptr,&eyePs));
    D3D11_TEXTURE2D_DESC depthDesc=stereoDesc;depthDesc.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;depthDesc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> depthArray;ComPtr<ID3D11DepthStencilView> layeredDepth;hr(device->CreateTexture2D(&depthDesc,nullptr,&depthArray));
    D3D11_DEPTH_STENCIL_VIEW_DESC dv{};dv.Format=depthDesc.Format;dv.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2DARRAY;dv.Texture2DArray.ArraySize=2;
    hr(device->CreateDepthStencilView(depthArray.Get(),&dv,&layeredDepth));
    context->ClearDepthStencilView(layeredDepth.Get(),D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,9);
    auto nativeLayered=layered.Get();context->OMSetRenderTargets(1,&nativeLayered,layeredDepth.Get());context->ClearRenderTargetView(nativeLayered,background);
    context->GSSetShader(gs.Get(),nullptr,0);context->PSSetShader(eyePs.Get(),nullptr,0);capture.beginFrame();
    {amalur::UiCapture::Binding layer(capture,context.Get(),true);require(bool(layer),"two-eye layered target accepted");context->Draw(3,0);}
    auto mono=capture.texture(context.Get());require(mono!=nullptr,"layered capture produces mono transport texture");
    mono->GetDesc(&capturedDesc);require(capturedDesc.ArraySize==1,"layered publication is one slice");
    auto gotMono=pixel(mono,32,24);printf("layered pixel=%d,%d,%d,%d\n",gotMono[0],gotMono[1],gotMono[2],gotMono[3]);
    require(pixel(mono,32,24)==std::array<int,4>{128,0,0,128},"first eye retained without second-eye blue or double alpha");
    require(pixel(stereoArray.Get(),32,24)==std::array<int,4>{0,255,0,255},"layered capture leaves native world untouched");
    ComPtr<ID3D11RenderTargetView> nativeAfter;ComPtr<ID3D11DepthStencilView> depthAfter;
    context->OMGetRenderTargets(1,&nativeAfter,&depthAfter);require(nativeAfter==layered&&depthAfter==layeredDepth,"both-eye RTV and depth array restored");
    context->GSSetShader(nullptr,nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);bind(leftRt.Get());capture.beginFrame();
    {amalur::UiCapture::Binding layer(capture,context.Get(),true);require(bool(layer),"switch back from layered to ordinary target");context->Draw(3,0);}
    require(pixel(capture.texture(context.Get()),32,24)==std::array<int,4>{128,0,0,128},"ordinary capture after layered frame remains correct");
    puts("PASS: layered geometry shader writes both eyes; mono publication/alpha/depth restore; UI isolation, both eyes, premultiplied coverage, native scissor/full height, state restoration, clear/close/resize, shader exclusions");return 0;
}catch(const std::exception& e){printf("FAIL: %s\n",e.what());return 1;}}
