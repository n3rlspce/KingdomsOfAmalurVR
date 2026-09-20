#define NOMINMAX
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <array>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <cmath>
using Microsoft::WRL::ComPtr;
static void check(bool ok,const char* msg){if(!ok){std::fprintf(stderr,"FAIL: %s\n",msg);std::exit(1);}}
static void hr(HRESULT result){check(SUCCEEDED(result),"D3D operation");}
int main(int argc,char** argv){
    check(argc==2,"pass generated ShaderFixes directory");
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
    auto buffer=[&](UINT size,UINT flags,const void* data){
        D3D11_BUFFER_DESC d{};d.ByteWidth=size;d.BindFlags=flags;
        D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=data;ComPtr<ID3D11Buffer> b;
        hr(device->CreateBuffer(&d,data?&initial:nullptr,&b));return b;
    };
    float constants[24]{};for(unsigned i=0;i<4;++i)constants[i*5]=1;
    auto cb=buffer(sizeof(constants),D3D11_BIND_CONSTANT_BUFFER,constants);auto cp=cb.Get();context->VSSetConstantBuffers(0,1,&cp);
    float ini[16]{};ini[8]=.67f;ini[9]=.70f;ini[10]=.60f;ini[11]=.25f;
    ComPtr<ID3D11Texture1D> control,params;ComPtr<ID3D11ShaderResourceView> controlView,paramsView;
    auto texture=[&](UINT width,const void* data,ComPtr<ID3D11Texture1D>& tex,ComPtr<ID3D11ShaderResourceView>& view){
        D3D11_TEXTURE1D_DESC d{};d.Width=width;d.MipLevels=1;d.ArraySize=1;d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=data;hr(device->CreateTexture1D(&d,data?&initial:nullptr,&tex));hr(device->CreateShaderResourceView(tex.Get(),nullptr,&view));
    };
    texture(4,nullptr,control,controlView);texture(4,ini,params,paramsView);
    ID3D11ShaderResourceView* views[]{controlView.Get(),paramsView.Get()};context->VSSetShaderResources(119,2,views);
    auto output=buffer(20,D3D11_BIND_STREAM_OUTPUT,nullptr);ComPtr<ID3D11Buffer> staging;
    D3D11_BUFFER_DESC sd{};sd.ByteWidth=20;sd.Usage=D3D11_USAGE_STAGING;sd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;hr(device->CreateBuffer(&sd,nullptr,&staging));
    for(const char* name:{"887f6506d28f9ff1","bf098be2e4587ca5"}){
        const auto file=std::filesystem::path(argv[1])/(std::string(name)+"-vs_replace.txt");
        ComPtr<ID3DBlob> code,error;hr(D3DCompileFromFile(file.c_str(),nullptr,nullptr,"main","vs_4_0",0,0,&code,&error));
        ComPtr<ID3D11VertexShader> vs;hr(device->CreateVertexShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&vs));
        const bool font=std::string(name)=="887f6506d28f9ff1";
        D3D11_SO_DECLARATION_ENTRY declarations[]={{0,"SV_POSITION",0,0,4,0},{0,"SV_ClipDistance",0,0,1,0}};UINT outputStride=font?16:20;
        ComPtr<ID3D11GeometryShader> stream;hr(device->CreateGeometryShaderWithStreamOutput(code->GetBufferPointer(),code->GetBufferSize(),declarations,font?1:2,&outputStride,1,D3D11_SO_NO_RASTERIZED_STREAM,nullptr,&stream));
        D3D11_INPUT_ELEMENT_DESC elements[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,28,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",1,DXGI_FORMAT_R32_FLOAT,0,44,D3D11_INPUT_PER_VERTEX_DATA,0}};
        ComPtr<ID3D11InputLayout> layout;hr(device->CreateInputLayout(elements,font?4:2,code->GetBufferPointer(),code->GetBufferSize(),&layout));
        context->IASetInputLayout(layout.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);context->VSSetShader(vs.Get(),nullptr,0);context->GSSetShader(stream.Get(),nullptr,0);
        auto run=[&](float size,bool dialogue,bool flat,bool black,bool menu=false,float yaw=0){
            float data[12]{.8f,.6f,0,black?0.f:1.f,black?0.f:1.f,black?0.f:1.f,1,0,0,0,0,0};
            auto vertex=buffer(sizeof(data),D3D11_BIND_VERTEX_BUFFER,data);auto vp=vertex.Get();UINT stride=sizeof(data),offset=0;context->IASetVertexBuffers(0,1,&vp,&stride,&offset);
            const float settings[]{size,1,flat?1.f:0.f,dialogue?1.f:(menu?2.f:0.f),std::cos(yaw),0,std::sin(yaw),0,0,1,0,0,-std::sin(yaw),0,std::cos(yaw),0};context->UpdateSubresource(control.Get(),0,nullptr,settings,sizeof(settings),0);
            auto op=output.Get();context->SOSetTargets(1,&op,&offset);context->Draw(1,0);context->SOSetTargets(0,nullptr,nullptr);
            context->CopyResource(staging.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE mapped{};hr(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
            std::array<float,5> result{};memcpy(result.data(),mapped.pData,outputStride);context->Unmap(staging.Get(),0);return result;
        };
        auto reduced=run(.4f,true,false,false),large=run(.8f,true,false,false);
        check(std::abs(large[0]-reduced[0]*2)<1e-5f&&std::abs((large[1]+.18f)-(reduced[1]+.18f)*2)<1e-5f,"actual font/solid shaders scale around the lowered dialogue anchor");
        const float expectedX=std::tan(.8f*.6f)/std::tan(.6f)*.67f*.8f*.85f;
        const float expectedY=.6f*.70f*.8f*.85f*std::cos(.6f)/std::cos(.8f*.6f)-.18f;
        check(std::abs(large[0]-expectedX)<1e-5f&&std::abs(large[1]-expectedY)<1e-5f,"dialogue canvas is compact and lowered consistently");
        const auto ordinary=run(.4f,false,false,true),flat=run(.4f,true,true,true);
        check(std::abs(ordinary[0]-.8f)<1e-5f&&std::abs(flat[0]-.8f)<1e-5f,"non-dialogue and flat-view black UI unchanged");
        const auto menuSmall=run(.4f,false,false,true,true),menuLarge=run(.8f,false,false,true,true);
        check(std::abs(menuLarge[0]-menuSmall[0]*2)<1e-5f&&std::abs(menuLarge[1]-menuSmall[1]*2)<1e-5f,"menu text and solid panels use common size without dialogue offset");
        const auto turned=run(.8f,false,false,false,true,.4f);
        const float denominator=-std::sin(.4f)*menuLarge[0]+std::cos(.4f);
        check(std::abs(turned[0]-(std::cos(.4f)*menuLarge[0]+std::sin(.4f)))<1e-5f&&std::abs(turned[1]-menuLarge[1])<1e-5f&&std::abs(turned[3]-denominator)<1e-5f,"actual menu shaders preserve homogeneous depth for perspective interpolation");
        if(!font)check(menuLarge[4]>0,"menu panels are not removed as dialogue bars");
        const auto black=run(.8f,true,false,true);
        check(std::abs(black[0]-large[0])<1e-5f&&std::abs(black[1]-large[1])<1e-5f,"black vertices retain placement; mixed-colour triangles cannot stretch");
        if(!font){
            check(black[4]<0&&large[4]>0&&ordinary[4]>0&&flat[4]>0,"clip distance hides only dialogue black backdrops");
            // Rasterize both uniform and mixed-colour triangles. A point-only
            // test cannot detect the stretched geometry in the user screenshot.
            D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=64;td.MipLevels=td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.BindFlags=D3D11_BIND_RENDER_TARGET;
            ComPtr<ID3D11Texture2D> target,readback;hr(device->CreateTexture2D(&td,nullptr,&target));
            ComPtr<ID3D11RenderTargetView> rtv;hr(device->CreateRenderTargetView(target.Get(),nullptr,&rtv));
            td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;hr(device->CreateTexture2D(&td,nullptr,&readback));
            const char* psText="float4 main():SV_Target{return float4(1,1,1,1);}";
            ComPtr<ID3DBlob> psCode;hr(D3DCompile(psText,strlen(psText),nullptr,nullptr,nullptr,"main","ps_4_0",0,0,&psCode,nullptr));
            ComPtr<ID3D11PixelShader> ps;hr(device->CreatePixelShader(psCode->GetBufferPointer(),psCode->GetBufferSize(),nullptr,&ps));
            D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;
            ComPtr<ID3D11RasterizerState> raster;hr(device->CreateRasterizerState(&rd,&raster));context->RSSetState(raster.Get());
            D3D11_VIEWPORT viewport{0,0,64,64,0,1};context->RSSetViewports(1,&viewport);
            context->GSSetShader(nullptr,nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);auto rt=rtv.Get();context->OMSetRenderTargets(1,&rt,nullptr);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            auto rasterize=[&](unsigned blackCount){
                float data[3][12]{};const float positions[3][2]{{-.8f,-.6f},{.8f,-.6f},{0,.6f}};
                for(unsigned i=0;i<3;++i){data[i][0]=positions[i][0];data[i][1]=positions[i][1];data[i][6]=1;for(unsigned j=3;j<6;++j)data[i][j]=i<blackCount?0.f:1.f;}
                auto vertex=buffer(sizeof(data),D3D11_BIND_VERTEX_BUFFER,data);auto vp=vertex.Get();UINT stride=sizeof(data[0]),offset=0;context->IASetVertexBuffers(0,1,&vp,&stride,&offset);
                const float clear[4]{};context->ClearRenderTargetView(rtv.Get(),clear);context->Draw(3,0);context->CopyResource(readback.Get(),target.Get());
                D3D11_MAPPED_SUBRESOURCE map{};hr(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&map));std::array<bool,4096> pixels{};
                for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x)pixels[y*64+x]=static_cast<const unsigned char*>(map.pData)[y*map.RowPitch+x*4]!=0;
                context->Unmap(readback.Get(),0);return pixels;
            };
            const auto full=rasterize(0),mixed=rasterize(1),hidden=rasterize(3);
            unsigned fullCount=0,mixedCount=0;
            for(unsigned i=0;i<4096;++i){fullCount+=full[i];mixedCount+=mixed[i];check(!hidden[i],"solid black dialogue triangle produces no pixels");check(!mixed[i]||full[i],"mixed-colour backdrop cannot extend beyond original triangle");}
            check(mixedCount>0&&mixedCount<fullCount,"mixed-colour backdrop clips without hiding coloured remainder");
            // Validate interpolation inside a rotated menu triangle, not only
            // its corners. Fixed W passes corner tests but warps the interior.
            const char* gradientPs="float4 main(float4 position:SV_Position,float4 c:COLOR0):SV_Target{return float4(c.rgb,1);}";
            hr(D3DCompile(gradientPs,strlen(gradientPs),nullptr,nullptr,nullptr,"main","ps_4_0",0,0,&psCode,nullptr));
            hr(device->CreatePixelShader(psCode->GetBufferPointer(),psCode->GetBufferSize(),nullptr,&ps));context->PSSetShader(ps.Get(),nullptr,0);
            float vertices[3][12]{{-.8f,-.6f,0,0,0,.25f,1},{.8f,-.6f,0,1,0,.25f,1},{0,.6f,0,.5f,1,.25f,1}};
            auto gradient=buffer(sizeof(vertices),D3D11_BIND_VERTEX_BUFFER,vertices);auto gp=gradient.Get();UINT stride=sizeof(vertices[0]),offset=0;context->IASetVertexBuffers(0,1,&gp,&stride,&offset);
            const float yaw=.4f,c=std::cos(yaw),s=std::sin(yaw);
            const float settings[]{.8f,1,0,2,c,0,s,0,0,1,0,0,-s,0,c,0};context->UpdateSubresource(control.Get(),0,nullptr,settings,sizeof(settings),0);
            const float clear[4]{};context->ClearRenderTargetView(rtv.Get(),clear);context->Draw(3,0);context->CopyResource(readback.Get(),target.Get());
            D3D11_MAPPED_SUBRESOURCE mapped{};hr(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped));unsigned checked=0;
            for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){
                const auto pixel=static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch+x*4;
                if(!pixel[3])continue;
                const float nx=(x+.5f)/32.f-1,ny=1-(y+.5f)/32.f;
                const float planeX=(nx*c-s)/(c+nx*s),planeY=ny*(c-s*planeX);
                const float red=(planeX/(.67f*.8f)+.8f)/1.6f,green=(planeY/(.70f*.8f)+.6f)/1.2f;
                check(std::abs(pixel[0]/255.f-red)<.01f&&std::abs(pixel[1]/255.f-green)<.01f,"rotated menu interior interpolates as one flat plane");++checked;
            }
            context->Unmap(readback.Get(),0);check(checked>50,"rotated menu raster test covers interior pixels");
            context->OMSetRenderTargets(0,nullptr,nullptr);
            context->GSSetShader(stream.Get(),nullptr,0);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
            const auto neutral=run(.8f,false,false,false,true,.4f);
            D3D11_TEXTURE2D_DESC stereoDesc{};stereoDesc.Width=stereoDesc.Height=stereoDesc.MipLevels=stereoDesc.ArraySize=1;
            stereoDesc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;stereoDesc.SampleDesc.Count=1;stereoDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            ComPtr<ID3D11Texture2D> stereoTexture;ComPtr<ID3D11ShaderResourceView> stereoView;
            hr(device->CreateTexture2D(&stereoDesc,nullptr,&stereoTexture));hr(device->CreateShaderResourceView(stereoTexture.Get(),nullptr,&stereoView));
            auto sv=stereoView.Get();context->VSSetShaderResources(125,1,&sv);
            for(float eye:{-1.f,1.f}){
                const float stereo[]{.02f,100,0,eye};context->UpdateSubresource(stereoTexture.Get(),0,nullptr,stereo,sizeof(stereo),0);
                const auto shifted=run(.8f,false,false,false,true,.4f);
                const float convertedX=shifted[0]+(shifted[3]!=1?(shifted[3]-stereo[1])*stereo[0]*stereo[3]:0);
                check(std::abs(convertedX-neutral[0])<1e-5f,"both eyes cancel verified geo-11 automatic W stereo shift");
            }
            sv=nullptr;context->VSSetShaderResources(125,1,&sv);
        }
    }
    context->ClearState();
    std::puts("PASS: WARP font scaling, black backdrop clipping, mixed-colour raster coverage; menus/flat view unchanged");
}
