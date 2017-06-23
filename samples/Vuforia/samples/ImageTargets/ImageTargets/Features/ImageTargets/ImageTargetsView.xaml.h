/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#pragma once

#include "Features\ImageTargets\ImageTargetsView.g.h"
#include "Common\DeviceResources.h"
#include "ImageTargetsMain.h"
#include "SampleApplication\AppSession.h"
#include "SampleApplication\AppControl.h"

#include <ppltasks.h>
#include <vector>

#include <Vuforia\DataSet.h>


namespace ImageTargets
{
    /// <summary>
    /// A page that hosts a DirectX SwapChainPanel.
    /// </summary>
    public ref class ImageTargetsView sealed : public AppControl
    {
    public:
        ImageTargetsView();
        virtual ~ImageTargetsView();

        void SaveInternalState(Windows::Foundation::Collections::IPropertySet^ state);
        void LoadInternalState(Windows::Foundation::Collections::IPropertySet^ state);

        void OnResume();
        void OnPause();

        // ============================== //
        // AppControl interface methods 
        
        // To be called to initialize the trackers
        virtual bool DoInitTrackers();

        // To be called to load the trackers' data
        virtual bool DoLoadTrackersData();

        // To be called to start the trackers
        virtual bool DoStartTrackers();

        // To be called to start the camera
        virtual bool DoStartCamera(int direction);

        // To be called to stop the camera
        virtual bool DoStopCamera();

        // To be called to stop the trackers
        virtual bool DoStopTrackers();

        // To be called to destroy the trackers' data
        virtual bool DoUnloadTrackersData();

        // To be called to deinitialize the trackers
        virtual bool DoDeinitTrackers();

        // This callback is called after the Vuforia initialization is complete,
        // the trackers are initialized, their data loaded and
        // tracking is ready to start
        virtual void OnInitARDone();

        // This callback is called after the Vuforia Camera and Trackers have started
        virtual void OnARStarted();

        // This callback is called every cycle
        virtual void OnVuforiaUpdate(VuforiaState^ vuforiaState);
        // ============================== //

    private:
        void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ e);
        void OnResuming(Platform::Object ^sender, Platform::Object ^args);

        // Window event handlers.
        void OnVisibilityChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args);

        // DisplayInformation event handlers.
        void OnDpiChanged(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);
        void OnOrientationChanged(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);
        void OnDisplayContentsInvalidated(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);

        // Other event handlers.
        
        void OnCompositionScaleChanged(Windows::UI::Xaml::Controls::SwapChainPanel^ sender, Object^ args);
        void OnSwapChainPanelSizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e);

        // Track our independent input on a background worker thread.
        Windows::Foundation::IAsyncAction^ m_inputLoopWorker;
        Windows::UI::Core::CoreIndependentInputSource^ m_coreInput;
        Windows::UI::Input::GestureRecognizer^ m_gestureRecognizer;

        // Independent input handling functions.
        void OnPointerPressed(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);
        void OnPointerMoved(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);
        void OnPointerReleased(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);

        // Sample App Menu control
        void ShowMenu();
        void HideMenu();
        void HideProgressIndicator();

        void OnExtTrackingToggled(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnAutofocusToggled(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        bool StartExtendedTracking(Vuforia::DataSet* dataset);
        void StopExtendedTracking(Vuforia::DataSet* dataset);

        void OnRearCameraChecked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnFrontCameraChecked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void RestartCameraAsync(Vuforia::CameraDevice::CAMERA_DIRECTION cameraDirection);

        void OnSwapDataset(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnExitClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnMenuAnimationDone(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        
        // Resources used to render the DirectX content in the XAML page background.
        std::shared_ptr<DX::DeviceResources> m_deviceResources;
        std::unique_ptr<ImageTargetsMain> m_main;
        
        // Vuforia Sample App Session
        std::shared_ptr<AppSession> m_appSession;

        // Vuforia Dataset
        Vuforia::DataSet *m_currentDataSet;
        Vuforia::DataSet *m_stonesDataSet;
        Vuforia::DataSet *m_tarmacDataSet;

        std::atomic<bool> m_swapDataset;
        std::atomic<bool> m_autofocusEnabled;
        std::atomic<bool> m_extTracking;
        std::atomic<bool> m_flashTorchEnabled;
        std::atomic<bool> m_windowVisible;
        std::atomic<bool> m_showingMenu;
        std::atomic<bool> m_showingProgress;
};
}

