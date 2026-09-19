#pragma once
// Read-only trace of HUD candidates actually bound for draws, including the
// GPU constant texture. Shader files existing on disk is not proof of use.
namespace hud_trace {
static const GUID tagId={0x9e1d42a1,0x6cb3,0x4ac2,{0x87,0x1b,0x21,0x76,0x98,0x5e,0x9c,0x30}};
struct Tag {uint64_t hash{};UINT candidate{};};
static unsigned budget=24;
static uint64_t seen[24]{};static unsigned seenCount{};
static void rearm(){budget=24;seenCount=0;}
static void tag(ID3D11VertexShader* shader,const void* bytes,SIZE_T size){
    Tag t;t.hash=14695981039346656037ull;
    for(SIZE_T i=0;i<size;++i){t.hash*=1099511628211ull;t.hash^=static_cast<const unsigned char*>(bytes)[i];}
    ComPtr<ID3D11ShaderReflection> reflection;
    if(SUCCEEDED(D3DReflect(bytes,size,IID_PPV_ARGS(&reflection)))){
        D3D11_SHADER_DESC d{};reflection->GetDesc(&d);
        if(d.ConstantBuffers==0&&d.InputParameters<=4)t.candidate=1;
        for(UINT i=0;i<d.ConstantBuffers;++i){D3D11_SHADER_BUFFER_DESC b{};
            if(SUCCEEDED(reflection->GetConstantBufferByIndex(i)->GetDesc(&b))&&
                (strcmp(b.Name,"g_instanceDatabuffer")==0||strcmp(b.Name,"g_constantDatabuffer")==0))t.candidate=1;
        }
    }
    shader->SetPrivateData(tagId,sizeof(t),&t);
    if(t.candidate)log("HUD candidate created hash=%016llx bytes=%zu\n",t.hash,size);
}
static void sample(ID3D11DeviceContext* context){
    if(!budget)return;
    ComPtr<ID3D11VertexShader> shader;context->VSGetShader(&shader,nullptr,nullptr);if(!shader)return;
    Tag t;UINT size=sizeof(t);if(FAILED(shader->GetPrivateData(tagId,&size,&t))||!t.candidate)return;
    for(unsigned i=0;i<seenCount;++i)if(seen[i]==t.hash)return;
    seen[seenCount++]=t.hash;--budget;
    ComPtr<ID3D11ShaderResourceView> view;context->VSGetShaderResources(120,1,&view);
    if(!view){log("HUD DRAW hash=%016llx IniParams=t120 UNBOUND\n",t.hash);return;}
    ComPtr<ID3D11Resource> resource;view->GetResource(&resource);ComPtr<ID3D11Texture1D> texture;
    if(FAILED(resource.As(&texture))){log("HUD DRAW hash=%016llx t120 is not Texture1D\n",t.hash);return;}
    D3D11_TEXTURE1D_DESC desc{};texture->GetDesc(&desc);
    log("HUD DRAW hash=%016llx t120 width=%u format=%u\n",t.hash,desc.Width,desc.Format);
    if(desc.Width<4||desc.Format!=DXGI_FORMAT_R32G32B32A32_FLOAT)return;
    desc.Width=4;desc.ArraySize=1;desc.MipLevels=1;desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.MiscFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Device> device;context->GetDevice(&device);ComPtr<ID3D11Texture1D> staging;
    if(FAILED(device->CreateTexture1D(&desc,nullptr,&staging)))return;
    D3D11_BOX region{0,0,0,4,1,1};context->CopySubresourceRegion(staging.Get(),0,0,0,0,texture.Get(),0,&region);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(SUCCEEDED(realMap(context,staging.Get(),0,D3D11_MAP_READ,0,&mapped))){
        auto values=static_cast<const float*>(mapped.pData);
        log("HUD GPU CONSTANTS hash=%016llx row0=%g,%g,%g,%g row2=%g,%g,%g,%g\n",t.hash,values[0],values[1],values[2],values[3],values[8],values[9],values[10],values[11]);
        log("HUD GPU GATE hash=%016llx enabled=%g frame=%lu\n",t.hash,values[12],presents.load());
        realUnmap(context,staging.Get(),0);
    }
}
using Draw=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT);
using Indexed=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,INT);
using Instanced=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT,UINT);
using IndexedInstanced=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT,INT,UINT);
static Draw draw;static Indexed indexed;static Instanced instanced;static IndexedInstanced indexedInstanced;
static void STDMETHODCALLTYPE onDraw(ID3D11DeviceContext* c,UINT n,UINT first){sample(c);draw(c,n,first);}
static void STDMETHODCALLTYPE onIndexed(ID3D11DeviceContext* c,UINT n,UINT first,INT base){sample(c);indexed(c,n,first,base);}
static void STDMETHODCALLTYPE onInstanced(ID3D11DeviceContext* c,UINT n,UINT instances,UINT first,UINT start){sample(c);instanced(c,n,instances,first,start);}
static void STDMETHODCALLTYPE onIndexedInstanced(ID3D11DeviceContext* c,UINT n,UINT instances,UINT first,INT base,UINT start){sample(c);indexedInstanced(c,n,instances,first,base,start);}
static void install(void** vt){
    hook(vt[13],reinterpret_cast<void*>(onDraw),reinterpret_cast<void**>(&draw),"HUD Draw trace");
    hook(vt[12],reinterpret_cast<void*>(onIndexed),reinterpret_cast<void**>(&indexed),"HUD DrawIndexed trace");
    hook(vt[21],reinterpret_cast<void*>(onInstanced),reinterpret_cast<void**>(&instanced),"HUD DrawInstanced trace");
    hook(vt[20],reinterpret_cast<void*>(onIndexedInstanced),reinterpret_cast<void**>(&indexedInstanced),"HUD DrawIndexedInstanced trace");
}
}
