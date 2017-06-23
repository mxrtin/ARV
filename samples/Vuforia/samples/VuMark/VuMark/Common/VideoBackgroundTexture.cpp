/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#include "pch.h"

#include "VideoBackgroundTexture.h"

#include <iostream>
#include <memory>
#include "DirectXHelper.h"

namespace SampleCommon
{
    VideoBackgroundTexture::VideoBackgroundTexture(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
        m_deviceResources(deviceResources), 
        m_initialized(false),
        m_texture(nullptr), 
        m_samplerState(nullptr),
        m_textureView(nullptr),
        m_imageWidth(0), 
        m_imageHeight(0)
    {
    }

    VideoBackgroundTexture::~VideoBackgroundTexture()
    {
        ReleaseResources();
    }

    void VideoBackgroundTexture::Init(size_t width, size_t height)
    {
        m_imageWidth = width;
        m_imageHeight = height;

        // Setup texture descriptor
        D3D11_TEXTURE2D_DESC texDesc;
        ZeroMemory(&texDesc, sizeof(D3D11_TEXTURE2D_DESC));
        texDesc.Width = static_cast<UINT>(m_imageWidth);
        texDesc.Height = static_cast<UINT>(m_imageHeight);
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.Usage = D3D11_USAGE_DEFAULT; // Resource requires read and write access by the GPU
        texDesc.CPUAccessFlags = 0; // CPU access is not required
        texDesc.MiscFlags = 0;
        texDesc.MipLevels = 1; // Most textures contain more than one MIP level.  For simplicity, this sample uses only one.
        texDesc.ArraySize = 1; // As this will not be a texture array, this parameter is ignored.
        texDesc.SampleDesc.Count = 1; // Don't use multi-sampling.
        texDesc.SampleDesc.Quality = 0;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET; // Allow the texture to be bound as a shade

        // Create the texture
        DX::ThrowIfFailed(
            m_deviceResources->GetD3DDevice()->CreateTexture2D(&texDesc, nullptr, m_texture.GetAddressOf())
        );

        // Create a shader resource view for the texture
        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
        memset(&SRVDesc, 0, sizeof(SRVDesc));
        SRVDesc.Format = texDesc.Format;
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Texture2D.MipLevels = -1;

        DX::ThrowIfFailed(
            m_deviceResources->GetD3DDevice()->CreateShaderResourceView(
                m_texture.Get(), &SRVDesc, m_textureView.GetAddressOf()
            )
        );
        
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
                &samplerDesc, m_samplerState.GetAddressOf()
            )
        );

        m_initialized = true;
    }

    void VideoBackgroundTexture::ReleaseResources()
    {
        m_samplerState.Reset();
        m_textureView.Reset();
        m_texture.Reset();
        m_deviceResources.reset();
        m_initialized = false;
    }
}