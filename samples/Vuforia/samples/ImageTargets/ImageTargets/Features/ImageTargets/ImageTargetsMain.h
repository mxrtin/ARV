/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#pragma once

#include "Common\StepTimer.h"
#include "Common\DeviceResources.h"
#include "Features\ImageTargets\ImageTargetsRenderer.h"
#include "SampleApplication\AppSession.h"

// Renders Direct2D and 3D content on the screen.
namespace ImageTargets
{
    class ImageTargetsMain : public DX::IDeviceNotify
    {
    public:
        ImageTargetsMain(
            const std::shared_ptr<DX::DeviceResources>& deviceResources,
            const std::shared_ptr<AppSession>& appSession
          );
        ~ImageTargetsMain();
        void CreateWindowSizeDependentResources();
        void StartRenderLoop();
        void StopRenderLoop();
        Concurrency::critical_section& GetCriticalSection() { return m_criticalSection; }

        // IDeviceNotify
        virtual void OnDeviceLost();
        virtual void OnDeviceRestored();

        SampleCommon::StepTimer& GetTimer() { return m_timer; }
        
        ImageTargetsRenderer* GetRenderer() { return m_imageTargetsRenderer.get(); }

    private:
        void ProcessInput();
        void Update();
        bool Render();

        // Cached pointer to device resources.
        std::shared_ptr<DX::DeviceResources> m_deviceResources;
        std::shared_ptr<AppSession> m_appSession;

        // Image Targets scene renderer
        std::shared_ptr<ImageTargetsRenderer> m_imageTargetsRenderer;

        Windows::Foundation::IAsyncAction^ m_renderLoopWorker;
        Concurrency::critical_section m_criticalSection;

        // Rendering loop timer.
        SampleCommon::StepTimer m_timer;
    };
}