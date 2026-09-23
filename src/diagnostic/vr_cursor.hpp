#pragma once
// geo11 draws its software cursor inside Present. Supply a dedicated flag only
// for that call, then restore the caller's resource slot. Desktop stays native.
namespace vr_cursor {
class Binding {
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11ShaderResourceView> previous;
    inline static ComPtr<ID3D11Device> owner;
    inline static ComPtr<ID3D11ShaderResourceView> views[2];
public:
    Binding(ID3D11DeviceContext* input,bool hidden){
        ComPtr<ID3D11Device> device;
        if(!input)return;
        input->GetDevice(&device);
        if(owner.Get()!=device.Get()){views[0].Reset();views[1].Reset();owner=device;}
        auto& view=views[hidden?1:0];
        if(!view){
        const float value[4]{hidden?1.f:0.f,0,0,0};
        D3D11_TEXTURE1D_DESC desc{};desc.Width=1;desc.MipLevels=1;desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{value,sizeof(value),0};
        ComPtr<ID3D11Texture1D> texture;
        if(FAILED(device->CreateTexture1D(&desc,&data,&texture))||FAILED(device->CreateShaderResourceView(texture.Get(),nullptr,&view)))return;
        }
        device->GetImmediateContext(&context);context->VSGetShaderResources(116,1,&previous);
        auto resource=view.Get();context->VSSetShaderResources(116,1,&resource);
    }
    ~Binding(){if(context){auto resource=previous.Get();context->VSSetShaderResources(116,1,&resource);}}
};
}
