/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#pragma once

#include "DeviceResources.h"
#include "ShaderStructures.h"

#include <wrl.h>
#include <d3d11.h>

namespace SampleCommon
{
    class QuadMesh
    {
    public:
        QuadMesh(const std::shared_ptr<DX::DeviceResources>& deviceResources);
        ~QuadMesh();
        
        void InitMesh();
        void ReleaseResources();

        Microsoft::WRL::ComPtr<ID3D11Buffer> & GetVertexBuffer() { return m_vertexBuffer; }
        Microsoft::WRL::ComPtr<ID3D11Buffer> & GetIndexBuffer() { return m_indexBuffer; }
        uint32_t GetIndexCount() const { return m_indexCount; }

    private:
        // Cached pointer to device resources.
        std::shared_ptr<DX::DeviceResources> m_deviceResources;

        // Vertex and index buffers
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;
        uint32    m_indexCount;
    };
} // namespace SampleCommon