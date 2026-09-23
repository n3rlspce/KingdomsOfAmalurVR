#include <initializer_list>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdio>
using Microsoft::WRL::ComPtr;
#include "../diagnostic/vr_cursor.hpp"
int main(){
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
 if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)))return 1;
 ComPtr<ID3DBlob> vsBlob,psBlob,errors;
 D3D_SHADER_MACRO macros[]{{"VERTEX_SHADER","1"},{nullptr,nullptr}};
 if(FAILED(D3DCompileFromFile(L"ShaderFixes/mouse.hlsl",macros,nullptr,"main","vs_5_0",0,0,&vsBlob,&errors)))return 2;
 const char ps[]="float4 main():SV_TARGET{return float4(1,1,1,1);}";
 if(FAILED(D3DCompile(ps,sizeof(ps),nullptr,nullptr,nullptr,"main","ps_5_0",0,0,&psBlob,&errors)))return 3;
 ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> pixel;
 device->CreateVertexShader(vsBlob->GetBufferPointer(),vsBlob->GetBufferSize(),nullptr,&vs);
 device->CreatePixelShader(psBlob->GetBufferPointer(),psBlob->GetBufferSize(),nullptr,&pixel);
 D3D11_TEXTURE2D_DESC desc{};desc.Width=64;desc.Height=64;desc.MipLevels=1;desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
 ComPtr<ID3D11Texture2D> target,read;ComPtr<ID3D11RenderTargetView> rtv;
 device->CreateTexture2D(&desc,nullptr,&target);device->CreateRenderTargetView(target.Get(),nullptr,&rtv);
 desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;device->CreateTexture2D(&desc,nullptr,&read);
 auto output=rtv.Get();context->OMSetRenderTargets(1,&output,nullptr);
 D3D11_VIEWPORT viewport{0,0,64,64,0,1};context->RSSetViewports(1,&viewport);
 D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;
 ComPtr<ID3D11RasterizerState> rs;device->CreateRasterizerState(&rd,&rs);context->RSSetState(rs.Get());
 float params[32]{};params[5*4+3]=1;params[6*4]=16;params[6*4+1]=16;params[7*4+1]=1;params[7*4+2]=64;params[7*4+3]=64;
 D3D11_TEXTURE1D_DESC pd{};pd.Width=8;pd.MipLevels=1;pd.ArraySize=1;pd.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;pd.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 D3D11_SUBRESOURCE_DATA init{params,sizeof(params),0};ComPtr<ID3D11Texture1D> ptex;ComPtr<ID3D11ShaderResourceView> pview;
 device->CreateTexture1D(&pd,&init,&ptex);device->CreateShaderResourceView(ptex.Get(),nullptr,&pview);auto raw=pview.Get();context->VSSetShaderResources(120,1,&raw);
 desc.Width=8;desc.Height=8;desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=0;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 ComPtr<ID3D11Texture2D> color;ComPtr<ID3D11ShaderResourceView> colorView;
 device->CreateTexture2D(&desc,nullptr,&color);device->CreateShaderResourceView(color.Get(),nullptr,&colorView);raw=colorView.Get();context->VSSetShaderResources(101,1,&raw);
 context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(pixel.Get(),nullptr,0);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
 // Sentinel resource must survive both visible and hidden Present bindings.
 raw=pview.Get();context->VSSetShaderResources(116,1,&raw);
 for(bool hide:{false,true,false}){
  float clear[4]{};context->ClearRenderTargetView(rtv.Get(),clear);
  {vr_cursor::Binding binding(context.Get(),hide);context->Draw(4,0);}
  ComPtr<ID3D11ShaderResourceView> restored;context->VSGetShaderResources(116,1,&restored);if(restored.Get()!=pview.Get())return 4;
  context->CopyResource(read.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
  if(FAILED(context->Map(read.Get(),0,D3D11_MAP_READ,0,&mapped)))return 5;
  unsigned lit=0;for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x)lit+=static_cast<unsigned char*>(mapped.pData)[y*mapped.RowPitch+x*4]!=0;
  context->Unmap(read.Get(),0);if(hide?lit!=0:lit==0)return 6;
 }
 puts("PASS: actual software cursor shader visible on desktop, hidden in VR, restored on exit; resource state preserved");return 0;
}
