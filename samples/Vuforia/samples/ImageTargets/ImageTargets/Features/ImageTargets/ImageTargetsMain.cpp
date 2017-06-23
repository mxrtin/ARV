/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#include "pch.h"
#include "ImageTargetsMain.h"
#include "Common\DirectXHelper.h"
#include <Vuforia\Vuforia_UWP.h>

using namespace ImageTargets;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;

// Loads and initializes application assets when the application is loaded.
ImageTargetsMain::ImageTargetsMain(
    const std::shared_ptr<DX::DeviceResources>& deviceResources,
    const std::shared_ptr<AppSession>& appSession) :
    m_deviceResources(deviceResources),
    m_appSession(appSession)
{
    // Register to be notified if the Device is lost or recreated
    m_deviceResources->RegisterDeviceNotify(this);

    // Init the Image Targets scene renderer
    m_imageTargetsRenderer = std::unique_ptr<ImageTargetsRenderer>(new ImageTargetsRenderer(m_deviceResources));

    // We set the desired frame rate here
    float fps = 30;
    m_timer.SetFixedTimeStep(true);
    m_timer.SetTargetElapsedSeconds(1.0 / fps);
}

ImageTargetsMain::~ImageTargetsMain()
{
    // Deregister device notification
    m_deviceResources->RegisterDeviceNotify(nullptr);
}

// Updates application state when the window size changes (e.g. device orientation change)
void ImageTargetsMain::CreateWindowSizeDependentResources()
{
    Vuforia::setCurrentOrientation(m_deviceResources->GetCurrentOrientation());

    // get the new window size
    Size screenSize = m_deviceResources->GetOutputSize();
    // inform Vuforia of the window size change
    Vuforia::onSurfaceChanged((int)screenSize.Width, (int)screenSize.Height);

    if (m_imageTargetsRenderer->IsVuforiaStarted())
    {
        m_appSession->ConfigureVideoBackground(
            m_deviceResources->GetOutputSize().Width,
            m_deviceResources->GetOutputSize().Height,
            m_deviceResources->GetCurrentOrientation()
        );
    }

    m_imageTargetsRenderer->CreateWindowSizeDependentResources();
}

void ImageTargetsMain::StartRenderLoop()
{
    // If the animation render loop is already running then do not start another thread.
    if (m_renderLoopWorker != nullptr && m_renderLoopWorker->Status == AsyncStatus::Started)
    {
        return;
    }

    // Create a task that will be run on a background thread.
    auto workItemHandler = ref new WorkItemHandler([this](IAsyncAction ^ action)
    {
        // Calculate the updated frame and render once per vertical blanking interval.
        while (action->Status == AsyncStatus::Started)
        {
            critical_section::scoped_lock lock(m_criticalSection);
            Update();
            if (Render())
            {
                m_deviceResources->Present();
            }
        }
    });

    // Run task on a dedicated high priority background thread.
    m_renderLoopWorker = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);
}

void ImageTargetsMain::StopRenderLoop()
{
    m_renderLoopWorker->Cancel();
}

// Updates the application state once per frame.
void ImageTargetsMain::Update()
{
    ProcessInput();

    // Update scene objects
    m_timer.Tick([&]()
    {
        if (m_imageTargetsRenderer->IsRendererInitialized())
        {
            m_imageTargetsRenderer->Update(m_timer);
        }
    });
}

// Process all input from the user before updating game state
void ImageTargetsMain::ProcessInput()
{
    // Add per frame input handling here.

}

// Renders the current frame according to the current application state.
// Returns true if the frame was rendered and is ready to be displayed.
bool ImageTargetsMain::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return false;
    }

    // Don't render before we finish loading renderer resources
    // (textures, meshes, shaders,...).
    if (!m_imageTargetsRenderer->IsRendererInitialized())
    {
        return false;
    }

    auto context = m_deviceResources->GetD3DDeviceContext();

    // Reset the viewport to target the whole screen.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);


    // Reset render targets to the screen.
    ID3D11RenderTargetView *const targets[1] = { m_deviceResources->GetBackBufferRenderTargetView() };
    context->OMSetRenderTargets(1, targets, m_deviceResources->GetDepthStencilView());

    // Clear the back buffer and depth stencil view.
    context->ClearRenderTargetView(m_deviceResources->GetBackBufferRenderTargetView(), DirectX::Colors::Black);
    context->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // Render the scene objects.
    m_imageTargetsRenderer->Render();

    return true;
}

// Notifies renderers that device resources need to be released.
void ImageTargetsMain::OnDeviceLost()
{
    m_imageTargetsRenderer->ReleaseDeviceDependentResources();
}

// Notifies renderers that device resources may now be recreated.
void ImageTargetsMain::OnDeviceRestored()
{
    m_imageTargetsRenderer->CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}
