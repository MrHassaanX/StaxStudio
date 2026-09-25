#pragma once

#ifdef Q_OS_WIN
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <QImage>
#include <QSize>
#include <array>
#include <stdexcept>

// All methods run on the output worker. No window, swapchain or Qt scene
// graph owns these resources. Readback slots never wait for the GPU.
class D3DProgramSurface final {
public:
    template<class T> using Com = Microsoft::WRL::ComPtr<T>;
    struct Readback { Com<ID3D11Texture2D> texture; qint64 timestamp = -1; };
    Com<ID3D11Device> device;
    Com<ID3D11DeviceContext> context;
    Com<ID3D11Texture2D> target, shared;
    Com<IDXGIKeyedMutex> keyed;
    HANDLE sharedHandle = nullptr;
    QSize size;
    std::array<Readback, 3> readbacks;

    static void check(HRESULT result, const char *operation) {
        if (FAILED(result)) throw std::runtime_error(QStringLiteral("%1 (HRESULT 0x%2)")
            .arg(QString::fromLatin1(operation)).arg(quint32(result), 8, 16, QLatin1Char('0')).toStdString());
    }
    D3DProgramSurface() {
        check(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                               nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context), "Create program D3D11 device");
        constexpr char code[] =
            "struct V { float2 p:POSITION; float4 c:COLOR; float2 uv:TEXCOORD; };"
            "struct P { float4 p:SV_POSITION; float4 c:COLOR; float2 uv:TEXCOORD; };"
            "P vs(V v) { P o; o.p=float4(v.p,0,1); o.c=v.c; o.uv=v.uv; return o; }"
            "Texture2D tex:register(t0); SamplerState smp:register(s0);"
            "float4 ps(P p):SV_TARGET { return tex.Sample(smp,p.uv)*p.c; }";
        Com<ID3DBlob> vs, ps, errors;
        check(D3DCompile(code, sizeof(code)-1, nullptr, nullptr, nullptr, "vs", "vs_5_0", 0, 0, &vs, &errors), "Compile vertex shader");
        check(D3DCompile(code, sizeof(code)-1, nullptr, nullptr, nullptr, "ps", "ps_5_0", 0, 0, &ps, &errors), "Compile pixel shader");
        check(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &vertexShader), "Create vertex shader");
        check(device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &pixelShader), "Create pixel shader");
        const D3D11_INPUT_ELEMENT_DESC elements[] = {
            {"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,8,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}};
        check(device->CreateInputLayout(elements, 3, vs->GetBufferPointer(), vs->GetBufferSize(), &input), "Create input layout");
        D3D11_BUFFER_DESC buffer{}; buffer.ByteWidth=6*32; buffer.Usage=D3D11_USAGE_DYNAMIC;
        buffer.BindFlags=D3D11_BIND_VERTEX_BUFFER; buffer.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        check(device->CreateBuffer(&buffer, nullptr, &vertices), "Create vertex buffer");
        D3D11_SAMPLER_DESC samplerDesc{}; samplerDesc.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU=samplerDesc.AddressV=samplerDesc.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.MaxLOD=D3D11_FLOAT32_MAX;
        check(device->CreateSamplerState(&samplerDesc, &sampler), "Create sampler");
        D3D11_RASTERIZER_DESC rasterDesc{}; rasterDesc.FillMode=D3D11_FILL_SOLID; rasterDesc.CullMode=D3D11_CULL_NONE;
        rasterDesc.DepthClipEnable=TRUE;
        check(device->CreateRasterizerState(&rasterDesc, &raster), "Create rasterizer");
        D3D11_BLEND_DESC blendDesc{};
        auto &b=blendDesc.RenderTarget[0]; b.BlendEnable=TRUE; b.SrcBlend=D3D11_BLEND_SRC_ALPHA;
        b.DestBlend=D3D11_BLEND_INV_SRC_ALPHA; b.BlendOp=b.BlendOpAlpha=D3D11_BLEND_OP_ADD;
        b.SrcBlendAlpha=D3D11_BLEND_ONE; b.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA; b.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
        check(device->CreateBlendState(&blendDesc, &blend), "Create blending state");
        QImage white(1,1,QImage::Format_ARGB32); white.fill(Qt::white);
        whiteTexture = upload(white);
    }
    Com<ID3D11ShaderResourceView> upload(const QImage &image) {
        QImage pixels=image.convertToFormat(QImage::Format_ARGB32);
        D3D11_TEXTURE2D_DESC d{}; d.Width=pixels.width(); d.Height=pixels.height();
        d.MipLevels=d.ArraySize=d.SampleDesc.Count=1; d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
        d.Usage=D3D11_USAGE_IMMUTABLE; d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{pixels.constBits(), UINT(pixels.bytesPerLine()),0};
        Com<ID3D11Texture2D> texture; check(device->CreateTexture2D(&d,&data,&texture),"Upload source image");
        return view(texture.Get());
    }
    Com<ID3D11ShaderResourceView> view(ID3D11Texture2D *texture) {
        Com<ID3D11ShaderResourceView> result;
        check(device->CreateShaderResourceView(texture,nullptr,&result),"Create source texture view"); return result;
    }
    void resize(QSize value) {
        context->ClearState();
        target.Reset(); shared.Reset(); keyed.Reset(); output.Reset();
        for(auto &slot:readbacks) slot={};
        size=value;
        D3D11_TEXTURE2D_DESC d{}; d.Width=size.width(); d.Height=size.height();
        d.MipLevels=d.ArraySize=d.SampleDesc.Count=1; d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
        d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        check(device->CreateTexture2D(&d,nullptr,&target),"Create program target");
        check(device->CreateRenderTargetView(target.Get(),nullptr,&output),"Create program render view");
        d.BindFlags=D3D11_BIND_SHADER_RESOURCE; d.MiscFlags=D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
        check(device->CreateTexture2D(&d,nullptr,&shared),"Create preview bridge");
        check(shared.As(&keyed),"Create preview mutex");
        Com<IDXGIResource> resource; check(shared.As(&resource),"Get shared resource");
        check(resource->GetSharedHandle(&sharedHandle),"Get preview handle");
        d.BindFlags=0; d.MiscFlags=0; d.Usage=D3D11_USAGE_STAGING; d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        for(auto &slot:readbacks) check(device->CreateTexture2D(&d,nullptr,&slot.texture),"Create staging texture");
    }
    void begin() {
        const float black[4]={0,0,0,1}; context->ClearRenderTargetView(output.Get(),black);
        context->OMSetRenderTargets(1,output.GetAddressOf(),nullptr);
        context->OMSetBlendState(blend.Get(),nullptr,0xffffffff);
        context->RSSetState(raster.Get());
        D3D11_VIEWPORT viewport{0,0,float(size.width()),float(size.height()),0,1}; context->RSSetViewports(1,&viewport);
        context->IASetInputLayout(input.Get()); context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        const UINT stride=32, offset=0; context->IASetVertexBuffers(0,1,vertices.GetAddressOf(),&stride,&offset);
        context->VSSetShader(vertexShader.Get(),nullptr,0); context->PSSetShader(pixelShader.Get(),nullptr,0);
        context->PSSetSamplers(0,1,sampler.GetAddressOf());
    }
    void draw(const void *data, ID3D11ShaderResourceView *texture=nullptr) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        check(context->Map(vertices.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped),"Map vertices");
        memcpy(mapped.pData,data,6*32); context->Unmap(vertices.Get(),0);
        auto *srv=texture ? texture : whiteTexture.Get(); context->PSSetShaderResources(0,1,&srv); context->Draw(6,0);
    }
    bool enqueue(qint64 timestamp) {
        for(auto &slot:readbacks) if(slot.timestamp<0) {
            context->CopyResource(slot.texture.Get(),target.Get()); slot.timestamp=timestamp; return true;
        }
        return false;
    }
    template<class Consumer> void drain(Consumer consume) {
        // Process oldest first so a late GPU completion cannot reorder PTS.
        for(size_t n=0;n<readbacks.size();++n) {
            Readback *oldest=nullptr;
            for(auto &slot:readbacks) if(slot.timestamp>=0 && (!oldest || slot.timestamp<oldest->timestamp)) oldest=&slot;
            if(!oldest) break;
            D3D11_MAPPED_SUBRESOURCE mapped{};
            HRESULT hr=context->Map(oldest->texture.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped);
            if(hr==DXGI_ERROR_WAS_STILL_DRAWING) break;
            check(hr,"Read program texture");
            QImage image(size,QImage::Format_ARGB32);
            for(int y=0;y<size.height();++y) memcpy(image.scanLine(y),static_cast<const char *>(mapped.pData)+y*mapped.RowPitch,size.width()*4);
            context->Unmap(oldest->texture.Get(),0);
            consume(std::move(image),oldest->timestamp); oldest->timestamp=-1;
        }
    }
private:
    Com<ID3D11RenderTargetView> output;
    Com<ID3D11VertexShader> vertexShader;
    Com<ID3D11PixelShader> pixelShader;
    Com<ID3D11InputLayout> input;
    Com<ID3D11Buffer> vertices;
    Com<ID3D11SamplerState> sampler;
    Com<ID3D11RasterizerState> raster;
    Com<ID3D11BlendState> blend;
    Com<ID3D11ShaderResourceView> whiteTexture;
};
#endif
