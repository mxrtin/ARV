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
#include <wincodec.h>

#include <Vuforia\Mesh.h>
#include <Vuforia\Renderer.h>
#include <Vuforia\RenderingPrimitives.h>

#include "VideoBackgroundTexture.h"


namespace SampleCommon
{
    class VideoBackground 
    {
    public:
        VideoBackground(const std::shared_ptr<DX::DeviceResources>& deviceResources);
        ~VideoBackground();

        void ReleaseResources();
        void SetVideoBackgroundTexture();

        void InitVertexShader(const void *shaderByteCode, SIZE_T byteCodeLength);
        void InitFragmentShader(const void *shaderByteCode, SIZE_T byteCodeLength);
        void InitRenderState();

        void ResetForNewRenderingPrimitives();

        void Render(Vuforia::Renderer &renderer,
            Vuforia::RenderingPrimitives *renderPrimitives,
            Vuforia::VIEW viewId);

    private:
        double GetSceneScaleFactor();
        void InitMesh(const Vuforia::Mesh &vbMesh, ID3D11Device *device);
        
        // Cached pointer to device resources.
        std::shared_ptr<DX::DeviceResources> m_deviceResources;

        // DX States for video background and augmentation rendering
        Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_vbRasterStateCounterClockwise;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_vbRasterStateClockwise;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_vbDepthStencilState;
        Microsoft::WRL::ComPtr<ID3D11BlendState>        m_vbBlendState;
        
        ProjectionConstantBuffer m_vbConstantBufferData;

        std::shared_ptr<SampleCommon::VideoBackgroundTexture> m_vbTexture;

        Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_vbInputLayout;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vbVertexShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_vbPixelShader;
        Microsoft::WRL::ComPtr<ID3D11Buffer>       m_vbConstantBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>       m_vbVertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>       m_vbIndexBuffer;

        int             m_vbMeshIndexCount;
        TexturedVertex *m_vbMeshVertices;
        bool            m_setVideoBackgroundTexture;

    };

} // SampleCommon
