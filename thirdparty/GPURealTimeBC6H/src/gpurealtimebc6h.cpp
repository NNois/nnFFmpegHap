#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "GPURealTimeBC6H-c.h"

#define SAFE_RELEASE(x) do { if (x) { (x)->Release(); (x) = NULL; } } while (0)
#ifndef ARRAYSIZE
#define ARRAYSIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

struct ShaderCB {
    float screen_size_rcp[2];
    uint32_t texture_size_in_blocks[2];
    float texture_size_rcp[2];
    float texel_bias[2];
    float texel_scale;
    float exposure;
    uint32_t blit_mode;
    uint32_t padding;
};

struct GPURTBC6HState {
    ID3D11Device *device;
    ID3D11DeviceContext *ctx;
    ID3D11ComputeShader *cs[2];
    ID3D11SamplerState *sampler;
    ID3D11Buffer *cbuffer;
    uint32_t preset;
    int initialized;
};

static GPURTBC6HState g_state;
static CRITICAL_SECTION g_lock;
static int g_lock_init;

static void ensure_lock(void)
{
    if (!g_lock_init) {
        InitializeCriticalSection(&g_lock);
        g_lock_init = 1;
    }
}

static uint32_t divide_and_round_up(uint32_t x, uint32_t divisor)
{
    return (x + divisor - 1) / divisor;
}

static uint16_t float_to_half(float f)
{
    union {
        float f;
        uint32_t u;
    } v;
    uint32_t sign;
    int exp;
    uint32_t mantissa;

    v.f = f;
    sign = (v.u >> 16) & 0x8000;
    exp = ((v.u >> 23) & 0xff) - 127 + 15;
    mantissa = v.u & 0x7fffff;

    if (exp <= 0) {
        if (exp < -10)
            return (uint16_t)sign;
        mantissa = (mantissa | 0x800000) >> (1 - exp);
        return (uint16_t)(sign | ((mantissa + 0x1000) >> 13));
    }

    if (exp >= 31) {
        if (mantissa == 0)
            return (uint16_t)(sign | 0x7c00);
        mantissa >>= 13;
        return (uint16_t)(sign | 0x7c00 | mantissa | (mantissa == 0));
    }

    return (uint16_t)(sign | (exp << 10) | ((mantissa + 0x1000) >> 13));
}

static int get_shader_path(wchar_t *out_path, size_t out_count)
{
    HMODULE module = NULL;
    wchar_t module_path[MAX_PATH];
    wchar_t *slash;

    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)&GPURealTimeBC6H_Initialize, &module)) {
        return 0;
    }
    if (!GetModuleFileNameW(module, module_path, ARRAYSIZE(module_path)))
        return 0;

    slash = wcsrchr(module_path, L'\\');
    if (!slash)
        slash = wcsrchr(module_path, L'/');
    if (!slash)
        return 0;
    slash[1] = L'\0';

    if (swprintf(out_path, out_count, L"%lscompress.hlsl", module_path) < 0)
        return 0;

    return 1;
}

static HRESULT compile_shader(int quality, ID3D11ComputeShader **out_cs)
{
    D3D_SHADER_MACRO macros[2];
    ID3DBlob *shader_blob = NULL;
    ID3DBlob *error_blob = NULL;
    wchar_t shader_path[MAX_PATH];
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
    HRESULT hr;

    if (!get_shader_path(shader_path, ARRAYSIZE(shader_path)))
        return E_FAIL;

    macros[0].Name = "QUALITY";
    macros[0].Definition = quality ? "1" : "0";
    macros[1].Name = NULL;
    macros[1].Definition = NULL;

    hr = D3DCompileFromFile(shader_path, macros, NULL, "CSMain", "cs_5_0",
                            flags, 0, &shader_blob, &error_blob);
    if (FAILED(hr)) {
        if (error_blob) {
            OutputDebugStringA((const char *)error_blob->GetBufferPointer());
            error_blob->Release();
        }
        SAFE_RELEASE(shader_blob);
        return hr;
    }

    hr = g_state.device->CreateComputeShader(shader_blob->GetBufferPointer(),
                                             shader_blob->GetBufferSize(),
                                             NULL, out_cs);
    SAFE_RELEASE(shader_blob);
    SAFE_RELEASE(error_blob);
    return hr;
}

static int ensure_device(void)
{
    HRESULT hr;
    D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL created_level;
    D3D11_SAMPLER_DESC sampler_desc;
    D3D11_BUFFER_DESC cb_desc;

    if (g_state.initialized)
        return 1;

    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
                           feature_levels, ARRAYSIZE(feature_levels),
                           D3D11_SDK_VERSION, &g_state.device,
                           &created_level, &g_state.ctx);
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0,
                               feature_levels, ARRAYSIZE(feature_levels),
                               D3D11_SDK_VERSION, &g_state.device,
                               &created_level, &g_state.ctx);
        if (FAILED(hr))
            return 0;
    }

    if (FAILED(compile_shader(0, &g_state.cs[0])))
        return 0;
    if (FAILED(compile_shader(1, &g_state.cs[1])))
        return 0;

    memset(&sampler_desc, 0, sizeof(sampler_desc));
    sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sampler_desc.MinLOD = 0.0f;
    sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(g_state.device->CreateSamplerState(&sampler_desc, &g_state.sampler)))
        return 0;

    memset(&cb_desc, 0, sizeof(cb_desc));
    cb_desc.Usage = D3D11_USAGE_DYNAMIC;
    cb_desc.ByteWidth = sizeof(ShaderCB);
    cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(g_state.device->CreateBuffer(&cb_desc, NULL, &g_state.cbuffer)))
        return 0;

    g_state.initialized = 1;
    return 1;
}

static void release_device(void)
{
    SAFE_RELEASE(g_state.cbuffer);
    SAFE_RELEASE(g_state.sampler);
    SAFE_RELEASE(g_state.cs[0]);
    SAFE_RELEASE(g_state.cs[1]);
    SAFE_RELEASE(g_state.ctx);
    SAFE_RELEASE(g_state.device);
    g_state.initialized = 0;
}

extern "C" __declspec(dllexport) void GPURealTimeBC6H_Initialize(uint32_t preset)
{
    ensure_lock();
    EnterCriticalSection(&g_lock);
    g_state.preset = preset ? 1 : 0;
    if (!ensure_device()) {
        release_device();
    }
    LeaveCriticalSection(&g_lock);
}

extern "C" __declspec(dllexport) void GPURealTimeBC6H_Release(void)
{
    ensure_lock();
    EnterCriticalSection(&g_lock);
    release_device();
    LeaveCriticalSection(&g_lock);
}

extern "C" __declspec(dllexport) bool GPURealTimeBC6H_Compress(
    const GPURealTimeBC6H_Image *srcImage,
    uint32_t srcFormat,
    GPURealTimeBC6H_Image *dstImage)
{
    D3D11_TEXTURE2D_DESC tex_desc;
    D3D11_SUBRESOURCE_DATA sub_data;
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
    D3D11_TEXTURE2D_DESC out_desc;
    D3D11_MAPPED_SUBRESOURCE mapped;
    ID3D11Texture2D *src_tex = NULL;
    ID3D11Texture2D *out_tex = NULL;
    ID3D11Texture2D *stage_tex = NULL;
    ID3D11ShaderResourceView *srv = NULL;
    ID3D11UnorderedAccessView *uav = NULL;
    uint32_t blocks_x;
    uint32_t blocks_y;
    uint32_t group_x;
    uint32_t group_y;
    uint16_t *half_data = NULL;
    size_t pixel_count;
    size_t half_count;
    size_t out_size;
    uint8_t *out_data;
    HRESULT hr;
    ShaderCB cb_data;

    if (!srcImage || !dstImage || srcFormat != GPURealTimeBC6H_ImageFormat_RGBA32F)
        return false;

    ensure_lock();
    EnterCriticalSection(&g_lock);
    if (!ensure_device()) {
        LeaveCriticalSection(&g_lock);
        return false;
    }

    pixel_count = (size_t)srcImage->width * srcImage->height;
    half_count = pixel_count * 4;
    half_data = (uint16_t *)malloc(half_count * sizeof(uint16_t));
    if (!half_data) {
        LeaveCriticalSection(&g_lock);
        return false;
    }

    {
        const float *src = (const float *)srcImage->data;
        size_t i;
        for (i = 0; i < half_count; i++)
            half_data[i] = float_to_half(src[i]);
    }

    memset(&tex_desc, 0, sizeof(tex_desc));
    tex_desc.Width = srcImage->width;
    tex_desc.Height = srcImage->height;
    tex_desc.MipLevels = 1;
    tex_desc.ArraySize = 1;
    tex_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    sub_data.pSysMem = half_data;
    sub_data.SysMemPitch = srcImage->width * 4 * sizeof(uint16_t);
    sub_data.SysMemSlicePitch = 0;

    hr = g_state.device->CreateTexture2D(&tex_desc, &sub_data, &src_tex);
    free(half_data);
    half_data = NULL;
    if (FAILED(hr)) {
        LeaveCriticalSection(&g_lock);
        return false;
    }

    memset(&srv_desc, 0, sizeof(srv_desc));
    srv_desc.Format = tex_desc.Format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;
    hr = g_state.device->CreateShaderResourceView(src_tex, &srv_desc, &srv);
    if (FAILED(hr)) {
        SAFE_RELEASE(src_tex);
        LeaveCriticalSection(&g_lock);
        return false;
    }

    blocks_x = divide_and_round_up(srcImage->width, 4);
    blocks_y = divide_and_round_up(srcImage->height, 4);

    memset(&out_desc, 0, sizeof(out_desc));
    out_desc.Width = blocks_x;
    out_desc.Height = blocks_y;
    out_desc.MipLevels = 1;
    out_desc.ArraySize = 1;
    out_desc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    out_desc.SampleDesc.Count = 1;
    out_desc.Usage = D3D11_USAGE_DEFAULT;
    out_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;

    hr = g_state.device->CreateTexture2D(&out_desc, NULL, &out_tex);
    if (FAILED(hr)) {
        SAFE_RELEASE(srv);
        SAFE_RELEASE(src_tex);
        LeaveCriticalSection(&g_lock);
        return false;
    }

    memset(&uav_desc, 0, sizeof(uav_desc));
    uav_desc.Format = out_desc.Format;
    uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    hr = g_state.device->CreateUnorderedAccessView(out_tex, &uav_desc, &uav);
    if (FAILED(hr)) {
        SAFE_RELEASE(out_tex);
        SAFE_RELEASE(srv);
        SAFE_RELEASE(src_tex);
        LeaveCriticalSection(&g_lock);
        return false;
    }

    memset(&out_desc, 0, sizeof(out_desc));
    out_desc.Width = blocks_x;
    out_desc.Height = blocks_y;
    out_desc.MipLevels = 1;
    out_desc.ArraySize = 1;
    out_desc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    out_desc.SampleDesc.Count = 1;
    out_desc.Usage = D3D11_USAGE_STAGING;
    out_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    hr = g_state.device->CreateTexture2D(&out_desc, NULL, &stage_tex);
    if (FAILED(hr)) {
        SAFE_RELEASE(uav);
        SAFE_RELEASE(out_tex);
        SAFE_RELEASE(srv);
        SAFE_RELEASE(src_tex);
        LeaveCriticalSection(&g_lock);
        return false;
    }

    memset(&cb_data, 0, sizeof(cb_data));
    cb_data.texture_size_in_blocks[0] = blocks_x;
    cb_data.texture_size_in_blocks[1] = blocks_y;
    cb_data.texture_size_rcp[0] = 1.0f / srcImage->width;
    cb_data.texture_size_rcp[1] = 1.0f / srcImage->height;
    cb_data.texel_scale = 1.0f;
    cb_data.exposure = 1.0f;

    if (SUCCEEDED(g_state.ctx->Map(g_state.cbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, &cb_data, sizeof(cb_data));
        g_state.ctx->Unmap(g_state.cbuffer, 0);
    }

    g_state.ctx->CSSetShader(g_state.cs[g_state.preset ? 1 : 0], NULL, 0);
    g_state.ctx->CSSetUnorderedAccessViews(0, 1, &uav, NULL);
    g_state.ctx->CSSetShaderResources(0, 1, &srv);
    g_state.ctx->CSSetSamplers(0, 1, &g_state.sampler);
    g_state.ctx->CSSetConstantBuffers(0, 1, &g_state.cbuffer);

    group_x = divide_and_round_up(blocks_x, 8);
    group_y = divide_and_round_up(blocks_y, 8);
    g_state.ctx->Dispatch(group_x, group_y, 1);

    g_state.ctx->CopyResource(stage_tex, out_tex);

    hr = g_state.ctx->Map(stage_tex, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        SAFE_RELEASE(stage_tex);
        SAFE_RELEASE(uav);
        SAFE_RELEASE(out_tex);
        SAFE_RELEASE(srv);
        SAFE_RELEASE(src_tex);
        LeaveCriticalSection(&g_lock);
        return false;
    }

    out_size = (size_t)blocks_x * blocks_y * 16;
    out_data = (uint8_t *)malloc(out_size);
    if (!out_data) {
        g_state.ctx->Unmap(stage_tex, 0);
        SAFE_RELEASE(stage_tex);
        SAFE_RELEASE(uav);
        SAFE_RELEASE(out_tex);
        SAFE_RELEASE(srv);
        SAFE_RELEASE(src_tex);
        LeaveCriticalSection(&g_lock);
        return false;
    }

    {
        uint32_t y;
        const uint8_t *src_row = (const uint8_t *)mapped.pData;
        size_t row_bytes = (size_t)blocks_x * 16;
        for (y = 0; y < blocks_y; y++) {
            memcpy(out_data + (size_t)y * row_bytes, src_row, row_bytes);
            src_row += mapped.RowPitch;
        }
    }

    g_state.ctx->Unmap(stage_tex, 0);

    SAFE_RELEASE(stage_tex);
    SAFE_RELEASE(uav);
    SAFE_RELEASE(out_tex);
    SAFE_RELEASE(srv);
    SAFE_RELEASE(src_tex);

    dstImage->width = srcImage->width;
    dstImage->height = srcImage->height;
    dstImage->data = out_data;
    dstImage->dataSize = out_size;

    LeaveCriticalSection(&g_lock);
    return true;
}

extern "C" __declspec(dllexport) void GPURealTimeBC6H_FreeImage(GPURealTimeBC6H_Image *image)
{
    if (!image || !image->data)
        return;
    free(image->data);
    image->data = NULL;
    image->dataSize = 0;
}
