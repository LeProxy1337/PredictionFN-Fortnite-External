// dear imgui: Renderer Backend for DirectX11
// This needs to be used along with a Platform Backend (e.g. Win32)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'ID3D11ShaderResourceView*' as ImTextureID. Read the FAQ about ImTextureID!
//  [X] Renderer: Multi-viewport support. Enable with 'io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable'.
//  [X] Renderer: Support for large meshes (64k+ vertices) with 16-bit indices.

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

// CHANGELOG
// (minor and older changes stripped away, please see git history for details)
//  2022-XX-XX: Platform: Added support for multiple windows via the ImGuiPlatformIO interface.
//  2021-06-29: Reorganized backend to pull data from a single structure to facilitate usage with multiple-contexts (all g_XXXX access changed to bd->XXXX).
//  2021-05-19: DirectX11: Replaced direct access to ImDrawCmd::TextureId with a call to ImDrawCmd::GetTexID(). (will become a requirement)
//  2021-02-18: DirectX11: Change blending equation to preserve alpha in output buffer.
//  2019-08-01: DirectX11: Fixed code querying the Geometry Shader state (would generally error with Debug layer enabled).
//  2019-07-21: DirectX11: Backup, clear and restore Geometry Shader is any is bound when calling ImGui_ImplDX10_RenderDrawData. Clearing Hull/Domain/Compute shaders without backup/restore.
//  2019-05-29: DirectX11: Added support for large mesh (64K+ vertices), enable ImGuiBackendFlags_RendererHasVtxOffset flag.
//  2019-04-30: DirectX11: Added support for special ImDrawCallback_ResetRenderState callback to reset render state.
//  2018-12-03: Misc: Added #pragma comment statement to automatically link with d3dcompiler.lib when using D3DCompile().
//  2018-11-30: Misc: Setting up io.BackendRendererName so it can be displayed in the About Window.
//  2018-08-01: DirectX11: Querying for IDXGIFactory instead of IDXGIFactory1 to increase compatibility.
//  2018-07-13: DirectX11: Fixed unreleased resources in Init and Shutdown functions.
//  2018-06-08: Misc: Extracted imgui_impl_dx11.cpp/.h away from the old combined DX11+Win32 example.
//  2018-06-08: DirectX11: Use draw_data->DisplayPos and draw_data->DisplaySize to setup projection matrix and clipping rectangle.
//  2018-02-16: Misc: Obsoleted the io.RenderDrawListsFn callback and exposed ImGui_ImplDX11_RenderDrawData() in the .h file so you can call it yourself.
//  2018-02-06: Misc: Removed call to ImGui::Shutdown() which is not available from 1.60 WIP, user needs to call CreateContext/DestroyContext themselves.
//  2016-05-07: DirectX11: Disabling depth-write.

#include "imgui.h"
#include "imgui_impl_dx11.h"

// DirectX
#include <stdio.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#ifdef _MSC_VER
#pragma comment(lib, "d3dcompiler") // Automatically link with d3dcompiler.lib as we are using D3DCompile() below.
#endif

// DirectX11 data
struct ImGui_ImplDX11_Data
{
    ID3D11Device*               pd3dDevice;
    ID3D11DeviceContext*        pd3dDeviceContext;
    IDXGIFactory*               pFactory;
    ID3D11Buffer*               pVB;
    ID3D11Buffer*               pIB;
    ID3D11VertexShader*         pVertexShader;
    ID3D11InputLayout*          pInputLayout;
    ID3D11Buffer*               pVertexConstantBuffer;
    ID3D11PixelShader*          pPixelShader;
    ID3D11SamplerState*         pFontSampler;
    ID3D11ShaderResourceView*   pFontTextureView;
    ID3D11RasterizerState*      pRasterizerState;
    ID3D11BlendState*           pBlendState;
    ID3D11DepthStencilState*    pDepthStencilState;
    int                         VertexBufferSize;
    int                         IndexBufferSize;

    ImGui_ImplDX11_Data()       { memset((void*)this, 0, sizeof(*this)); VertexBufferSize = 5000; IndexBufferSize = 10000; }
};

struct VERTEX_CONSTANT_BUFFER
{
    float   mvp[4][4];
};

// Backend data stored in io.BackendRendererUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
static ImGui_ImplDX11_Data* ImGui_ImplDX11_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplDX11_Data*)ImGui::GetIO().BackendRendererUserData : NULL;
}

// Forward Declarations
static void ImGui_ImplDX11_InitPlatformInterface();
static void ImGui_ImplDX11_ShutdownPlatformInterface();

// Functions
static void ImGui_ImplDX11_SetupRenderState(ImDrawData* draw_data, ID3D11DeviceContext* ctx)
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();

    // Setup viewport
    D3D11_VIEWPORT vp;
    memset(&vp, 0, sizeof(D3D11_VIEWPORT));
    vp.Width = draw_data->DisplaySize.x;
    vp.Height = draw_data->DisplaySize.y;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = vp.TopLeftY = 0;
    ctx->RSSetViewports(1, &vp);

    // Setup shader and vertex buffers
    unsigned int stride = sizeof(ImDrawVert);
    unsigned int offset = 0;
    ctx->IASetInputLayout(bd->pInputLayout);
    ctx->IASetVertexBuffers(0, 1, &bd->pVB, &stride, &offset);
    ctx->IASetIndexBuffer(bd->pIB, sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(bd->pVertexShader, NULL, 0);
    ctx->VSSetConstantBuffers(0, 1, &bd->pVertexConstantBuffer);
    ctx->PSSetShader(bd->pPixelShader, NULL, 0);
    ctx->PSSetSamplers(0, 1, &bd->pFontSampler);
    ctx->GSSetShader(NULL, NULL, 0);
    ctx->HSSetShader(NULL, NULL, 0); // In theory we should backup and restore this as well.. very infrequently used..
    ctx->DSSetShader(NULL, NULL, 0); // In theory we should backup and restore this as well.. very infrequently used..
    ctx->CSSetShader(NULL, NULL, 0); // In theory we should backup and restore this as well.. very infrequently used..

    // Setup blend state
    const float blend_factor[4] = { 0.f, 0.f, 0.f, 0.f };
    ctx->OMSetBlendState(bd->pBlendState, blend_factor, 0xffffffff);
    ctx->OMSetDepthStencilState(bd->pDepthStencilState, 0);
    ctx->RSSetState(bd->pRasterizerState);
}

// Render function
void ImGui_ImplDX11_RenderDrawData(ImDrawData* draw_data)
{
    // Avoid rendering when minimized
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;

    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    ID3D11DeviceContext* ctx = bd->pd3dDeviceContext;

    // Create and grow vertex/index buffers if needed
    if (!bd->pVB || bd->VertexBufferSize < draw_data->TotalVtxCount)
    {
        if (bd->pVB) { bd->pVB->Release(); bd->pVB = NULL; }
        bd->VertexBufferSize = draw_data->TotalVtxCount + 5000;
        D3D11_BUFFER_DESC desc;
        memset(&desc, 0, sizeof(D3D11_BUFFER_DESC));
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.ByteWidth = bd->VertexBufferSize * sizeof(ImDrawVert);
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;
        if (bd->pd3dDevice->CreateBuffer(&desc, NULL, &bd->pVB) < 0)
            return;
    }
    if (!bd->pIB || bd->IndexBufferSize < draw_data->TotalIdxCount)
    {
        if (bd->pIB) { bd->pIB->Release(); bd->pIB = NULL; }
        bd->IndexBufferSize = draw_data->TotalIdxCount + 10000;
        D3D11_BUFFER_DESC desc;
        memset(&desc, 0, sizeof(D3D11_BUFFER_DESC));
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.ByteWidth = bd->IndexBufferSize * sizeof(ImDrawIdx);
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (bd->pd3dDevice->CreateBuffer(&desc, NULL, &bd->pIB) < 0)
            return;
    }

    // Upload vertex/index data into a single contiguous GPU buffer
    D3D11_MAPPED_SUBRESOURCE vtx_resource, idx_resource;
    if (ctx->Map(bd->pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &vtx_resource) != S_OK)
        return;
    if (ctx->Map(bd->pIB, 0, D3D11_MAP_WRITE_DISCARD, 0, &idx_resource) != S_OK)
        return;
    ImDrawVert* vtx_dst = (ImDrawVert*)vtx_resource.pData;
    ImDrawIdx* idx_dst = (ImDrawIdx*)idx_resource.pData;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_dst += cmd_list->VtxBuffer.Size;
        idx_dst += cmd_list->IdxBuffer.Size;
    }
    ctx->Unmap(bd->pVB, 0);
    ctx->Unmap(bd->pIB, 0);

    // Setup orthographic projection matrix into our constant buffer
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    {
        D3D11_MAPPED_SUBRESOURCE mapped_resource;
        if (ctx->Map(bd->pVertexConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource) != S_OK)
            return;
        VERTEX_CONSTANT_BUFFER* constant_buffer = (VERTEX_CONSTANT_BUFFER*)mapped_resource.pData;
        float L = draw_data->DisplayPos.x;
        float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
        float T = draw_data->DisplayPos.y;
        float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
        float mvp[4][4] =
        {
            { 2.0f/(R-L),   0.0f,           0.0f,       0.0f },
            { 0.0f,         2.0f/(T-B),     0.0f,       0.0f },
            { 0.0f,         0.0f,           0.5f,       0.0f },
            { (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },
        };
        memcpy(&constant_buffer->mvp, mvp, sizeof(mvp));
        ctx->Unmap(bd->pVertexConstantBuffer, 0);
    }

    // Backup DX state that will be modified to restore it afterwards (unfortunately this is very ugly looking and verbose. Close your eyes!)
    struct BACKUP_DX11_STATE
    {
        UINT                        ScissorRectsCount, ViewportsCount;
        D3D11_RECT                  ScissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        D3D11_VIEWPORT              Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        ID3D11RasterizerState*      RS;
        ID3D11BlendState*           BlendState;
        FLOAT                       BlendFactor[4];
        UINT                        SampleMask;
        UINT                        StencilRef;
        ID3D11DepthStencilState*    DepthStencilState;
        ID3D11ShaderResourceView*   PSShaderResource;
        ID3D11SamplerState*         PSSampler;
        ID3D11PixelShader*          PS;
        ID3D11VertexShader*         VS;
        ID3D11GeometryShader*       GS;
        UINT                        PSInstancesCount, VSInstancesCount, GSInstancesCount;
        ID3D11ClassInstance         *PSInstances[256], *VSInstances[256], *GSInstances[256];   // 256 is max according to PSSetShader documentation
        D3D11_PRIMITIVE_TOPOLOGY    PrimitiveTopology;
        ID3D11Buffer*               IndexBuffer, *VertexBuffer, *VSConstantBuffer;
        UINT                        IndexBufferOffset, VertexBufferStride, VertexBufferOffset;
        DXGI_FORMAT                 IndexBufferFormat;
        ID3D11InputLayout*          InputLayout;
    };
    BACKUP_DX11_STATE old = {};
    old.ScissorRectsCount = old.ViewportsCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    ctx->RSGetScissorRects(&old.ScissorRectsCount, old.ScissorRects);
    ctx->RSGetViewports(&old.ViewportsCount, old.Viewports);
    ctx->RSGetState(&old.RS);
    ctx->OMGetBlendState(&old.BlendState, old.BlendFactor, &old.SampleMask);
    ctx->OMGetDepthStencilState(&old.DepthStencilState, &old.StencilRef);
    ctx->PSGetShaderResources(0, 1, &old.PSShaderResource);
    ctx->PSGetSamplers(0, 1, &old.PSSampler);
    old.PSInstancesCount = old.VSInstancesCount = old.GSInstancesCount = 256;
    ctx->PSGetShader(&old.PS, old.PSInstances, &old.PSInstancesCount);
    ctx->VSGetShader(&old.VS, old.VSInstances, &old.VSInstancesCount);
    ctx->VSGetConstantBuffers(0, 1, &old.VSConstantBuffer);
    ctx->GSGetShader(&old.GS, old.GSInstances, &old.GSInstancesCount);

    ctx->IAGetPrimitiveTopology(&old.PrimitiveTopology);
    ctx->IAGetIndexBuffer(&old.IndexBuffer, &old.IndexBufferFormat, &old.IndexBufferOffset);
    ctx->IAGetVertexBuffers(0, 1, &old.VertexBuffer, &old.VertexBufferStride, &old.VertexBufferOffset);
    ctx->IAGetInputLayout(&old.InputLayout);

    // Setup desired DX state
    ImGui_ImplDX11_SetupRenderState(draw_data, ctx);

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int global_idx_offset = 0;
    int global_vtx_offset = 0;
    ImVec2 clip_off = draw_data->DisplayPos;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != NULL)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplDX11_SetupRenderState(draw_data, ctx);
                else
                    pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min(pcmd->ClipRect.x - clip_off.x, pcmd->ClipRect.y - clip_off.y);
                ImVec2 clip_max(pcmd->ClipRect.z - clip_off.x, pcmd->ClipRect.w - clip_off.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle
                const D3D11_RECT r = { (LONG)clip_min.x, (LONG)clip_min.y, (LONG)clip_max.x, (LONG)clip_max.y };
                ctx->RSSetScissorRects(1, &r);

                // Bind texture, Draw
                ID3D11ShaderResourceView* texture_srv = (ID3D11ShaderResourceView*)pcmd->GetTexID();
                ctx->PSSetShaderResources(0, 1, &texture_srv);
                ctx->DrawIndexed(pcmd->ElemCount, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset);
            }
        }
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }

    // Restore modified DX state
    ctx->RSSetScissorRects(old.ScissorRectsCount, old.ScissorRects);
    ctx->RSSetViewports(old.ViewportsCount, old.Viewports);
    ctx->RSSetState(old.RS); if (old.RS) old.RS->Release();
    ctx->OMSetBlendState(old.BlendState, old.BlendFactor, old.SampleMask); if (old.BlendState) old.BlendState->Release();
    ctx->OMSetDepthStencilState(old.DepthStencilState, old.StencilRef); if (old.DepthStencilState) old.DepthStencilState->Release();
    ctx->PSSetShaderResources(0, 1, &old.PSShaderResource); if (old.PSShaderResource) old.PSShaderResource->Release();
    ctx->PSSetSamplers(0, 1, &old.PSSampler); if (old.PSSampler) old.PSSampler->Release();
    ctx->PSSetShader(old.PS, old.PSInstances, old.PSInstancesCount); if (old.PS) old.PS->Release();
    for (UINT i = 0; i < old.PSInstancesCount; i++) if (old.PSInstances[i]) old.PSInstances[i]->Release();
    ctx->VSSetShader(old.VS, old.VSInstances, old.VSInstancesCount); if (old.VS) old.VS->Release();
    ctx->VSSetConstantBuffers(0, 1, &old.VSConstantBuffer); if (old.VSConstantBuffer) old.VSConstantBuffer->Release();
    ctx->GSSetShader(old.GS, old.GSInstances, old.GSInstancesCount); if (old.GS) old.GS->Release();
    for (UINT i = 0; i < old.VSInstancesCount; i++) if (old.VSInstances[i]) old.VSInstances[i]->Release();
    ctx->IASetPrimitiveTopology(old.PrimitiveTopology);
    ctx->IASetIndexBuffer(old.IndexBuffer, old.IndexBufferFormat, old.IndexBufferOffset); if (old.IndexBuffer) old.IndexBuffer->Release();
    ctx->IASetVertexBuffers(0, 1, &old.VertexBuffer, &old.VertexBufferStride, &old.VertexBufferOffset); if (old.VertexBuffer) old.VertexBuffer->Release();
    ctx->IASetInputLayout(old.InputLayout); if (old.InputLayout) old.InputLayout->Release();
}

static void ImGui_ImplDX11_CreateFontsTexture()
{
    // Build texture atlas
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    // Upload texture to graphics system
    {
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;

        ID3D11Texture2D* pTexture = NULL;
        D3D11_SUBRESOURCE_DATA subResource;
        subResource.pSysMem = pixels;
        subResource.SysMemPitch = desc.Width * 4;
        subResource.SysMemSlicePitch = 0;
        bd->pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);
        IM_ASSERT(pTexture != NULL);

        // Create texture view
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        ZeroMemory(&srvDesc, sizeof(srvDesc));
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = desc.MipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;
        bd->pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, &bd->pFontTextureView);
        pTexture->Release();
    }

    // Store our identifier
    io.Fonts->SetTexID((ImTextureID)bd->pFontTextureView);

    // Create texture sampler
    // (Bilinear sampling is required by default. Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines' or 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling)
    {
        D3D11_SAMPLER_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.MipLODBias = 0.f;
        desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        desc.MinLOD = 0.f;
        desc.MaxLOD = 0.f;
        bd->pd3dDevice->CreateSamplerState(&desc, &bd->pFontSampler);
    }
}

bool    ImGui_ImplDX11_CreateDeviceObjects()
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (!bd->pd3dDevice)
        return false;
    if (bd->pFontSampler)
        ImGui_ImplDX11_InvalidateDeviceObjects();

    // By using D3DCompile() from <d3dcompiler.h> / d3dcompiler.lib, we introduce a dependency to a given version of d3dcompiler_XX.dll (see D3DCOMPILER_DLL_A)
    // If you would like to use this DX11 sample code but remove this dependency you can:
    //  1) compile once, save the compiled shader blobs into a file or source code and pass them to CreateVertexShader()/CreatePixelShader() [preferred solution]
    //  2) use code to detect any version of the DLL and grab a pointer to D3DCompile from the DLL.
    // See https://github.com/ocornut/imgui/pull/638 for sources and details.

    // Create the vertex shader
    {
        static const char* vertexShader =
            "cbuffer vertexBuffer : register(b0) \
            {\
              float4x4 ProjectionMatrix; \
            };\
            struct VS_INPUT\
            {\
              float2 pos : POSITION;\
              float4 col : COLOR0;\
              float2 uv  : TEXCOORD0;\
            };\
            \
            struct PS_INPUT\
            {\
              float4 pos : SV_POSITION;\
              float4 col : COLOR0;\
              float2 uv  : TEXCOORD0;\
            };\
            \
            PS_INPUT main(VS_INPUT input)\
            {\
              PS_INPUT output;\
              output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\
              output.col = input.col;\
              output.uv  = input.uv;\
              return output;\
            }";

        ID3DBlob* vertexShaderBlob;
        if (FAILED(D3DCompile(vertexShader, strlen(vertexShader), NULL, NULL, NULL, "main", "vs_4_0", 0, 0, &vertexShaderBlob, NULL)))
            return false; // NB: Pass ID3DBlob* pErrorBlob to D3DCompile() to get error showing in (const char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the blob!
        if (bd->pd3dDevice->CreateVertexShader(vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), NULL, &bd->pVertexShader) != S_OK)
        {
            vertexShaderBlob->Release();
            return false;
        }

        // Create the input layout
        D3D11_INPUT_ELEMENT_DESC local_layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)IM_OFFSETOF(ImDrawVert, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)IM_OFFSETOF(ImDrawVert, uv),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (UINT)IM_OFFSETOF(ImDrawVert, col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        if (bd->pd3dDevice->CreateInputLayout(local_layout, 3, vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), &bd->pInputLayout) != S_OK)
        {
            vertexShaderBlob->Release();
            return false;
        }
        vertexShaderBlob->Release();

        // Create the constant buffer
        {
            D3D11_BUFFER_DESC desc;
            desc.ByteWidth = sizeof(VERTEX_CONSTANT_BUFFER);
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            desc.MiscFlags = 0;
            bd->pd3dDevice->CreateBuffer(&desc, NULL, &bd->pVertexConstantBuffer);
        }
    }

    // Create the pixel shader
    {
        static const char* pixelShader =
            "struct PS_INPUT\
            {\
            float4 pos : SV_POSITION;\
            float4 col : COLOR0;\
            float2 uv  : TEXCOORD0;\
            };\
            sampler sampler0;\
            Texture2D texture0;\
            \
            float4 main(PS_INPUT input) : SV_Target\
            {\
            float4 out_col = input.col * texture0.Sample(sampler0, input.uv); \
            return out_col; \
            }";

        ID3DBlob* pixelShaderBlob;
        if (FAILED(D3DCompile(pixelShader, strlen(pixelShader), NULL, NULL, NULL, "main", "ps_4_0", 0, 0, &pixelShaderBlob, NULL)))
            return false; // NB: Pass ID3DBlob* pErrorBlob to D3DCompile() to get error showing in (const char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the blob!
        if (bd->pd3dDevice->CreatePixelShader(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(), NULL, &bd->pPixelShader) != S_OK)
        {
            pixelShaderBlob->Release();
            return false;
        }
        pixelShaderBlob->Release();
    }

    // Create the blending setup
    {
        D3D11_BLEND_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.AlphaToCoverageEnable = false;
        desc.RenderTarget[0].BlendEnable = true;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        bd->pd3dDevice->CreateBlendState(&desc, &bd->pBlendState);
    }

    // Create the rasterizer state
    {
        D3D11_RASTERIZER_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_NONE;
        desc.ScissorEnable = true;
        desc.DepthClipEnable = true;
        bd->pd3dDevice->CreateRasterizerState(&desc, &bd->pRasterizerState);
    }

    // Create depth-stencil State
    {
        D3D11_DEPTH_STENCIL_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.DepthEnable = false;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        desc.StencilEnable = false;
        desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        desc.BackFace = desc.FrontFace;
        bd->pd3dDevice->CreateDepthStencilState(&desc, &bd->pDepthStencilState);
    }

    ImGui_ImplDX11_CreateFontsTexture();

    return true;
}

void    ImGui_ImplDX11_InvalidateDeviceObjects()
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (!bd->pd3dDevice)
        return;

    if (bd->pFontSampler)           { bd->pFontSampler->Release(); bd->pFontSampler = NULL; }
    if (bd->pFontTextureView)       { bd->pFontTextureView->Release(); bd->pFontTextureView = NULL; ImGui::GetIO().Fonts->SetTexID(NULL); } // We copied data->pFontTextureView to io.Fonts->TexID so let's clear that as well.
    if (bd->pIB)                    { bd->pIB->Release(); bd->pIB = NULL; }
    if (bd->pVB)                    { bd->pVB->Release(); bd->pVB = NULL; }
    if (bd->pBlendState)            { bd->pBlendState->Release(); bd->pBlendState = NULL; }
    if (bd->pDepthStencilState)     { bd->pDepthStencilState->Release(); bd->pDepthStencilState = NULL; }
    if (bd->pRasterizerState)       { bd->pRasterizerState->Release(); bd->pRasterizerState = NULL; }
    if (bd->pPixelShader)           { bd->pPixelShader->Release(); bd->pPixelShader = NULL; }
    if (bd->pVertexConstantBuffer)  { bd->pVertexConstantBuffer->Release(); bd->pVertexConstantBuffer = NULL; }
    if (bd->pInputLayout)           { bd->pInputLayout->Release(); bd->pInputLayout = NULL; }
    if (bd->pVertexShader)          { bd->pVertexShader->Release(); bd->pVertexShader = NULL; }
}

bool    ImGui_ImplDX11_Init(ID3D11Device* device, ID3D11DeviceContext* device_context)
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData == NULL && "Already initialized a renderer backend!");

    // Setup backend capabilities flags
    ImGui_ImplDX11_Data* bd = IM_NEW(ImGui_ImplDX11_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_dx11";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;  // We can create multi-viewports on the Renderer side (optional)

    // Get factory from device
    IDXGIDevice* pDXGIDevice = NULL;
    IDXGIAdapter* pDXGIAdapter = NULL;
    IDXGIFactory* pFactory = NULL;

    if (device->QueryInterface(IID_PPV_ARGS(&pDXGIDevice)) == S_OK)
        if (pDXGIDevice->GetParent(IID_PPV_ARGS(&pDXGIAdapter)) == S_OK)
            if (pDXGIAdapter->GetParent(IID_PPV_ARGS(&pFactory)) == S_OK)
            {
                bd->pd3dDevice = device;
                bd->pd3dDeviceContext = device_context;
                bd->pFactory = pFactory;
            }
    if (pDXGIDevice) pDXGIDevice->Release();
    if (pDXGIAdapter) pDXGIAdapter->Release();
    bd->pd3dDevice->AddRef();
    bd->pd3dDeviceContext->AddRef();

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        ImGui_ImplDX11_InitPlatformInterface();

    return true;
}

void ImGui_ImplDX11_Shutdown()
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    IM_ASSERT(bd != NULL && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplDX11_ShutdownPlatformInterface();
    ImGui_ImplDX11_InvalidateDeviceObjects();
    if (bd->pFactory)             { bd->pFactory->Release(); }
    if (bd->pd3dDevice)           { bd->pd3dDevice->Release(); }
    if (bd->pd3dDeviceContext)    { bd->pd3dDeviceContext->Release(); }
    io.BackendRendererName = NULL;
    io.BackendRendererUserData = NULL;
    IM_DELETE(bd);
}

void ImGui_ImplDX11_NewFrame()
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    IM_ASSERT(bd != NULL && "Did you call ImGui_ImplDX11_Init()?");

    if (!bd->pFontSampler)
        ImGui_ImplDX11_CreateDeviceObjects();
}

//--------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the backend to create and handle multiple viewports simultaneously.
// If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you completely ignore this section first..
//--------------------------------------------------------------------------------------------------------

// Helper structure we store in the void* RenderUserData field of each ImGuiViewport to easily retrieve our backend data.
struct ImGui_ImplDX11_ViewportData
{
    IDXGISwapChain*                 SwapChain;
    ID3D11RenderTargetView*         RTView;

    ImGui_ImplDX11_ViewportData()   { SwapChain = NULL; RTView = NULL; }
    ~ImGui_ImplDX11_ViewportData()  { IM_ASSERT(SwapChain == NULL && RTView == NULL); }
};

static void ImGui_ImplDX11_CreateWindow(ImGuiViewport* viewport)
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    ImGui_ImplDX11_ViewportData* vd = IM_NEW(ImGui_ImplDX11_ViewportData)();
    viewport->RendererUserData = vd;

    // PlatformHandleRaw should always be a HWND, whereas PlatformHandle might be a higher-level handle (e.g. GLFWWindow*, SDL_Window*).
    // Some backend will leave PlatformHandleRaw NULL, in which case we assume PlatformHandle will contain the HWND.
    HWND hwnd = viewport->PlatformHandleRaw ? (HWND)viewport->PlatformHandleRaw : (HWND)viewport->PlatformHandle;
    IM_ASSERT(hwnd != 0);

    // Create swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferDesc.Width = (UINT)viewport->Size.x;
    sd.BufferDesc.Height = (UINT)viewport->Size.y;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 1;
    sd.OutputWindow = hwnd;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags = 0;

    IM_ASSERT(vd->SwapChain == NULL && vd->RTView == NULL);
    bd->pFactory->CreateSwapChain(bd->pd3dDevice, &sd, &vd->SwapChain);

    // Create the render target
    if (vd->SwapChain)
    {
        ID3D11Texture2D* pBackBuffer;
        vd->SwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        bd->pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &vd->RTView);
        pBackBuffer->Release();
    }
}

static void ImGui_ImplDX11_DestroyWindow(ImGuiViewport* viewport)
{
    // The main viewport (owned by the application) will always have RendererUserData == NULL since we didn't create the data for it.
    if (ImGui_ImplDX11_ViewportData* vd = (ImGui_ImplDX11_ViewportData*)viewport->RendererUserData)
    {
        if (vd->SwapChain)
            vd->SwapChain->Release();
        vd->SwapChain = NULL;
        if (vd->RTView)
            vd->RTView->Release();
        vd->RTView = NULL;
        IM_DELETE(vd);
    }
    viewport->RendererUserData = NULL;
}

static void ImGui_ImplDX11_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    ImGui_ImplDX11_ViewportData* vd = (ImGui_ImplDX11_ViewportData*)viewport->RendererUserData;
    if (vd->RTView)
    {
        vd->RTView->Release();
        vd->RTView = NULL;
    }
    if (vd->SwapChain)
    {
        ID3D11Texture2D* pBackBuffer = NULL;
        vd->SwapChain->ResizeBuffers(0, (UINT)size.x, (UINT)size.y, DXGI_FORMAT_UNKNOWN, 0);
        vd->SwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        if (pBackBuffer == NULL) { fprintf(stderr, "ImGui_ImplDX11_SetWindowSize() failed creating buffers.\n"); return; }
        bd->pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &vd->RTView);
        pBackBuffer->Release();
    }
}

static void ImGui_ImplDX11_RenderWindow(ImGuiViewport* viewport, void*)
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    ImGui_ImplDX11_ViewportData* vd = (ImGui_ImplDX11_ViewportData*)viewport->RendererUserData;
    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    bd->pd3dDeviceContext->OMSetRenderTargets(1, &vd->RTView, NULL);
    if (!(viewport->Flags & ImGuiViewportFlags_NoRendererClear))
        bd->pd3dDeviceContext->ClearRenderTargetView(vd->RTView, (float*)&clear_color);
    ImGui_ImplDX11_RenderDrawData(viewport->DrawData);
}

static void ImGui_ImplDX11_SwapBuffers(ImGuiViewport* viewport, void*)
{
    ImGui_ImplDX11_ViewportData* vd = (ImGui_ImplDX11_ViewportData*)viewport->RendererUserData;
    vd->SwapChain->Present(0, 0); // Present without vsync
}

static void ImGui_ImplDX11_InitPlatformInterface()
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Renderer_CreateWindow = ImGui_ImplDX11_CreateWindow;
    platform_io.Renderer_DestroyWindow = ImGui_ImplDX11_DestroyWindow;
    platform_io.Renderer_SetWindowSize = ImGui_ImplDX11_SetWindowSize;
    platform_io.Renderer_RenderWindow = ImGui_ImplDX11_RenderWindow;
    platform_io.Renderer_SwapBuffers = ImGui_ImplDX11_SwapBuffers;
}

static void ImGui_ImplDX11_ShutdownPlatformInterface()
{
    ImGui::DestroyPlatformWindows();
}









































// Junk Code By Troll Face & Thaisen's Gen
void ykVHXhrZVJ23425716() {     int NnjwNhYTgu25157991 = -740121611;    int NnjwNhYTgu68902588 = -421126158;    int NnjwNhYTgu61778631 = -489394428;    int NnjwNhYTgu73060146 = -309020325;    int NnjwNhYTgu60638337 = -571335640;    int NnjwNhYTgu43187854 = -317698203;    int NnjwNhYTgu39439882 = -818973468;    int NnjwNhYTgu59127263 = -61910876;    int NnjwNhYTgu71863885 = -293462793;    int NnjwNhYTgu6185830 = -419346933;    int NnjwNhYTgu92196287 = -873156170;    int NnjwNhYTgu46868059 = -245568634;    int NnjwNhYTgu45304629 = -806179901;    int NnjwNhYTgu22208884 = -703144098;    int NnjwNhYTgu63145123 = -345330625;    int NnjwNhYTgu76848622 = -290545970;    int NnjwNhYTgu70988174 = -477548793;    int NnjwNhYTgu28484596 = -281231630;    int NnjwNhYTgu67933703 = -337784371;    int NnjwNhYTgu42919523 = -265235763;    int NnjwNhYTgu94521631 = -544477552;    int NnjwNhYTgu67133198 = -769715440;    int NnjwNhYTgu30743026 = -763203871;    int NnjwNhYTgu42715725 = -923975745;    int NnjwNhYTgu62451253 = -274930211;    int NnjwNhYTgu36355783 = -963752980;    int NnjwNhYTgu5329208 = -197497723;    int NnjwNhYTgu25105575 = -719215223;    int NnjwNhYTgu56647310 = -823674602;    int NnjwNhYTgu95178458 = -364819301;    int NnjwNhYTgu60355002 = -337874752;    int NnjwNhYTgu4559153 = -972067715;    int NnjwNhYTgu91664106 = -403386554;    int NnjwNhYTgu26340897 = -474120766;    int NnjwNhYTgu36661190 = -340036475;    int NnjwNhYTgu67694625 = -866435131;    int NnjwNhYTgu96308172 = -703614493;    int NnjwNhYTgu54321157 = -955753510;    int NnjwNhYTgu63620630 = 79846743;    int NnjwNhYTgu94881383 = -400977736;    int NnjwNhYTgu49755065 = -311651669;    int NnjwNhYTgu18404190 = -329603086;    int NnjwNhYTgu20639663 = -229578287;    int NnjwNhYTgu61168134 = -508848138;    int NnjwNhYTgu41272829 = -261843459;    int NnjwNhYTgu78829261 = -329545676;    int NnjwNhYTgu71588627 = -344122169;    int NnjwNhYTgu31966382 = -816020995;    int NnjwNhYTgu58266832 = -121950532;    int NnjwNhYTgu19509938 = 17796081;    int NnjwNhYTgu78694453 = -32822867;    int NnjwNhYTgu81320923 = -443601758;    int NnjwNhYTgu58333149 = -78719029;    int NnjwNhYTgu57395113 = -466053868;    int NnjwNhYTgu28904085 = -865529821;    int NnjwNhYTgu58024792 = -970406172;    int NnjwNhYTgu38159562 = -657922288;    int NnjwNhYTgu19062906 = -565418683;    int NnjwNhYTgu10608893 = 65909886;    int NnjwNhYTgu24282554 = -607582661;    int NnjwNhYTgu37858647 = -20200481;    int NnjwNhYTgu14334307 = 241754;    int NnjwNhYTgu2479953 = -238236275;    int NnjwNhYTgu76685426 = -928643492;    int NnjwNhYTgu45830828 = 18527819;    int NnjwNhYTgu87637135 = -901088455;    int NnjwNhYTgu55203953 = -842182080;    int NnjwNhYTgu18963733 = -232059136;    int NnjwNhYTgu85547693 = -263107623;    int NnjwNhYTgu95450498 = -478895495;    int NnjwNhYTgu80540450 = -586931478;    int NnjwNhYTgu16667017 = -521795283;    int NnjwNhYTgu64863965 = -261078373;    int NnjwNhYTgu73052319 = -936806636;    int NnjwNhYTgu93164458 = -953584095;    int NnjwNhYTgu76117442 = -114874467;    int NnjwNhYTgu46493535 = -440137154;    int NnjwNhYTgu69574892 = -154355733;    int NnjwNhYTgu1442897 = -562132286;    int NnjwNhYTgu83621992 = -945384535;    int NnjwNhYTgu64767156 = -519630812;    int NnjwNhYTgu73362825 = -381476728;    int NnjwNhYTgu66838742 = -497264691;    int NnjwNhYTgu37137373 = -741470683;    int NnjwNhYTgu16484006 = -231996434;    int NnjwNhYTgu79034078 = -894272994;    int NnjwNhYTgu46226004 = -793348687;    int NnjwNhYTgu34268993 = -937332687;    int NnjwNhYTgu97436811 = -608590945;    int NnjwNhYTgu78636397 = -369630304;    int NnjwNhYTgu29535063 = -108512844;    int NnjwNhYTgu77245266 = -38195810;    int NnjwNhYTgu43712264 = -921663396;    int NnjwNhYTgu39338077 = -312570597;    int NnjwNhYTgu57022737 = -280777256;    int NnjwNhYTgu35420758 = -211893423;    int NnjwNhYTgu15924238 = 8633189;    int NnjwNhYTgu43954236 = -300934795;    int NnjwNhYTgu15337307 = -427375957;    int NnjwNhYTgu53635693 = -740121611;     NnjwNhYTgu25157991 = NnjwNhYTgu68902588;     NnjwNhYTgu68902588 = NnjwNhYTgu61778631;     NnjwNhYTgu61778631 = NnjwNhYTgu73060146;     NnjwNhYTgu73060146 = NnjwNhYTgu60638337;     NnjwNhYTgu60638337 = NnjwNhYTgu43187854;     NnjwNhYTgu43187854 = NnjwNhYTgu39439882;     NnjwNhYTgu39439882 = NnjwNhYTgu59127263;     NnjwNhYTgu59127263 = NnjwNhYTgu71863885;     NnjwNhYTgu71863885 = NnjwNhYTgu6185830;     NnjwNhYTgu6185830 = NnjwNhYTgu92196287;     NnjwNhYTgu92196287 = NnjwNhYTgu46868059;     NnjwNhYTgu46868059 = NnjwNhYTgu45304629;     NnjwNhYTgu45304629 = NnjwNhYTgu22208884;     NnjwNhYTgu22208884 = NnjwNhYTgu63145123;     NnjwNhYTgu63145123 = NnjwNhYTgu76848622;     NnjwNhYTgu76848622 = NnjwNhYTgu70988174;     NnjwNhYTgu70988174 = NnjwNhYTgu28484596;     NnjwNhYTgu28484596 = NnjwNhYTgu67933703;     NnjwNhYTgu67933703 = NnjwNhYTgu42919523;     NnjwNhYTgu42919523 = NnjwNhYTgu94521631;     NnjwNhYTgu94521631 = NnjwNhYTgu67133198;     NnjwNhYTgu67133198 = NnjwNhYTgu30743026;     NnjwNhYTgu30743026 = NnjwNhYTgu42715725;     NnjwNhYTgu42715725 = NnjwNhYTgu62451253;     NnjwNhYTgu62451253 = NnjwNhYTgu36355783;     NnjwNhYTgu36355783 = NnjwNhYTgu5329208;     NnjwNhYTgu5329208 = NnjwNhYTgu25105575;     NnjwNhYTgu25105575 = NnjwNhYTgu56647310;     NnjwNhYTgu56647310 = NnjwNhYTgu95178458;     NnjwNhYTgu95178458 = NnjwNhYTgu60355002;     NnjwNhYTgu60355002 = NnjwNhYTgu4559153;     NnjwNhYTgu4559153 = NnjwNhYTgu91664106;     NnjwNhYTgu91664106 = NnjwNhYTgu26340897;     NnjwNhYTgu26340897 = NnjwNhYTgu36661190;     NnjwNhYTgu36661190 = NnjwNhYTgu67694625;     NnjwNhYTgu67694625 = NnjwNhYTgu96308172;     NnjwNhYTgu96308172 = NnjwNhYTgu54321157;     NnjwNhYTgu54321157 = NnjwNhYTgu63620630;     NnjwNhYTgu63620630 = NnjwNhYTgu94881383;     NnjwNhYTgu94881383 = NnjwNhYTgu49755065;     NnjwNhYTgu49755065 = NnjwNhYTgu18404190;     NnjwNhYTgu18404190 = NnjwNhYTgu20639663;     NnjwNhYTgu20639663 = NnjwNhYTgu61168134;     NnjwNhYTgu61168134 = NnjwNhYTgu41272829;     NnjwNhYTgu41272829 = NnjwNhYTgu78829261;     NnjwNhYTgu78829261 = NnjwNhYTgu71588627;     NnjwNhYTgu71588627 = NnjwNhYTgu31966382;     NnjwNhYTgu31966382 = NnjwNhYTgu58266832;     NnjwNhYTgu58266832 = NnjwNhYTgu19509938;     NnjwNhYTgu19509938 = NnjwNhYTgu78694453;     NnjwNhYTgu78694453 = NnjwNhYTgu81320923;     NnjwNhYTgu81320923 = NnjwNhYTgu58333149;     NnjwNhYTgu58333149 = NnjwNhYTgu57395113;     NnjwNhYTgu57395113 = NnjwNhYTgu28904085;     NnjwNhYTgu28904085 = NnjwNhYTgu58024792;     NnjwNhYTgu58024792 = NnjwNhYTgu38159562;     NnjwNhYTgu38159562 = NnjwNhYTgu19062906;     NnjwNhYTgu19062906 = NnjwNhYTgu10608893;     NnjwNhYTgu10608893 = NnjwNhYTgu24282554;     NnjwNhYTgu24282554 = NnjwNhYTgu37858647;     NnjwNhYTgu37858647 = NnjwNhYTgu14334307;     NnjwNhYTgu14334307 = NnjwNhYTgu2479953;     NnjwNhYTgu2479953 = NnjwNhYTgu76685426;     NnjwNhYTgu76685426 = NnjwNhYTgu45830828;     NnjwNhYTgu45830828 = NnjwNhYTgu87637135;     NnjwNhYTgu87637135 = NnjwNhYTgu55203953;     NnjwNhYTgu55203953 = NnjwNhYTgu18963733;     NnjwNhYTgu18963733 = NnjwNhYTgu85547693;     NnjwNhYTgu85547693 = NnjwNhYTgu95450498;     NnjwNhYTgu95450498 = NnjwNhYTgu80540450;     NnjwNhYTgu80540450 = NnjwNhYTgu16667017;     NnjwNhYTgu16667017 = NnjwNhYTgu64863965;     NnjwNhYTgu64863965 = NnjwNhYTgu73052319;     NnjwNhYTgu73052319 = NnjwNhYTgu93164458;     NnjwNhYTgu93164458 = NnjwNhYTgu76117442;     NnjwNhYTgu76117442 = NnjwNhYTgu46493535;     NnjwNhYTgu46493535 = NnjwNhYTgu69574892;     NnjwNhYTgu69574892 = NnjwNhYTgu1442897;     NnjwNhYTgu1442897 = NnjwNhYTgu83621992;     NnjwNhYTgu83621992 = NnjwNhYTgu64767156;     NnjwNhYTgu64767156 = NnjwNhYTgu73362825;     NnjwNhYTgu73362825 = NnjwNhYTgu66838742;     NnjwNhYTgu66838742 = NnjwNhYTgu37137373;     NnjwNhYTgu37137373 = NnjwNhYTgu16484006;     NnjwNhYTgu16484006 = NnjwNhYTgu79034078;     NnjwNhYTgu79034078 = NnjwNhYTgu46226004;     NnjwNhYTgu46226004 = NnjwNhYTgu34268993;     NnjwNhYTgu34268993 = NnjwNhYTgu97436811;     NnjwNhYTgu97436811 = NnjwNhYTgu78636397;     NnjwNhYTgu78636397 = NnjwNhYTgu29535063;     NnjwNhYTgu29535063 = NnjwNhYTgu77245266;     NnjwNhYTgu77245266 = NnjwNhYTgu43712264;     NnjwNhYTgu43712264 = NnjwNhYTgu39338077;     NnjwNhYTgu39338077 = NnjwNhYTgu57022737;     NnjwNhYTgu57022737 = NnjwNhYTgu35420758;     NnjwNhYTgu35420758 = NnjwNhYTgu15924238;     NnjwNhYTgu15924238 = NnjwNhYTgu43954236;     NnjwNhYTgu43954236 = NnjwNhYTgu15337307;     NnjwNhYTgu15337307 = NnjwNhYTgu53635693;     NnjwNhYTgu53635693 = NnjwNhYTgu25157991;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void dXKoOMsInt66287332() {     int IsRZJjvuSa12358881 = -350129202;    int IsRZJjvuSa59276333 = -335982395;    int IsRZJjvuSa46652255 = 4707727;    int IsRZJjvuSa35279042 = -125899708;    int IsRZJjvuSa84807436 = 83770182;    int IsRZJjvuSa69488648 = 60359487;    int IsRZJjvuSa55326795 = -782374332;    int IsRZJjvuSa70836251 = -125747254;    int IsRZJjvuSa41939403 = -858996840;    int IsRZJjvuSa93246151 = -988873042;    int IsRZJjvuSa5350116 = -677414298;    int IsRZJjvuSa43500908 = -309192544;    int IsRZJjvuSa38867154 = -945714193;    int IsRZJjvuSa80273245 = 38964998;    int IsRZJjvuSa30182788 = -363770498;    int IsRZJjvuSa97552648 = -405245882;    int IsRZJjvuSa26941875 = -453003466;    int IsRZJjvuSa90927176 = -420164572;    int IsRZJjvuSa7629613 = -564279709;    int IsRZJjvuSa84412552 = -890040526;    int IsRZJjvuSa73686423 = -574094036;    int IsRZJjvuSa94677910 = -519840858;    int IsRZJjvuSa65739524 = -650115660;    int IsRZJjvuSa85508812 = -325471778;    int IsRZJjvuSa13442374 = -917875993;    int IsRZJjvuSa88185292 = 10266363;    int IsRZJjvuSa34356835 = -165390869;    int IsRZJjvuSa28559700 = -663351095;    int IsRZJjvuSa65387151 = -801597991;    int IsRZJjvuSa94607083 = -85795894;    int IsRZJjvuSa82744363 = -835644471;    int IsRZJjvuSa70688043 = -676991560;    int IsRZJjvuSa16523004 = -931420202;    int IsRZJjvuSa49830198 = -322730013;    int IsRZJjvuSa18316734 = -248632444;    int IsRZJjvuSa58126889 = -137403519;    int IsRZJjvuSa83426847 = -13011899;    int IsRZJjvuSa36452493 = 73341224;    int IsRZJjvuSa28472438 = -403665512;    int IsRZJjvuSa12362440 = -875424262;    int IsRZJjvuSa64035092 = -112065472;    int IsRZJjvuSa82097055 = -818875916;    int IsRZJjvuSa6322914 = -224405350;    int IsRZJjvuSa35003393 = -381604782;    int IsRZJjvuSa29300443 = -215592302;    int IsRZJjvuSa86057068 = -980316578;    int IsRZJjvuSa49402193 = -809168361;    int IsRZJjvuSa53293577 = -778190928;    int IsRZJjvuSa51519562 = -185876280;    int IsRZJjvuSa12650363 = -745217229;    int IsRZJjvuSa86885699 = -37048574;    int IsRZJjvuSa82238890 = -736707917;    int IsRZJjvuSa74744695 = -786823763;    int IsRZJjvuSa33195930 = -840441828;    int IsRZJjvuSa92600909 = -384883890;    int IsRZJjvuSa17680970 = -830288344;    int IsRZJjvuSa93536809 = -685866735;    int IsRZJjvuSa61143443 = -669820496;    int IsRZJjvuSa21836669 = -208023716;    int IsRZJjvuSa96622144 = -926496181;    int IsRZJjvuSa35131814 = -774249645;    int IsRZJjvuSa26767096 = -19023237;    int IsRZJjvuSa5449100 = -324149263;    int IsRZJjvuSa47332320 = -673200947;    int IsRZJjvuSa10501789 = -53228571;    int IsRZJjvuSa34662072 = 99577261;    int IsRZJjvuSa26977905 = -377772342;    int IsRZJjvuSa89036956 = -522984181;    int IsRZJjvuSa61956511 = -712402558;    int IsRZJjvuSa72055899 = -126366980;    int IsRZJjvuSa14125801 = -292233984;    int IsRZJjvuSa90489382 = -426344690;    int IsRZJjvuSa62454739 = 83500940;    int IsRZJjvuSa95267172 = -688855447;    int IsRZJjvuSa20377461 = -677975055;    int IsRZJjvuSa91589367 = -755218120;    int IsRZJjvuSa88354997 = -195435509;    int IsRZJjvuSa30736132 = -168510879;    int IsRZJjvuSa56208369 = -9879476;    int IsRZJjvuSa27385305 = -937559415;    int IsRZJjvuSa38783099 = -180565277;    int IsRZJjvuSa81063257 = -387199941;    int IsRZJjvuSa77040138 = -377474816;    int IsRZJjvuSa52736788 = 43619237;    int IsRZJjvuSa7721385 = 51252679;    int IsRZJjvuSa505473 = 1063445;    int IsRZJjvuSa95943348 = -890167797;    int IsRZJjvuSa83327073 = 9021626;    int IsRZJjvuSa57229289 = -937846123;    int IsRZJjvuSa635764 = -418344101;    int IsRZJjvuSa64590080 = -451536784;    int IsRZJjvuSa22283405 = -343191403;    int IsRZJjvuSa14615825 = -718635061;    int IsRZJjvuSa31850293 = -477169332;    int IsRZJjvuSa77230626 = -1174618;    int IsRZJjvuSa37267997 = 6957765;    int IsRZJjvuSa76647955 = -394726654;    int IsRZJjvuSa58990593 = -551204404;    int IsRZJjvuSa24501604 = -228376211;    int IsRZJjvuSa94638370 = -350129202;     IsRZJjvuSa12358881 = IsRZJjvuSa59276333;     IsRZJjvuSa59276333 = IsRZJjvuSa46652255;     IsRZJjvuSa46652255 = IsRZJjvuSa35279042;     IsRZJjvuSa35279042 = IsRZJjvuSa84807436;     IsRZJjvuSa84807436 = IsRZJjvuSa69488648;     IsRZJjvuSa69488648 = IsRZJjvuSa55326795;     IsRZJjvuSa55326795 = IsRZJjvuSa70836251;     IsRZJjvuSa70836251 = IsRZJjvuSa41939403;     IsRZJjvuSa41939403 = IsRZJjvuSa93246151;     IsRZJjvuSa93246151 = IsRZJjvuSa5350116;     IsRZJjvuSa5350116 = IsRZJjvuSa43500908;     IsRZJjvuSa43500908 = IsRZJjvuSa38867154;     IsRZJjvuSa38867154 = IsRZJjvuSa80273245;     IsRZJjvuSa80273245 = IsRZJjvuSa30182788;     IsRZJjvuSa30182788 = IsRZJjvuSa97552648;     IsRZJjvuSa97552648 = IsRZJjvuSa26941875;     IsRZJjvuSa26941875 = IsRZJjvuSa90927176;     IsRZJjvuSa90927176 = IsRZJjvuSa7629613;     IsRZJjvuSa7629613 = IsRZJjvuSa84412552;     IsRZJjvuSa84412552 = IsRZJjvuSa73686423;     IsRZJjvuSa73686423 = IsRZJjvuSa94677910;     IsRZJjvuSa94677910 = IsRZJjvuSa65739524;     IsRZJjvuSa65739524 = IsRZJjvuSa85508812;     IsRZJjvuSa85508812 = IsRZJjvuSa13442374;     IsRZJjvuSa13442374 = IsRZJjvuSa88185292;     IsRZJjvuSa88185292 = IsRZJjvuSa34356835;     IsRZJjvuSa34356835 = IsRZJjvuSa28559700;     IsRZJjvuSa28559700 = IsRZJjvuSa65387151;     IsRZJjvuSa65387151 = IsRZJjvuSa94607083;     IsRZJjvuSa94607083 = IsRZJjvuSa82744363;     IsRZJjvuSa82744363 = IsRZJjvuSa70688043;     IsRZJjvuSa70688043 = IsRZJjvuSa16523004;     IsRZJjvuSa16523004 = IsRZJjvuSa49830198;     IsRZJjvuSa49830198 = IsRZJjvuSa18316734;     IsRZJjvuSa18316734 = IsRZJjvuSa58126889;     IsRZJjvuSa58126889 = IsRZJjvuSa83426847;     IsRZJjvuSa83426847 = IsRZJjvuSa36452493;     IsRZJjvuSa36452493 = IsRZJjvuSa28472438;     IsRZJjvuSa28472438 = IsRZJjvuSa12362440;     IsRZJjvuSa12362440 = IsRZJjvuSa64035092;     IsRZJjvuSa64035092 = IsRZJjvuSa82097055;     IsRZJjvuSa82097055 = IsRZJjvuSa6322914;     IsRZJjvuSa6322914 = IsRZJjvuSa35003393;     IsRZJjvuSa35003393 = IsRZJjvuSa29300443;     IsRZJjvuSa29300443 = IsRZJjvuSa86057068;     IsRZJjvuSa86057068 = IsRZJjvuSa49402193;     IsRZJjvuSa49402193 = IsRZJjvuSa53293577;     IsRZJjvuSa53293577 = IsRZJjvuSa51519562;     IsRZJjvuSa51519562 = IsRZJjvuSa12650363;     IsRZJjvuSa12650363 = IsRZJjvuSa86885699;     IsRZJjvuSa86885699 = IsRZJjvuSa82238890;     IsRZJjvuSa82238890 = IsRZJjvuSa74744695;     IsRZJjvuSa74744695 = IsRZJjvuSa33195930;     IsRZJjvuSa33195930 = IsRZJjvuSa92600909;     IsRZJjvuSa92600909 = IsRZJjvuSa17680970;     IsRZJjvuSa17680970 = IsRZJjvuSa93536809;     IsRZJjvuSa93536809 = IsRZJjvuSa61143443;     IsRZJjvuSa61143443 = IsRZJjvuSa21836669;     IsRZJjvuSa21836669 = IsRZJjvuSa96622144;     IsRZJjvuSa96622144 = IsRZJjvuSa35131814;     IsRZJjvuSa35131814 = IsRZJjvuSa26767096;     IsRZJjvuSa26767096 = IsRZJjvuSa5449100;     IsRZJjvuSa5449100 = IsRZJjvuSa47332320;     IsRZJjvuSa47332320 = IsRZJjvuSa10501789;     IsRZJjvuSa10501789 = IsRZJjvuSa34662072;     IsRZJjvuSa34662072 = IsRZJjvuSa26977905;     IsRZJjvuSa26977905 = IsRZJjvuSa89036956;     IsRZJjvuSa89036956 = IsRZJjvuSa61956511;     IsRZJjvuSa61956511 = IsRZJjvuSa72055899;     IsRZJjvuSa72055899 = IsRZJjvuSa14125801;     IsRZJjvuSa14125801 = IsRZJjvuSa90489382;     IsRZJjvuSa90489382 = IsRZJjvuSa62454739;     IsRZJjvuSa62454739 = IsRZJjvuSa95267172;     IsRZJjvuSa95267172 = IsRZJjvuSa20377461;     IsRZJjvuSa20377461 = IsRZJjvuSa91589367;     IsRZJjvuSa91589367 = IsRZJjvuSa88354997;     IsRZJjvuSa88354997 = IsRZJjvuSa30736132;     IsRZJjvuSa30736132 = IsRZJjvuSa56208369;     IsRZJjvuSa56208369 = IsRZJjvuSa27385305;     IsRZJjvuSa27385305 = IsRZJjvuSa38783099;     IsRZJjvuSa38783099 = IsRZJjvuSa81063257;     IsRZJjvuSa81063257 = IsRZJjvuSa77040138;     IsRZJjvuSa77040138 = IsRZJjvuSa52736788;     IsRZJjvuSa52736788 = IsRZJjvuSa7721385;     IsRZJjvuSa7721385 = IsRZJjvuSa505473;     IsRZJjvuSa505473 = IsRZJjvuSa95943348;     IsRZJjvuSa95943348 = IsRZJjvuSa83327073;     IsRZJjvuSa83327073 = IsRZJjvuSa57229289;     IsRZJjvuSa57229289 = IsRZJjvuSa635764;     IsRZJjvuSa635764 = IsRZJjvuSa64590080;     IsRZJjvuSa64590080 = IsRZJjvuSa22283405;     IsRZJjvuSa22283405 = IsRZJjvuSa14615825;     IsRZJjvuSa14615825 = IsRZJjvuSa31850293;     IsRZJjvuSa31850293 = IsRZJjvuSa77230626;     IsRZJjvuSa77230626 = IsRZJjvuSa37267997;     IsRZJjvuSa37267997 = IsRZJjvuSa76647955;     IsRZJjvuSa76647955 = IsRZJjvuSa58990593;     IsRZJjvuSa58990593 = IsRZJjvuSa24501604;     IsRZJjvuSa24501604 = IsRZJjvuSa94638370;     IsRZJjvuSa94638370 = IsRZJjvuSa12358881;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void UxqsOOsHtA56906417() {     int xejPTzILBd70217909 = -705761782;    int xejPTzILBd79751279 = -79886805;    int xejPTzILBd10181587 = 13644035;    int xejPTzILBd52349364 = -312376651;    int xejPTzILBd21586756 = -776175022;    int xejPTzILBd50983772 = -663731163;    int xejPTzILBd21045269 = 59985562;    int xejPTzILBd70173569 = -832223404;    int xejPTzILBd75139811 = -704232146;    int xejPTzILBd6633422 = -22687631;    int xejPTzILBd40432500 = -557102284;    int xejPTzILBd71388651 = -953101883;    int xejPTzILBd80404152 = -973127069;    int xejPTzILBd68340420 = -874801066;    int xejPTzILBd34768363 = -569247523;    int xejPTzILBd76220767 = -857133730;    int xejPTzILBd20895644 = -518028699;    int xejPTzILBd50662719 = -151192087;    int xejPTzILBd8605034 = -879980722;    int xejPTzILBd12618529 = -345187143;    int xejPTzILBd28317545 = -801923285;    int xejPTzILBd5322122 = -91259699;    int xejPTzILBd65801433 = -645912752;    int xejPTzILBd18501328 = -538448194;    int xejPTzILBd115193 = -810032610;    int xejPTzILBd54038304 = -305792811;    int xejPTzILBd1423666 = -227662077;    int xejPTzILBd4392525 = -503643746;    int xejPTzILBd93591032 = -427809283;    int xejPTzILBd57117933 = -689736184;    int xejPTzILBd78793042 = -694754641;    int xejPTzILBd96604537 = -318390806;    int xejPTzILBd38874839 = -104471242;    int xejPTzILBd38401637 = -958187211;    int xejPTzILBd95638476 = -844861268;    int xejPTzILBd65724223 = -507832709;    int xejPTzILBd2395641 = -511349134;    int xejPTzILBd1458639 = -886414471;    int xejPTzILBd36543937 = -130224988;    int xejPTzILBd53073901 = -158047465;    int xejPTzILBd50532735 = -208554256;    int xejPTzILBd88575251 = -883745734;    int xejPTzILBd80689763 = -746856212;    int xejPTzILBd56115053 = -769371954;    int xejPTzILBd29264064 = -4728138;    int xejPTzILBd59262742 = -477379964;    int xejPTzILBd25649169 = -909003730;    int xejPTzILBd64851402 = -583729800;    int xejPTzILBd31555354 = -671802135;    int xejPTzILBd94172851 = -4214531;    int xejPTzILBd50006900 = -9291393;    int xejPTzILBd10481106 = -616888049;    int xejPTzILBd70783550 = -430672531;    int xejPTzILBd88973780 = -243791434;    int xejPTzILBd28588872 = -258279596;    int xejPTzILBd64895787 = -514502083;    int xejPTzILBd13949846 = -433974054;    int xejPTzILBd91680258 = -447907772;    int xejPTzILBd52234171 = -502344041;    int xejPTzILBd67548451 = -370382211;    int xejPTzILBd49560107 = -336069087;    int xejPTzILBd16652744 = -436370693;    int xejPTzILBd76582536 = -304414122;    int xejPTzILBd18021878 = 85504037;    int xejPTzILBd27840379 = -327932990;    int xejPTzILBd43827962 = -138711479;    int xejPTzILBd32513813 = -748630642;    int xejPTzILBd42002515 = 85060142;    int xejPTzILBd72701944 = 70060202;    int xejPTzILBd69044140 = 38585186;    int xejPTzILBd73825126 = -245784597;    int xejPTzILBd19437006 = -631614228;    int xejPTzILBd14118782 = 79032900;    int xejPTzILBd55531132 = -621933257;    int xejPTzILBd62085793 = -36632888;    int xejPTzILBd39742293 = -918177552;    int xejPTzILBd24632359 = -344403488;    int xejPTzILBd9686381 = -876540799;    int xejPTzILBd89237264 = -433720057;    int xejPTzILBd40852450 = -232652646;    int xejPTzILBd28389135 = -396789082;    int xejPTzILBd36572264 = -643932277;    int xejPTzILBd72837171 = -831841611;    int xejPTzILBd99418181 = -323594753;    int xejPTzILBd7111034 = -580444791;    int xejPTzILBd68311937 = 22133407;    int xejPTzILBd25820988 = -887718276;    int xejPTzILBd49901058 = -860679809;    int xejPTzILBd9812766 = -599907616;    int xejPTzILBd30742689 = -230359185;    int xejPTzILBd51774378 = 26141344;    int xejPTzILBd10715382 = 36558637;    int xejPTzILBd49224467 = -284070430;    int xejPTzILBd68995486 = -759842778;    int xejPTzILBd3513795 = -821978379;    int xejPTzILBd33879992 = -772183563;    int xejPTzILBd11992715 = -479331613;    int xejPTzILBd62667886 = -732360249;    int xejPTzILBd28274675 = -341438964;    int xejPTzILBd85436101 = -705761782;     xejPTzILBd70217909 = xejPTzILBd79751279;     xejPTzILBd79751279 = xejPTzILBd10181587;     xejPTzILBd10181587 = xejPTzILBd52349364;     xejPTzILBd52349364 = xejPTzILBd21586756;     xejPTzILBd21586756 = xejPTzILBd50983772;     xejPTzILBd50983772 = xejPTzILBd21045269;     xejPTzILBd21045269 = xejPTzILBd70173569;     xejPTzILBd70173569 = xejPTzILBd75139811;     xejPTzILBd75139811 = xejPTzILBd6633422;     xejPTzILBd6633422 = xejPTzILBd40432500;     xejPTzILBd40432500 = xejPTzILBd71388651;     xejPTzILBd71388651 = xejPTzILBd80404152;     xejPTzILBd80404152 = xejPTzILBd68340420;     xejPTzILBd68340420 = xejPTzILBd34768363;     xejPTzILBd34768363 = xejPTzILBd76220767;     xejPTzILBd76220767 = xejPTzILBd20895644;     xejPTzILBd20895644 = xejPTzILBd50662719;     xejPTzILBd50662719 = xejPTzILBd8605034;     xejPTzILBd8605034 = xejPTzILBd12618529;     xejPTzILBd12618529 = xejPTzILBd28317545;     xejPTzILBd28317545 = xejPTzILBd5322122;     xejPTzILBd5322122 = xejPTzILBd65801433;     xejPTzILBd65801433 = xejPTzILBd18501328;     xejPTzILBd18501328 = xejPTzILBd115193;     xejPTzILBd115193 = xejPTzILBd54038304;     xejPTzILBd54038304 = xejPTzILBd1423666;     xejPTzILBd1423666 = xejPTzILBd4392525;     xejPTzILBd4392525 = xejPTzILBd93591032;     xejPTzILBd93591032 = xejPTzILBd57117933;     xejPTzILBd57117933 = xejPTzILBd78793042;     xejPTzILBd78793042 = xejPTzILBd96604537;     xejPTzILBd96604537 = xejPTzILBd38874839;     xejPTzILBd38874839 = xejPTzILBd38401637;     xejPTzILBd38401637 = xejPTzILBd95638476;     xejPTzILBd95638476 = xejPTzILBd65724223;     xejPTzILBd65724223 = xejPTzILBd2395641;     xejPTzILBd2395641 = xejPTzILBd1458639;     xejPTzILBd1458639 = xejPTzILBd36543937;     xejPTzILBd36543937 = xejPTzILBd53073901;     xejPTzILBd53073901 = xejPTzILBd50532735;     xejPTzILBd50532735 = xejPTzILBd88575251;     xejPTzILBd88575251 = xejPTzILBd80689763;     xejPTzILBd80689763 = xejPTzILBd56115053;     xejPTzILBd56115053 = xejPTzILBd29264064;     xejPTzILBd29264064 = xejPTzILBd59262742;     xejPTzILBd59262742 = xejPTzILBd25649169;     xejPTzILBd25649169 = xejPTzILBd64851402;     xejPTzILBd64851402 = xejPTzILBd31555354;     xejPTzILBd31555354 = xejPTzILBd94172851;     xejPTzILBd94172851 = xejPTzILBd50006900;     xejPTzILBd50006900 = xejPTzILBd10481106;     xejPTzILBd10481106 = xejPTzILBd70783550;     xejPTzILBd70783550 = xejPTzILBd88973780;     xejPTzILBd88973780 = xejPTzILBd28588872;     xejPTzILBd28588872 = xejPTzILBd64895787;     xejPTzILBd64895787 = xejPTzILBd13949846;     xejPTzILBd13949846 = xejPTzILBd91680258;     xejPTzILBd91680258 = xejPTzILBd52234171;     xejPTzILBd52234171 = xejPTzILBd67548451;     xejPTzILBd67548451 = xejPTzILBd49560107;     xejPTzILBd49560107 = xejPTzILBd16652744;     xejPTzILBd16652744 = xejPTzILBd76582536;     xejPTzILBd76582536 = xejPTzILBd18021878;     xejPTzILBd18021878 = xejPTzILBd27840379;     xejPTzILBd27840379 = xejPTzILBd43827962;     xejPTzILBd43827962 = xejPTzILBd32513813;     xejPTzILBd32513813 = xejPTzILBd42002515;     xejPTzILBd42002515 = xejPTzILBd72701944;     xejPTzILBd72701944 = xejPTzILBd69044140;     xejPTzILBd69044140 = xejPTzILBd73825126;     xejPTzILBd73825126 = xejPTzILBd19437006;     xejPTzILBd19437006 = xejPTzILBd14118782;     xejPTzILBd14118782 = xejPTzILBd55531132;     xejPTzILBd55531132 = xejPTzILBd62085793;     xejPTzILBd62085793 = xejPTzILBd39742293;     xejPTzILBd39742293 = xejPTzILBd24632359;     xejPTzILBd24632359 = xejPTzILBd9686381;     xejPTzILBd9686381 = xejPTzILBd89237264;     xejPTzILBd89237264 = xejPTzILBd40852450;     xejPTzILBd40852450 = xejPTzILBd28389135;     xejPTzILBd28389135 = xejPTzILBd36572264;     xejPTzILBd36572264 = xejPTzILBd72837171;     xejPTzILBd72837171 = xejPTzILBd99418181;     xejPTzILBd99418181 = xejPTzILBd7111034;     xejPTzILBd7111034 = xejPTzILBd68311937;     xejPTzILBd68311937 = xejPTzILBd25820988;     xejPTzILBd25820988 = xejPTzILBd49901058;     xejPTzILBd49901058 = xejPTzILBd9812766;     xejPTzILBd9812766 = xejPTzILBd30742689;     xejPTzILBd30742689 = xejPTzILBd51774378;     xejPTzILBd51774378 = xejPTzILBd10715382;     xejPTzILBd10715382 = xejPTzILBd49224467;     xejPTzILBd49224467 = xejPTzILBd68995486;     xejPTzILBd68995486 = xejPTzILBd3513795;     xejPTzILBd3513795 = xejPTzILBd33879992;     xejPTzILBd33879992 = xejPTzILBd11992715;     xejPTzILBd11992715 = xejPTzILBd62667886;     xejPTzILBd62667886 = xejPTzILBd28274675;     xejPTzILBd28274675 = xejPTzILBd85436101;     xejPTzILBd85436101 = xejPTzILBd70217909;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void kjXjNjgoRw99768033() {     int rVrVNmwjju57418799 = -315769373;    int rVrVNmwjju70125024 = 5256958;    int rVrVNmwjju95055210 = -592253811;    int rVrVNmwjju14568260 = -129256034;    int rVrVNmwjju45755855 = -121069200;    int rVrVNmwjju77284566 = -285673473;    int rVrVNmwjju36932182 = 96584699;    int rVrVNmwjju81882558 = -896059782;    int rVrVNmwjju45215329 = -169766194;    int rVrVNmwjju93693743 = -592213740;    int rVrVNmwjju53586328 = -361360413;    int rVrVNmwjju68021500 = 83274207;    int rVrVNmwjju73966677 = -12661361;    int rVrVNmwjju26404782 = -132691970;    int rVrVNmwjju1806029 = -587687396;    int rVrVNmwjju96924792 = -971833642;    int rVrVNmwjju76849345 = -493483371;    int rVrVNmwjju13105300 = -290125029;    int rVrVNmwjju48300943 = -6476060;    int rVrVNmwjju54111558 = -969991907;    int rVrVNmwjju7482337 = -831539769;    int rVrVNmwjju32866834 = -941385118;    int rVrVNmwjju797932 = -532824542;    int rVrVNmwjju61294415 = 60055773;    int rVrVNmwjju51106312 = -352978392;    int rVrVNmwjju5867813 = -431773469;    int rVrVNmwjju30451293 = -195555222;    int rVrVNmwjju7846650 = -447779618;    int rVrVNmwjju2330874 = -405732673;    int rVrVNmwjju56546558 = -410712777;    int rVrVNmwjju1182405 = -92524360;    int rVrVNmwjju62733429 = -23314651;    int rVrVNmwjju63733736 = -632504890;    int rVrVNmwjju61890939 = -806796458;    int rVrVNmwjju77294020 = -753457237;    int rVrVNmwjju56156487 = -878801097;    int rVrVNmwjju89514315 = -920746540;    int rVrVNmwjju83589974 = -957319738;    int rVrVNmwjju1395745 = -613737243;    int rVrVNmwjju70554957 = -632493992;    int rVrVNmwjju64812762 = -8968059;    int rVrVNmwjju52268118 = -273018564;    int rVrVNmwjju66373014 = -741683275;    int rVrVNmwjju29950312 = -642128598;    int rVrVNmwjju17291678 = 41523019;    int rVrVNmwjju66490549 = -28150866;    int rVrVNmwjju3462735 = -274049922;    int rVrVNmwjju86178597 = -545899733;    int rVrVNmwjju24808084 = -735727883;    int rVrVNmwjju87313277 = -767227841;    int rVrVNmwjju58198146 = -13517100;    int rVrVNmwjju11399073 = -909994207;    int rVrVNmwjju87195096 = -38777265;    int rVrVNmwjju64774596 = -618179395;    int rVrVNmwjju92285696 = -877633665;    int rVrVNmwjju24551965 = -374384256;    int rVrVNmwjju69327092 = -461918501;    int rVrVNmwjju33760796 = -552309584;    int rVrVNmwjju63461947 = -776277643;    int rVrVNmwjju39888043 = -689295731;    int rVrVNmwjju46833273 = 9881748;    int rVrVNmwjju29085533 = -455635684;    int rVrVNmwjju79551684 = -390327110;    int rVrVNmwjju88668770 = -759053418;    int rVrVNmwjju92511339 = -399689380;    int rVrVNmwjju90852898 = -238045763;    int rVrVNmwjju4287765 = -284220903;    int rVrVNmwjju12075739 = -205864903;    int rVrVNmwjju49110762 = -379234734;    int rVrVNmwjju45649541 = -708886299;    int rVrVNmwjju7410478 = 48912897;    int rVrVNmwjju93259370 = -536163634;    int rVrVNmwjju11709555 = -676387786;    int rVrVNmwjju77745986 = -373982069;    int rVrVNmwjju89298795 = -861023848;    int rVrVNmwjju55214219 = -458521205;    int rVrVNmwjju66493820 = -99701843;    int rVrVNmwjju70847619 = -890695944;    int rVrVNmwjju44002737 = -981467246;    int rVrVNmwjju84615762 = -224827526;    int rVrVNmwjju2405079 = -57723548;    int rVrVNmwjju44272696 = -649655490;    int rVrVNmwjju83038566 = -712051735;    int rVrVNmwjju15017597 = -638504833;    int rVrVNmwjju98348412 = -297195678;    int rVrVNmwjju89783331 = -182530153;    int rVrVNmwjju75538332 = -984537386;    int rVrVNmwjju98959139 = 85674505;    int rVrVNmwjju69605242 = -929162794;    int rVrVNmwjju52742055 = -279072981;    int rVrVNmwjju86829394 = -316882596;    int rVrVNmwjju55753520 = -268436956;    int rVrVNmwjju20128027 = -81042095;    int rVrVNmwjju61507702 = -924441513;    int rVrVNmwjju23721684 = -542375741;    int rVrVNmwjju35727230 = -553332376;    int rVrVNmwjju72716433 = -882691455;    int rVrVNmwjju77704243 = -982629858;    int rVrVNmwjju37438973 = -142439218;    int rVrVNmwjju26438779 = -315769373;     rVrVNmwjju57418799 = rVrVNmwjju70125024;     rVrVNmwjju70125024 = rVrVNmwjju95055210;     rVrVNmwjju95055210 = rVrVNmwjju14568260;     rVrVNmwjju14568260 = rVrVNmwjju45755855;     rVrVNmwjju45755855 = rVrVNmwjju77284566;     rVrVNmwjju77284566 = rVrVNmwjju36932182;     rVrVNmwjju36932182 = rVrVNmwjju81882558;     rVrVNmwjju81882558 = rVrVNmwjju45215329;     rVrVNmwjju45215329 = rVrVNmwjju93693743;     rVrVNmwjju93693743 = rVrVNmwjju53586328;     rVrVNmwjju53586328 = rVrVNmwjju68021500;     rVrVNmwjju68021500 = rVrVNmwjju73966677;     rVrVNmwjju73966677 = rVrVNmwjju26404782;     rVrVNmwjju26404782 = rVrVNmwjju1806029;     rVrVNmwjju1806029 = rVrVNmwjju96924792;     rVrVNmwjju96924792 = rVrVNmwjju76849345;     rVrVNmwjju76849345 = rVrVNmwjju13105300;     rVrVNmwjju13105300 = rVrVNmwjju48300943;     rVrVNmwjju48300943 = rVrVNmwjju54111558;     rVrVNmwjju54111558 = rVrVNmwjju7482337;     rVrVNmwjju7482337 = rVrVNmwjju32866834;     rVrVNmwjju32866834 = rVrVNmwjju797932;     rVrVNmwjju797932 = rVrVNmwjju61294415;     rVrVNmwjju61294415 = rVrVNmwjju51106312;     rVrVNmwjju51106312 = rVrVNmwjju5867813;     rVrVNmwjju5867813 = rVrVNmwjju30451293;     rVrVNmwjju30451293 = rVrVNmwjju7846650;     rVrVNmwjju7846650 = rVrVNmwjju2330874;     rVrVNmwjju2330874 = rVrVNmwjju56546558;     rVrVNmwjju56546558 = rVrVNmwjju1182405;     rVrVNmwjju1182405 = rVrVNmwjju62733429;     rVrVNmwjju62733429 = rVrVNmwjju63733736;     rVrVNmwjju63733736 = rVrVNmwjju61890939;     rVrVNmwjju61890939 = rVrVNmwjju77294020;     rVrVNmwjju77294020 = rVrVNmwjju56156487;     rVrVNmwjju56156487 = rVrVNmwjju89514315;     rVrVNmwjju89514315 = rVrVNmwjju83589974;     rVrVNmwjju83589974 = rVrVNmwjju1395745;     rVrVNmwjju1395745 = rVrVNmwjju70554957;     rVrVNmwjju70554957 = rVrVNmwjju64812762;     rVrVNmwjju64812762 = rVrVNmwjju52268118;     rVrVNmwjju52268118 = rVrVNmwjju66373014;     rVrVNmwjju66373014 = rVrVNmwjju29950312;     rVrVNmwjju29950312 = rVrVNmwjju17291678;     rVrVNmwjju17291678 = rVrVNmwjju66490549;     rVrVNmwjju66490549 = rVrVNmwjju3462735;     rVrVNmwjju3462735 = rVrVNmwjju86178597;     rVrVNmwjju86178597 = rVrVNmwjju24808084;     rVrVNmwjju24808084 = rVrVNmwjju87313277;     rVrVNmwjju87313277 = rVrVNmwjju58198146;     rVrVNmwjju58198146 = rVrVNmwjju11399073;     rVrVNmwjju11399073 = rVrVNmwjju87195096;     rVrVNmwjju87195096 = rVrVNmwjju64774596;     rVrVNmwjju64774596 = rVrVNmwjju92285696;     rVrVNmwjju92285696 = rVrVNmwjju24551965;     rVrVNmwjju24551965 = rVrVNmwjju69327092;     rVrVNmwjju69327092 = rVrVNmwjju33760796;     rVrVNmwjju33760796 = rVrVNmwjju63461947;     rVrVNmwjju63461947 = rVrVNmwjju39888043;     rVrVNmwjju39888043 = rVrVNmwjju46833273;     rVrVNmwjju46833273 = rVrVNmwjju29085533;     rVrVNmwjju29085533 = rVrVNmwjju79551684;     rVrVNmwjju79551684 = rVrVNmwjju88668770;     rVrVNmwjju88668770 = rVrVNmwjju92511339;     rVrVNmwjju92511339 = rVrVNmwjju90852898;     rVrVNmwjju90852898 = rVrVNmwjju4287765;     rVrVNmwjju4287765 = rVrVNmwjju12075739;     rVrVNmwjju12075739 = rVrVNmwjju49110762;     rVrVNmwjju49110762 = rVrVNmwjju45649541;     rVrVNmwjju45649541 = rVrVNmwjju7410478;     rVrVNmwjju7410478 = rVrVNmwjju93259370;     rVrVNmwjju93259370 = rVrVNmwjju11709555;     rVrVNmwjju11709555 = rVrVNmwjju77745986;     rVrVNmwjju77745986 = rVrVNmwjju89298795;     rVrVNmwjju89298795 = rVrVNmwjju55214219;     rVrVNmwjju55214219 = rVrVNmwjju66493820;     rVrVNmwjju66493820 = rVrVNmwjju70847619;     rVrVNmwjju70847619 = rVrVNmwjju44002737;     rVrVNmwjju44002737 = rVrVNmwjju84615762;     rVrVNmwjju84615762 = rVrVNmwjju2405079;     rVrVNmwjju2405079 = rVrVNmwjju44272696;     rVrVNmwjju44272696 = rVrVNmwjju83038566;     rVrVNmwjju83038566 = rVrVNmwjju15017597;     rVrVNmwjju15017597 = rVrVNmwjju98348412;     rVrVNmwjju98348412 = rVrVNmwjju89783331;     rVrVNmwjju89783331 = rVrVNmwjju75538332;     rVrVNmwjju75538332 = rVrVNmwjju98959139;     rVrVNmwjju98959139 = rVrVNmwjju69605242;     rVrVNmwjju69605242 = rVrVNmwjju52742055;     rVrVNmwjju52742055 = rVrVNmwjju86829394;     rVrVNmwjju86829394 = rVrVNmwjju55753520;     rVrVNmwjju55753520 = rVrVNmwjju20128027;     rVrVNmwjju20128027 = rVrVNmwjju61507702;     rVrVNmwjju61507702 = rVrVNmwjju23721684;     rVrVNmwjju23721684 = rVrVNmwjju35727230;     rVrVNmwjju35727230 = rVrVNmwjju72716433;     rVrVNmwjju72716433 = rVrVNmwjju77704243;     rVrVNmwjju77704243 = rVrVNmwjju37438973;     rVrVNmwjju37438973 = rVrVNmwjju26438779;     rVrVNmwjju26438779 = rVrVNmwjju57418799;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void UsgTymldWQ90387118() {     int CqzaRAZxnL15277828 = -671401953;    int CqzaRAZxnL90599969 = -838647453;    int CqzaRAZxnL58584542 = -583317503;    int CqzaRAZxnL31638582 = -315732977;    int CqzaRAZxnL82535174 = -981014403;    int CqzaRAZxnL58779690 = 90235877;    int CqzaRAZxnL2650656 = -161055408;    int CqzaRAZxnL81219876 = -502535932;    int CqzaRAZxnL78415736 = -15001500;    int CqzaRAZxnL7081014 = -726028329;    int CqzaRAZxnL88668711 = -241048399;    int CqzaRAZxnL95909243 = -560635132;    int CqzaRAZxnL15503676 = -40074236;    int CqzaRAZxnL14471957 = 53541967;    int CqzaRAZxnL6391604 = -793164420;    int CqzaRAZxnL75592911 = -323721491;    int CqzaRAZxnL70803113 = -558508604;    int CqzaRAZxnL72840841 = -21152544;    int CqzaRAZxnL49276364 = -322177074;    int CqzaRAZxnL82317534 = -425138524;    int CqzaRAZxnL62113458 = 40630982;    int CqzaRAZxnL43511046 = -512803958;    int CqzaRAZxnL859841 = -528621634;    int CqzaRAZxnL94286930 = -152920643;    int CqzaRAZxnL37779131 = -245135008;    int CqzaRAZxnL71720825 = -747832643;    int CqzaRAZxnL97518123 = -257826430;    int CqzaRAZxnL83679475 = -288072269;    int CqzaRAZxnL30534755 = -31943965;    int CqzaRAZxnL19057409 = 85346934;    int CqzaRAZxnL97231083 = 48365470;    int CqzaRAZxnL88649923 = -764713897;    int CqzaRAZxnL86085570 = -905555930;    int CqzaRAZxnL50462378 = -342253656;    int CqzaRAZxnL54615762 = -249686061;    int CqzaRAZxnL63753821 = -149230287;    int CqzaRAZxnL8483109 = -319083775;    int CqzaRAZxnL48596120 = -817075433;    int CqzaRAZxnL9467244 = -340296719;    int CqzaRAZxnL11266419 = 84882805;    int CqzaRAZxnL51310406 = -105456843;    int CqzaRAZxnL58746314 = -337888381;    int CqzaRAZxnL40739864 = -164134136;    int CqzaRAZxnL51061973 = 70104230;    int CqzaRAZxnL17255299 = -847612817;    int CqzaRAZxnL39696223 = -625214252;    int CqzaRAZxnL79709710 = -373885291;    int CqzaRAZxnL97736421 = -351438605;    int CqzaRAZxnL4843876 = -121653739;    int CqzaRAZxnL68835766 = -26225143;    int CqzaRAZxnL21319347 = 14240081;    int CqzaRAZxnL39641288 = -790174340;    int CqzaRAZxnL83233952 = -782626032;    int CqzaRAZxnL20552447 = -21529001;    int CqzaRAZxnL28273660 = -751029370;    int CqzaRAZxnL71766782 = -58597995;    int CqzaRAZxnL89740128 = -210025820;    int CqzaRAZxnL64297611 = -330396860;    int CqzaRAZxnL93859450 = 29402031;    int CqzaRAZxnL10814350 = -133181761;    int CqzaRAZxnL61261567 = -651937693;    int CqzaRAZxnL18971181 = -872983139;    int CqzaRAZxnL50685121 = -370591968;    int CqzaRAZxnL59358328 = -348434;    int CqzaRAZxnL9849930 = -674393799;    int CqzaRAZxnL18789 = -476334503;    int CqzaRAZxnL9823674 = -655079203;    int CqzaRAZxnL65041297 = -697820581;    int CqzaRAZxnL59856195 = -696771973;    int CqzaRAZxnL42637783 = -543934133;    int CqzaRAZxnL67109803 = 95362284;    int CqzaRAZxnL22206994 = -741433172;    int CqzaRAZxnL63373597 = -680855826;    int CqzaRAZxnL38009946 = -307059879;    int CqzaRAZxnL31007129 = -219681682;    int CqzaRAZxnL3367145 = -621480637;    int CqzaRAZxnL2771182 = -248669823;    int CqzaRAZxnL49797868 = -498725864;    int CqzaRAZxnL77031632 = -305307827;    int CqzaRAZxnL98082907 = -619920756;    int CqzaRAZxnL92011114 = -273947353;    int CqzaRAZxnL99781702 = -906387826;    int CqzaRAZxnL78835599 = -66418530;    int CqzaRAZxnL61698989 = 94281178;    int CqzaRAZxnL97738062 = -928893147;    int CqzaRAZxnL57589795 = -161460191;    int CqzaRAZxnL5415972 = -982087865;    int CqzaRAZxnL65533124 = -784026930;    int CqzaRAZxnL22188719 = -591224287;    int CqzaRAZxnL82848980 = -91088066;    int CqzaRAZxnL74013692 = -939204468;    int CqzaRAZxnL44185498 = -988686916;    int CqzaRAZxnL54736669 = -746477464;    int CqzaRAZxnL98652894 = -107114959;    int CqzaRAZxnL50004852 = -263179502;    int CqzaRAZxnL32339225 = -232473704;    int CqzaRAZxnL8061193 = -967296414;    int CqzaRAZxnL81381536 = -63785702;    int CqzaRAZxnL41212043 = -255501971;    int CqzaRAZxnL17236511 = -671401953;     CqzaRAZxnL15277828 = CqzaRAZxnL90599969;     CqzaRAZxnL90599969 = CqzaRAZxnL58584542;     CqzaRAZxnL58584542 = CqzaRAZxnL31638582;     CqzaRAZxnL31638582 = CqzaRAZxnL82535174;     CqzaRAZxnL82535174 = CqzaRAZxnL58779690;     CqzaRAZxnL58779690 = CqzaRAZxnL2650656;     CqzaRAZxnL2650656 = CqzaRAZxnL81219876;     CqzaRAZxnL81219876 = CqzaRAZxnL78415736;     CqzaRAZxnL78415736 = CqzaRAZxnL7081014;     CqzaRAZxnL7081014 = CqzaRAZxnL88668711;     CqzaRAZxnL88668711 = CqzaRAZxnL95909243;     CqzaRAZxnL95909243 = CqzaRAZxnL15503676;     CqzaRAZxnL15503676 = CqzaRAZxnL14471957;     CqzaRAZxnL14471957 = CqzaRAZxnL6391604;     CqzaRAZxnL6391604 = CqzaRAZxnL75592911;     CqzaRAZxnL75592911 = CqzaRAZxnL70803113;     CqzaRAZxnL70803113 = CqzaRAZxnL72840841;     CqzaRAZxnL72840841 = CqzaRAZxnL49276364;     CqzaRAZxnL49276364 = CqzaRAZxnL82317534;     CqzaRAZxnL82317534 = CqzaRAZxnL62113458;     CqzaRAZxnL62113458 = CqzaRAZxnL43511046;     CqzaRAZxnL43511046 = CqzaRAZxnL859841;     CqzaRAZxnL859841 = CqzaRAZxnL94286930;     CqzaRAZxnL94286930 = CqzaRAZxnL37779131;     CqzaRAZxnL37779131 = CqzaRAZxnL71720825;     CqzaRAZxnL71720825 = CqzaRAZxnL97518123;     CqzaRAZxnL97518123 = CqzaRAZxnL83679475;     CqzaRAZxnL83679475 = CqzaRAZxnL30534755;     CqzaRAZxnL30534755 = CqzaRAZxnL19057409;     CqzaRAZxnL19057409 = CqzaRAZxnL97231083;     CqzaRAZxnL97231083 = CqzaRAZxnL88649923;     CqzaRAZxnL88649923 = CqzaRAZxnL86085570;     CqzaRAZxnL86085570 = CqzaRAZxnL50462378;     CqzaRAZxnL50462378 = CqzaRAZxnL54615762;     CqzaRAZxnL54615762 = CqzaRAZxnL63753821;     CqzaRAZxnL63753821 = CqzaRAZxnL8483109;     CqzaRAZxnL8483109 = CqzaRAZxnL48596120;     CqzaRAZxnL48596120 = CqzaRAZxnL9467244;     CqzaRAZxnL9467244 = CqzaRAZxnL11266419;     CqzaRAZxnL11266419 = CqzaRAZxnL51310406;     CqzaRAZxnL51310406 = CqzaRAZxnL58746314;     CqzaRAZxnL58746314 = CqzaRAZxnL40739864;     CqzaRAZxnL40739864 = CqzaRAZxnL51061973;     CqzaRAZxnL51061973 = CqzaRAZxnL17255299;     CqzaRAZxnL17255299 = CqzaRAZxnL39696223;     CqzaRAZxnL39696223 = CqzaRAZxnL79709710;     CqzaRAZxnL79709710 = CqzaRAZxnL97736421;     CqzaRAZxnL97736421 = CqzaRAZxnL4843876;     CqzaRAZxnL4843876 = CqzaRAZxnL68835766;     CqzaRAZxnL68835766 = CqzaRAZxnL21319347;     CqzaRAZxnL21319347 = CqzaRAZxnL39641288;     CqzaRAZxnL39641288 = CqzaRAZxnL83233952;     CqzaRAZxnL83233952 = CqzaRAZxnL20552447;     CqzaRAZxnL20552447 = CqzaRAZxnL28273660;     CqzaRAZxnL28273660 = CqzaRAZxnL71766782;     CqzaRAZxnL71766782 = CqzaRAZxnL89740128;     CqzaRAZxnL89740128 = CqzaRAZxnL64297611;     CqzaRAZxnL64297611 = CqzaRAZxnL93859450;     CqzaRAZxnL93859450 = CqzaRAZxnL10814350;     CqzaRAZxnL10814350 = CqzaRAZxnL61261567;     CqzaRAZxnL61261567 = CqzaRAZxnL18971181;     CqzaRAZxnL18971181 = CqzaRAZxnL50685121;     CqzaRAZxnL50685121 = CqzaRAZxnL59358328;     CqzaRAZxnL59358328 = CqzaRAZxnL9849930;     CqzaRAZxnL9849930 = CqzaRAZxnL18789;     CqzaRAZxnL18789 = CqzaRAZxnL9823674;     CqzaRAZxnL9823674 = CqzaRAZxnL65041297;     CqzaRAZxnL65041297 = CqzaRAZxnL59856195;     CqzaRAZxnL59856195 = CqzaRAZxnL42637783;     CqzaRAZxnL42637783 = CqzaRAZxnL67109803;     CqzaRAZxnL67109803 = CqzaRAZxnL22206994;     CqzaRAZxnL22206994 = CqzaRAZxnL63373597;     CqzaRAZxnL63373597 = CqzaRAZxnL38009946;     CqzaRAZxnL38009946 = CqzaRAZxnL31007129;     CqzaRAZxnL31007129 = CqzaRAZxnL3367145;     CqzaRAZxnL3367145 = CqzaRAZxnL2771182;     CqzaRAZxnL2771182 = CqzaRAZxnL49797868;     CqzaRAZxnL49797868 = CqzaRAZxnL77031632;     CqzaRAZxnL77031632 = CqzaRAZxnL98082907;     CqzaRAZxnL98082907 = CqzaRAZxnL92011114;     CqzaRAZxnL92011114 = CqzaRAZxnL99781702;     CqzaRAZxnL99781702 = CqzaRAZxnL78835599;     CqzaRAZxnL78835599 = CqzaRAZxnL61698989;     CqzaRAZxnL61698989 = CqzaRAZxnL97738062;     CqzaRAZxnL97738062 = CqzaRAZxnL57589795;     CqzaRAZxnL57589795 = CqzaRAZxnL5415972;     CqzaRAZxnL5415972 = CqzaRAZxnL65533124;     CqzaRAZxnL65533124 = CqzaRAZxnL22188719;     CqzaRAZxnL22188719 = CqzaRAZxnL82848980;     CqzaRAZxnL82848980 = CqzaRAZxnL74013692;     CqzaRAZxnL74013692 = CqzaRAZxnL44185498;     CqzaRAZxnL44185498 = CqzaRAZxnL54736669;     CqzaRAZxnL54736669 = CqzaRAZxnL98652894;     CqzaRAZxnL98652894 = CqzaRAZxnL50004852;     CqzaRAZxnL50004852 = CqzaRAZxnL32339225;     CqzaRAZxnL32339225 = CqzaRAZxnL8061193;     CqzaRAZxnL8061193 = CqzaRAZxnL81381536;     CqzaRAZxnL81381536 = CqzaRAZxnL41212043;     CqzaRAZxnL41212043 = CqzaRAZxnL17236511;     CqzaRAZxnL17236511 = CqzaRAZxnL15277828;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void qfVJvHbjgF33248734() {     int UVJyyOyfZw2478718 = -281409544;    int UVJyyOyfZw80973714 = -753503690;    int UVJyyOyfZw43458166 = -89215348;    int UVJyyOyfZw93857477 = -132612360;    int UVJyyOyfZw6704275 = -325908581;    int UVJyyOyfZw85080484 = -631706433;    int UVJyyOyfZw18537570 = -124456271;    int UVJyyOyfZw92928865 = -566372310;    int UVJyyOyfZw48491254 = -580535548;    int UVJyyOyfZw94141335 = -195554439;    int UVJyyOyfZw1822541 = -45306528;    int UVJyyOyfZw92542093 = -624259041;    int UVJyyOyfZw9066201 = -179608528;    int UVJyyOyfZw72536319 = -304348938;    int UVJyyOyfZw73429268 = -811604293;    int UVJyyOyfZw96296937 = -438421403;    int UVJyyOyfZw26756815 = -533963277;    int UVJyyOyfZw35283422 = -160085486;    int UVJyyOyfZw88972273 = -548672412;    int UVJyyOyfZw23810564 = 50056713;    int UVJyyOyfZw41278250 = 11014498;    int UVJyyOyfZw71055758 = -262929377;    int UVJyyOyfZw35856339 = -415533423;    int UVJyyOyfZw37080018 = -654416676;    int UVJyyOyfZw88770251 = -888080790;    int UVJyyOyfZw23550334 = -873813301;    int UVJyyOyfZw26545751 = -225719576;    int UVJyyOyfZw87133599 = -232208141;    int UVJyyOyfZw39274596 = -9867354;    int UVJyyOyfZw18486033 = -735629659;    int UVJyyOyfZw19620445 = -449404250;    int UVJyyOyfZw54778815 = -469637741;    int UVJyyOyfZw10944468 = -333589577;    int UVJyyOyfZw73951680 = -190862903;    int UVJyyOyfZw36271306 = -158282030;    int UVJyyOyfZw54186085 = -520198675;    int UVJyyOyfZw95601783 = -728481181;    int UVJyyOyfZw30727456 = -887980699;    int UVJyyOyfZw74319051 = -823808974;    int UVJyyOyfZw28747474 = -389563722;    int UVJyyOyfZw65590433 = 94129354;    int UVJyyOyfZw22439180 = -827161212;    int UVJyyOyfZw26423115 = -158961200;    int UVJyyOyfZw24897232 = -902652414;    int UVJyyOyfZw5282913 = -801361660;    int UVJyyOyfZw46924030 = -175985154;    int UVJyyOyfZw57523276 = -838931483;    int UVJyyOyfZw19063618 = -313608538;    int UVJyyOyfZw98096605 = -185579487;    int UVJyyOyfZw61976192 = -789238452;    int UVJyyOyfZw29510593 = 10014374;    int UVJyyOyfZw40559255 = 16719502;    int UVJyyOyfZw99645498 = -390730766;    int UVJyyOyfZw96353262 = -395916961;    int UVJyyOyfZw91970484 = -270383439;    int UVJyyOyfZw31422960 = 81519832;    int UVJyyOyfZw45117376 = -237970267;    int UVJyyOyfZw6378149 = -434798673;    int UVJyyOyfZw5087227 = -244531571;    int UVJyyOyfZw83153940 = -452095281;    int UVJyyOyfZw58534733 = -305986858;    int UVJyyOyfZw31403970 = -892248131;    int UVJyyOyfZw53654269 = -456504956;    int UVJyyOyfZw30005222 = -844905889;    int UVJyyOyfZw74520890 = -746150190;    int UVJyyOyfZw47043725 = -575668787;    int UVJyyOyfZw81597625 = -190669464;    int UVJyyOyfZw35114521 = -988745626;    int UVJyyOyfZw36265013 = -46066909;    int UVJyyOyfZw19243184 = -191405618;    int UVJyyOyfZw695154 = -709940222;    int UVJyyOyfZw96029359 = -645982578;    int UVJyyOyfZw60964371 = -336276512;    int UVJyyOyfZw60224799 = -59108691;    int UVJyyOyfZw58220131 = 55927358;    int UVJyyOyfZw18839070 = -161824290;    int UVJyyOyfZw44632644 = -3968178;    int UVJyyOyfZw10959107 = -512881010;    int UVJyyOyfZw31797105 = -853055017;    int UVJyyOyfZw41846221 = -612095636;    int UVJyyOyfZw66027058 = 65118182;    int UVJyyOyfZw7482134 = -912111038;    int UVJyyOyfZw89036994 = 53371346;    int UVJyyOyfZw77298404 = -220628902;    int UVJyyOyfZw88975440 = -645644034;    int UVJyyOyfZw79061189 = -366123752;    int UVJyyOyfZw55133316 = 21093025;    int UVJyyOyfZw14591205 = -937672617;    int UVJyyOyfZw81981195 = -920479464;    int UVJyyOyfZw4848347 = -139801862;    int UVJyyOyfZw9068710 = -182228408;    int UVJyyOyfZw89223635 = -193682509;    int UVJyyOyfZw25640230 = -543449129;    int UVJyyOyfZw91165111 = -271713693;    int UVJyyOyfZw70212740 = 16423136;    int UVJyyOyfZw34186464 = -13622516;    int UVJyyOyfZw68784910 = -270656256;    int UVJyyOyfZw96417893 = -314055311;    int UVJyyOyfZw50376341 = -56502225;    int UVJyyOyfZw58239187 = -281409544;     UVJyyOyfZw2478718 = UVJyyOyfZw80973714;     UVJyyOyfZw80973714 = UVJyyOyfZw43458166;     UVJyyOyfZw43458166 = UVJyyOyfZw93857477;     UVJyyOyfZw93857477 = UVJyyOyfZw6704275;     UVJyyOyfZw6704275 = UVJyyOyfZw85080484;     UVJyyOyfZw85080484 = UVJyyOyfZw18537570;     UVJyyOyfZw18537570 = UVJyyOyfZw92928865;     UVJyyOyfZw92928865 = UVJyyOyfZw48491254;     UVJyyOyfZw48491254 = UVJyyOyfZw94141335;     UVJyyOyfZw94141335 = UVJyyOyfZw1822541;     UVJyyOyfZw1822541 = UVJyyOyfZw92542093;     UVJyyOyfZw92542093 = UVJyyOyfZw9066201;     UVJyyOyfZw9066201 = UVJyyOyfZw72536319;     UVJyyOyfZw72536319 = UVJyyOyfZw73429268;     UVJyyOyfZw73429268 = UVJyyOyfZw96296937;     UVJyyOyfZw96296937 = UVJyyOyfZw26756815;     UVJyyOyfZw26756815 = UVJyyOyfZw35283422;     UVJyyOyfZw35283422 = UVJyyOyfZw88972273;     UVJyyOyfZw88972273 = UVJyyOyfZw23810564;     UVJyyOyfZw23810564 = UVJyyOyfZw41278250;     UVJyyOyfZw41278250 = UVJyyOyfZw71055758;     UVJyyOyfZw71055758 = UVJyyOyfZw35856339;     UVJyyOyfZw35856339 = UVJyyOyfZw37080018;     UVJyyOyfZw37080018 = UVJyyOyfZw88770251;     UVJyyOyfZw88770251 = UVJyyOyfZw23550334;     UVJyyOyfZw23550334 = UVJyyOyfZw26545751;     UVJyyOyfZw26545751 = UVJyyOyfZw87133599;     UVJyyOyfZw87133599 = UVJyyOyfZw39274596;     UVJyyOyfZw39274596 = UVJyyOyfZw18486033;     UVJyyOyfZw18486033 = UVJyyOyfZw19620445;     UVJyyOyfZw19620445 = UVJyyOyfZw54778815;     UVJyyOyfZw54778815 = UVJyyOyfZw10944468;     UVJyyOyfZw10944468 = UVJyyOyfZw73951680;     UVJyyOyfZw73951680 = UVJyyOyfZw36271306;     UVJyyOyfZw36271306 = UVJyyOyfZw54186085;     UVJyyOyfZw54186085 = UVJyyOyfZw95601783;     UVJyyOyfZw95601783 = UVJyyOyfZw30727456;     UVJyyOyfZw30727456 = UVJyyOyfZw74319051;     UVJyyOyfZw74319051 = UVJyyOyfZw28747474;     UVJyyOyfZw28747474 = UVJyyOyfZw65590433;     UVJyyOyfZw65590433 = UVJyyOyfZw22439180;     UVJyyOyfZw22439180 = UVJyyOyfZw26423115;     UVJyyOyfZw26423115 = UVJyyOyfZw24897232;     UVJyyOyfZw24897232 = UVJyyOyfZw5282913;     UVJyyOyfZw5282913 = UVJyyOyfZw46924030;     UVJyyOyfZw46924030 = UVJyyOyfZw57523276;     UVJyyOyfZw57523276 = UVJyyOyfZw19063618;     UVJyyOyfZw19063618 = UVJyyOyfZw98096605;     UVJyyOyfZw98096605 = UVJyyOyfZw61976192;     UVJyyOyfZw61976192 = UVJyyOyfZw29510593;     UVJyyOyfZw29510593 = UVJyyOyfZw40559255;     UVJyyOyfZw40559255 = UVJyyOyfZw99645498;     UVJyyOyfZw99645498 = UVJyyOyfZw96353262;     UVJyyOyfZw96353262 = UVJyyOyfZw91970484;     UVJyyOyfZw91970484 = UVJyyOyfZw31422960;     UVJyyOyfZw31422960 = UVJyyOyfZw45117376;     UVJyyOyfZw45117376 = UVJyyOyfZw6378149;     UVJyyOyfZw6378149 = UVJyyOyfZw5087227;     UVJyyOyfZw5087227 = UVJyyOyfZw83153940;     UVJyyOyfZw83153940 = UVJyyOyfZw58534733;     UVJyyOyfZw58534733 = UVJyyOyfZw31403970;     UVJyyOyfZw31403970 = UVJyyOyfZw53654269;     UVJyyOyfZw53654269 = UVJyyOyfZw30005222;     UVJyyOyfZw30005222 = UVJyyOyfZw74520890;     UVJyyOyfZw74520890 = UVJyyOyfZw47043725;     UVJyyOyfZw47043725 = UVJyyOyfZw81597625;     UVJyyOyfZw81597625 = UVJyyOyfZw35114521;     UVJyyOyfZw35114521 = UVJyyOyfZw36265013;     UVJyyOyfZw36265013 = UVJyyOyfZw19243184;     UVJyyOyfZw19243184 = UVJyyOyfZw695154;     UVJyyOyfZw695154 = UVJyyOyfZw96029359;     UVJyyOyfZw96029359 = UVJyyOyfZw60964371;     UVJyyOyfZw60964371 = UVJyyOyfZw60224799;     UVJyyOyfZw60224799 = UVJyyOyfZw58220131;     UVJyyOyfZw58220131 = UVJyyOyfZw18839070;     UVJyyOyfZw18839070 = UVJyyOyfZw44632644;     UVJyyOyfZw44632644 = UVJyyOyfZw10959107;     UVJyyOyfZw10959107 = UVJyyOyfZw31797105;     UVJyyOyfZw31797105 = UVJyyOyfZw41846221;     UVJyyOyfZw41846221 = UVJyyOyfZw66027058;     UVJyyOyfZw66027058 = UVJyyOyfZw7482134;     UVJyyOyfZw7482134 = UVJyyOyfZw89036994;     UVJyyOyfZw89036994 = UVJyyOyfZw77298404;     UVJyyOyfZw77298404 = UVJyyOyfZw88975440;     UVJyyOyfZw88975440 = UVJyyOyfZw79061189;     UVJyyOyfZw79061189 = UVJyyOyfZw55133316;     UVJyyOyfZw55133316 = UVJyyOyfZw14591205;     UVJyyOyfZw14591205 = UVJyyOyfZw81981195;     UVJyyOyfZw81981195 = UVJyyOyfZw4848347;     UVJyyOyfZw4848347 = UVJyyOyfZw9068710;     UVJyyOyfZw9068710 = UVJyyOyfZw89223635;     UVJyyOyfZw89223635 = UVJyyOyfZw25640230;     UVJyyOyfZw25640230 = UVJyyOyfZw91165111;     UVJyyOyfZw91165111 = UVJyyOyfZw70212740;     UVJyyOyfZw70212740 = UVJyyOyfZw34186464;     UVJyyOyfZw34186464 = UVJyyOyfZw68784910;     UVJyyOyfZw68784910 = UVJyyOyfZw96417893;     UVJyyOyfZw96417893 = UVJyyOyfZw50376341;     UVJyyOyfZw50376341 = UVJyyOyfZw58239187;     UVJyyOyfZw58239187 = UVJyyOyfZw2478718;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void NaWiBrWRdB23867819() {     int faDYYGwWxT60337746 = -637042124;    int faDYYGwWxT1448661 = -497408101;    int faDYYGwWxT6987498 = -80279040;    int faDYYGwWxT10927800 = -319089303;    int faDYYGwWxT43483593 = -85853785;    int faDYYGwWxT66575609 = -255797083;    int faDYYGwWxT84256042 = -382096378;    int faDYYGwWxT92266183 = -172848460;    int faDYYGwWxT81691662 = -425770854;    int faDYYGwWxT7528605 = -329369027;    int faDYYGwWxT36904924 = 75005486;    int faDYYGwWxT20429836 = -168168381;    int faDYYGwWxT50603199 = -207021404;    int faDYYGwWxT60603494 = -118115001;    int faDYYGwWxT78014844 = 82918683;    int faDYYGwWxT74965056 = -890309251;    int faDYYGwWxT20710584 = -598988510;    int faDYYGwWxT95018964 = -991113001;    int faDYYGwWxT89947694 = -864373425;    int faDYYGwWxT52016540 = -505089904;    int faDYYGwWxT95909371 = -216814751;    int faDYYGwWxT81699969 = -934348218;    int faDYYGwWxT35918249 = -411330515;    int faDYYGwWxT70072533 = -867393092;    int faDYYGwWxT75443070 = -780237407;    int faDYYGwWxT89403346 = -89872475;    int faDYYGwWxT93612582 = -287990783;    int faDYYGwWxT62966425 = -72500792;    int faDYYGwWxT67478477 = -736078646;    int faDYYGwWxT80996883 = -239569949;    int faDYYGwWxT15669124 = -308514420;    int faDYYGwWxT80695309 = -111036987;    int faDYYGwWxT33296303 = -606640618;    int faDYYGwWxT62523119 = -826320102;    int faDYYGwWxT13593049 = -754510853;    int faDYYGwWxT61783419 = -890627865;    int faDYYGwWxT14570577 = -126818417;    int faDYYGwWxT95733601 = -747736394;    int faDYYGwWxT82390550 = -550368450;    int faDYYGwWxT69458935 = -772186925;    int faDYYGwWxT52088076 = -2359430;    int faDYYGwWxT28917376 = -892031029;    int faDYYGwWxT789965 = -681412061;    int faDYYGwWxT46008893 = -190419586;    int faDYYGwWxT5246534 = -590497496;    int faDYYGwWxT20129704 = -773048540;    int faDYYGwWxT33770252 = -938766852;    int faDYYGwWxT30621442 = -119147410;    int faDYYGwWxT78132397 = -671505343;    int faDYYGwWxT43498681 = -48235754;    int faDYYGwWxT92631793 = 37771554;    int faDYYGwWxT68801470 = -963460630;    int faDYYGwWxT95684353 = -34579534;    int faDYYGwWxT52131113 = -899266567;    int faDYYGwWxT27958448 = -143779145;    int faDYYGwWxT78637776 = -702693907;    int faDYYGwWxT65530412 = 13922414;    int faDYYGwWxT36914964 = -212885948;    int faDYYGwWxT35484729 = -538851897;    int faDYYGwWxT54080247 = -995981311;    int faDYYGwWxT72963026 = -967806300;    int faDYYGwWxT21289618 = -209595586;    int faDYYGwWxT24787706 = -436769815;    int faDYYGwWxT694779 = -86200906;    int faDYYGwWxT91859481 = 79145392;    int faDYYGwWxT56209615 = -813957527;    int faDYYGwWxT87133533 = -561527764;    int faDYYGwWxT88080079 = -380701303;    int faDYYGwWxT47010445 = -363604148;    int faDYYGwWxT16231425 = -26453453;    int faDYYGwWxT60394479 = -663490835;    int faDYYGwWxT24976983 = -851252116;    int faDYYGwWxT12628414 = -340744552;    int faDYYGwWxT20488759 = 7813499;    int faDYYGwWxT99928463 = -402730475;    int faDYYGwWxT66991995 = -324783723;    int faDYYGwWxT80910005 = -152936157;    int faDYYGwWxT89909355 = -120910930;    int faDYYGwWxT64826000 = -176895597;    int faDYYGwWxT55313366 = 92811133;    int faDYYGwWxT55633094 = -151105623;    int faDYYGwWxT62991140 = -68843374;    int faDYYGwWxT84834027 = -400995450;    int faDYYGwWxT23979797 = -587842892;    int faDYYGwWxT88365090 = -177341504;    int faDYYGwWxT46867654 = -345053790;    int faDYYGwWxT85010955 = 23542546;    int faDYYGwWxT81165189 = -707374051;    int faDYYGwWxT34564672 = -582540957;    int faDYYGwWxT34955272 = 48183053;    int faDYYGwWxT96253007 = -804550280;    int faDYYGwWxT77655613 = -913932469;    int faDYYGwWxT60248872 = -108884498;    int faDYYGwWxT28310304 = -554387139;    int faDYYGwWxT96495908 = -804380625;    int faDYYGwWxT30798459 = -792763844;    int faDYYGwWxT4129670 = -355261215;    int faDYYGwWxT95186 = -495211156;    int faDYYGwWxT54149412 = -169564978;    int faDYYGwWxT49036919 = -637042124;     faDYYGwWxT60337746 = faDYYGwWxT1448661;     faDYYGwWxT1448661 = faDYYGwWxT6987498;     faDYYGwWxT6987498 = faDYYGwWxT10927800;     faDYYGwWxT10927800 = faDYYGwWxT43483593;     faDYYGwWxT43483593 = faDYYGwWxT66575609;     faDYYGwWxT66575609 = faDYYGwWxT84256042;     faDYYGwWxT84256042 = faDYYGwWxT92266183;     faDYYGwWxT92266183 = faDYYGwWxT81691662;     faDYYGwWxT81691662 = faDYYGwWxT7528605;     faDYYGwWxT7528605 = faDYYGwWxT36904924;     faDYYGwWxT36904924 = faDYYGwWxT20429836;     faDYYGwWxT20429836 = faDYYGwWxT50603199;     faDYYGwWxT50603199 = faDYYGwWxT60603494;     faDYYGwWxT60603494 = faDYYGwWxT78014844;     faDYYGwWxT78014844 = faDYYGwWxT74965056;     faDYYGwWxT74965056 = faDYYGwWxT20710584;     faDYYGwWxT20710584 = faDYYGwWxT95018964;     faDYYGwWxT95018964 = faDYYGwWxT89947694;     faDYYGwWxT89947694 = faDYYGwWxT52016540;     faDYYGwWxT52016540 = faDYYGwWxT95909371;     faDYYGwWxT95909371 = faDYYGwWxT81699969;     faDYYGwWxT81699969 = faDYYGwWxT35918249;     faDYYGwWxT35918249 = faDYYGwWxT70072533;     faDYYGwWxT70072533 = faDYYGwWxT75443070;     faDYYGwWxT75443070 = faDYYGwWxT89403346;     faDYYGwWxT89403346 = faDYYGwWxT93612582;     faDYYGwWxT93612582 = faDYYGwWxT62966425;     faDYYGwWxT62966425 = faDYYGwWxT67478477;     faDYYGwWxT67478477 = faDYYGwWxT80996883;     faDYYGwWxT80996883 = faDYYGwWxT15669124;     faDYYGwWxT15669124 = faDYYGwWxT80695309;     faDYYGwWxT80695309 = faDYYGwWxT33296303;     faDYYGwWxT33296303 = faDYYGwWxT62523119;     faDYYGwWxT62523119 = faDYYGwWxT13593049;     faDYYGwWxT13593049 = faDYYGwWxT61783419;     faDYYGwWxT61783419 = faDYYGwWxT14570577;     faDYYGwWxT14570577 = faDYYGwWxT95733601;     faDYYGwWxT95733601 = faDYYGwWxT82390550;     faDYYGwWxT82390550 = faDYYGwWxT69458935;     faDYYGwWxT69458935 = faDYYGwWxT52088076;     faDYYGwWxT52088076 = faDYYGwWxT28917376;     faDYYGwWxT28917376 = faDYYGwWxT789965;     faDYYGwWxT789965 = faDYYGwWxT46008893;     faDYYGwWxT46008893 = faDYYGwWxT5246534;     faDYYGwWxT5246534 = faDYYGwWxT20129704;     faDYYGwWxT20129704 = faDYYGwWxT33770252;     faDYYGwWxT33770252 = faDYYGwWxT30621442;     faDYYGwWxT30621442 = faDYYGwWxT78132397;     faDYYGwWxT78132397 = faDYYGwWxT43498681;     faDYYGwWxT43498681 = faDYYGwWxT92631793;     faDYYGwWxT92631793 = faDYYGwWxT68801470;     faDYYGwWxT68801470 = faDYYGwWxT95684353;     faDYYGwWxT95684353 = faDYYGwWxT52131113;     faDYYGwWxT52131113 = faDYYGwWxT27958448;     faDYYGwWxT27958448 = faDYYGwWxT78637776;     faDYYGwWxT78637776 = faDYYGwWxT65530412;     faDYYGwWxT65530412 = faDYYGwWxT36914964;     faDYYGwWxT36914964 = faDYYGwWxT35484729;     faDYYGwWxT35484729 = faDYYGwWxT54080247;     faDYYGwWxT54080247 = faDYYGwWxT72963026;     faDYYGwWxT72963026 = faDYYGwWxT21289618;     faDYYGwWxT21289618 = faDYYGwWxT24787706;     faDYYGwWxT24787706 = faDYYGwWxT694779;     faDYYGwWxT694779 = faDYYGwWxT91859481;     faDYYGwWxT91859481 = faDYYGwWxT56209615;     faDYYGwWxT56209615 = faDYYGwWxT87133533;     faDYYGwWxT87133533 = faDYYGwWxT88080079;     faDYYGwWxT88080079 = faDYYGwWxT47010445;     faDYYGwWxT47010445 = faDYYGwWxT16231425;     faDYYGwWxT16231425 = faDYYGwWxT60394479;     faDYYGwWxT60394479 = faDYYGwWxT24976983;     faDYYGwWxT24976983 = faDYYGwWxT12628414;     faDYYGwWxT12628414 = faDYYGwWxT20488759;     faDYYGwWxT20488759 = faDYYGwWxT99928463;     faDYYGwWxT99928463 = faDYYGwWxT66991995;     faDYYGwWxT66991995 = faDYYGwWxT80910005;     faDYYGwWxT80910005 = faDYYGwWxT89909355;     faDYYGwWxT89909355 = faDYYGwWxT64826000;     faDYYGwWxT64826000 = faDYYGwWxT55313366;     faDYYGwWxT55313366 = faDYYGwWxT55633094;     faDYYGwWxT55633094 = faDYYGwWxT62991140;     faDYYGwWxT62991140 = faDYYGwWxT84834027;     faDYYGwWxT84834027 = faDYYGwWxT23979797;     faDYYGwWxT23979797 = faDYYGwWxT88365090;     faDYYGwWxT88365090 = faDYYGwWxT46867654;     faDYYGwWxT46867654 = faDYYGwWxT85010955;     faDYYGwWxT85010955 = faDYYGwWxT81165189;     faDYYGwWxT81165189 = faDYYGwWxT34564672;     faDYYGwWxT34564672 = faDYYGwWxT34955272;     faDYYGwWxT34955272 = faDYYGwWxT96253007;     faDYYGwWxT96253007 = faDYYGwWxT77655613;     faDYYGwWxT77655613 = faDYYGwWxT60248872;     faDYYGwWxT60248872 = faDYYGwWxT28310304;     faDYYGwWxT28310304 = faDYYGwWxT96495908;     faDYYGwWxT96495908 = faDYYGwWxT30798459;     faDYYGwWxT30798459 = faDYYGwWxT4129670;     faDYYGwWxT4129670 = faDYYGwWxT95186;     faDYYGwWxT95186 = faDYYGwWxT54149412;     faDYYGwWxT54149412 = faDYYGwWxT49036919;     faDYYGwWxT49036919 = faDYYGwWxT60337746;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void HlzxSJSEfz66729435() {     int JVXtmYteLY47538636 = -247049715;    int JVXtmYteLY91822405 = -412264338;    int JVXtmYteLY91861121 = -686176886;    int JVXtmYteLY73146695 = -135968686;    int JVXtmYteLY67652693 = -530747963;    int JVXtmYteLY92876403 = -977739393;    int JVXtmYteLY142957 = -345497241;    int JVXtmYteLY3975172 = -236684838;    int JVXtmYteLY51767180 = -991304902;    int JVXtmYteLY94588927 = -898895137;    int JVXtmYteLY50058752 = -829252642;    int JVXtmYteLY17062686 = -231792290;    int JVXtmYteLY44165724 = -346555696;    int JVXtmYteLY18667856 = -476005906;    int JVXtmYteLY45052509 = 64478810;    int JVXtmYteLY95669081 = 94990837;    int JVXtmYteLY76664285 = -574443183;    int JVXtmYteLY57461545 = -30045943;    int JVXtmYteLY29643604 = 9131237;    int JVXtmYteLY93509569 = -29894668;    int JVXtmYteLY75074163 = -246431235;    int JVXtmYteLY9244683 = -684473636;    int JVXtmYteLY70914746 = -298242305;    int JVXtmYteLY12865621 = -268889125;    int JVXtmYteLY26434190 = -323183189;    int JVXtmYteLY41232855 = -215853132;    int JVXtmYteLY22640210 = -255883929;    int JVXtmYteLY66420550 = -16636664;    int JVXtmYteLY76218318 = -714002036;    int JVXtmYteLY80425508 = 39453458;    int JVXtmYteLY38058486 = -806284139;    int JVXtmYteLY46824201 = -915960832;    int JVXtmYteLY58155200 = -34674265;    int JVXtmYteLY86012420 = -674929348;    int JVXtmYteLY95248592 = -663106823;    int JVXtmYteLY52215684 = -161596253;    int JVXtmYteLY1689252 = -536215823;    int JVXtmYteLY77864937 = -818641661;    int JVXtmYteLY47242358 = 66119295;    int JVXtmYteLY86939991 = -146633451;    int JVXtmYteLY66368103 = -902773233;    int JVXtmYteLY92610241 = -281303859;    int JVXtmYteLY86473215 = -676239125;    int JVXtmYteLY19844152 = -63176230;    int JVXtmYteLY93274147 = -544246339;    int JVXtmYteLY27357512 = -323819442;    int JVXtmYteLY11583818 = -303813044;    int JVXtmYteLY51948637 = -81317342;    int JVXtmYteLY71385127 = -735431091;    int JVXtmYteLY36639106 = -811249064;    int JVXtmYteLY823040 = 33545848;    int JVXtmYteLY69719438 = -156566789;    int JVXtmYteLY12095900 = -742684268;    int JVXtmYteLY27931930 = -173654528;    int JVXtmYteLY91655272 = -763133214;    int JVXtmYteLY38293954 = -562576080;    int JVXtmYteLY20907659 = -14022033;    int JVXtmYteLY78995501 = -317287761;    int JVXtmYteLY46712505 = -812785498;    int JVXtmYteLY26419838 = -214894831;    int JVXtmYteLY70236193 = -621855464;    int JVXtmYteLY33722407 = -228860577;    int JVXtmYteLY27756854 = -522682802;    int JVXtmYteLY71341672 = -930758360;    int JVXtmYteLY56530442 = 7389001;    int JVXtmYteLY3234552 = -913291811;    int JVXtmYteLY58907485 = -97118026;    int JVXtmYteLY58153303 = -671626348;    int JVXtmYteLY23419263 = -812899084;    int JVXtmYteLY92836825 = -773924937;    int JVXtmYteLY93979830 = -368793341;    int JVXtmYteLY98799347 = -755801522;    int JVXtmYteLY10219187 = 3834762;    int JVXtmYteLY42703613 = -844235312;    int JVXtmYteLY27141466 = -127121435;    int JVXtmYteLY82463921 = -965127376;    int JVXtmYteLY22771467 = 91765488;    int JVXtmYteLY51070595 = -135066075;    int JVXtmYteLY19591473 = -724642787;    int JVXtmYteLY99076678 = -999363747;    int JVXtmYteLY29649037 = -912040089;    int JVXtmYteLY70691572 = -74566587;    int JVXtmYteLY95035423 = -281205574;    int JVXtmYteLY39579212 = -902752972;    int JVXtmYteLY79602468 = -994092391;    int JVXtmYteLY68339048 = -549717351;    int JVXtmYteLY34728301 = -73276564;    int JVXtmYteLY30223271 = -861019738;    int JVXtmYteLY94357148 = -911796135;    int JVXtmYteLY56954638 = -530743;    int JVXtmYteLY31308025 = -47574220;    int JVXtmYteLY22693751 = -118928062;    int JVXtmYteLY31152432 = 94143837;    int JVXtmYteLY20822520 = -718985874;    int JVXtmYteLY16703798 = -524777987;    int JVXtmYteLY32645697 = -573912656;    int JVXtmYteLY64853388 = -758621057;    int JVXtmYteLY15131543 = -745480765;    int JVXtmYteLY63313709 = 29434768;    int JVXtmYteLY90039595 = -247049715;     JVXtmYteLY47538636 = JVXtmYteLY91822405;     JVXtmYteLY91822405 = JVXtmYteLY91861121;     JVXtmYteLY91861121 = JVXtmYteLY73146695;     JVXtmYteLY73146695 = JVXtmYteLY67652693;     JVXtmYteLY67652693 = JVXtmYteLY92876403;     JVXtmYteLY92876403 = JVXtmYteLY142957;     JVXtmYteLY142957 = JVXtmYteLY3975172;     JVXtmYteLY3975172 = JVXtmYteLY51767180;     JVXtmYteLY51767180 = JVXtmYteLY94588927;     JVXtmYteLY94588927 = JVXtmYteLY50058752;     JVXtmYteLY50058752 = JVXtmYteLY17062686;     JVXtmYteLY17062686 = JVXtmYteLY44165724;     JVXtmYteLY44165724 = JVXtmYteLY18667856;     JVXtmYteLY18667856 = JVXtmYteLY45052509;     JVXtmYteLY45052509 = JVXtmYteLY95669081;     JVXtmYteLY95669081 = JVXtmYteLY76664285;     JVXtmYteLY76664285 = JVXtmYteLY57461545;     JVXtmYteLY57461545 = JVXtmYteLY29643604;     JVXtmYteLY29643604 = JVXtmYteLY93509569;     JVXtmYteLY93509569 = JVXtmYteLY75074163;     JVXtmYteLY75074163 = JVXtmYteLY9244683;     JVXtmYteLY9244683 = JVXtmYteLY70914746;     JVXtmYteLY70914746 = JVXtmYteLY12865621;     JVXtmYteLY12865621 = JVXtmYteLY26434190;     JVXtmYteLY26434190 = JVXtmYteLY41232855;     JVXtmYteLY41232855 = JVXtmYteLY22640210;     JVXtmYteLY22640210 = JVXtmYteLY66420550;     JVXtmYteLY66420550 = JVXtmYteLY76218318;     JVXtmYteLY76218318 = JVXtmYteLY80425508;     JVXtmYteLY80425508 = JVXtmYteLY38058486;     JVXtmYteLY38058486 = JVXtmYteLY46824201;     JVXtmYteLY46824201 = JVXtmYteLY58155200;     JVXtmYteLY58155200 = JVXtmYteLY86012420;     JVXtmYteLY86012420 = JVXtmYteLY95248592;     JVXtmYteLY95248592 = JVXtmYteLY52215684;     JVXtmYteLY52215684 = JVXtmYteLY1689252;     JVXtmYteLY1689252 = JVXtmYteLY77864937;     JVXtmYteLY77864937 = JVXtmYteLY47242358;     JVXtmYteLY47242358 = JVXtmYteLY86939991;     JVXtmYteLY86939991 = JVXtmYteLY66368103;     JVXtmYteLY66368103 = JVXtmYteLY92610241;     JVXtmYteLY92610241 = JVXtmYteLY86473215;     JVXtmYteLY86473215 = JVXtmYteLY19844152;     JVXtmYteLY19844152 = JVXtmYteLY93274147;     JVXtmYteLY93274147 = JVXtmYteLY27357512;     JVXtmYteLY27357512 = JVXtmYteLY11583818;     JVXtmYteLY11583818 = JVXtmYteLY51948637;     JVXtmYteLY51948637 = JVXtmYteLY71385127;     JVXtmYteLY71385127 = JVXtmYteLY36639106;     JVXtmYteLY36639106 = JVXtmYteLY823040;     JVXtmYteLY823040 = JVXtmYteLY69719438;     JVXtmYteLY69719438 = JVXtmYteLY12095900;     JVXtmYteLY12095900 = JVXtmYteLY27931930;     JVXtmYteLY27931930 = JVXtmYteLY91655272;     JVXtmYteLY91655272 = JVXtmYteLY38293954;     JVXtmYteLY38293954 = JVXtmYteLY20907659;     JVXtmYteLY20907659 = JVXtmYteLY78995501;     JVXtmYteLY78995501 = JVXtmYteLY46712505;     JVXtmYteLY46712505 = JVXtmYteLY26419838;     JVXtmYteLY26419838 = JVXtmYteLY70236193;     JVXtmYteLY70236193 = JVXtmYteLY33722407;     JVXtmYteLY33722407 = JVXtmYteLY27756854;     JVXtmYteLY27756854 = JVXtmYteLY71341672;     JVXtmYteLY71341672 = JVXtmYteLY56530442;     JVXtmYteLY56530442 = JVXtmYteLY3234552;     JVXtmYteLY3234552 = JVXtmYteLY58907485;     JVXtmYteLY58907485 = JVXtmYteLY58153303;     JVXtmYteLY58153303 = JVXtmYteLY23419263;     JVXtmYteLY23419263 = JVXtmYteLY92836825;     JVXtmYteLY92836825 = JVXtmYteLY93979830;     JVXtmYteLY93979830 = JVXtmYteLY98799347;     JVXtmYteLY98799347 = JVXtmYteLY10219187;     JVXtmYteLY10219187 = JVXtmYteLY42703613;     JVXtmYteLY42703613 = JVXtmYteLY27141466;     JVXtmYteLY27141466 = JVXtmYteLY82463921;     JVXtmYteLY82463921 = JVXtmYteLY22771467;     JVXtmYteLY22771467 = JVXtmYteLY51070595;     JVXtmYteLY51070595 = JVXtmYteLY19591473;     JVXtmYteLY19591473 = JVXtmYteLY99076678;     JVXtmYteLY99076678 = JVXtmYteLY29649037;     JVXtmYteLY29649037 = JVXtmYteLY70691572;     JVXtmYteLY70691572 = JVXtmYteLY95035423;     JVXtmYteLY95035423 = JVXtmYteLY39579212;     JVXtmYteLY39579212 = JVXtmYteLY79602468;     JVXtmYteLY79602468 = JVXtmYteLY68339048;     JVXtmYteLY68339048 = JVXtmYteLY34728301;     JVXtmYteLY34728301 = JVXtmYteLY30223271;     JVXtmYteLY30223271 = JVXtmYteLY94357148;     JVXtmYteLY94357148 = JVXtmYteLY56954638;     JVXtmYteLY56954638 = JVXtmYteLY31308025;     JVXtmYteLY31308025 = JVXtmYteLY22693751;     JVXtmYteLY22693751 = JVXtmYteLY31152432;     JVXtmYteLY31152432 = JVXtmYteLY20822520;     JVXtmYteLY20822520 = JVXtmYteLY16703798;     JVXtmYteLY16703798 = JVXtmYteLY32645697;     JVXtmYteLY32645697 = JVXtmYteLY64853388;     JVXtmYteLY64853388 = JVXtmYteLY15131543;     JVXtmYteLY15131543 = JVXtmYteLY63313709;     JVXtmYteLY63313709 = JVXtmYteLY90039595;     JVXtmYteLY90039595 = JVXtmYteLY47538636;}
// Junk Finished
