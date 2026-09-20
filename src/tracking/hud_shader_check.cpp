#define NOMINMAX
#include "../diagnostic/hud_shader.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
using Microsoft::WRL::ComPtr;
static void check(bool ok,const char* msg){if(!ok){std::fprintf(stderr,"FAIL: %s\n",msg);std::exit(1);}}
int main(int argc,char** argv){
    const char* shader="Texture1D<float4> settings:register(t119);float4 main(float4 p:POSITION):SV_POSITION{return p*settings.Load(int2(0,0)).x;}";
    ComPtr<ID3DBlob> compiled,stripped,error;
    check(SUCCEEDED(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,"main","vs_4_0",0,0,&compiled,&error)),"compile HUD shader");
    check(hud_shader::usesControl(compiled->GetBufferPointer(),compiled->GetBufferSize()),"reflected resource found");
    check(SUCCEEDED(D3DStripShader(compiled->GetBufferPointer(),compiled->GetBufferSize(),D3DCOMPILER_STRIP_REFLECTION_DATA,&stripped)),"strip reflection");
    check(hud_shader::usesControl(stripped->GetBufferPointer(),stripped->GetBufferSize()),"stripped resource declaration still found");
    const char* ordinary="float4 main(float4 p:POSITION):SV_POSITION{return p;}";
    compiled.Reset();error.Reset();
    check(SUCCEEDED(D3DCompile(ordinary,strlen(ordinary),nullptr,nullptr,nullptr,"main","vs_4_0",0,0,&compiled,&error)),"compile ordinary shader");
    check(!hud_shader::usesControl(compiled->GetBufferPointer(),compiled->GetBufferSize()),"ordinary shader rejected");
    check(!hud_shader::usesControl("bad",3),"invalid bytecode rejected");
    for(int n=1;n<argc;++n){
        std::ifstream file(argv[n],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
        check(!bytes.empty(),"fixture exists");
        ComPtr<ID3D11ShaderReflection> reflection;D3D11_SHADER_DESC desc{};
        const auto result=D3DReflect(bytes.data(),bytes.size(),IID_PPV_ARGS(&reflection));
        if(SUCCEEDED(result))reflection->GetDesc(&desc);
        check(hud_shader::usesControl(bytes.data(),bytes.size()),"installed converted HUD shader recognized");
        std::printf("Converted fixture: reflection=%08lx resources=%u control=1\n",static_cast<unsigned long>(result),desc.BoundResources);
    }
    std::puts("PASS: reflected/stripped/converted HUD recognition and ordinary shader rejection");
}
