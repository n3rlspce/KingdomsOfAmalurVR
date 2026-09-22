#pragma once
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <wrl/client.h>
#include <cstring>
namespace amalur {
// Exact native UI layouts corresponding to the four existing HUD replacements.
// Broad "small shader" or cbuffer-name candidates are diagnostic only.
inline bool nativeUiShader(const void* code,SIZE_T length){
    Microsoft::WRL::ComPtr<ID3D11ShaderReflection> r;
    if(FAILED(D3DReflect(code,length,IID_PPV_ARGS(&r))))return false;
    D3D11_SHADER_DESC d{};r->GetDesc(&d);
    auto input=[&](UINT i,const char* name,UINT index,BYTE mask){D3D11_SIGNATURE_PARAMETER_DESC p{};return SUCCEEDED(r->GetInputParameterDesc(i,&p))&&!strcmp(p.SemanticName,name)&&p.SemanticIndex==index&&p.Mask==mask&&p.ComponentType==D3D_REGISTER_COMPONENT_FLOAT32;};
    if(d.ConstantBuffers==0&&d.BoundResources==0&&input(0,"POSITION",0,3)&&input(1,"COLOR",0,15))
        return (d.InputParameters==2&&d.OutputParameters==2)||(d.InputParameters==3&&d.OutputParameters==3&&input(2,"TEXCOORD",0,3));
    if(d.ConstantBuffers!=1||d.BoundResources!=1)return false;
    D3D11_SHADER_BUFFER_DESC b{};r->GetConstantBufferByIndex(0)->GetDesc(&b);
    D3D11_SHADER_INPUT_BIND_DESC binding{};r->GetResourceBindingDesc(0,&binding);
    if(binding.Type!=D3D_SIT_CBUFFER||binding.BindPoint!=0)return false;
    if(b.Size==64&&!strcmp(b.Name,"g_instanceDatabuffer"))
        return d.InputParameters==4&&d.OutputParameters==4&&input(0,"POSITION",0,7)&&input(1,"COLOR",0,15)&&input(2,"TEXCOORD",0,3)&&input(3,"TEXCOORD",1,1);
    return b.Size==96&&!strcmp(b.Name,"g_constantDatabuffer")&&d.InputParameters==1&&d.OutputParameters==3&&input(0,"POSITION",0,7);
}
}
