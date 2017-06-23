/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#include "pch.h"

#include "QuadMesh.h"
#include "ShaderStructures.h"

#include <iostream>
#include <memory>
#include "DirectXHelper.h"

using namespace SampleCommon;
using namespace DirectX;

QuadMesh::QuadMesh(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
        m_deviceResources(deviceResources), m_indexCount(0)
{
}

QuadMesh::~QuadMesh()
{
    ReleaseResources();
}

void QuadMesh::InitMesh()
{
    static TexturedVertex meshVertices[4] = { 
        { XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3( 0.5f, -0.5f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3( 0.5f,  0.5f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(-0.5f,  0.5f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
    };

    static uint16 meshIndices[6] = { 0,1,2, 0,2,3 };

    D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
    vertexBufferData.pSysMem = meshVertices;
    vertexBufferData.SysMemPitch = 0;
    vertexBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(meshVertices), D3D11_BIND_VERTEX_BUFFER);
    DX::ThrowIfFailed(
        m_deviceResources->GetD3DDevice()->CreateBuffer(
            &vertexBufferDesc,
            &vertexBufferData,
            &m_vertexBuffer
            )
        );

    m_indexCount = ARRAYSIZE(meshIndices);

    D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
    indexBufferData.pSysMem = meshIndices;
    indexBufferData.SysMemPitch = 0;
    indexBufferData.SysMemSlicePitch = 0;
    
    CD3D11_BUFFER_DESC indexBufferDesc(sizeof(meshIndices), D3D11_BIND_INDEX_BUFFER);
    DX::ThrowIfFailed(
            m_deviceResources->GetD3DDevice()->CreateBuffer(
                &indexBufferDesc,
                &indexBufferData,
                &m_indexBuffer
                )
            );
}

void QuadMesh::ReleaseResources()
{
    m_vertexBuffer.Reset();    
    m_indexBuffer.Reset();
    m_indexCount = 0;
    m_deviceResources.reset();
}
