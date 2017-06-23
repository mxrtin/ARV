/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#include "pch.h"

#include "VideoBackground.h"

#include <iostream>
#include <memory>
#include "DirectXHelper.h"
#include "RenderUtil.h"
#include "SampleUtil.h"

#include <Vuforia\CameraDevice.h>
#include <Vuforia\Device.h>
#include <Vuforia\DXRenderer.h>
#include <Vuforia\Renderer.h>
#include <Vuforia\VideoBackgroundConfig.h>
#include <Vuforia\VideoBackgroundTextureInfo.h>
#include <Vuforia\Tool.h>

using namespace DirectX;
using namespace SampleCommon;

static const float VIRTUAL_FOV_Y_DEGS = 85.0f;
static const float M_PI = 3.14159f;

    
VideoBackground::VideoBackground(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
    m_deviceResources(deviceResources),
    m_vbMeshIndexCount(0),
    m_vbMeshVertices(nullptr),
    m_setVideoBackgroundTexture(true)
{
}

VideoBackground::~VideoBackground()
{
    ReleaseResources();
}

void VideoBackground::InitVertexShader(const void *shaderByteCode, SIZE_T byteCodeLength)
{
    DX::ThrowIfFailed(
        m_deviceResources->GetD3DDevice()->CreateVertexShader(
            shaderByteCode,
            byteCodeLength,
            nullptr,
            &m_vbVertexShader
        )
    );

    static const D3D11_INPUT_ELEMENT_DESC vertexDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    DX::ThrowIfFailed(
        m_deviceResources->GetD3DDevice()->CreateInputLayout(
            vertexDesc,
            ARRAYSIZE(vertexDesc),
            shaderByteCode,
            byteCodeLength,
            &m_vbInputLayout
        )
    );
}

void VideoBackground::InitFragmentShader(const void *shaderByteCode, SIZE_T byteCodeLength)
{
    DX::ThrowIfFailed(
        m_deviceResources->GetD3DDevice()->CreatePixelShader(
            shaderByteCode,
            byteCodeLength,
            nullptr,
            &m_vbPixelShader
        )
    );

    CD3D11_BUFFER_DESC constantBufferDesc(
        sizeof(ProjectionConstantBuffer),
        D3D11_BIND_CONSTANT_BUFFER);

    DX::ThrowIfFailed(
        m_deviceResources->GetD3DDevice()->CreateBuffer(
            &constantBufferDesc,
            nullptr,
            &m_vbConstantBuffer
        )
    );
}

void VideoBackground::InitRenderState()
{
    // setup the rasterizer
    auto context = m_deviceResources->GetD3DDeviceContext();

    ID3D11Device *device;
    context->GetDevice(&device);

    // Create the rasterizers for video background rendering, two are created one for
    // the case where reflection is on and off and that is typically when using the rear
    // or front facing camera.
    D3D11_RASTERIZER_DESC videoRasterDescCounterClockwise = RenderUtil::CreateRasterizerDesc(
        D3D11_FILL_SOLID, // rendering
        D3D11_CULL_NONE, // culling
        true, // Counter Clockwise front face
        false //no depth clipping
    );
    device->CreateRasterizerState(&videoRasterDescCounterClockwise, m_vbRasterStateCounterClockwise.GetAddressOf());

    D3D11_RASTERIZER_DESC videoRasterDescClockwise = RenderUtil::CreateRasterizerDesc(
        D3D11_FILL_SOLID, // rendering
        D3D11_CULL_NONE, // culling
        false, // Clockwise front face
        false //no depth clipping
    );
    device->CreateRasterizerState(&videoRasterDescClockwise, m_vbRasterStateClockwise.GetAddressOf());

    // Create Depth-Stencil State without depth testing
    // for video background rendering
    D3D11_DEPTH_STENCIL_DESC videoDepthStencilDesc = RenderUtil::CreateDepthStencilDesc(
        false,
        D3D11_DEPTH_WRITE_MASK_ZERO,
        D3D11_COMPARISON_ALWAYS
    );
    device->CreateDepthStencilState(&videoDepthStencilDesc, m_vbDepthStencilState.GetAddressOf());

    // Create blend state for video background rendering
    D3D11_BLEND_DESC videoBackgroundBlendDesc = RenderUtil::CreateBlendDesc(false);
    device->CreateBlendState(&videoBackgroundBlendDesc, m_vbBlendState.GetAddressOf());
}

void VideoBackground::InitMesh(const Vuforia::Mesh &vbMesh, ID3D11Device *device)
{
    // Setup the vertex and index buffers
    const int numVertices = vbMesh.getNumVertices();
    const int numTriangles = vbMesh.getNumTriangles();
    m_vbMeshIndexCount = numTriangles * 3;

    const Vuforia::Vec3F *vbVertices = vbMesh.getPositions();
    const Vuforia::Vec2F *vbTexCoords = vbMesh.getUVs();
    const unsigned short *vbIndices = vbMesh.getTriangles();

    if (m_vbMeshVertices != nullptr)
    {
        delete[] m_vbMeshVertices;
        m_vbMeshVertices = nullptr;
    }

    m_vbMeshVertices = new TexturedVertex[numVertices];

    for (int i = 0; i < numVertices; ++i)
    {
        m_vbMeshVertices[i].pos = DirectX::XMFLOAT3(
            vbVertices[i].data[0],
            vbVertices[i].data[1],
            vbVertices[i].data[2]
        );

        m_vbMeshVertices[i].texcoord = DirectX::XMFLOAT2(
            vbTexCoords[i].data[0],
            vbTexCoords[i].data[1]
        );
    }

    D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
    vertexBufferData.pSysMem = m_vbMeshVertices;
    vertexBufferData.SysMemPitch = 0;
    vertexBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC vertexBufferDesc(numVertices * sizeof(TexturedVertex), D3D11_BIND_VERTEX_BUFFER);
    DX::ThrowIfFailed(
        device->CreateBuffer(
            &vertexBufferDesc,
            &vertexBufferData,
            &m_vbVertexBuffer
        )
    );

    D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
    indexBufferData.pSysMem = vbIndices;
    indexBufferData.SysMemPitch = 0;
    indexBufferData.SysMemSlicePitch = 0;

    CD3D11_BUFFER_DESC indexBufferDesc(
        m_vbMeshIndexCount * sizeof(unsigned short),
        D3D11_BIND_INDEX_BUFFER
    );

    DX::ThrowIfFailed(
        device->CreateBuffer(
            &indexBufferDesc,
            &indexBufferData,
            &m_vbIndexBuffer
        )
    );
}

void VideoBackground::ResetForNewRenderingPrimitives()
{
    if (m_vbTexture != nullptr)
    {
        m_vbTexture->ReleaseResources();
        m_vbTexture.reset();
        m_setVideoBackgroundTexture = true;
    }
}

void VideoBackground::Render(
        Vuforia::Renderer &renderer, 
        Vuforia::RenderingPrimitives *renderPrimitives,
        Vuforia::VIEW viewId)
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto device = m_deviceResources->GetD3DDevice();

    if (m_vbTexture == nullptr) {
        m_vbTexture = std::shared_ptr<VideoBackgroundTexture>(
            new VideoBackgroundTexture(m_deviceResources));
    }

    if (!m_vbTexture->IsInitialized())
    {
        // Init the video background texture
        Vuforia::Vec2I texSize = renderPrimitives->getVideoBackgroundTextureSize();
        m_vbTexture->Init(texSize.data[0], texSize.data[1]);

        // Initialize the video background mesh
        const Vuforia::Mesh &vbMesh = renderPrimitives->getVideoBackgroundMesh(viewId);
        InitMesh(vbMesh, device);
    }

    if (m_setVideoBackgroundTexture && m_vbTexture->IsInitialized())
    {
        // Hand over a texture to Vuforia, it will then render into this texture each
        // time updateVideoBackgroundTexture is called. Note this texture is stored per
        // camera so we need to do it when the camera is changed.
        ID3D11Texture2D* vbD3DTexture = m_vbTexture->GetD3DTexture().Get();
        renderer.setVideoBackgroundTexture(Vuforia::DXTextureData(vbD3DTexture));
        m_setVideoBackgroundTexture = false;
    }

    // Update the camera video-background texture
    if (!renderer.updateVideoBackgroundTexture(nullptr))
    {
        SampleUtil::Log(L"ImageTargetsRenderer", L"Unable to update video background texture");
        return;
    }

    // Setup rendering pipeline for video background rendering
    if (Vuforia::Renderer::getInstance().getVideoBackgroundConfig().mReflection == Vuforia::VIDEO_BACKGROUND_REFLECTION_ON)
    {
        context->RSSetState(m_vbRasterStateClockwise.Get()); // Typically when using the front facing camera
    }
    else
    {
        context->RSSetState(m_vbRasterStateCounterClockwise.Get()); // Typically when using the rear facing camera
    }

    context->OMSetDepthStencilState(m_vbDepthStencilState.Get(), 1);
    context->OMSetBlendState(m_vbBlendState.Get(), NULL, 0xffffffff);

    // Get the Vuforia video-background projection matrix
    Vuforia::Matrix34F vbProjection = renderPrimitives->getVideoBackgroundProjectionMatrix(
        viewId, Vuforia::COORDINATE_SYSTEM_CAMERA);

    // Convert the matrix to 4x4 format
    Vuforia::Matrix44F vbProjection44F = Vuforia::Tool::convert2GLMatrix(vbProjection);

    // Convert the matrix to XMFLOAT4X4 format
    XMFLOAT4X4 vbProjectionDX;
    memcpy(vbProjectionDX.m, vbProjection44F.data, sizeof(float) * 16);
    XMStoreFloat4x4(&vbProjectionDX, XMMatrixTranspose(XMLoadFloat4x4(&vbProjectionDX)));

    // Convert the XMFLOAT4X4 to XMMATRIX
    auto vbProjectionMatrix = XMLoadFloat4x4(&vbProjectionDX);

    // Apply the scene scale on video see-through eyewear, to scale the video background and augmentation
    // so that the display lines up with the real world
    // This should not be applied on optical see-through devices, as there is no video background,
    // and the calibration ensures that the augmentation matches the real world
    if (Vuforia::Device::getInstance().isViewerActive())
    {
        float sceneScaleFactor = (float)GetSceneScaleFactor();
        auto scaleMatrix = XMMatrixScaling(sceneScaleFactor, sceneScaleFactor, 1.0f);

        // Multiply the video background projection matrix by the scaling matrix
        vbProjectionMatrix *= scaleMatrix;
    }

    // Set the projection matrix
    XMStoreFloat4x4(&m_vbConstantBufferData.projection, vbProjectionMatrix);

    // Prepare the constant buffer to send it to the graphics device.
    context->UpdateSubresource1(
        m_vbConstantBuffer.Get(),
        0,
        NULL,
        &m_vbConstantBufferData,
        0,
        0,
        0
    );

    // Each vertex is one instance of the TexturedVertex struct.
    UINT stride = sizeof(TexturedVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(
        0,
        1,
        m_vbVertexBuffer.GetAddressOf(),
        &stride,
        &offset
    );

    context->IASetIndexBuffer(
        m_vbIndexBuffer.Get(),
        DXGI_FORMAT_R16_UINT, // Each index is one 16-bit unsigned integer (short).
        0
    );

    context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->IASetInputLayout(m_vbInputLayout.Get());

    // Attach our vertex shader.
    context->VSSetShader(m_vbVertexShader.Get(), nullptr, 0);

    // Send the constant buffer to the graphics device.
    context->VSSetConstantBuffers1(
        0,
        1,
        m_vbConstantBuffer.GetAddressOf(),
        nullptr,
        nullptr
    );

    // Attach our pixel shader.
    context->PSSetShader(m_vbPixelShader.Get(), nullptr, 0);

    // Set the texture in the shader
    context->PSSetSamplers(0, 1, m_vbTexture->GetD3DSamplerState().GetAddressOf());
    context->PSSetShaderResources(0, 1, m_vbTexture->GetD3DTextureView().GetAddressOf());

    // Draw the objects.
    context->DrawIndexed(m_vbMeshIndexCount, 0, 0);

    // Clear the shader resources, as the video background texture is now 
    // the input for the next stage of rendering
    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    context->PSSetShaderResources(0, 1, nullSRV);
}

double VideoBackground::GetSceneScaleFactor()
{
    // Get the y-dimension of the physical camera field of view
    Vuforia::Vec2F fovVector = Vuforia::CameraDevice::getInstance().getCameraCalibration().getFieldOfViewRads();
    float cameraFovYRads = fovVector.data[1];

    // Get the y-dimension of the virtual camera field of view
    float virtualFovYRads = VIRTUAL_FOV_Y_DEGS * M_PI / 180;

    // The scene-scale factor represents the proportion of the viewport that is filled by
    // the video background when projected onto the same plane.
    // In order to calculate this, let 'd' be the distance between the cameras and the plane.
    // The height of the projected image 'h' on this plane can then be calculated:
    //   tan(fov/2) = h/2d
    // which rearranges to:
    //   2d = h/tan(fov/2)
    // Since 'd' is the same for both cameras, we can combine the equations for the two cameras:
    //   hPhysical/tan(fovPhysical/2) = hVirtual/tan(fovVirtual/2)
    // Which rearranges to:
    //   hPhysical/hVirtual = tan(fovPhysical/2)/tan(fovVirtual/2)
    // ... which is the scene-scale factor
    return tan(cameraFovYRads / 2) / tan(virtualFovYRads / 2);
}

void VideoBackground::ReleaseResources()
{
    m_vbTexture->ReleaseResources();
    m_vbTexture.reset();

    m_vbInputLayout.Reset();
    m_vbVertexShader.Reset();
    m_vbPixelShader.Reset();
    m_vbConstantBuffer.Reset();
    if (m_vbMeshVertices != nullptr) {
        delete[] m_vbMeshVertices;
        m_vbMeshVertices = nullptr;
    }
}

void VideoBackground::SetVideoBackgroundTexture()
{
    // Update the VideoBackgroundTexture passed to Vuforia, for example on a camera swap
    m_setVideoBackgroundTexture = true;
}
