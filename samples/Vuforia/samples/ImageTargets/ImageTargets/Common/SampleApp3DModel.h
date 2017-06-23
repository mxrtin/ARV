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
    class SampleApp3DModel
    {
    public:
        SampleApp3DModel(
            const std::shared_ptr<DX::DeviceResources>& deviceResources, 
            const char* filename);
        ~SampleApp3DModel();

        void InitMesh();
        void ReleaseResources();

        Microsoft::WRL::ComPtr<ID3D11Buffer> & GetVertexBuffer() { return m_vertexBuffer; }
        uint32_t GetVertexCount() const { return m_vertexCount; }

    private:
        bool LoadMeshFromFile();

        char *m_filename;

        float* m_vertices;
        float* m_normals;
        float* m_texCoords;

        TexturedVertex *m_meshVertices;

        // Cached pointer to device resources.
        std::shared_ptr<DX::DeviceResources> m_deviceResources;

        // Vertex and index buffers
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
        uint32    m_vertexCount;

    };

}// namespace SampleCommon
