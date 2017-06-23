/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#include "pch.h"

#include "SampleApp3DModel.h"
#include "ShaderStructures.h"
#include "DirectXHelper.h"
#include "SampleUtil.h"

#include <string>
#include <fstream>
#include <iostream>
#include <memory>

using namespace SampleCommon;
using namespace std;


SampleApp3DModel::SampleApp3DModel(
    const std::shared_ptr<DX::DeviceResources>& deviceResources,
    const char *filename)
    : m_filename((char*)filename), m_deviceResources(deviceResources), m_vertexCount(0)
{
    if (!LoadMeshFromFile()) {
        throw ref new Platform::Exception(E_FAIL, "Failed to load 3D model.");
    }
}

SampleApp3DModel::~SampleApp3DModel()
{
    ReleaseResources();
}

void SampleApp3DModel::ReleaseResources()
{
    if (m_vertices != nullptr) {
        free(m_vertices);
        m_vertices = nullptr;
    }

    if (m_normals != nullptr) {
        free(m_normals);
        m_normals = nullptr;
    }
    
    if (m_texCoords != nullptr) {
        free(m_texCoords);
        m_texCoords = nullptr;
    }
    
    m_vertexBuffer.Reset();
    m_vertexCount = 0;
    m_deviceResources.reset();
}

bool SampleApp3DModel::LoadMeshFromFile()
{
    char buffer[132];
    int nbItems = 0;
    int index = 0;
    float *data = nullptr;

    std::ifstream file(m_filename);
    if (file.fail()) {
        SampleUtil::Log("SampleApp3DModel", "Failed to open 3D model file.");
        return false;
    }

    std::string line;
    int state = 0;
    while (getline(file, line))
    {
        // Process line:
        char ch = line[0];
        int ci = 0;
        while ((ch != '\n') && (ch != '\0')) {
            ch = buffer[ci] = line[ci];
            ci++;
        }

        // process buffer (line)
        if (buffer[0] == ':') {
            if ((state > 0) && (index != nbItems)) {
                // check that we got all the data we needed
                SampleUtil::Log("SampleApp3DModel", "Buffer overflow!");
            }
            state++;
            nbItems = atoi(&buffer[1]);
            index = 0;

            switch (state) {
            case 1:
                m_vertexCount = nbItems / 3;
                m_vertices = (float*)malloc(nbItems * sizeof(float));
                data = m_vertices;
                break;
            case 2:
                m_normals = (float*)malloc(nbItems * sizeof(float));
                data = m_normals;
                break;
            case 3:
                m_texCoords = (float*)malloc(nbItems * sizeof(float));
                data = m_texCoords;
                break;
            }
        }
        else {
            if (index >= nbItems) {
                // check that we don't get too many data
                SampleUtil::Log("SampleApp3DModel", "Buffer overflow!");
            }
            else {
                data[index++] = (float)atof(buffer);
            }
        }
    }
    return true;
}

void SampleApp3DModel::InitMesh()
{
    m_meshVertices = new TexturedVertex[m_vertexCount];
    for (uint32 i = 0; i < m_vertexCount; ++i)
    {
        m_meshVertices[i].pos = DirectX::XMFLOAT3(
            m_vertices[3 * i],
            m_vertices[3 * i + 1],
            m_vertices[3 * i + 2]);
        m_meshVertices[i].texcoord = DirectX::XMFLOAT2(
            m_texCoords[2 * i],
            m_texCoords[2 * i + 1]);
    }

    D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
    vertexBufferData.pSysMem = m_meshVertices;
    vertexBufferData.SysMemPitch = 0;
    vertexBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC vertexBufferDesc(m_vertexCount * sizeof(TexturedVertex), D3D11_BIND_VERTEX_BUFFER);
    DX::ThrowIfFailed(
        m_deviceResources->GetD3DDevice()->CreateBuffer(
            &vertexBufferDesc,
            &vertexBufferData,
            &m_vertexBuffer
            )
        );
}
