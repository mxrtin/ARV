/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#pragma once

#include <d3d11.h>

namespace SampleCommon
{
    class RenderUtil
    {
    public:
        static D3D11_BLEND_DESC CreateBlendDesc(bool transparent) {
            D3D11_BLEND_DESC blendDesc;
            ZeroMemory(&blendDesc, sizeof(D3D11_BLEND_DESC));
            blendDesc.RenderTarget[0].BlendEnable = transparent;
            blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
            blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
            blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
            blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            return blendDesc;
        }

        static D3D11_RASTERIZER_DESC CreateRasterizerDesc(
            D3D11_FILL_MODE fillMode,
            D3D11_CULL_MODE cullMode,
            bool frontFaceCounterClockwise,
            bool depthClipping)
        {
            D3D11_RASTERIZER_DESC rasterDesc;
            rasterDesc.AntialiasedLineEnable = false;
            rasterDesc.CullMode = cullMode;// culling
            rasterDesc.DepthBias = 0;
            rasterDesc.DepthBiasClamp = 0.0f;
            rasterDesc.DepthClipEnable = depthClipping;// depth clipping
            rasterDesc.FillMode = fillMode;// solid or wireframe
            rasterDesc.FrontCounterClockwise = frontFaceCounterClockwise; //CCW or CW front face
            rasterDesc.MultisampleEnable = false;
            rasterDesc.ScissorEnable = false;
            rasterDesc.SlopeScaledDepthBias = 0.0f;
            return rasterDesc;
        }

        static D3D11_DEPTH_STENCIL_DESC CreateDepthStencilDesc(
            bool depthEnabled,
            D3D11_DEPTH_WRITE_MASK depthWriteMask,
            D3D11_COMPARISON_FUNC depthCompareFunc)
        {
            D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
            depthStencilDesc.DepthEnable = depthEnabled;
            depthStencilDesc.DepthWriteMask = depthWriteMask;
            depthStencilDesc.DepthFunc = depthCompareFunc;
                                                          
            // Stencil test parameters
            depthStencilDesc.StencilEnable = false;
            depthStencilDesc.StencilReadMask = 0xFF;
            depthStencilDesc.StencilWriteMask = 0xFF;

            // Stencil operations if pixel is front-facing
            depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
            depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
            depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
            depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

            // Stencil operations if pixel is back-facing
            depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
            depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
            depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
            depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

            return depthStencilDesc;
        }
    };

} // namespace SampleCommon