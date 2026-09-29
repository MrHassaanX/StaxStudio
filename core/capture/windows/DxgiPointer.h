#pragma once

#include <QImage>
#include <QByteArray>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

// Only the small pointer shape is CPU decoded/uploaded. Desktop pixels never
// leave the GPU. In mask mode alpha encodes AND, RGB encodes XOR.
inline QImage decodeDxgiPointer(const QByteArray &bytes, const DXGI_OUTDUPL_POINTER_SHAPE_INFO &info)
{
    const bool mono = info.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME;
    if (!info.Width || !info.Height || info.Width > 4096 || info.Height > 8192
        || quint64(info.Pitch) * info.Height > quint64(bytes.size())
        || info.Pitch < (mono ? (info.Width + 7) / 8 : info.Width * 4)
        || (mono && info.Height % 2)) throw std::runtime_error("Invalid DXGI pointer shape");
    QImage shape(int(info.Width), int(mono ? info.Height / 2 : info.Height), QImage::Format_ARGB32);
    for (int y = 0; y < shape.height(); ++y) {
        auto *out = reinterpret_cast<QRgb *>(shape.scanLine(y));
        const auto *row = reinterpret_cast<const unsigned char *>(bytes.constData()) + y * info.Pitch;
        for (int x = 0; x < shape.width(); ++x) {
            if (mono) {
                const int bit = 0x80 >> (x % 8);
                const int andMask = (row[x / 8] & bit) ? 255 : 0;
                const int xorMask = (row[shape.height() * info.Pitch + x / 8] & bit) ? 255 : 0;
                out[x] = qRgba(xorMask, xorMask, xorMask, andMask);
            } else if (info.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR
                       || info.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR) {
                out[x] = qRgba(row[x*4+2], row[x*4+1], row[x*4], row[x*4+3]);
            } else throw std::runtime_error("Unsupported DXGI pointer shape");
        }
    }
    return shape;
}

// Used before the program render pass; begin() in D3DProgramSurface restores
// its own pipeline. No independent device, frame readback or scene item.
class DxgiPointerRenderer final {
    template<class T> using Com = Microsoft::WRL::ComPtr<T>;
    Com<ID3D11VertexShader> vertex_;
    Com<ID3D11PixelShader> color_, mask_;
    Com<ID3D11RasterizerState> raster_;
    Com<ID3D11ShaderResourceView> shape_;
    QSize size_;
    bool masked_ = false;
    static void check(HRESULT result) {
        if (FAILED(result)) throw std::runtime_error("DXGI pointer GPU resource failed");
    }
public:
    void setShape(ID3D11Device *device, const QByteArray &bytes, const DXGI_OUTDUPL_POINTER_SHAPE_INFO &info) {
        const QImage image = decodeDxgiPointer(bytes, info);
        D3D11_TEXTURE2D_DESC d{};
        d.Width=image.width(); d.Height=image.height(); d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
        d.Format=DXGI_FORMAT_B8G8R8A8_UNORM; d.BindFlags=D3D11_BIND_SHADER_RESOURCE; d.Usage=D3D11_USAGE_IMMUTABLE;
        D3D11_SUBRESOURCE_DATA pixels{image.constBits(),UINT(image.bytesPerLine()),0};
        Com<ID3D11Texture2D> texture;
        check(device->CreateTexture2D(&d,&pixels,&texture));
        Com<ID3D11ShaderResourceView> view;
        check(device->CreateShaderResourceView(texture.Get(),nullptr,&view));
        shape_=view; size_=image.size(); masked_=info.Type != DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR;
    }
    bool hasShape() const { return bool(shape_); }
    void draw(ID3D11Device *device, ID3D11DeviceContext *context, ID3D11Texture2D *desktop,
              ID3D11Texture2D *destination, POINT position) {
        if (!shape_) return;
        if (!vertex_) {
            constexpr char shader[] =
                "struct P { float4 p:SV_POSITION; float2 uv:TEXCOORD; };"
                "P vs(uint id:SV_VertexID) { P o; o.uv=float2((id<<1)&2,id&2);"
                "o.p=float4(o.uv*float2(2,-2)+float2(-1,1),0,1); return o; }"
                "Texture2D desktop:register(t0); Texture2D shape:register(t1);"
                "float4 cursor(P p) { uint w,h; shape.GetDimensions(w,h); return shape.Load(int3(int2(p.uv*float2(w,h)),0)); }"
                "float4 color(P p):SV_TARGET { float4 s=cursor(p); float3 b=desktop.Load(int3(int2(p.p.xy),0)).rgb;"
                "return float4(s.rgb*s.a+b*(1-s.a),1); }"
                "float4 mask(P p):SV_TARGET { float4 s=cursor(p);"
                "uint3 b=(uint3)round(desktop.Load(int3(int2(p.p.xy),0)).rgb*255);"
                "uint a=s.a>0.5?255:0; return float4(float3((b&a)^(uint3)round(s.rgb*255))/255,1); }";
            Com<ID3DBlob> vs, color, mask, errors;
            check(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"vs","vs_5_0",0,0,&vs,&errors));
            check(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"color","ps_5_0",0,0,&color,&errors));
            check(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"mask","ps_5_0",0,0,&mask,&errors));
            check(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex_));
            check(device->CreatePixelShader(color->GetBufferPointer(),color->GetBufferSize(),nullptr,&color_));
            check(device->CreatePixelShader(mask->GetBufferPointer(),mask->GetBufferSize(),nullptr,&mask_));
            D3D11_RASTERIZER_DESC r{}; r.FillMode=D3D11_FILL_SOLID; r.CullMode=D3D11_CULL_NONE; r.DepthClipEnable=TRUE;
            check(device->CreateRasterizerState(&r,&raster_));
        }
        Com<ID3D11RenderTargetView> output;
        Com<ID3D11ShaderResourceView> background;
        check(device->CreateRenderTargetView(destination,nullptr,&output));
        check(device->CreateShaderResourceView(desktop,nullptr,&background));
        context->OMSetRenderTargets(1,output.GetAddressOf(),nullptr);
        context->OMSetBlendState(nullptr,nullptr,0xffffffff);
        context->RSSetState(raster_.Get());
        D3D11_VIEWPORT viewport{float(position.x),float(position.y),float(size_.width()),float(size_.height()),0,1};
        context->RSSetViewports(1,&viewport);
        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vertex_.Get(),nullptr,0);
        context->PSSetShader(masked_ ? mask_.Get() : color_.Get(),nullptr,0);
        ID3D11ShaderResourceView *views[]{background.Get(),shape_.Get()};
        context->PSSetShaderResources(0,2,views); context->Draw(3,0);
        ID3D11ShaderResourceView *empty[]{nullptr,nullptr};
        context->PSSetShaderResources(0,2,empty);
        context->OMSetRenderTargets(0,nullptr,nullptr);
    }
};
#endif
