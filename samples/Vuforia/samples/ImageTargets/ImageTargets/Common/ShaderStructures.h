/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#pragma once

#include <DirectXColors.h>
#include <DirectXMath.h>

namespace SampleCommon
{
    // Constant buffer used to send MVP matrices to the vertex shader.
    struct ModelViewProjectionConstantBuffer
    {
        DirectX::XMFLOAT4X4 model;
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 projection;
    };

    // Constant buffer used to send projection matrices to the vertex shader.
    struct ProjectionConstantBuffer
    {
        DirectX::XMFLOAT4X4 projection;
    };

    // Used to send per-vertex data to the vertex shader.
    struct TexturedVertex
    {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT2 texcoord;
    };
}