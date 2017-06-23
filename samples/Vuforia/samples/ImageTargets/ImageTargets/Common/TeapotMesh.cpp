/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#include "pch.h"

#include "TeapotMesh.h"
#include "ShaderStructures.h"

#include <iostream>
#include <memory>
#include "DirectXHelper.h"

using namespace SampleCommon;

TeapotMesh::TeapotMesh(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
        m_deviceResources(deviceResources), m_indexCount(0)
{
}

TeapotMesh::~TeapotMesh()
{
    ReleaseResources();
}

void TeapotMesh::InitMesh()
{
    static TexturedVertex meshVertices[NUM_TEAPOT_OBJECT_VERTEX];
    for (int i = 0; i < NUM_TEAPOT_OBJECT_VERTEX; ++i)
    {
        meshVertices[i].pos = DirectX::XMFLOAT3(
            (float)teapotVertices[3 * i],
            (float)teapotVertices[3 * i + 1],
            (float)teapotVertices[3 * i + 2]);
        meshVertices[i].texcoord = DirectX::XMFLOAT2(
            (float)teapotTexCoords[2 * i],
            (float)teapotTexCoords[2 * i + 1]);
    }

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

    m_indexCount = ARRAYSIZE(teapotIndices);

    D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
    indexBufferData.pSysMem = teapotIndices;
    indexBufferData.SysMemPitch = 0;
    indexBufferData.SysMemSlicePitch = 0;
    
    CD3D11_BUFFER_DESC indexBufferDesc(sizeof(teapotIndices), D3D11_BIND_INDEX_BUFFER);
    DX::ThrowIfFailed(
            m_deviceResources->GetD3DDevice()->CreateBuffer(
                &indexBufferDesc,
                &indexBufferData,
                &m_indexBuffer
                )
            );
}

void TeapotMesh::ReleaseResources()
{
    m_vertexBuffer.Reset();    
    m_indexBuffer.Reset();
    m_indexCount = 0;
    m_deviceResources.reset();
}
