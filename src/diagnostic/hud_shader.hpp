#pragma once
#include <d3d11shader.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <string_view>

namespace hud_shader {
// geo-11 reassembles converted shaders. Their resource declarations survive,
// but RDEF reflection can be absent; reflection alone must not disable sizing.
inline bool usesControl(const void* bytes,SIZE_T size){
    Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflection;
    if(SUCCEEDED(D3DReflect(bytes,size,IID_PPV_ARGS(&reflection)))){
        D3D11_SHADER_DESC desc{};reflection->GetDesc(&desc);
        for(UINT i=0;i<desc.BoundResources;++i){
            D3D11_SHADER_INPUT_BIND_DESC resource{};
            if(SUCCEEDED(reflection->GetResourceBindingDesc(i,&resource))&&
               resource.Type==D3D_SIT_TEXTURE&&resource.BindPoint==119)return true;
        }
        if(desc.BoundResources)return false;
    }
    Microsoft::WRL::ComPtr<ID3DBlob> assembly;
    if(FAILED(D3DDisassemble(bytes,size,0,nullptr,&assembly)))return false;
    const std::string_view text(static_cast<const char*>(assembly->GetBufferPointer()),assembly->GetBufferSize());
    size_t start=0;
    while((start=text.find("dcl_resource_texture1d ",start))!=std::string_view::npos){
        const auto end=text.find('\n',start);
        const auto line=text.substr(start,end==std::string_view::npos?text.size()-start:end-start);
        const auto slot=line.find(" t119");
        if(slot!=std::string_view::npos){
            const auto after=slot+5;
            if(after==line.size()||line[after]=='\r'||line[after]==' '||line[after]=='\t')return true;
        }
        start+=22;
    }
    return false;
}
}
