/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#include "pch.h"
#include "VuMarkMain.h"
#include "Common\DirectXHelper.h"
#include <Vuforia\Vuforia_UWP.h>

using namespace VuMark;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;

// Loads and initializes application assets when the application is loaded.
VuMarkMain::VuMarkMain(
    const std::shared_ptr<DX::DeviceResources>& deviceResources,
    const std::shared_ptr<AppSession>& appSession) :
    m_deviceResources(deviceResources),
    m_appSession(appSession)
{
    // Register to be notified if the Device is lost or recreated
    m_deviceResources->RegisterDeviceNotify(this);

    // Init the Image Targets scene renderer
    m_vuMarkRenderer = std::unique_ptr<VuMarkRenderer>(new VuMarkRenderer(m_deviceResources));

    // We set the desired frame rate here
    float fps = 30;
    m_timer.SetFixedTimeStep(true);
    m_timer.SetTargetElapsedSeconds(1.0 / fps);
}

VuMarkMain::~VuMarkMain()
{
    // Deregister device notification
    m_deviceResources->RegisterDeviceNotify(nullptr);
}

// Updates application state when the window size changes (e.g. device orientation change)
void VuMarkMain::CreateWindowSizeDependentResources()
{
    Vuforia::setCurrentOrientation(m_deviceResources->GetCurrentOrientation());

    // get the new window size
    Size screenSize = m_deviceResources->GetOutputSize();
    // inform Vuforia of the window size change
    Vuforia::onSurfaceChanged((int)screenSize.Width, (int)screenSize.Height);

    if (m_vuMarkRenderer->IsVuforiaStarted())
    {
        m_appSession->ConfigureVideoBackground(
            m_deviceResources->GetOutputSize().Width,
            m_deviceResources->GetOutputSize().Height,
            m_deviceResources->GetCurrentOrientation()
        );
    }

    m_vuMarkRenderer->CreateWindowSizeDependentResources();
}

void VuMarkMain::StartRenderLoop()
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
            // We provide a dispatcher to the renderer so that it can run 
            // certain UI-related tasks on the UI thread
            m_vuMarkRenderer->SetUIDispatcher(
                Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher
            );

            critical_section::scoped_lock lock(m_criticalSection);
            Update();
            if (Render())
            {
                m_deviceResources->Present();
            }

            // We release the dispatcher as we don't need to hold it any more
            m_vuMarkRenderer->SetUIDispatcher(nullptr);
        }
    });

    // Run task on a dedicated high priority background thread.
    m_renderLoopWorker = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);
}

void VuMarkMain::StopRenderLoop()
{
    m_renderLoopWorker->Cancel();
}

// Updates the application state once per frame.
void VuMarkMain::Update()
{
    ProcessInput();

    // Update scene objects
    m_timer.Tick([&]()
    {
        if (m_vuMarkRenderer->IsRendererInitialized())
        {
            m_vuMarkRenderer->Update(m_timer);
        }
    });
}

// Process all input from the user before updating game state
void VuMarkMain::ProcessInput()
{
    // Add per frame input handling here.

}

// Renders the current frame according to the current application state.
// Returns true if the frame was rendered and is ready to be displayed.
bool VuMarkMain::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return false;
    }

    // Don't render before we finish loading renderer resources
    // (textures, meshes, shaders,...).
    if (!m_vuMarkRenderer->IsRendererInitialized())
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
    m_vuMarkRenderer->Render();

    return true;
}

// Notifies renderers that device resources need to be released.
void VuMarkMain::OnDeviceLost()
{
    m_vuMarkRenderer->ReleaseDeviceDependentResources();
}

// Notifies renderers that device resources may now be recreated.
void VuMarkMain::OnDeviceRestored()
{
    m_vuMarkRenderer->CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}
