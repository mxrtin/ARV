/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#include "pch.h"

#include "VuMarkRenderer.h"
#include "..\..\Common\DirectXHelper.h"
#include "..\..\Common\SampleUtil.h"
#include "..\..\Common\RenderUtil.h"

#include <robuffer.h>

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
#include <Vuforia\VuMarkTarget.h>
#include <Vuforia\VuMarkTargetResult.h>

using namespace VuMark;
using namespace DirectX;
using namespace Windows::Foundation;
using namespace Microsoft::WRL;
using namespace Windows::Storage::Streams;
using namespace Windows::UI::Xaml::Media::Imaging;

static const float VUMARK_SCALE = 1.06f;

#define VUMARK_ID_MAX_LENGTH 100

float DistanceSquared(const Vuforia::Vec2F & p1, const Vuforia::Vec2F & p2) {
    return (float)(pow(p1.data[0] - p2.data[0], 2.0) + pow(p1.data[1] - p2.data[1], 2.0));
}

void ConvertInstanceIdForBytes(const Vuforia::InstanceId& instanceId, char* dest)
{
    const size_t MAXLEN = 100;
    const char * src = instanceId.getBuffer();
    size_t len = instanceId.getLength();

    static const char* hexTable = "0123456789abcdef";

    if (len * 2 + 1 > MAXLEN) {
        len = (MAXLEN - 1) / 2;
    }

    // Go in reverse so the string is readable left-to-right.
    size_t bufIdx = 0;
    for (int i = (int)(len - 1); i >= 0; i--)
    {
        char upper = hexTable[(src[i] >> 4) & 0xf];
        char lower = hexTable[(src[i] & 0xf)];
        dest[bufIdx++] = upper;
        dest[bufIdx++] = lower;
    }

    // null terminate the string.
    dest[bufIdx] = 0;
}

void ConvertInstanceIdToString(const Vuforia::InstanceId& instanceId, char dest[])
{
    switch (instanceId.getDataType()) {
    case Vuforia::InstanceId::BYTES:
        ConvertInstanceIdForBytes(instanceId, dest);
        break;
    case Vuforia::InstanceId::STRING:
        strcpy_s(dest, instanceId.getLength()+1, instanceId.getBuffer());
        break;
    case Vuforia::InstanceId::NUMERIC:
        sprintf_s(dest, instanceId.getLength(), "%I64u", instanceId.getNumericValue());
        break;
    default:
        sprintf_s(dest, sizeof("Unknown"), "Unknown");
    }
}

void GetInstanceType(const Vuforia::InstanceId& instanceId, char dest[])
{
    switch (instanceId.getDataType()) {
    case Vuforia::InstanceId::BYTES:
        sprintf_s(dest, sizeof("Bytes"), "Bytes");
        break;
    case Vuforia::InstanceId::STRING:
        sprintf_s(dest, sizeof("String"), "String");
        break;
    case Vuforia::InstanceId::NUMERIC:
        sprintf_s(dest, sizeof("Numeric"), "Numeric");
        break;
    default:
        sprintf_s(dest, sizeof("Unknown"), "Unknown");
    }
}

// Loads vertex and pixel shaders from files, create the teapot mesh and load the textures.
VuMarkRenderer::VuMarkRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
    m_deviceResources(deviceResources),
    m_rendererInitialized(false),
    m_vuforiaInitialized(false),
    m_vuforiaStarted(false),
    m_extTracking(false),
    m_currentTime(0),
    m_uiDispatcher(nullptr)
{
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}

void VuMarkRenderer::SetVuforiaStarted(bool started)
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
void VuMarkRenderer::CreateWindowSizeDependentResources()
{
    if (m_vuforiaStarted) {
        UpdateRenderingPrimitives();
    }
}

void VuMarkRenderer::UpdateRenderingPrimitives()
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
    
    m_projection = dxProjection;

    m_videoBackground->ResetForNewRenderingPrimitives();
}

// Called once per frame
void VuMarkRenderer::Update(SampleCommon::StepTimer const& timer)
{
    m_currentTime = timer.GetTotalSeconds();
}

// Renders one frame using the vertex and pixel shaders.
void VuMarkRenderer::Render()
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

    RenderReticle();

    Vuforia::Renderer::getInstance().end();
}

void VuMarkRenderer::RenderReticle()
{
    auto context = m_deviceResources->GetD3DDeviceContext();

    if (!m_reticleTexture->IsInitialized()) {
        m_reticleTexture->Init();
    }

    Size outputSize = m_deviceResources->GetOutputSize();
    float aspectRatio = outputSize.Width / outputSize.Height;
    float fovAngleY = 45.0f * XM_PI / 180.0f;

    // This sample makes use of a right-handed coordinate system using row-major matrices.
    XMMATRIX perspectiveMatrix = XMMatrixPerspectiveFovRH(
        fovAngleY,
        aspectRatio,
        m_near,
        m_far
    );

    XMFLOAT4X4 orientation = m_deviceResources->GetOrientationTransform3D();
    XMMATRIX orientationMatrix = XMLoadFloat4x4(&orientation);

    XMStoreFloat4x4(
        &m_constantBufferData.projection,
        XMMatrixTranspose(perspectiveMatrix * orientationMatrix)
    );

    // We place the reticle at the near clipping plane
    float reticleDistance = 1.1f * m_near;
    static const XMVECTORF32 eye = { 0.0f, 0.0f, reticleDistance, 0.0f };
    static const XMVECTORF32 at = { 0.0f, 0.0f, 0.0f, 0.0f };
    static const XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.0f };

    XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(XMMatrixLookAtRH(eye, at, up)));

    float reticleScale = reticleDistance * tan(fovAngleY/2);
    auto reticleScaleMatrix = XMMatrixScaling(reticleScale, reticleScale, reticleScale);
    XMStoreFloat4x4(&m_constantBufferData.model, XMMatrixTranspose(reticleScaleMatrix));

    // Set the color mask
    m_constantBufferData.colorMask = { 1.0F, 1.0F, 1.0F, 1.0F };

    // Prepare the constant buffer to send it to the graphics device.
    context->UpdateSubresource1(
        m_constantBuffer.Get(),
        0,
        NULL,
        &m_constantBufferData,
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
        m_quadMesh->GetVertexBuffer().GetAddressOf(),
        &stride,
        &offset
    );

    context->IASetIndexBuffer(
        m_quadMesh->GetIndexBuffer().Get(),
        DXGI_FORMAT_R16_UINT, // Each index is a 16-bit unsigned integer (short).
        0
    );

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->IASetInputLayout(m_inputLayout.Get());

    // Attach our vertex shader.
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);

    // Send the constant buffer to the graphics device.
    context->VSSetConstantBuffers1(
        0,
        1,
        m_constantBuffer.GetAddressOf(),
        nullptr,
        nullptr
    );

    // Attach our pixel shader.
    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    context->PSSetSamplers(0, 1, m_reticleTexture->GetD3DSamplerState().GetAddressOf());
    context->PSSetShaderResources(0, 1, m_reticleTexture->GetD3DTextureView().GetAddressOf());

    // Draw the objects.
    context->DrawIndexed(m_quadMesh->GetIndexCount(), 0, 0);
}

void VuMarkRenderer::RenderScene(Vuforia::Renderer &renderer, Vuforia::State &state)
{
    Concurrency::critical_section::scoped_lock lock(m_renderingPrimitivesLock);

    Vuforia::ViewList &viewList = m_renderingPrimitives->getRenderingViews();
    if (!viewList.contains(Vuforia::VIEW::VIEW_SINGULAR))
    {
        SampleCommon::SampleUtil::Log("VuMarksRenderer", "Monocular view not found.");
        return;
    }

    auto context = m_deviceResources->GetD3DDeviceContext();

    XMMATRIX xmProjection = XMLoadFloat4x4(&m_projection);

    bool gotVuMark = false;

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
        XMMATRIX projectionMatrix = xmProjection * eyeAdjustmentMatrix;

        if (viewId == Vuforia::VIEW::VIEW_SINGULAR)
        {
            // Render the camera video background
            m_videoBackground->Render(renderer, m_renderingPrimitives.get(), viewId);

            // Set state for augmentation rendering
            if (renderer.getVideoBackgroundConfig().mReflection == Vuforia::VIDEO_BACKGROUND_REFLECTION_ON)
                context->RSSetState(m_augmentationRasterStateCullFront.Get()); //Front camera
            else
                context->RSSetState(m_augmentationRasterState.Get()); //Back camera

            context->OMSetDepthStencilState(m_augmentationDepthStencilState.Get(), 1);
            context->OMSetBlendState(m_augmentationBlendState.Get(), NULL, 0xffffffff);

            int indexVuMarkToDisplay = -1;

            if (state.getNumTrackableResults() > 1) {
                float minimumDistance = FLT_MAX;
                const Vuforia::CameraCalibration& camCalib = Vuforia::CameraDevice::getInstance().getCameraCalibration();
                const Vuforia::Vec2F camSize = camCalib.getSize();
                const Vuforia::Vec2F camCenter = Vuforia::Vec2F(camSize.data[0] / 2.0f, camSize.data[1] / 2.0f);

                for (int tIdx = 0; tIdx < state.getNumTrackableResults(); ++tIdx) {
                    const Vuforia::TrackableResult* result = state.getTrackableResult(tIdx);
                    if (result->isOfType(Vuforia::VuMarkTargetResult::getClassType()))
                    {
                        Vuforia::Vec3F point = Vuforia::Vec3F(0.0, 0.0, 0.0);
                        Vuforia::Vec2F projPoint = Vuforia::Tool::projectPoint(
                            camCalib, result->getPose(), point);

                        float distance = DistanceSquared(projPoint, camCenter);
                        if (distance < minimumDistance) {
                            minimumDistance = distance;
                            indexVuMarkToDisplay = tIdx;
                        }
                    }
                }
            }

            std::string currVuMarkId;
            SampleCommon::SampleUtil::ToStdString(m_vuMarkView->GetCurrentVuMarkId(), currVuMarkId);

            for (int tIdx = 0; tIdx < state.getNumTrackableResults(); tIdx++)
            {
                // Get the trackable:
                const Vuforia::TrackableResult *result = state.getTrackableResult(tIdx);
                const Vuforia::Trackable &trackable = result->getTrackable();
                const char* trackableName = trackable.getName();

                if (result->isOfType(Vuforia::VuMarkTargetResult::getClassType()))
                {
                    gotVuMark = true;

                    const Vuforia::VuMarkTargetResult* vmResult = (const Vuforia::VuMarkTargetResult*)result;
                    const Vuforia::VuMarkTarget &vmTarget = vmResult->getTrackable();

                    // This boolean teels if the current VuMark is the 'main' one,
                    // i.e either the closest one to the camera center or the only one
                    bool isMainVumark = (indexVuMarkToDisplay < 0) || (indexVuMarkToDisplay == tIdx);

                    const Vuforia::VuMarkTemplate& vmTemplate = vmTarget.getTemplate();
                    const Vuforia::InstanceId & vmId = vmTarget.getInstanceId();
                    const Vuforia::Image & vmImage = vmTarget.getInstanceImage();

                    if (isMainVumark)
                    {
                        char vmId_cstr[VUMARK_ID_MAX_LENGTH + 1];
                        ConvertInstanceIdToString(vmId, vmId_cstr);

                        char vmType_cstr[16];
                        GetInstanceType(vmId, vmType_cstr);

                        // if the vumark has changed, we hide the card
                        // and reset the animation
                        if (strcmp(vmId_cstr, currVuMarkId.c_str()) != 0)
                        {
                            BlinkVumark(true);

                            // Hide the VuMark Card
                            if (m_uiDispatcher != nullptr)
                            {
                                m_uiDispatcher->RunAsync(
                                    Windows::UI::Core::CoreDispatcherPriority::Normal,
                                    ref new Windows::UI::Core::DispatchedHandler([this]()
                                {
                                    m_vuMarkView->HideVuMarkCard();
                                }));
                            }
                        }

                        Platform::String^ vmIdStr = SampleCommon::SampleUtil::ToPlatformString(vmId_cstr);
                        Platform::String^ vmTypeStr = SampleCommon::SampleUtil::ToPlatformString(vmType_cstr);

                        // build Bitmap from Vuforia::Image and pass it here
                        int vmImgWid = vmImage.getWidth();
                        int vmImgHgt = vmImage.getHeight();
                        m_vuMarkView->UpdateVuMarkInstance(
                            vmIdStr, vmTypeStr, vmImgWid, vmImgHgt,
                            (byte*)vmImage.getPixels()
                        );
                    }

                    // Set up the modelview matrix
                    auto poseGL = Vuforia::Tool::convertPose2GLMatrix(result->getPose());
                    XMFLOAT4X4 poseDX;
                    memcpy(poseDX.m, poseGL.data, sizeof(float) * 16);
                    XMStoreFloat4x4(&poseDX, XMMatrixTranspose(XMLoadFloat4x4(&poseDX)));
                    XMMATRIX xmPose = XMLoadFloat4x4(&poseDX);

                    float opacity = isMainVumark ? BlinkVumark(false) : 1.0f;
                    float vmOrigX = -vmTemplate.getOrigin().data[0];
                    float vmOrigY = -vmTemplate.getOrigin().data[1];
                    float vmWidth = vmTarget.getSize().data[0];
                    float vmHeight = vmTarget.getSize().data[1];
                    if (!m_augmentationTexture->IsInitialized()) {
                        m_augmentationTexture->Init();
                    }
                    RenderVuMark(vmOrigX, vmOrigY, vmWidth, vmHeight, xmPose, xmProjection, m_augmentationTexture, opacity);
                }
            }
        }
    }

    if (gotVuMark)
    {
        if (m_uiDispatcher != nullptr) 
        {
            m_uiDispatcher->RunAsync(
                Windows::UI::Core::CoreDispatcherPriority::Normal,
                ref new Windows::UI::Core::DispatchedHandler([this]()
            {
                m_vuMarkView->ShowVuMarkCard();
            }));
        }
    }
    else
    {
        // We reset the state of the animation so that
        // it triggers next time a vumark is detected
        BlinkVumark(true);
        // We also reset the value of the current value of the vumark on card
        // so that we hide and show the vumark if we redetect the same vumark
        m_vuMarkView->UpdateVuMarkInstance(nullptr, nullptr, 0, 0, nullptr);
    }
}

void VuMarkRenderer::RenderVuMark(
    float vuMarkOriginX,
    float vuMarkOriginY,
    float vuMarkWidth,
    float vuMarkHeight,
    const DirectX::XMMATRIX &poseMatrix,
    const DirectX::XMMATRIX &projectionMatrix,
    const std::shared_ptr<SampleCommon::Texture> texture,
    float opacity
    )
{
    auto context = m_deviceResources->GetD3DDeviceContext();

    ZeroMemory(&m_constantBufferData, sizeof(SampleCommon::ModelViewProjectionConstantBuffer));

    // Set the model matrix (the 'model' part of the 'model-view' matrix)
    auto translate = XMMatrixTranslation(-vuMarkOriginX, -vuMarkOriginY, 0.0f);
    auto scale = XMMatrixScaling(vuMarkWidth*VUMARK_SCALE, vuMarkHeight*VUMARK_SCALE, 1.0f);
    auto modelMatrix = XMMatrixTranspose(translate) * XMMatrixTranspose(scale);
    XMStoreFloat4x4(&m_constantBufferData.model, modelMatrix);

    // Set the pose matrix (the 'view' part of the 'model-view' matrix)
    XMStoreFloat4x4(&m_constantBufferData.view, poseMatrix);

    // Set the projection matrix
    XMStoreFloat4x4(&m_constantBufferData.projection, projectionMatrix);

    // Set the color mask
    m_constantBufferData.colorMask = {1.0F, 1.0F, 1.0F, opacity};

    // Prepare the constant buffer to send it to the graphics device.
    context->UpdateSubresource1(
        m_constantBuffer.Get(),
        0,
        NULL,
        &m_constantBufferData,
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
        m_quadMesh->GetVertexBuffer().GetAddressOf(),
        &stride,
        &offset
        );

    context->IASetIndexBuffer(
        m_quadMesh->GetIndexBuffer().Get(),
        DXGI_FORMAT_R16_UINT, // Each index is one 16-bit unsigned integer (short).
        0
        );

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->IASetInputLayout(m_inputLayout.Get());

    // Attach our vertex shader.
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);

    // Send the constant buffer to the graphics device.
    context->VSSetConstantBuffers1(
        0,
        1,
        m_constantBuffer.GetAddressOf(),
        nullptr,
        nullptr
        );

    // Attach our pixel shader.
    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    context->PSSetSamplers(0, 1, texture->GetD3DSamplerState().GetAddressOf());
    context->PSSetShaderResources(0, 1, texture->GetD3DTextureView().GetAddressOf());

    // Draw the objects.
    context->DrawIndexed(m_quadMesh->GetIndexCount(), 0, 0);
}

float VuMarkRenderer::BlinkVumark(bool reset)
{
    static double t0 = -1;
    if (reset || t0 < 0) {
        t0 = m_currentTime;
    }
    if (reset) {
        return 0.0f;
    }

    double delta = (m_currentTime - t0);
    if (delta > 1.0) {
        return 1.0;
    }
    if ((delta < 0.3) || (delta > 0.5 && delta < 0.8)) {
        return 1.0;
    }
    return 0.0;
}

void VuMarkRenderer::CreateDeviceDependentResources()
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
                &m_vertexShader
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
                &m_inputLayout
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
                &m_pixelShader
                )
            );

        CD3D11_BUFFER_DESC constantBufferDesc(sizeof(SampleCommon::ModelViewProjectionConstantBuffer) , D3D11_BIND_CONSTANT_BUFFER);
        DX::ThrowIfFailed(
            m_deviceResources->GetD3DDevice()->CreateBuffer(
                &constantBufferDesc,
                nullptr,
                &m_constantBuffer
                )
            );
    });

    // After the pixel shader file is loaded, create the shader and constant buffer.
    auto createVideoBgPSTask = loadVideoBgPSTask.then([this](const std::vector<byte>& fileData) {
        m_videoBackground->InitFragmentShader(&fileData[0], fileData.size());
    });

    // Once both shaders are loaded, create the mesh.
    auto createAugmentationModelsTask = (createPSTask && createVSTask).then([this] () {
        m_quadMesh = std::shared_ptr<SampleCommon::QuadMesh>(new SampleCommon::QuadMesh(m_deviceResources));
        m_quadMesh->InitMesh();
    });

    auto loadTextureTask = createAugmentationModelsTask.then([this]() {
        m_augmentationTexture = std::shared_ptr<SampleCommon::Texture>(
            new SampleCommon::Texture(m_deviceResources));
        m_augmentationTexture->CreateFromFile(L"Assets\\VuMark\\VuMarkOutline.png");

        m_reticleTexture = std::shared_ptr<SampleCommon::Texture>(
            new SampleCommon::Texture(m_deviceResources));
        m_reticleTexture->CreateFromFile(L"Assets\\VuMark\\reticle.png");
    });

    auto setupRasterizersTask = loadTextureTask.then([this]() {
        // setup the rasterizer
        auto context = m_deviceResources->GetD3DDeviceContext();

        ID3D11Device *device;
        context->GetDevice(&device);

        // Init rendering pipeline state for video background
        m_videoBackground->InitRenderState();

        // Create the rasterizer for video background rendering
        D3D11_RASTERIZER_DESC videoRasterDesc = SampleCommon::RenderUtil::CreateRasterizerDesc(
            D3D11_FILL_SOLID, // solid rendering
            D3D11_CULL_NONE, // no culling
            true, // CCW front face
            false //no depth clipping
            );
        device->CreateRasterizerState(&videoRasterDesc, m_videoRasterState.GetAddressOf());

        // Create the rasterizer for augmentation rendering,
        // with back-face culling
        D3D11_RASTERIZER_DESC augmentationRasterDesc = SampleCommon::RenderUtil::CreateRasterizerDesc(
            D3D11_FILL_SOLID, // solid rendering
            D3D11_CULL_BACK, // back face culling
            true, // CCW front face
            true // depth clipping enabled
            );
        device->CreateRasterizerState(&augmentationRasterDesc, m_augmentationRasterState.GetAddressOf());

        // Create the rasterizer for augmentation rendering,
        // with front-face culling
        D3D11_RASTERIZER_DESC augmentationRasterDescCullFront = SampleCommon::RenderUtil::CreateRasterizerDesc(
            D3D11_FILL_SOLID, // solid rendering
            D3D11_CULL_NONE, // culling
            false, // CW front face
            true // depth clipping enabled
        );
        device->CreateRasterizerState(&augmentationRasterDescCullFront, m_augmentationRasterStateCullFront.GetAddressOf());

        // Create Depth-Stencil State with depth testing ON,
        // for augmentation rendering
        D3D11_DEPTH_STENCIL_DESC augDepthStencilDesc = SampleCommon::RenderUtil::CreateDepthStencilDesc(
            true,
            D3D11_DEPTH_WRITE_MASK_ZERO,// transparent
            D3D11_COMPARISON_LESS
            );
        device->CreateDepthStencilState(&augDepthStencilDesc, m_augmentationDepthStencilState.GetAddressOf());

        // Create blend state for video background (no transparency)
        D3D11_BLEND_DESC videoBlendDesc = SampleCommon::RenderUtil::CreateBlendDesc(false);
        device->CreateBlendState(&videoBlendDesc, m_videoBlendState.GetAddressOf());

        // Create augmentation blend state (with transparency)
        D3D11_BLEND_DESC augmentationBlendDesc = SampleCommon::RenderUtil::CreateBlendDesc(true);
        device->CreateBlendState(&augmentationBlendDesc, m_augmentationBlendState.GetAddressOf());

        // Set initial depth-stencil state and raster states
        context->RSSetState(m_videoRasterState.Get());
        context->OMSetDepthStencilState(m_videoDepthStencilState.Get(), 1);
        context->OMSetBlendState(m_videoBlendState.Get(), NULL, 0xffffffff);
    });

    setupRasterizersTask.then([this](Concurrency::task<void> t) {
        try
        {
            // If any exceptions were thrown back in the async chain then
            // this call throws that exception here and we can catch it below
            t.get();

            m_rendererInitialized = true;
        }
        catch (Platform::Exception ^ex) {
            SampleCommon::SampleUtil::ShowError(L"Renderer init error", ex->Message);
        }
    });
}

void VuMarkRenderer::ReleaseDeviceDependentResources()
{
    m_rendererInitialized = false;

    m_videoBackground->ReleaseResources();
    m_videoBackground.reset();

    m_vertexShader.Reset();
    m_inputLayout.Reset();
    m_pixelShader.Reset();
    m_constantBuffer.Reset();

    m_quadMesh->ReleaseResources();
    m_augmentationTexture->ReleaseResources();
    m_reticleTexture->ReleaseResources();

    m_renderingPrimitives.reset();
}
