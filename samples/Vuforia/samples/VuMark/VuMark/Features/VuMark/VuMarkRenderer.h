/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#pragma once

#include "VuMarkView.xaml.h"
#include "..\..\Common\DeviceResources.h"
#include "..\..\Common\ShaderStructures.h"
#include "..\..\Common\StepTimer.h"
#include "..\..\Common\Texture.h"
#include "..\..\Common\QuadMesh.h"
#include "..\..\Common\VideoBackground.h"

#include <Vuforia\Matrices.h>
#include <Vuforia\Renderer.h>
#include <Vuforia\RenderingPrimitives.h>
#include <Vuforia\State.h>

namespace VuMark
{
    // This sample renderer instantiates a basic rendering pipeline.
    class VuMarkRenderer
    {
    public:
        VuMarkRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources);

        void SetVuMarkView(VuMarkView^ view) { m_vuMarkView = view; }
        
        void CreateDeviceDependentResources();
        void CreateWindowSizeDependentResources();
        void ReleaseDeviceDependentResources();
        
        void Update(SampleCommon::StepTimer const& timer);
        void Render();
        
        bool IsRendererInitialized() { return m_rendererInitialized; }
        bool IsVuforiaInitialized() { return m_vuforiaInitialized; }
        bool IsVuforiaStarted() { return m_vuforiaStarted; }
        void SetVuforiaInitialized(bool initialized) { m_vuforiaInitialized = initialized; }
        void SetVuforiaStarted(bool started);
        void SetExtendedTracking(bool enabled) { m_extTracking = enabled; }

        void UpdateRenderingPrimitives();
        
        void SetUIDispatcher(Windows::UI::Core::CoreDispatcher^ dispatcher) { m_uiDispatcher = dispatcher; }

    private:
        void RenderScene(Vuforia::Renderer &renderer, Vuforia::State &state);
        void RenderReticle();

        void RenderVuMark(
            float vuMarkOriginX,
            float vuMarkOriginY,
            float vuMarkWidth,
            float vuMarkHeight,
            const DirectX::XMMATRIX &poseMatrix,
            const DirectX::XMMATRIX &projectionMatrix,
            const std::shared_ptr<SampleCommon::Texture> texture,
            float opacity);

        float BlinkVumark(bool reset);

        VuMarkView^ m_vuMarkView;
        Windows::UI::Core::CoreDispatcher^ m_uiDispatcher;

        // Lock to protect updates to RenderingPrimitives
        Concurrency::critical_section m_renderingPrimitivesLock;

        // Cached pointer to the rendering primitives
        std::shared_ptr<Vuforia::RenderingPrimitives> m_renderingPrimitives = nullptr;

        // Cached pointer to device resources.
        std::shared_ptr<DX::DeviceResources> m_deviceResources;

        // Video background
        std::shared_ptr<SampleCommon::VideoBackground> m_videoBackground;

        // DX States for video background and augmentation rendering
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_videoRasterState;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_augmentationRasterState;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_augmentationRasterStateCullFront;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_videoDepthStencilState;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_augmentationDepthStencilState;
        Microsoft::WRL::ComPtr<ID3D11BlendState> m_videoBlendState;
        Microsoft::WRL::ComPtr<ID3D11BlendState> m_augmentationBlendState;

       // Direct3D resources for mesh geometry.
        Microsoft::WRL::ComPtr<ID3D11InputLayout>    m_inputLayout;
        Microsoft::WRL::ComPtr<ID3D11VertexShader>    m_vertexShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>    m_pixelShader;
        Microsoft::WRL::ComPtr<ID3D11Buffer>        m_constantBuffer;

        // Quad mesh and texture
        std::shared_ptr<SampleCommon::QuadMesh> m_quadMesh;
        std::shared_ptr<SampleCommon::Texture> m_augmentationTexture;
        std::shared_ptr<SampleCommon::Texture> m_reticleTexture;

        // System resources for model-view projection.
        SampleCommon::ModelViewProjectionConstantBuffer    m_constantBufferData;

        // Projection matrix
        DirectX::XMFLOAT4X4 m_projection;

        // Variables used with the rendering loop.
        std::atomic<bool> m_rendererInitialized;
        std::atomic<bool> m_vuforiaInitialized;
        std::atomic<bool> m_vuforiaStarted;
        std::atomic<bool> m_extTracking;

        // Near and Far clipping planes
        const float m_near = .02f;
        const float m_far = 5000.0f;

        // Time
        double m_currentTime;
    };
}

