/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#pragma once

#include "DeviceResources.h"
#include <wrl.h>
#include <d3d11.h>
#include <wincodec.h>

namespace SampleCommon
{
    class Texture 
    {
    public:
        Texture(const std::shared_ptr<DX::DeviceResources>& deviceResources);
        ~Texture();

        void CreateFromFile(wchar_t *filename);
        void Init();
        void ReleaseResources();

        bool IsInitialized() const { return m_initialized; }
        Microsoft::WRL::ComPtr<ID3D11SamplerState> & GetD3DSamplerState() { return m_samplerState; }
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> & GetD3DTextureView() { return m_textureView; }
        Microsoft::WRL::ComPtr<ID3D11Texture2D> & GetD3DTexture() { return m_texture; }

    private:
        // Cached pointer to device resources.
        std::shared_ptr<DX::DeviceResources> m_deviceResources;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_texture;
        Microsoft::WRL::ComPtr<ID3D11SamplerState>  m_samplerState;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_textureView;

        Microsoft::WRL::ComPtr<IWICImagingFactory> m_imagingFactory;
        Microsoft::WRL::ComPtr<IWICFormatConverter> m_formatConverter;

        UINT m_imageWidth;
        UINT m_imageHeight;
        size_t m_rowPitch;
        size_t m_imageSize;
        std::unique_ptr<uint8_t[]> m_imageBytes;
        bool m_initialized;
    };
} // SampleCommon