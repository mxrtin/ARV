/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#pragma once

#include "..\..\Common\DeviceResources.h"
#include "..\..\Common\ShaderStructures.h"
#include "..\..\Common\StepTimer.h"
#include "..\..\Common\Texture.h"
#include "..\..\Common\TeapotMesh.h"
#include "..\..\Common\SampleApp3DModel.h"
#include "..\..\Common\VideoBackground.h"

#include <Vuforia\Matrices.h>
#include <Vuforia\Renderer.h>
#include <Vuforia\RenderingPrimitives.h>
#include <Vuforia\State.h>

namespace ImageTargets
{
    // This sample renderer instantiates a basic rendering pipeline.
    class ImageTargetsRenderer
    {
    public:
        ImageTargetsRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources);
        
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
        
    private:
        void RenderScene(Vuforia::Renderer &renderer, Vuforia::State &state);
        
        void RenderTeapot(
            const DirectX::XMMATRIX &poseMatrix,
            const DirectX::XMMATRIX &projectionMatrix,
            const std::shared_ptr<SampleCommon::Texture> texture);

        void RenderTower(
            const DirectX::XMMATRIX &poseMatrix,
            const DirectX::XMMATRIX &projectionMatrix,
            const std::shared_ptr<SampleCommon::Texture> texture);

        const std::shared_ptr<SampleCommon::Texture> GetAugmentationTexture(const char *targetName);

        // Lock to protect updates to RenderingPrimitives
        Concurrency::critical_section m_renderingPrimitivesLock;

        // Cached pointer to the rendering primitives
        std::shared_ptr<Vuforia::RenderingPrimitives> m_renderingPrimitives;

        // Cached pointer to device resources.
        std::shared_ptr<DX::DeviceResources> m_deviceResources;

        // Video background
        std::shared_ptr<SampleCommon::VideoBackground> m_videoBackground;

        // DX States for video background and augmentation rendering
        Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_augmentationRasterStateCullBack;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_augmentationRasterStateCullFront;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_augmentationDepthStencilState;
        Microsoft::WRL::ComPtr<ID3D11BlendState>        m_augmentationBlendState;

       // Direct3D resources for mesh rendering
        Microsoft::WRL::ComPtr<ID3D11InputLayout>    m_augmentationInputLayout;
        Microsoft::WRL::ComPtr<ID3D11VertexShader>    m_augmentationVertexShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>    m_augmentationPixelShader;
        Microsoft::WRL::ComPtr<ID3D11Buffer>        m_augmentationConstantBuffer;
        
        // Teapot mesh
        std::shared_ptr<SampleCommon::TeapotMesh> m_teapotMesh;
        std::shared_ptr<SampleCommon::SampleApp3DModel> m_towerModel;

        // Textures
        std::shared_ptr<SampleCommon::Texture> m_textureTeapotBlue;
        std::shared_ptr<SampleCommon::Texture> m_textureTeapotBrass;
        std::shared_ptr<SampleCommon::Texture> m_textureTeapotRed;
        std::shared_ptr<SampleCommon::Texture> m_textureTower;
        
        // System resources for model-view projection.
        SampleCommon::ModelViewProjectionConstantBuffer    m_augmentationConstantBufferData;
        
        // Projection matrix, stored as an XMFLOAT4X4 to avoid imposing SSE alignment constraints 
        // required by an XMMATRIX on this class
        DirectX::XMFLOAT4X4 m_cameraProjection;

        // Variables used with the rendering loop.
        std::atomic<bool> m_rendererInitialized;
        std::atomic<bool> m_vuforiaInitialized;
        std::atomic<bool> m_vuforiaStarted;
        std::atomic<bool> m_extTracking;

        // Near and Far clipping planes
        const float m_near = 0.01f;
        const float m_far = 100.0f;
    };
}

