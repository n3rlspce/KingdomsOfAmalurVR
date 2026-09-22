#pragma once
#include <d3d11.h>
#include <wrl/client.h>
namespace amalur {
// Native NDC: both top corners occupy 30% of each canvas dimension.
// Two disjoint remaining rectangles keep every other native HUD pixel.
class WristClip {
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
    Ptr<ID3D11Texture1D> texture_;
    Ptr<ID3D11ShaderResourceView> view_;
public:
    bool initialize(ID3D11DeviceContext* c){
        if(view_)return true;
        Ptr<ID3D11Device> device;c->GetDevice(&device);
        D3D11_TEXTURE1D_DESC d{};d.Width=2;d.MipLevels=d.ArraySize=1;
        d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        return SUCCEEDED(device->CreateTexture1D(&d,nullptr,&texture_))&&SUCCEEDED(device->CreateShaderResourceView(texture_.Get(),nullptr,&view_));
    }
    class Binding {
        ID3D11DeviceContext* c_;Ptr<ID3D11ShaderResourceView> previous_;
    public:
        Binding(WristClip& owner,ID3D11DeviceContext* c,int region):c_(c){
            // -1 disabled; 0 bottom; 1 central top.
            float data[8]{-1,1,-1,.4f,1,0,0,0};
            if(region==1){data[0]=-.4f;data[1]=.4f;data[2]=.4f;data[3]=1;}
            if(region<0)data[4]=0;
            c->UpdateSubresource(owner.texture_.Get(),0,nullptr,data,sizeof(data),0);
            c->VSGetShaderResources(117,1,&previous_);auto v=owner.view_.Get();c->VSSetShaderResources(117,1,&v);
        }
        ~Binding(){auto v=previous_.Get();c_->VSSetShaderResources(117,1,&v);}
    };
};
}
