/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#include "pch.h"

#include "VuMarkView.xaml.h"
#include "Common\SampleUtil.h"

#include <robuffer.h>

#include <Vuforia\Vuforia.h>
#include <Vuforia\CameraDevice.h>
#include <Vuforia\State.h>
#include <Vuforia\TrackerManager.h>
#include <Vuforia\Tracker.h>
#include <Vuforia\ObjectTracker.h>
#include <Vuforia\ObjectTarget.h>

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Graphics::Display;
using namespace Windows::Storage;
using namespace Windows::System::Threading;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::Foundation;
using namespace Microsoft::WRL;
using namespace Windows::Storage::Streams;
using namespace concurrency;

using namespace VuMark;


VuMarkView::VuMarkView():
    m_windowVisible(true),
    m_showingMenu(false),
    m_showingVuMarkCard(false),
    m_showingProgress(true),
    m_coreInput(nullptr),
    m_autofocusEnabled(true),
    m_extTracking(false),
    m_vuMarkDataSet(nullptr),
    m_vuMarkId(L"Unknown"),
    m_vuMarkType(L"Bytes"),
    m_vuMarkImagePixels(nullptr),
    m_vuMarkImageWidth(0),
    m_vuMarkImageHeight(0)
{
    InitializeComponent();
    Application^ app = Application::Current; 
    app->Suspending += ref new SuspendingEventHandler(this, &VuMarkView::OnSuspending);
    app->Resuming += ref new EventHandler<Object^>(this, &VuMarkView::OnResuming);

    // Register event handlers for page lifecycle.
    CoreWindow^ window = Window::Current->CoreWindow;

    window->VisibilityChanged +=
        ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &VuMarkView::OnVisibilityChanged);

    DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

    currentDisplayInformation->DpiChanged +=
        ref new TypedEventHandler<DisplayInformation^, Object^>(this, &VuMarkView::OnDpiChanged);

    currentDisplayInformation->OrientationChanged +=
        ref new TypedEventHandler<DisplayInformation^, Object^>(this, &VuMarkView::OnOrientationChanged);

    DisplayInformation::DisplayContentsInvalidated +=
        ref new TypedEventHandler<DisplayInformation^, Object^>(this, &VuMarkView::OnDisplayContentsInvalidated);

    swapChainPanel->CompositionScaleChanged += 
        ref new TypedEventHandler<SwapChainPanel^, Object^>(this, &VuMarkView::OnCompositionScaleChanged);

    swapChainPanel->SizeChanged +=
        ref new SizeChangedEventHandler(this, &VuMarkView::OnSwapChainPanelSizeChanged);

    // At this point we have access to the device. 
    // We can create the device-dependent resources.
    m_deviceResources = std::make_shared<DX::DeviceResources>();
    m_deviceResources->SetSwapChainPanel(swapChainPanel);

    // Register our SwapChainPanel to get independent input pointer events
    auto workItemHandler = ref new WorkItemHandler([this] (IAsyncAction ^)
    {
        // The CoreIndependentInputSource will raise pointer events for the specified device types on whichever thread it's created on.
        m_coreInput = swapChainPanel->CreateCoreIndependentInputSource(
            Windows::UI::Core::CoreInputDeviceTypes::Mouse |
            Windows::UI::Core::CoreInputDeviceTypes::Touch |
            Windows::UI::Core::CoreInputDeviceTypes::Pen
            );

        // Register for pointer events, which will be raised on the background thread.
        m_coreInput->PointerPressed += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &VuMarkView::OnPointerPressed);
        m_coreInput->PointerMoved += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &VuMarkView::OnPointerMoved);
        m_coreInput->PointerReleased += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &VuMarkView::OnPointerReleased);
        
        // Begin processing input messages as they're delivered.
        m_coreInput->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);

    });

    // Run task on a dedicated high priority background thread.
    m_inputLoopWorker = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);

    // Init Vuforia App Session
    m_appSession = std::shared_ptr<AppSession>(new AppSession(this));
    m_appSession->InitAR();

    m_main = std::unique_ptr<VuMarkMain>(new VuMarkMain(m_deviceResources, m_appSession));
    m_main->GetRenderer()->SetVuMarkView(this);
    m_main->StartRenderLoop();
}

VuMarkView::~VuMarkView()
{
    // Stop rendering and processing events on destruction.
    m_main->StopRenderLoop();
    m_coreInput->Dispatcher->StopProcessEvents();

    // Stop and deinit Vuforia
    if (m_appSession != nullptr) {
        m_appSession->StopAR();
    }

    m_main->GetRenderer()->SetVuforiaInitialized(false);
}

// Saves the current state of the app for suspend and terminate events.
void VuMarkView::SaveInternalState(IPropertySet^ state)
{
    critical_section::scoped_lock lock(m_main->GetCriticalSection());
    m_deviceResources->Trim();

    // Stop rendering when the app is suspended.
    m_main->StopRenderLoop();

    // Put code to save app state here.
}

// Loads the current state of the app for resume events.
void VuMarkView::LoadInternalState(IPropertySet^ state)
{
    // Put code to load app state here.

    // Start rendering when the app is resumed.
    m_main->StartRenderLoop();
}

void VuMarkView::OnPause()
{
    try
    {
        m_appSession->PauseAR();
    }
    catch (Platform::Exception^ ex)
    {
        SampleCommon::SampleUtil::Log("VuMarkView", ex->Message);
    }
}

void VuMarkView::OnResume()
{
    try
    {
        m_appSession->ResumeAR();
    }
    catch (Platform::Exception^ ex)
    {
        SampleCommon::SampleUtil::Log("VuMarkView", ex->Message);
    }
}

bool VuMarkView::DoInitTrackers()
{ 
    Vuforia::TrackerManager &trackerMgr = Vuforia::TrackerManager::getInstance();
    Vuforia::Tracker *objTracker = trackerMgr.initTracker(Vuforia::ObjectTracker::getClassType());
    if (objTracker == nullptr) {
        SampleCommon::SampleUtil::Log("VuMarkView", "Failed to initialize object tracker.");
        return false;
    }
    return true;
}

bool VuMarkView::DoLoadTrackersData()
{    
    Vuforia::TrackerManager &trackerMgr = Vuforia::TrackerManager::getInstance();
    Vuforia::ObjectTracker *objTracker = static_cast<Vuforia::ObjectTracker*>(
        trackerMgr.getTracker(Vuforia::ObjectTracker::getClassType()));

    if (objTracker == nullptr)
    {
        SampleCommon::SampleUtil::Log("VuMarkView", "Object Tracker not started. Failed to load data.");
        return false;
    }

    m_vuMarkDataSet = objTracker->createDataSet();
    if (m_vuMarkDataSet == nullptr) {
        SampleCommon::SampleUtil::Log("VuMarkView", "Failed to create dataset.");
        return false;
    }
    if (!m_vuMarkDataSet->load("Assets\\VuMark\\Vuforia.xml", 
                                Vuforia::STORAGE_TYPE::STORAGE_APPRESOURCE))
    {
        SampleCommon::SampleUtil::Log("VuMarkView", "Failed to load dataset.");
        return false;
    }

    if (!objTracker->activateDataSet(m_vuMarkDataSet))
    {
        SampleCommon::SampleUtil::Log("VuMarkView", "Failed to activate dataset.");
        return false;
    }

    return true;
}

bool VuMarkView::DoStartCamera(int direction)
{
    Vuforia::CameraDevice::CAMERA_DIRECTION camDirection =
        static_cast<Vuforia::CameraDevice::CAMERA_DIRECTION>(direction);

    if (!Vuforia::CameraDevice::getInstance().init(camDirection))
    {
        SampleCommon::SampleUtil::Log("VuMarkView", "Failed to init camera.");
        return false;
    }

    if (!Vuforia::CameraDevice::getInstance().selectVideoMode(
            Vuforia::CameraDevice::MODE_DEFAULT))
    {
        return false;
    }

    m_appSession->ConfigureVideoBackground(
        m_deviceResources->GetOutputSize().Width,
        m_deviceResources->GetOutputSize().Height,
        m_deviceResources->GetCurrentOrientation()
      );

    m_main->GetRenderer()->UpdateRenderingPrimitives();

    if (!Vuforia::CameraDevice::getInstance().start())
    {
        SampleCommon::SampleUtil::Log("VuMarkView", "Failed to start camera.");
        return false;
    }

    return true;
}

bool VuMarkView::DoStopCamera()
{
    if (!Vuforia::CameraDevice::getInstance().stop())
    {
        SampleCommon::SampleUtil::Log("ImageTargetsView", "Failed to stop camera.");
        return false;
    }

    if (!Vuforia::CameraDevice::getInstance().deinit())
    {
        SampleCommon::SampleUtil::Log("ImageTargetsView", "Failed to deinit camera.");
        return false;
    }

    // Notify renderer
    m_main->GetRenderer()->SetVuforiaStarted(false);
    return true;
}

bool VuMarkView::DoStartTrackers()
{ 
    Vuforia::TrackerManager &trackerMgr = Vuforia::TrackerManager::getInstance();
    Vuforia::Tracker *objTracker = trackerMgr.getTracker(Vuforia::ObjectTracker::getClassType());
    if (objTracker == nullptr)
    {
        SampleCommon::SampleUtil::Log("VuMarkView", "Cannot start tracker, tracker is null.");
        return false;
    }
    if (!objTracker->start()) {
        SampleCommon::SampleUtil::Log("VuMarkView", "Failed to start tracker.");
        return false;
    }
    return true;
}

// To be called to stop the trackers
bool VuMarkView::DoStopTrackers()
{ 
    Vuforia::TrackerManager &trackerMgr = Vuforia::TrackerManager::getInstance();
    Vuforia::Tracker *objTracker = trackerMgr.getTracker(Vuforia::ObjectTracker::getClassType());
    if (objTracker == nullptr)
    {
        SampleCommon::SampleUtil::Log("VuMarkView", "Cannot stop tracker, tracker is null.");
        return false;
    }
    objTracker->stop();
    return true;
}

// To be called to destroy the trackers' data
bool VuMarkView::DoUnloadTrackersData()
{ 
    Vuforia::TrackerManager &trackerMgr = Vuforia::TrackerManager::getInstance();
    Vuforia::ObjectTracker *objTracker = static_cast<Vuforia::ObjectTracker*>(
        trackerMgr.getTracker(Vuforia::ObjectTracker::getClassType()));

    if (objTracker == nullptr)
    {
        SampleCommon::SampleUtil::Log("VuMarkView", "Object Tracker not started. Failed to unload data.");
        return false;
    }

    Vuforia::DataSet *activeDataSet = objTracker->getActiveDataSet(0);
    if (activeDataSet == m_vuMarkDataSet)
    {
        if (!objTracker->deactivateDataSet(m_vuMarkDataSet))
        {
            SampleCommon::SampleUtil::Log("VuMarkView", "Failed to deactivate dataset.");
            return false;
        }
        if (!objTracker->destroyDataSet(m_vuMarkDataSet)) {
            SampleCommon::SampleUtil::Log("VuMarkView", "Failed to destroy dataset.");
            return false;
        }
    }

    m_vuMarkDataSet = nullptr;
    return true;
}

// To be called to deinitialize the trackers
bool VuMarkView::DoDeinitTrackers()
{ 
    Vuforia::TrackerManager &trackerMgr = Vuforia::TrackerManager::getInstance();
    if (!trackerMgr.deinitTracker(Vuforia::ObjectTracker::getClassType())) {
        SampleCommon::SampleUtil::Log("VuMarkView", "Failed to deinit object tracker.");
        return false;
    }
    return true;
}

// This callback is called after the Vuforia initialization is complete,
// the trackers are initialized, their data loaded and
// tracking is ready to start
void VuMarkView::OnInitARDone()
{ 
    m_main->GetRenderer()->SetVuforiaInitialized(true);
}

void VuMarkView::OnARStarted()
{
    m_main->GetRenderer()->SetVuforiaStarted(true);

    // Try enabling continuous autofocus after camera start
    m_autofocusEnabled = Vuforia::CameraDevice::getInstance().setFocusMode(
        Vuforia::CameraDevice::FOCUS_MODE_CONTINUOUSAUTO);

    if (!m_autofocusEnabled)
    {
        // Failed, it means continuous autofocus is not supported by this device
        SampleCommon::SampleUtil::Log("VuMarkView", "Failed to enable autofocus.");

        // Fall back to normal focus mode
        Vuforia::CameraDevice::getInstance().setFocusMode(Vuforia::CameraDevice::FOCUS_MODE_NORMAL);

        // Update UI Toggle (switch it OFF)
        Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
            Windows::UI::Core::CoreDispatcherPriority::Normal,
            ref new Windows::UI::Core::DispatchedHandler([=]()
        {
            this->AutofocusToggleSwitch->IsOn = false;
        }));
    }

    // Hide init progress indicator
    HideProgressIndicator();

    Vuforia::setHint(Vuforia::HINT_MAX_SIMULTANEOUS_IMAGE_TARGETS, 10);

    Vuforia::onSurfaceCreated();
}

// This callback is called every cycle
void VuMarkView::OnVuforiaUpdate(VuforiaState^ vuforiaState)
{
    //Vuforia::State* state = vuforiaState->m_nativeState;
}

// Window event handlers.

void VuMarkView::OnSuspending(Platform::Object ^ sender, Windows::ApplicationModel::SuspendingEventArgs ^ e)
{
    SaveInternalState(ApplicationData::Current->LocalSettings->Values);
    OnPause();
}

void VuMarkView::OnResuming(Platform::Object ^ sender, Platform::Object ^ args)
{
    LoadInternalState(ApplicationData::Current->LocalSettings->Values);
    OnResume();
}

void VuMarkView::OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args)
{
    m_windowVisible = args->Visible;
    if (m_windowVisible)
    {
        m_appSession->ResumeAR();
        m_main->StartRenderLoop();
    }
    else
    {
        m_main->StopRenderLoop();
        m_appSession->PauseAR();
    }
}

// DisplayInformation event handlers.

void VuMarkView::OnDpiChanged(DisplayInformation^ sender, Object^ args)
{
    critical_section::scoped_lock lock(m_main->GetCriticalSection());
    // Note: The value for LogicalDpi retrieved here may not match the effective DPI of the app
    // if it is being scaled for high resolution devices. Once the DPI is set on DeviceResources,
    // you should always retrieve it using the GetDpi method.
    // See DeviceResources.cpp for more details.
    m_deviceResources->SetDpi(sender->LogicalDpi);
    m_main->CreateWindowSizeDependentResources();
}

void VuMarkView::OnOrientationChanged(DisplayInformation^ sender, Object^ args)
{
    critical_section::scoped_lock lock(m_main->GetCriticalSection());
    m_deviceResources->SetCurrentOrientation(sender->CurrentOrientation);
    m_main->CreateWindowSizeDependentResources();
}

void VuMarkView::OnDisplayContentsInvalidated(DisplayInformation^ sender, Object^ args)
{
    critical_section::scoped_lock lock(m_main->GetCriticalSection());
    m_deviceResources->ValidateDevice();
}

void VuMarkView::OnCompositionScaleChanged(SwapChainPanel^ sender, Object^ args)
{
    critical_section::scoped_lock lock(m_main->GetCriticalSection());
    m_deviceResources->SetCompositionScale(sender->CompositionScaleX, sender->CompositionScaleY);
    m_main->CreateWindowSizeDependentResources();
}

void VuMarkView::OnSwapChainPanelSizeChanged(Object^ sender, SizeChangedEventArgs^ e)
{
    critical_section::scoped_lock lock(m_main->GetCriticalSection());
    m_deviceResources->SetLogicalSize(e->NewSize);
    m_main->CreateWindowSizeDependentResources();
}

// Pointer (touch) callbacks

void VuMarkView::OnPointerPressed(Object^ sender, PointerEventArgs^ e)
{
    static std::atomic<int> tapCount = 0;
    tapCount++;
    static double lastTime = 0;
    double currentTime = m_main->GetTimer().GetTotalSeconds();
    double dt = currentTime - lastTime;
    lastTime = currentTime;

    if (tapCount == 1)
    {
        // Run async task
        Concurrency::create_task([this]() {
            // Wait for half-second, if no second tap comes,
            // then we have confirmed a single-tap
            // else we have a double-tap
            WaitForSingleObjectEx(GetCurrentThread(), 500, FALSE);

            if (tapCount == 1) {
                // tapCount is still 1, nothing changed,
                // so we have confirmed a single-tap,
                // so we consume the single-tap event:
                tapCount = 0;

                if (m_showingMenu)
                {
                    // Hide menu (async on UI thread)
                    Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
                        Windows::UI::Core::CoreDispatcherPriority::Normal,
                        ref new Windows::UI::Core::DispatchedHandler([=]()
                    {
                        HideMenu();
                    }));
                }
                else
                {
                    // Trigger an autofocus event
                    Vuforia::CameraDevice::getInstance().setFocusMode(Vuforia::CameraDevice::FOCUS_MODE_TRIGGERAUTO);

                    // Make sure original focus mode is restored after ~2 seconds
                    WaitForSingleObjectEx(GetCurrentThread(), 2500, FALSE);

                    if (m_autofocusEnabled)
                    {
                        // Restore continuous autofocus
                        Vuforia::CameraDevice::getInstance().setFocusMode(
                            Vuforia::CameraDevice::FOCUS_MODE_CONTINUOUSAUTO);
                    }
                    else
                    {
                        // Restore normal focus mode
                        Vuforia::CameraDevice::getInstance().setFocusMode(
                            Vuforia::CameraDevice::FOCUS_MODE_NORMAL);
                    }
                }
            }
            else if (tapCount >= 2)
            {
                // We process the double-tap event
                tapCount = 0;

                // Show the menu (async on UI thread)
                Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
                    Windows::UI::Core::CoreDispatcherPriority::Normal,
                    ref new Windows::UI::Core::DispatchedHandler([=]()
                {
                    ShowMenu();
                }));
            }
        });
    }
}

void VuMarkView::OnPointerMoved(Object^ sender, PointerEventArgs^ e)
{
    // Move/drag event
}

void VuMarkView::OnPointerReleased(Object^ sender, PointerEventArgs^ e)
{
    // Touch released
}

// Vuforia Menu callbacks

void VuMarkView::OnExitClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    HideMenu();
}

void VuMarkView::OnAutofocusToggled(Object^ sender, RoutedEventArgs^ e)
{
    ToggleSwitch^ toggleSwitch = (ToggleSwitch^)sender;
    if (m_appSession != nullptr && m_appSession->VuforiaInitialized())
    {
        if (toggleSwitch->IsOn)
        {
            // Try enabling continuous autofocus
            m_autofocusEnabled = Vuforia::CameraDevice::getInstance().setFocusMode(
                Vuforia::CameraDevice::FOCUS_MODE_CONTINUOUSAUTO);

            if (!m_autofocusEnabled)
            {
                // Failed, it means continuous autofocus is not supported by this device
                SampleCommon::SampleUtil::Log("ImageTargetsView", "Failed to enabled autofocus.");

                // Fall back to normal focus mode
                Vuforia::CameraDevice::getInstance().setFocusMode(Vuforia::CameraDevice::FOCUS_MODE_NORMAL);

                // Update UI Toggle (switch it OFF)
                Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
                    Windows::UI::Core::CoreDispatcherPriority::Normal,
                    ref new Windows::UI::Core::DispatchedHandler([=]()
                {
                    this->AutofocusToggleSwitch->IsOn = false;
                }));
            }
        }
        else
        {
            m_autofocusEnabled = false;
            Vuforia::CameraDevice::getInstance().setFocusMode(Vuforia::CameraDevice::FOCUS_MODE_NORMAL);
        }
    }

    HideMenu();
}

void VuMarkView::OnExtTrackingToggled(Object^ sender, RoutedEventArgs^ e)
{
    ToggleSwitch^ toggleSwitch = (ToggleSwitch^)sender;
    if (m_appSession != nullptr && m_appSession->VuforiaInitialized()) 
    {
        if (toggleSwitch->IsOn)
        {
            m_extTracking = StartExtendedTracking(m_vuMarkDataSet);
        }
        else
        {
            StopExtendedTracking(m_vuMarkDataSet);
            m_extTracking = false;
        }
        m_main->GetRenderer()->SetExtendedTracking(m_extTracking);
    }

    HideMenu();
}

bool VuMarkView::StartExtendedTracking(Vuforia::DataSet* dataset)
{
    for (int tIdx = 0; tIdx < dataset->getNumTrackables(); tIdx++)
    {
        Vuforia::Trackable* trackable = dataset->getTrackable(tIdx);
        Vuforia::ObjectTarget* objTarget = static_cast<Vuforia::ObjectTarget*>(trackable);
        if (!objTarget->startExtendedTracking())
        {
            Platform::String^ trackableName = ref new Platform::String(
                (wchar_t*)trackable->getName());

            SampleCommon::SampleUtil::Log(
                "VuMarkView",
                "Failed to start extended tracking on target " + trackableName);
            return false;
        }
    }
    //
    return true;
}

void VuMarkView::StopExtendedTracking(Vuforia::DataSet* dataset)
{
    for (int tIdx = 0; tIdx < dataset->getNumTrackables(); tIdx++)
    {
        Vuforia::Trackable* trackable = dataset->getTrackable(tIdx);
        Vuforia::ObjectTarget* objTarget = static_cast<Vuforia::ObjectTarget*>(trackable);
        if (objTarget->isExtendedTrackingStarted())
        {
            objTarget->stopExtendedTracking();
        }
    }
}

void VuMarkView::RestartCameraAsync(Vuforia::CameraDevice::CAMERA_DIRECTION cameraDirection)
{
    auto restartCameraTask = Concurrency::create_task([this, cameraDirection]() {
        m_appSession->StopCamera();
        m_appSession->StartAR(cameraDirection);
    }, task_continuation_context::use_current());

    restartCameraTask.then([this](task<void> t) {
        try 
        {
            // If any exceptions were thrown back in the async chain then
            // this call throws that exception here and we can catch it below
            t.get();
        }
        catch (Platform::Exception^ ex)
        {
            SampleCommon::SampleUtil::ShowError(L"Camera restart error", ex->Message);
        }
    });
}

void VuMarkView::ShowMenu()
{
    this->OptionsMenu->Visibility = Windows::UI::Xaml::Visibility::Visible;

    // Animate it
    this->ShowMenuStoryboard->Begin();
    m_showingMenu = true;  
}

void VuMarkView::HideMenu()
{
    this->OptionsMenu->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    m_showingMenu = false;
}

void VuMarkView::UpdateVuMarkInstance(
    Platform::String^ vuMarkId,
    Platform::String^ vuMarkType,
    int vuMarkImageWidth,
    int vuMarkImageHeight,
    byte* vuMarkImagePixels)
{
    // We don't update right away the value of the vumark on the card
    // as the card will be first dismissed. 
    // The ShowCard method will get the value and 
    // display it only at the end of the animation
    // so that the disappearing card keeps the old value.
    Concurrency::reader_writer_lock::scoped_lock lock(m_vuMarkCardLock);
    m_vuMarkId = vuMarkId;
    m_vuMarkType = vuMarkType;

    if (vuMarkImagePixels == nullptr) {
        return;
    }

    int wid = vuMarkImageWidth;
    int hgt = vuMarkImageHeight;
    int imageSize = wid * hgt * 4;

    if (m_vuMarkImageWidth != wid || m_vuMarkImageHeight != hgt)
    {
        m_vuMarkImageWidth = wid;
        m_vuMarkImageHeight = hgt;

        if (m_vuMarkImagePixels != nullptr) {
            // Delete the old pixel buffer
            delete[] m_vuMarkImagePixels;
        }
        m_vuMarkImagePixels = new byte[imageSize];
    }

    // Copy pixels and convert from RGBA to BGRA
    for (int r = 0; r < hgt; ++r)
    {
        for (int c = 0; c < wid; ++c)
        {
            m_vuMarkImagePixels[4 * (r*wid + c) + 0] = vuMarkImagePixels[4 * (r*wid + c) + 2];
            m_vuMarkImagePixels[4 * (r*wid + c) + 1] = vuMarkImagePixels[4 * (r*wid + c) + 1];
            m_vuMarkImagePixels[4 * (r*wid + c) + 2] = vuMarkImagePixels[4 * (r*wid + c) + 0];
            m_vuMarkImagePixels[4 * (r*wid + c) + 3] = vuMarkImagePixels[4 * (r*wid + c) + 3];
        }
    }
}

void VuMarkView::UpdateVuMarkCard()
{
    Concurrency::reader_writer_lock::scoped_lock lock(m_vuMarkCardLock);

    if (m_vuMarkId == this->VuMarkIdTextBlock->Text) return;

    this->VuMarkIdTextBlock->Text = (m_vuMarkId != nullptr) ? m_vuMarkId : "";
    this->VuMarkTypeTextBlock->Text = (m_vuMarkType != nullptr) ? m_vuMarkType : "";
    
    if (m_vuMarkImagePixels != nullptr && m_vuMarkImageWidth > 0)
    {
        auto writeableBmp = ref new WriteableBitmap(m_vuMarkImageWidth, m_vuMarkImageHeight);

        ComPtr<IInspectable> insp(reinterpret_cast<IInspectable*>(writeableBmp->PixelBuffer));

        // Query the IBufferByteAccess interface.
        ComPtr<Windows::Storage::Streams::IBufferByteAccess> bufferByteAccess;
        insp.As(&bufferByteAccess);

        // Get native pointer to the buffer data.
        byte* pixels = nullptr;
        bufferByteAccess->Buffer(&pixels);

        // copy image data in BGRA8 format into 'pixels' buffer
        memcpy_s(
            pixels, writeableBmp->PixelBuffer->Capacity,
            m_vuMarkImagePixels, m_vuMarkImageWidth * m_vuMarkImageHeight * 4);

        this->VuMarkImage->Source = writeableBmp;
    }
}

void VuMarkView::ShowVuMarkCard()
{
    if (m_showingVuMarkCard) {
        // Already showing, skip
        return;
    }

    UpdateVuMarkCard();

    m_showingVuMarkCard = true;
    this->VuMarkCard->Visibility = Windows::UI::Xaml::Visibility::Visible;

    // Animate card (appearing)
    this->ShowVuMarkCardStoryboard->Begin();
}

void VuMarkView::HideVuMarkCard()
{
    if (!m_showingVuMarkCard) {
        // Already hidden or hiding
        return;
    }

    // Animate card (disappearing)
    m_showingVuMarkCard = false;
    this->HideVuMarkCardStoryboard->Begin();
}

void VuMarkView::OnVuMarkCardAnimationDone(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    this->VuMarkCard->Visibility = m_showingVuMarkCard ? 
        Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
}

void VuMarkView::OnVuMarkCardTapped(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    HideVuMarkCard();
}

void VuMarkView::HideProgressIndicator()
{
    auto dispatcher = Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher;
    dispatcher->RunAsync(
        Windows::UI::Core::CoreDispatcherPriority::High,
        ref new Windows::UI::Core::DispatchedHandler([this]()
    {
        this->InitProgressRing->IsActive = false;
        this->InitProgressRing->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
        m_showingProgress = false;
    }));
}
