/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#include "pch.h"

#include "Texture.h"

#include <iostream>
#include <memory>
#include "DirectXHelper.h"

namespace SampleCommon
{
    Texture::Texture(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
        m_deviceResources(deviceResources), 
        m_texture(nullptr), m_initialized(false),
        m_imageWidth(0), m_imageHeight(0), m_rowPitch(0), m_imageSize(0)
    {
    }

    Texture::~Texture()
    {
        ReleaseResources();
    }

    void Texture::CreateFromFile(wchar_t *filename)
    {
        IWICBitmapDecoder *decoder = nullptr;
        if (nullptr == m_imagingFactory)
        {
            // Create the ImagingFactory
            DX::ThrowIfFailed(
                CoCreateInstance(
                    CLSID_WICImagingFactory, nullptr,
                    CLSCTX_INPROC_SERVER, IID_PPV_ARGS(m_imagingFactory.GetAddressOf()))
                );
        }

        DX::ThrowIfFailed(
            m_imagingFactory->CreateDecoderFromFilename(
                filename,
                NULL,
                GENERIC_READ,
                WICDecodeMetadataCacheOnDemand,
                &decoder
                )
            );

        // Retrieve the first frame of the image from the decoder
        IWICBitmapFrameDecode *frame = NULL;
        DX::ThrowIfFailed(
            decoder->GetFrame(0, &frame)
            );

        DX::ThrowIfFailed(
            m_imagingFactory->CreateFormatConverter(m_formatConverter.GetAddressOf())
            );

        DX::ThrowIfFailed(
            m_formatConverter->Initialize(
                frame,  // Input bitmap to convert
                GUID_WICPixelFormat32bppBGRA, // Destination pixel format
                WICBitmapDitherTypeNone,                    
                nullptr, 
                0.f, // Alpha threshold
                WICBitmapPaletteTypeCustom
                )
            );
        
        DX::ThrowIfFailed(
            m_formatConverter->GetSize(&m_imageWidth, &m_imageHeight)
            );

        // Allocate temporary memory for image
        m_rowPitch = m_imageWidth * 4; // 4 bytes per pixel (32bit RGBA)
        m_imageSize = m_rowPitch * m_imageHeight;

        std::unique_ptr<uint8_t[]> imageBytes(new (std::nothrow) uint8_t[m_imageSize]);
        DX::ThrowIfFailed(
            m_formatConverter->CopyPixels(
                0, static_cast<UINT>(m_rowPitch), static_cast<UINT>(m_imageSize), imageBytes.get()
                )
            );

        // As the mesh texture coordinates assume (0,0) at bottom-left corner of image,
        // while the image is loaded top-down,
        // we reorder the image rows to flip the image vertically.
        m_imageBytes = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[m_imageSize]);
        for (UINT r = 0; r < m_imageHeight; ++r)
        {
            memcpy(m_imageBytes.get() + m_rowPitch * r,
                imageBytes.get() + m_rowPitch * (m_imageHeight - 1 - r),
                m_rowPitch);
        }

        D3D11_SUBRESOURCE_DATA initData;
        initData.pSysMem = m_imageBytes.get();
        initData.SysMemPitch = static_cast<UINT>(m_rowPitch);
        initData.SysMemSlicePitch = static_cast<UINT>(m_imageSize);

        D3D11_TEXTURE2D_DESC texDesc;
        ZeroMemory(&texDesc, sizeof(D3D11_TEXTURE2D_DESC));
        texDesc.Width = m_imageWidth;
        texDesc.Height = m_imageHeight;
        texDesc.MipLevels = 0;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.CPUAccessFlags = 0;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

        DX::ThrowIfFailed(
            m_deviceResources->GetD3DDevice()->CreateTexture2D(&texDesc, nullptr, m_texture.GetAddressOf())
            );

        if (m_texture != nullptr)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
            memset(&SRVDesc, 0, sizeof(SRVDesc));
            SRVDesc.Format = texDesc.Format;
            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            SRVDesc.Texture2D.MipLevels = -1;

            DX::ThrowIfFailed(
                m_deviceResources->GetD3DDevice()->CreateShaderResourceView(
                    m_texture.Get(), &SRVDesc, m_textureView.GetAddressOf())
                );
        }

        // Create a texture sampler state description.
        D3D11_SAMPLER_DESC samplerDesc;
        ZeroMemory(&samplerDesc, sizeof(D3D11_SAMPLER_DESC));
        samplerDesc.Filter = D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.MipLODBias = 0.0f;
        samplerDesc.MaxAnisotropy = 1;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        samplerDesc.BorderColor[0] = 0;
        samplerDesc.BorderColor[1] = 0;
        samplerDesc.BorderColor[2] = 0;
        samplerDesc.BorderColor[3] = 0;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

        DX::ThrowIfFailed(
            m_deviceResources->GetD3DDevice()->CreateSamplerState(
                &samplerDesc, m_samplerState.GetAddressOf())
            );
    }

    void Texture::Init()
    {
        if (m_texture != nullptr)
        {
            m_deviceResources->GetD3DDeviceContext()->UpdateSubresource(
                m_texture.Get(), 0, nullptr, m_imageBytes.get(),
                static_cast<UINT>(m_rowPitch), static_cast<UINT>(m_imageSize)
                );

            m_deviceResources->GetD3DDeviceContext()->GenerateMips(m_textureView.Get());
        }
        m_initialized = true;
    }

    void Texture::ReleaseResources()
    {
        m_imagingFactory.Reset();
        m_formatConverter.Reset();
        m_samplerState.Reset();
        m_textureView.Reset();
        m_texture.Reset();
        m_deviceResources.reset();
    }
}