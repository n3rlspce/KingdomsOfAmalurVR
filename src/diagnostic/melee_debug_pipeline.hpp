#pragma once
// Keep geo-11's wrapper bookkeeping in sync: never SwapDeviceContextState.
// Every state mutation goes through the same wrapped D3D11 interface as the game.
namespace melee_debug {
struct Pipeline {
    ID3D11DeviceContext* c;
    ComPtr<ID3D11InputLayout> layout;ComPtr<ID3D11Buffer> vb,cb;
    UINT stride{},offset{};D3D11_PRIMITIVE_TOPOLOGY topology{};
    ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;ComPtr<ID3D11GeometryShader> gs;
    ComPtr<ID3D11HullShader> hs;ComPtr<ID3D11DomainShader> ds;
    ID3D11ClassInstance* vi[256]{},*pi[256]{},*gi[256]{},*hi[256]{},*di[256]{};
    UINT vn=256,pn=256,gn=256,hn=256,dn=256;
    ID3D11RenderTargetView* targets[8]{};ComPtr<ID3D11DepthStencilView> dsv;
    ComPtr<ID3D11BlendState> blend;FLOAT factors[4]{};UINT mask{},ref{};
    ComPtr<ID3D11DepthStencilState> depth;ComPtr<ID3D11RasterizerState> raster;
    D3D11_VIEWPORT viewports[16]{};UINT viewportCount=16;
    ComPtr<ID3D11Predicate> predicate;BOOL predicateValue{};
    explicit Pipeline(ID3D11DeviceContext* context):c(context){
        c->IAGetInputLayout(&layout);c->IAGetPrimitiveTopology(&topology);c->IAGetVertexBuffers(0,1,&vb,&stride,&offset);
        c->VSGetConstantBuffers(0,1,&cb);
        c->VSGetShader(&vs,vi,&vn);c->PSGetShader(&ps,pi,&pn);c->GSGetShader(&gs,gi,&gn);c->HSGetShader(&hs,hi,&hn);c->DSGetShader(&ds,di,&dn);
        c->OMGetRenderTargets(8,targets,&dsv);c->OMGetBlendState(&blend,factors,&mask);c->OMGetDepthStencilState(&depth,&ref);
        c->RSGetState(&raster);c->RSGetViewports(&viewportCount,viewports);c->GetPredication(&predicate,&predicateValue);
    }
    ~Pipeline(){
        c->IASetInputLayout(layout.Get());c->IASetPrimitiveTopology(topology);auto v=vb.Get();c->IASetVertexBuffers(0,1,&v,&stride,&offset);
        auto b=cb.Get();c->VSSetConstantBuffers(0,1,&b);
        c->VSSetShader(vs.Get(),vi,vn);c->PSSetShader(ps.Get(),pi,pn);c->GSSetShader(gs.Get(),gi,gn);c->HSSetShader(hs.Get(),hi,hn);c->DSSetShader(ds.Get(),di,dn);
        c->OMSetRenderTargets(8,targets,dsv.Get());c->OMSetBlendState(blend.Get(),factors,mask);c->OMSetDepthStencilState(depth.Get(),ref);
        c->RSSetState(raster.Get());c->RSSetViewports(viewportCount,viewports);c->SetPredication(predicate.Get(),predicateValue);
        for(auto* p:targets)if(p)p->Release();
        for(auto* p:vi)if(p)p->Release();for(auto* p:pi)if(p)p->Release();for(auto* p:gi)if(p)p->Release();for(auto* p:hi)if(p)p->Release();for(auto* p:di)if(p)p->Release();
    }
};
inline bool simpleOutput(ID3D11DeviceContext* c){
    // Avoid disturbing stream output or pixel UAV counters. The game's normal
    // final-color pass has neither; skip unfamiliar output configurations.
    ID3D11UnorderedAccessView* uavs[8]{};c->OMGetRenderTargetsAndUnorderedAccessViews(0,nullptr,nullptr,0,8,uavs);
    ID3D11Buffer* streams[4]{};c->SOGetTargets(4,streams);bool simple=true;
    for(auto* p:uavs)if(p){simple=false;p->Release();}for(auto* p:streams)if(p){simple=false;p->Release();}return simple;
}
}
