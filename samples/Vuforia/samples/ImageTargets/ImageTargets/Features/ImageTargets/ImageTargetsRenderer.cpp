/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#include "pch.h"
#include "ImageTargetsRenderer.h"
#include "..\..\Common\DirectXHelper.h"
#include "..\..\Common\SampleUtil.h"
#include "..\..\Common\RenderUtil.h"

#include <Vuforia\Vuforia.h>
#include <Vuforia\Vuforia_UWP.h>
#include <Vuforia\CameraDevice.h>
#include <Vuforia\Device.h>
#include <Vuforia\DXRenderer.h>
#include <Vuforia\Renderer.h>
#include <Vuforia\Tool.h>
#include <Vuforia\VideoBackgroundConfig.h>
#include <Vuforia\VideoBackgroundTextureInfo.h>
#include <Vuforia\State.h>
#include <Vuforia\Trackable.h>
#include <Vuforia\TrackableResult.h>
#include <Vuforia\ImageTarget.h>

using namespace ImageTargets;
using namespace DirectX;
using namespace Windows::Foundation;
using namespace Microsoft::WRL;

static const float TEAPOT_SCALE = 0.003f;
static const float TOWER_SCALE = 0.012f;

static const float VIRTUAL_FOV_Y_DEGS = 85.0f;
static const float M_PI = 3.14159f;


// Loads vertex and pixel shaders from files, create the teapot mesh and load the textures.
ImageTargetsRenderer::ImageTargetsRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
    m_deviceResources(deviceResources),
    m_rendererInitialized(false),
    m_vuforiaInitialized(false),
    m_vuforiaStarted(false),
    m_extTracking(false)
{
    memset(&m_cameraProjection, 0, sizeof(float) * 16);
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}

void ImageTargetsRenderer::SetVuforiaStarted(bool started)
{
    m_vuforiaStarted = started;
    if (started) {
        UpdateRenderingPrimitives();
    }
    else
    {
        m_videoBackground->SetVideoBackgroundTexture();
    }
}

// Initializes view parameters when the window size changes.
void ImageTargetsRenderer::CreateWindowSizeDependentResources()
{
    if (m_vuforiaStarted) {
        UpdateRenderingPrimitives();
    }
}

void ImageTargetsRenderer::UpdateRenderingPrimitives()
{
    Concurrency::critical_section::scoped_lock lock(m_renderingPrimitivesLock);
    if (m_renderingPrimitives != nullptr)
    {
        m_renderingPrimitives.reset();
    }
    m_renderingPrimitives = std::shared_ptr<Vuforia::RenderingPrimitives>(
        new Vuforia::RenderingPrimitives(Vuforia::Device::getInstance().getRenderingPrimitives())
        );

    // Calculate the DX Projection matrix
    auto projectionMatrix = Vuforia::Tool::convertPerspectiveProjection2GLMatrix(
        m_renderingPrimitives->getProjectionMatrix(Vuforia::VIEW_SINGULAR, Vuforia::COORDINATE_SYSTEM_CAMERA), 
        m_near, m_far);

    XMFLOAT4X4 dxProjection;
    memcpy(dxProjection.m, projectionMatrix.data, sizeof(float) * 16);
    XMStoreFloat4x4(&dxProjection, XMMatrixTranspose(XMLoadFloat4x4(&dxProjection)));

    m_cameraProjection = dxProjection;

    m_videoBackground->ResetForNewRenderingPrimitives();
}

// Called once per frame
void ImageTargetsRenderer::Update(SampleCommon::StepTimer const& timer)
{    
}

// Renders one frame using the vertex and pixel shaders.
void ImageTargetsRenderer::Render()
{
    // Vuforia initialization and data loading is asynchronous.
    // Only starts rendering after Vuforia init/loading is complete.
    if (!m_rendererInitialized || !m_vuforiaStarted)
    {
        return;
    }

    // Get the state from Vuforia and mark the beginning of a rendering section
    Vuforia::DXRenderData dxRenderData(m_deviceResources->GetD3DDevice());
    Vuforia::Renderer &vuforiaRenderer = Vuforia::Renderer::getInstance();
    Vuforia::State state = vuforiaRenderer.begin(&dxRenderData);

    RenderScene(vuforiaRenderer, state);

    Vuforia::Renderer::getInstance().end();
}

void ImageTargetsRenderer::RenderScene(Vuforia::Renderer &renderer, Vuforia::State &state)
{
    Concurrency::critical_section::scoped_lock lock(m_renderingPrimitivesLock);

    Vuforia::ViewList &viewList = m_renderingPrimitives->getRenderingViews();
    if (!viewList.contains(Vuforia::VIEW::VIEW_SINGULAR))
    {
        SampleCommon::SampleUtil::Log("ImageTargetsRenderer", "Monocular view not found.");
        return;
    }

    auto context = m_deviceResources->GetD3DDeviceContext();
    XMMATRIX xmCameraProjection = XMLoadFloat4x4(&m_cameraProjection);

    for (size_t v = 0; v < viewList.getNumViews(); v++) 
    {
        Vuforia::VIEW viewId = viewList.getView(static_cast<int>(v));

        // Apply the appropriate eye adjustment to the raw projection matrix, and assign to the global variable
        Vuforia::Matrix44F eyeAdjustment44F = Vuforia::Tool::convert2GLMatrix(
            m_renderingPrimitives->getEyeDisplayAdjustmentMatrix(viewId));

        // Convert the matrix to XMFLOAT4X4 format
        XMFLOAT4X4 eyeAdjustmentDX;
        memcpy(eyeAdjustmentDX.m, eyeAdjustment44F.data, sizeof(float) * 16);
        XMStoreFloat4x4(&eyeAdjustmentDX, XMMatrixTranspose(XMLoadFloat4x4(&eyeAdjustmentDX)));
        XMMATRIX eyeAdjustmentMatrix = XMLoadFloat4x4(&eyeAdjustmentDX);

        // Compute final adjusted projection matrix
        XMMATRIX projectionMatrix = xmCameraProjection * eyeAdjustmentMatrix;

        if (viewId == Vuforia::VIEW::VIEW_SINGULAR)
        {
            // Render the camera video background
            m_videoBackground->Render(renderer, m_renderingPrimitives.get(), viewId);

            // Setup rendering pipeline for augmentation rendering
            if (renderer.getVideoBackgroundConfig().mReflection == Vuforia::VIDEO_BACKGROUND_REFLECTION_ON)
            {
                context->RSSetState(m_augmentationRasterStateCullFront.Get()); // Typically when using the front facing camera
            }
            else
            {
                context->RSSetState(m_augmentationRasterStateCullBack.Get()); // Typically when using the rear facing camera
            }

            context->OMSetDepthStencilState(m_augmentationDepthStencilState.Get(), 1);
            context->OMSetBlendState(m_augmentationBlendState.Get(), NULL, 0xffffffff);

            for (int tIdx = 0; tIdx < state.getNumTrackableResults(); tIdx++)
            {
                // Get the trackable:
                const Vuforia::TrackableResult *result = state.getTrackableResult(tIdx);
                const Vuforia::Trackable &trackable = result->getTrackable();
                const char* trackableName = trackable.getName();

                // Set up the modelview matrix
                auto poseGL = Vuforia::Tool::convertPose2GLMatrix(result->getPose());
                XMFLOAT4X4 poseDX;
                memcpy(poseDX.m, poseGL.data, sizeof(float) * 16);
                XMStoreFloat4x4(&poseDX, XMMatrixTranspose(XMLoadFloat4x4(&poseDX)));
                XMMATRIX poseMatrix = XMLoadFloat4x4(&poseDX);

                std::shared_ptr<SampleCommon::Texture> texture = GetAugmentationTexture(trackable.getName());
                if (!texture->IsInitialized()) {
                    texture->Init();
                }

                if (m_extTracking) {
                    RenderTower(poseMatrix, projectionMatrix, texture);
                }
                else {
                    RenderTeapot(poseMatrix, projectionMatrix, texture);
                }
            }
        }
    }
}

void ImageTargetsRenderer::RenderTeapot(
    const XMMATRIX &poseMatrix,
    const XMMATRIX &projectionMatrix,
    const std::shared_ptr<SampleCommon::Texture> texture
    )
{
    auto context = m_deviceResources->GetD3DDeviceContext();
 
    auto scale = XMMatrixScaling(TEAPOT_SCALE, TEAPOT_SCALE, TEAPOT_SCALE);
    auto modelMatrix = XMMatrixIdentity() * scale;

    // Set the model matrix (the 'model' part of the 'model-view' matrix)
    XMStoreFloat4x4(&m_augmentationConstantBufferData.model, modelMatrix);

    // Set the pose matrix (the 'view' part of the 'model-view' matrix)
    XMStoreFloat4x4(&m_augmentationConstantBufferData.view, poseMatrix);

    // Set the projection matrix
    XMStoreFloat4x4(&m_augmentationConstantBufferData.projection, projectionMatrix);

    // Prepare the constant buffer to send it to the graphics device.
    context->UpdateSubresource1(
        m_augmentationConstantBuffer.Get(),
        0,
        NULL,
        &m_augmentationConstantBufferData,
        0,
        0,
        0
        );

    // Each vertex is one instance of the TexturedVertex struct.
    UINT stride = sizeof(SampleCommon::TexturedVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(
        0,
        1,
        m_teapotMesh->GetVertexBuffer().GetAddressOf(),
        &stride,
        &offset
        );

    context->IASetIndexBuffer(
        m_teapotMesh->GetIndexBuffer().Get(),
        DXGI_FORMAT_R16_UINT, // Each index is one 16-bit unsigned integer (short).
        0
        );

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->IASetInputLayout(m_augmentationInputLayout.Get());

    // Attach our vertex shader.
    context->VSSetShader(m_augmentationVertexShader.Get(), nullptr, 0);

    // Send the constant buffer to the graphics device.
    context->VSSetConstantBuffers1(
        0,
        1,
        m_augmentationConstantBuffer.GetAddressOf(),
        nullptr,
        nullptr
        );

    // Attach our pixel shader.
    context->PSSetShader(m_augmentationPixelShader.Get(), nullptr, 0);

    context->PSSetSamplers(0, 1, texture->GetD3DSamplerState().GetAddressOf());
    context->PSSetShaderResources(0, 1, texture->GetD3DTextureView().GetAddressOf());

    // Draw the objects.
    context->DrawIndexed(m_teapotMesh->GetIndexCount(), 0, 0);
}

void ImageTargetsRenderer::RenderTower(
    const XMMATRIX &poseMatrix,
    const XMMATRIX &projectionMatrix,
    const std::shared_ptr<SampleCommon::Texture> texture
    )
{
    auto context = m_deviceResources->GetD3DDeviceContext();

    // Set pose matrix (the 'view' part of the 'model-view' matrix)
    XMStoreFloat4x4(&m_augmentationConstantBufferData.view, poseMatrix);

    // Set model matrix (the 'model' part of the 'model-view' matrix)
    auto scale = XMMatrixScaling(TOWER_SCALE, TOWER_SCALE, TOWER_SCALE);
    auto rotation = XMMatrixTranspose(XMMatrixRotationX(3.14159f / 2));
    auto modelMatrix = XMMatrixIdentity() * rotation * scale;
    XMStoreFloat4x4(&m_augmentationConstantBufferData.model, modelMatrix);

    // Set projection matrix
    XMStoreFloat4x4(&m_augmentationConstantBufferData.projection, projectionMatrix);

    // Prepare the constant buffer to send it to the graphics device.
    context->UpdateSubresource1(
        m_augmentationConstantBuffer.Get(),
        0,
        NULL,
        &m_augmentationConstantBufferData,
        0,
        0,
        0
        );

    // Each vertex is one instance of the TexturedVertex struct.
    UINT stride = sizeof(SampleCommon::TexturedVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(
        0,
        1,
        m_towerModel->GetVertexBuffer().GetAddressOf(),
        &stride,
        &offset
        );

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->IASetInputLayout(m_augmentationInputLayout.Get());

    // Attach our vertex shader.
    context->VSSetShader(m_augmentationVertexShader.Get(), nullptr, 0);

    // Send the constant buffer to the graphics device.
    context->VSSetConstantBuffers1(
        0,
        1,
        m_augmentationConstantBuffer.GetAddressOf(),
        nullptr,
        nullptr
        );

    // Attach our pixel shader.
    context->PSSetShader(m_augmentationPixelShader.Get(), nullptr, 0);

    context->PSSetSamplers(0, 1, texture->GetD3DSamplerState().GetAddressOf());
    context->PSSetShaderResources(0, 1, texture->GetD3DTextureView().GetAddressOf());

    // Draw the objects.
    context->Draw(m_towerModel->GetVertexCount(), 0);
}

const std::shared_ptr<SampleCommon::Texture> ImageTargetsRenderer::GetAugmentationTexture(const char *targetName)
{
    if (m_extTracking) {
        return m_textureTower;
    }

    // Choose the texture based on the target name:
    if (strcmp(targetName, "chips") == 0)
    {
        return m_textureTeapotBrass;
    }
    else if (strcmp(targetName, "stones") == 0)
    {
        return m_textureTeapotBlue;
    }
    else
    {
        return m_textureTeapotRed;
    }
}

void ImageTargetsRenderer::CreateDeviceDependentResources()
{
    m_rendererInitialized = false;

    m_videoBackground = std::shared_ptr<SampleCommon::VideoBackground>(
        new SampleCommon::VideoBackground(m_deviceResources));

    // Load shaders asynchronously.
    auto loadVSTask = DX::ReadDataAsync(L"TexturedVertexShader.cso");
    auto loadPSTask = DX::ReadDataAsync(L"TexturedPixelShader.cso");
    auto loadVideoBgVSTask = DX::ReadDataAsync(L"VideoBackgroundVertexShader.cso");
    auto loadVideoBgPSTask = DX::ReadDataAsync(L"VideoBackgroundPixelShader.cso");

    // After the vertex shader file is loaded, create the shader and input layout.
    auto createVSTask = loadVSTask.then([this](const std::vector<byte>& fileData) {
        DX::ThrowIfFailed(
            m_deviceResources->GetD3DDevice()->CreateVertexShader(
                &fileData[0],
                fileData.size(),
                nullptr,
                &m_augmentationVertexShader
                )
            );

        static const D3D11_INPUT_ELEMENT_DESC vertexDesc [] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        DX::ThrowIfFailed(
            m_deviceResources->GetD3DDevice()->CreateInputLayout(
                vertexDesc,
                ARRAYSIZE(vertexDesc),
                &fileData[0],
                fileData.size(),
                &m_augmentationInputLayout
                )
            );
    });

    auto createVideoBgVSTask = loadVideoBgVSTask.then([this](const std::vector<byte>& fileData) {
        m_videoBackground->InitVertexShader(&fileData[0], fileData.size());
    });

    // After the pixel shader file is loaded, create the shader and constant buffer.
    auto createPSTask = loadPSTask.then([this](const std::vector<byte>& fileData) {
        DX::ThrowIfFailed(
            m_deviceResources->GetD3DDevice()->CreatePixelShader(
                &fileData[0],
                fileData.size(),
                nullptr,
                &m_augmentationPixelShader
                )
            );

        CD3D11_BUFFER_DESC constantBufferDesc(
            sizeof(SampleCommon::ModelViewProjectionConstantBuffer), 
            D3D11_BIND_CONSTANT_BUFFER);

        DX::ThrowIfFailed(
            m_deviceResources->GetD3DDevice()->CreateBuffer(
                &constantBufferDesc,
                nullptr,
                &m_augmentationConstantBuffer
                )
            );
    });

    // After the pixel shader file is loaded, create the shader and constant buffer.
    auto createVideoBgPSTask = loadVideoBgPSTask.then([this](const std::vector<byte>& fileData) {
        m_videoBackground->InitFragmentShader(&fileData[0], fileData.size());
    });

    // Once both shaders are loaded, create the mesh.
    auto createAugmentationModelsTask = (createPSTask && createVSTask && createVideoBgPSTask && createVideoBgVSTask).then([this] () {
        m_teapotMesh = std::shared_ptr<SampleCommon::TeapotMesh>(
            new SampleCommon::TeapotMesh(m_deviceResources));
        m_teapotMesh->InitMesh();

        m_towerModel = std::shared_ptr<SampleCommon::SampleApp3DModel>(
            new SampleCommon::SampleApp3DModel(m_deviceResources, "Assets/ImageTargets/buildings.txt"));
        m_towerModel->InitMesh();
    });

    auto createTextureTask = createAugmentationModelsTask.then([this]() {
        m_textureTeapotBlue = std::shared_ptr<SampleCommon::Texture>(new SampleCommon::Texture(m_deviceResources));
        m_textureTeapotBlue->CreateFromFile(L"Assets/TextureTeapotBlue.png");

        m_textureTeapotBrass = std::shared_ptr<SampleCommon::Texture>(new SampleCommon::Texture(m_deviceResources));
        m_textureTeapotBrass->CreateFromFile(L"Assets/TextureTeapotBrass.png");

        m_textureTeapotRed = std::shared_ptr<SampleCommon::Texture>(new SampleCommon::Texture(m_deviceResources));
        m_textureTeapotRed->CreateFromFile(L"Assets/TextureTeapotRed.png");

        m_textureTower = std::shared_ptr<SampleCommon::Texture>(new SampleCommon::Texture(m_deviceResources));
        m_textureTower->CreateFromFile(L"Assets/ImageTargets/building_texture.jpeg");
    });

    auto setupRasterizersTask = createTextureTask.then([this]() {
        // setup the rasterizer
        auto context = m_deviceResources->GetD3DDeviceContext();

        ID3D11Device *device;
        context->GetDevice(&device);

        // Init rendering pipeline state for video background
        m_videoBackground->InitRenderState();

        // Create the rasterizer for augmentation rendering
        // with back-face culling
        D3D11_RASTERIZER_DESC augmentationRasterDescCullBack = SampleCommon::RenderUtil::CreateRasterizerDesc(
            D3D11_FILL_SOLID, // solid rendering
            D3D11_CULL_BACK, // culling
            true, // Counter clockwise front face
            true // depth clipping enabled
            );
        device->CreateRasterizerState(&augmentationRasterDescCullBack, m_augmentationRasterStateCullBack.GetAddressOf());

        // Create the rasterizer for augmentation rendering,
        // with front-face culling
        D3D11_RASTERIZER_DESC augmentationRasterDescCullFront = SampleCommon::RenderUtil::CreateRasterizerDesc(
            D3D11_FILL_SOLID, // solid rendering
            D3D11_CULL_NONE, // culling
            false, // Clockwise front face
            true // depth clipping enabled
        );
        device->CreateRasterizerState(&augmentationRasterDescCullFront, m_augmentationRasterStateCullFront.GetAddressOf());

        // Create Depth-Stencil State with depth testing ON,
        // for augmentation rendering
        D3D11_DEPTH_STENCIL_DESC augmentDepthStencilDesc = SampleCommon::RenderUtil::CreateDepthStencilDesc(
            true,
            D3D11_DEPTH_WRITE_MASK_ALL,
            D3D11_COMPARISON_LESS
            );
        device->CreateDepthStencilState(&augmentDepthStencilDesc, m_augmentationDepthStencilState.GetAddressOf());

        // Create blend state for augmentation rendering
        bool translucentAugmentation = false;// set to TRUE for rendering translucent models
        D3D11_BLEND_DESC augmentationBlendDesc = SampleCommon::RenderUtil::CreateBlendDesc(translucentAugmentation);
        device->CreateBlendState(&augmentationBlendDesc, m_augmentationBlendState.GetAddressOf());
    });

    setupRasterizersTask.then([this](Concurrency::task<void> t) {
        try
        {
            // If any exceptions were thrown back in the async chain then
            // this call throws that exception here and we can catch it below
            t.get();

            // Now we are ready for rendering
            m_rendererInitialized = true;
        }
        catch (Platform::Exception ^ex) {
            SampleCommon::SampleUtil::ShowError(L"Renderer init error", ex->Message);
        }
    });
}

void ImageTargetsRenderer::ReleaseDeviceDependentResources()
{
    m_rendererInitialized = false;

    m_videoBackground->ReleaseResources();
    m_videoBackground.reset();

    m_augmentationInputLayout.Reset();
    m_augmentationVertexShader.Reset();
    m_augmentationPixelShader.Reset();
    m_augmentationConstantBuffer.Reset();

    m_teapotMesh->ReleaseResources();
    m_towerModel->ReleaseResources();
    m_textureTeapotBlue->ReleaseResources();
    m_textureTeapotBrass->ReleaseResources();
    m_textureTeapotRed->ReleaseResources();
    m_textureTower->ReleaseResources();
    
    m_renderingPrimitives.reset();
}
