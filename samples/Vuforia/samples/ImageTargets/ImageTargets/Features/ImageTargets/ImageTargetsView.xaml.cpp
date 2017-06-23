/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#include "pch.h"

#include "ImageTargetsView.xaml.h"
#include "Common\SampleUtil.h"

#include <Vuforia\Vuforia.h>
#include <Vuforia\Vuforia_UWP.h>
#include <Vuforia\CameraDevice.h>
#include <Vuforia\Device.h>
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
using namespace Windows::UI::Xaml::Navigation;
using namespace concurrency;

using namespace ImageTargets;


ImageTargetsView::ImageTargetsView():
    m_windowVisible(true),
    m_showingMenu(false),
    m_showingProgress(true),
    m_coreInput(nullptr),
    m_swapDataset(false),
    m_autofocusEnabled(true),
    m_extTracking(false),
    m_flashTorchEnabled(false),
    m_currentDataSet(nullptr),
    m_stonesDataSet(nullptr),
    m_tarmacDataSet(nullptr)
{
    InitializeComponent();
    Application^ app = Application::Current; 
    app->Suspending += ref new SuspendingEventHandler(this, &ImageTargetsView::OnSuspending);
    app->Resuming += ref new EventHandler<Object^>(this, &ImageTargetsView::OnResuming);

    // Register event handlers for page lifecycle.
    CoreWindow^ window = Window::Current->CoreWindow;

    window->VisibilityChanged +=
        ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &ImageTargetsView::OnVisibilityChanged);

    DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

    currentDisplayInformation->DpiChanged +=
        ref new TypedEventHandler<DisplayInformation^, Object^>(this, &ImageTargetsView::OnDpiChanged);

    currentDisplayInformation->OrientationChanged +=
        ref new TypedEventHandler<DisplayInformation^, Object^>(this, &ImageTargetsView::OnOrientationChanged);

    DisplayInformation::DisplayContentsInvalidated +=
        ref new TypedEventHandler<DisplayInformation^, Object^>(this, &ImageTargetsView::OnDisplayContentsInvalidated);

    swapChainPanel->CompositionScaleChanged += 
        ref new TypedEventHandler<SwapChainPanel^, Object^>(this, &ImageTargetsView::OnCompositionScaleChanged);

    swapChainPanel->SizeChanged +=
        ref new SizeChangedEventHandler(this, &ImageTargetsView::OnSwapChainPanelSizeChanged);

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
        m_coreInput->PointerPressed += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &ImageTargetsView::OnPointerPressed);
        m_coreInput->PointerMoved += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &ImageTargetsView::OnPointerMoved);
        m_coreInput->PointerReleased += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &ImageTargetsView::OnPointerReleased);
        
        // Begin processing input messages as they're delivered.
        m_coreInput->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);

    });

    // Run task on a dedicated high priority background thread.
    m_inputLoopWorker = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);

    // Init Vuforia App Session
    m_appSession = std::shared_ptr<AppSession>(new AppSession(this));
    m_appSession->InitAR();

    m_main = std::unique_ptr<ImageTargetsMain>(new ImageTargetsMain(m_deviceResources, m_appSession));
    m_main->StartRenderLoop();
}

ImageTargetsView::~ImageTargetsView()
{
    // Stop rendering and processing events on destruction.
    m_main->StopRenderLoop();
    m_coreInput->Dispatcher->StopProcessEvents();

    // Stop and deinit Vuforia
    m_appSession->StopAR();
    
    // Notify renderer
    m_main->GetRenderer()->SetVuforiaInitialized(false);
}

// Saves the current state of the app for suspend and terminate events.
void ImageTargetsView::SaveInternalState(IPropertySet^ state)
{
    critical_section::scoped_lock lock(m_main->GetCriticalSection());
    m_deviceResources->Trim();

    // Stop rendering when the app is suspended.
    m_main->StopRenderLoop();

    // Put code to save app state here.
}

// Loads the current state of the app for resume events.
void ImageTargetsView::LoadInternalState(IPropertySet^ state)
{
    // Put code to load app state here.

    // Start rendering when the app is resumed.
    m_main->StartRenderLoop();
}

void ImageTargetsView::OnPause()
{
    try
    {
        m_appSession->PauseAR();
    }
    catch (Platform::Exception^ ex)
    {
        SampleCommon::SampleUtil::Log("ImageTargetsView", ex->Message);
    }
}

void ImageTargetsView::OnResume()
{
    try
    {
        m_appSession->ResumeAR();
    }
    catch (Platform::Exception^ ex)
    {
        SampleCommon::SampleUtil::Log("ImageTargetsView", ex->Message);
    }
}

bool ImageTargetsView::DoInitTrackers()
{ 
    Vuforia::TrackerManager &trackerMgr = Vuforia::TrackerManager::getInstance();
    Vuforia::Tracker *objTracker = trackerMgr.initTracker(Vuforia::ObjectTracker::getClassType());
    if (objTracker == nullptr) {
        SampleCommon::SampleUtil::Log("ImageTargetsView", "Failed to initialize object tracker.");
        return false;
    }
    return true;
}

bool ImageTargetsView::DoLoadTrackersData()
{    
    Vuforia::TrackerManager &trackerMgr = Vuforia::TrackerManager::getInstance();
    Vuforia::ObjectTracker *objTracker = static_cast<Vuforia::ObjectTracker*>(
        trackerMgr.getTracker(Vuforia::ObjectTracker::getClassType()));

    if (objTracker == nullptr)
    {
        SampleCommon::SampleUtil::Log("ImageTargetsView", "Object Tracker not started. Failed to load data.");
        return false;
    }

    m_stonesDataSet = objTracker->createDataSet();
    if (m_stonesDataSet == nullptr) {
        SampleCommon::SampleUtil::Log("ImageTargetsView", "Failed to create dataset.");
        return false;
    }
    if (!m_stonesDataSet->load("Assets\\ImageTargets\\StonesAndChips.xml", Vuforia::STORAGE_TYPE::STORAGE_APPRESOURCE))
    {
        SampleCommon::SampleUtil::Log("ImageTargetsView", "Failed to load 'StonesAndChips' dataset.");
        return false;
    }

    m_tarmacDataSet = objTracker->createDataSet();
    if (m_tarmacDataSet == nullptr) {
        SampleCommon::SampleUtil::Log("ImageTargetsView", "Failed to create dataset.");
        return false;
    }
    if (!m_tarmacDataSet->load("Assets\\ImageTargets\\Tarmac.xml", Vuforia::STORAGE_TYPE::STORAGE_APPRESOURCE))
    {
        SampleCommon::SampleUtil::Log("ImageTargetsView", "Failed to load 'Tarmac' dataset.");
        return false;
    }

    m_currentDataSet = m_stonesDataSet;

    if (!objTracker->activateDataSet(m_currentDataSet))
    {
        SampleCommon::SampleUtil::Log("ImageTargetsView", "Failed to activate dataset.");
        return false;
    }

    return true;
}

bool ImageTargetsView::DoStartCamera(int direction)
{
    Vuforia::Device::getInstance().setMode(Vuforia::Device::MODE_AR);
    
    Vuforia::CameraDevice::CAMERA_DIRECTION camDirection = 
        static_cast<Vuforia::CameraDevice::CAMERA_DIRECTION>(direction);

    if (!Vuforia::CameraDevice::getInstance().init(camDirection))
    {
        SampleCommon::SampleUtil::Log("ImageTargetsView", "Failed to init camera.");
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
        SampleCommon::SampleUtil::Log("ImageTargetsView", "Failed to start camera.");
        return false;
    }

    return true;
}

bool ImageTargetsView::DoStopCamera()
{
    // Notify renderer, do this before stopping the camera
    m_main->GetRenderer()->SetVuforiaStarted(false);

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

    return true;
}

bool ImageTargetsView::DoStartTrackers()
{ 
    Vuforia::TrackerManager &trackerMgr = Vuforia::TrackerManager::getInstance();
    Vuforia::Tracker *objTracker = trackerMgr.getTracker(Vuforia::ObjectTracker::getClassType());
    if (objTracker == nullptr)
    {
        SampleCommon::SampleUtil::Log("ImageTargetsView", "Cannot start tracker, tracker is null.");
        return false;
    }

    if (!objTracker->start()) {
        SampleCommon::SampleUtil::Log("ImageTargetsView", "Failed to start tracker.");
        return false;
    }
        
    return true;
}

// To be called to stop the trackers
bool ImageTargetsView::DoStopTrackers()
{ 
    Vuforia::TrackerManager &trackerMgr = Vuforia::TrackerManager::getInstance();
    Vuforia::Tracker *objTracker = trackerMgr.getTracker(Vuforia::ObjectTracker::getClassType());
    if (objTracker == nullptr)
    {
        SampleCommon::SampleUtil::Log("ImageTargetsView", "Cannot stop tracker, tracker is null.");
        return false;
    }
    objTracker->stop();
    return true;
}

// To be called to destroy the trackers' data
bool ImageTargetsView::DoUnloadTrackersData()
{ 
    Vuforia::TrackerManager &trackerMgr = Vuforia::TrackerManager::getInstance();
    Vuforia::ObjectTracker *objTracker = static_cast<Vuforia::ObjectTracker*>(
        trackerMgr.getTracker(Vuforia::ObjectTracker::getClassType()));

    if (objTracker == nullptr)
    {
        SampleCommon::SampleUtil::Log("ImageTargetsView", "Object Tracker not started. Failed to unload data.");
        return false;
    }

    Vuforia::DataSet *activeDataSet = objTracker->getActiveDataSet(0);
    if (activeDataSet == m_currentDataSet)
    {
        if (!objTracker->deactivateDataSet(m_currentDataSet))
        {
            SampleCommon::SampleUtil::Log("ImageTargetsView", "Failed to deactivate dataset.");
            return false;
        }
        if (!objTracker->destroyDataSet(m_currentDataSet)) {
            SampleCommon::SampleUtil::Log("ImageTargetsView", "Failed to destroy dataset.");
            return false;
        }
    }

    m_currentDataSet = nullptr;
    return true;
}

// To be called to deinitialize the trackers
bool ImageTargetsView::DoDeinitTrackers()
{ 
    Vuforia::TrackerManager &trackerMgr = Vuforia::TrackerManager::getInstance();
    if (!trackerMgr.deinitTracker(Vuforia::ObjectTracker::getClassType())) {
        SampleCommon::SampleUtil::Log("ImageTargetsView", "Failed to deinit object tracker.");
        return false;
    }
    return true;
}

// This callback is called after the Vuforia initialization is complete,
// the trackers are initialized, their data loaded and
// tracking is ready to start
void ImageTargetsView::OnInitARDone()
{ 
    m_main->GetRenderer()->SetVuforiaInitialized(true);
}

void ImageTargetsView::OnARStarted()
{
    m_main->GetRenderer()->SetVuforiaStarted(true);

    // Try enabling continuous autofocus at start
    m_autofocusEnabled = Vuforia::CameraDevice::getInstance().setFocusMode(
        Vuforia::CameraDevice::FOCUS_MODE_CONTINUOUSAUTO);

    if (!m_autofocusEnabled)
    {
        // Failed, it means continuous autofocus is not supported by this device
        SampleCommon::SampleUtil::Log("ImageTargetsView", "Failed to enable autofocus.");

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

    // You can set HINT_MAX_SIMULTANEOUS_IMAGE_TARGETS to 2 or more
    // if you wish to track multiple image targets simultaneously
    Vuforia::setHint(Vuforia::HINT_MAX_SIMULTANEOUS_IMAGE_TARGETS, 1);

    Vuforia::onSurfaceCreated();
}

// This callback is called every cycle
void ImageTargetsView::OnVuforiaUpdate(VuforiaState^ vuforiaState)
{
    Vuforia::State* state = vuforiaState->m_nativeState;

    if (m_swapDataset)
    {
        m_swapDataset = false;
           
        Vuforia::TrackerManager &trackerMgr = Vuforia::TrackerManager::getInstance();
        Vuforia::ObjectTracker *objTracker = static_cast<Vuforia::ObjectTracker*>(
            trackerMgr.getTracker(Vuforia::ObjectTracker::getClassType()));

        if (objTracker != nullptr) {
            Vuforia::DataSet* activeDataset = objTracker->getActiveDataSet(0);
            Vuforia::DataSet* datasetToActivate = (activeDataset == m_stonesDataSet) ? 
                m_tarmacDataSet : m_stonesDataSet;

            // Deactivate currently active dataset
            objTracker->deactivateDataSet(activeDataset);
            
            // Activate the 'other' dataset
            if (!objTracker->activateDataSet(datasetToActivate)) {
                SampleCommon::SampleUtil::Log("ImageTargetsView", "Failed to activate dataset.");
            }

            m_currentDataSet = datasetToActivate;
        }
    }
}

// Window event handlers.

void ImageTargetsView::OnSuspending(Platform::Object ^ sender, Windows::ApplicationModel::SuspendingEventArgs ^ e)
{
    SaveInternalState(ApplicationData::Current->LocalSettings->Values);
    OnPause();
}

void ImageTargetsView::OnResuming(Platform::Object ^ sender, Platform::Object ^ args)
{
    LoadInternalState(ApplicationData::Current->LocalSettings->Values);
    OnResume();
}

void ImageTargetsView::OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args)
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

void ImageTargetsView::OnDpiChanged(DisplayInformation^ sender, Object^ args)
{
    critical_section::scoped_lock lock(m_main->GetCriticalSection());
    // Note: The value for LogicalDpi retrieved here may not match the effective DPI of the app
    // if it is being scaled for high resolution devices. Once the DPI is set on DeviceResources,
    // you should always retrieve it using the GetDpi method.
    // See DeviceResources.cpp for more details.
    m_deviceResources->SetDpi(sender->LogicalDpi);
    m_main->CreateWindowSizeDependentResources();
}

void ImageTargetsView::OnOrientationChanged(DisplayInformation^ sender, Object^ args)
{
    critical_section::scoped_lock lock(m_main->GetCriticalSection());
    m_deviceResources->SetCurrentOrientation(sender->CurrentOrientation);
    m_main->CreateWindowSizeDependentResources();
}

void ImageTargetsView::OnDisplayContentsInvalidated(DisplayInformation^ sender, Object^ args)
{
    critical_section::scoped_lock lock(m_main->GetCriticalSection());
    m_deviceResources->ValidateDevice();
}

void ImageTargetsView::OnCompositionScaleChanged(SwapChainPanel^ sender, Object^ args)
{
    critical_section::scoped_lock lock(m_main->GetCriticalSection());
    m_deviceResources->SetCompositionScale(sender->CompositionScaleX, sender->CompositionScaleY);
    m_main->CreateWindowSizeDependentResources();
}

void ImageTargetsView::OnSwapChainPanelSizeChanged(Object^ sender, SizeChangedEventArgs^ e)
{
    critical_section::scoped_lock lock(m_main->GetCriticalSection());
    m_deviceResources->SetLogicalSize(e->NewSize);
    m_main->CreateWindowSizeDependentResources();
}

// Pointer (touch) callbacks

void ImageTargetsView::OnPointerPressed(Object^ sender, PointerEventArgs^ e)
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

void ImageTargetsView::OnPointerMoved(Object^ sender, PointerEventArgs^ e)
{
    // Move/drag event
}

void ImageTargetsView::OnPointerReleased(Object^ sender, PointerEventArgs^ e)
{
    // Touch released
}

// Vuforia Menu callbacks

void ImageTargetsView::OnExitClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    HideMenu();
}

void ImageTargetsView::OnAutofocusToggled(Object^ sender, RoutedEventArgs^ e)
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

                // Update UI (set Switch OFF)
                toggleSwitch->IsOn = false;
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

void ImageTargetsView::OnExtTrackingToggled(Object^ sender, RoutedEventArgs^ e)
{
    ToggleSwitch^ toggleSwitch = (ToggleSwitch^)sender;
    if (m_appSession != nullptr && m_appSession->VuforiaInitialized()) 
    {
        if (toggleSwitch->IsOn)
        {
            m_extTracking = 
                StartExtendedTracking(m_stonesDataSet) &&
                StartExtendedTracking(m_tarmacDataSet);
        }
        else
        {
            StopExtendedTracking(m_stonesDataSet);
            StopExtendedTracking(m_tarmacDataSet);
            m_extTracking = false;
        }
        m_main->GetRenderer()->SetExtendedTracking(m_extTracking);
    }

    HideMenu();
}

bool ImageTargetsView::StartExtendedTracking(Vuforia::DataSet* dataset)
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
                "ImageTargetsView",
                "Failed to start extended tracking on target " + trackableName);
            return false;
        }
    }
    //
    return true;
}

void ImageTargetsView::StopExtendedTracking(Vuforia::DataSet* dataset)
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

void ImageTargetsView::RestartCameraAsync(Vuforia::CameraDevice::CAMERA_DIRECTION cameraDirection)
{
    auto restartCameraTask = Concurrency::create_task([this, cameraDirection]() {
        m_appSession->StopCamera();

        // Wait a few milliseconds after camera stop,
        // before restarting the camera
        WaitForSingleObjectEx(GetCurrentThread(), 200, FALSE);
        SampleCommon::SampleUtil::Log("ImageTargetsView", "Restart camera...");

        m_appSession->StartAR(cameraDirection);
        OnARStarted();
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

void ImageTargetsView::OnRearCameraChecked(Object^ sender, RoutedEventArgs^ e)
{
    if (m_appSession != nullptr && m_appSession->VuforiaInitialized()) 
    {
        if (m_appSession->CameraDirection() != Vuforia::CameraDevice::CAMERA_DIRECTION_BACK) {
            RestartCameraAsync(Vuforia::CameraDevice::CAMERA_DIRECTION_BACK);
        }

        HideMenu();
    }
}

void ImageTargetsView::OnFrontCameraChecked(Object^ sender, RoutedEventArgs^ e)
{
    if (m_appSession != nullptr && m_appSession->VuforiaInitialized()) 
    {
        if (m_appSession->CameraDirection() != Vuforia::CameraDevice::CAMERA_DIRECTION_FRONT) {
            RestartCameraAsync(Vuforia::CameraDevice::CAMERA_DIRECTION_FRONT);
        }

        HideMenu();
    }
}

void ImageTargetsView::OnSwapDataset(Object^ sender, RoutedEventArgs^ e)
{
    if (m_appSession != nullptr && m_appSession->VuforiaInitialized()) {
        m_swapDataset = true;

        HideMenu();
    }
}

void ImageTargetsView::ShowMenu()
{
    if (m_showingMenu) return;

    m_showingMenu = true;
    this->OptionsMenu->Visibility = Windows::UI::Xaml::Visibility::Visible;

    // Animate Menu (opening)
    this->ShowMenuStoryboard->Begin();
}

void ImageTargetsView::HideMenu()
{
    if (!m_showingMenu) return;

    m_showingMenu = false;

    // Animate Menu (closing)
    this->HideMenuStoryboard->Begin();
}

void ImageTargetsView::OnMenuAnimationDone(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    this->OptionsMenu->Visibility = m_showingMenu ?
        Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
}

void ImageTargetsView::HideProgressIndicator()
{
    if (!m_showingProgress) {
        // Skip, as already hidden
        return;
    }

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
