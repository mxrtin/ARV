/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#include "pch.h"
#include "AppSession.h"

#include "..\Common\SampleUtil.h"

#include <Vuforia\Vuforia.h>
#include <Vuforia\Vuforia_UWP.h>
#include <Vuforia\CameraDevice.h>
#include <Vuforia\DXRenderer.h>
#include <Vuforia\Renderer.h>
#include <Vuforia\Tracker.h>
#include <Vuforia\VideoBackgroundConfig.h>

using namespace Concurrency;
using namespace Windows::Foundation;

using namespace Vuforia;
using namespace VuMark;

AppSession::AppSession(AppControl^ appControl) :
    m_cameraRunning(false),
    m_vuforiaInitialized(false),
    m_cameraDirection(Vuforia::CameraDevice::CAMERA_DIRECTION_DEFAULT),
    m_appControl(appControl)
{
}

void AppSession::Vuforia_onUpdate(Vuforia::State& state)
{
    VuforiaState^ vuforiaState = ref new VuforiaState();
    vuforiaState->m_nativeState = &state;
    m_appControl->OnVuforiaUpdate(vuforiaState);
}

void AppSession::InitAR()
{
    Vuforia::setInitParameters("");
    
    auto initVuforiaTask = InitVuforiaAsync();

    auto initTrackersTask = initVuforiaTask.then([this](int result) {    
        if (result < 0) {
            ThrowInitError(result);
        }
        
        if (!m_appControl->DoInitTrackers()) {
            throw ref new Platform::Exception(E_FAIL, "Failed to init Trackers.");
        }
    });

    auto loadTrackersDataTask = initTrackersTask.then([this]() {
        if (!m_appControl->DoLoadTrackersData()) {
            throw ref new Platform::Exception(E_FAIL, "Failed to Load Tracker Data.");
        }

        m_vuforiaInitialized = true;
        m_appControl->OnInitARDone();
    });

    auto startARTask = loadTrackersDataTask.then([this]() {
        // Wait a couple of seconds
        WaitForSingleObjectEx(GetCurrentThread(), 2000, FALSE);

        // Start default camera (typically this is the rear camera)
        m_cameraDirection = Vuforia::CameraDevice::CAMERA_DIRECTION_DEFAULT;
        StartAR(m_cameraDirection);

        // Register Vuforia UpdateCallback
        Vuforia::registerCallback(this);
    });

    startARTask.then([this](task<void> t) {
        try
        {
            // If any exceptions were thrown back in the async chain then
            // this call throws that exception here and we can catch it below
            t.get();

            // If we are here, it means Vuforia has started successfully
            m_appControl->OnARStarted();
        }
        catch (Platform::Exception^ ex)
        {
            SampleCommon::SampleUtil::ShowError(L"Vuforia Initialization Error", ex->Message);
        }
    });
}

void AppSession::StartAR(Vuforia::CameraDevice::CAMERA_DIRECTION cameraDirection)
{
    if (m_cameraRunning)
    {
        // Camera already running
        return;
    }
    
    if (!m_appControl->DoStartCamera((int)cameraDirection))
    {
        throw ref new Platform::Exception(E_FAIL, "Failed to start camera.");
    }
    
    m_cameraDirection = cameraDirection;

    // Start Trackers
    if (!m_appControl->DoStartTrackers())
    {
        throw ref new Platform::Exception(E_FAIL, "Failed to start trackers.");
    }

    // AR camera is now up and running
    m_cameraRunning = true;
}

void AppSession::ConfigureVideoBackground(
    float screenWidth, float screenHeight, 
    Windows::Graphics::Display::DisplayOrientations orientation)
{
    if (screenWidth <= 0 || screenHeight <= 0)
    {
        SampleCommon::SampleUtil::Log(
            "AppSession", 
            "Invalid screen size, skipping video background config.");
        return;
    }
    
    // Get the default video mode:
    Vuforia::CameraDevice& cameraDevice = Vuforia::CameraDevice::getInstance();
    Vuforia::VideoMode videoMode = cameraDevice.getVideoMode(Vuforia::CameraDevice::MODE_DEFAULT);

    if (videoMode.mWidth <= 0 || videoMode.mHeight <= 0)
    {
        SampleCommon::SampleUtil::Log(
            "AppSession",
            "Video Mode not ready, skipping video background config.");
        return;
    }

    // Configure the video background
    Vuforia::VideoBackgroundConfig config;
    config.mEnabled = true;
    config.mPosition.data[0] = 0;
    config.mPosition.data[1] = 0;
   
    bool isPortrait = 
        (orientation == Windows::Graphics::Display::DisplayOrientations::Portrait) ||
        (orientation == Windows::Graphics::Display::DisplayOrientations::PortraitFlipped);

    if (isPortrait)
    {
        config.mSize.data[0] = (int)(videoMode.mHeight * 
            (screenHeight / (float)videoMode.mWidth));
        config.mSize.data[1] = (int)screenHeight;

        if (config.mSize.data[0] < screenWidth)
        {
            config.mSize.data[0] = (int)screenWidth;
            config.mSize.data[1] = (int)(screenWidth * 
                (videoMode.mWidth / (float)videoMode.mHeight));
        }
    }
    else
    {
        config.mSize.data[0] = (int)screenWidth;
        config.mSize.data[1] = (int)(videoMode.mHeight * 
            (screenWidth / (float)videoMode.mWidth));

        if (config.mSize.data[1] < screenHeight)
        {
            config.mSize.data[0] = (int)(screenHeight * 
                (videoMode.mWidth / (float)videoMode.mHeight));
            config.mSize.data[1] = (int)screenHeight;
        }
    }

    Platform::String^ videoBgMsg = L"Configure Video Background: ";
    videoBgMsg += L" video: " + videoMode.mWidth + "," + videoMode.mHeight;
    videoBgMsg += L" screen: " + screenWidth + "," + screenHeight;
    videoBgMsg += L" size: " + config.mSize.data[0] + "," + config.mSize.data[1];

    SampleCommon::SampleUtil::Log("AppSession", videoBgMsg);
    
    // Set the config:
    Vuforia::Renderer::getInstance().setVideoBackgroundConfig(config);
}

void AppSession::StopAR()
{
    if (!m_vuforiaInitialized)
        return;

    StopCamera();

    // Unregister the Vuforia callback
    Vuforia::registerCallback(nullptr);

    // Ensure that all asynchronous operations to initialize Vuforia
    // and loading the tracker datasets do not overlap:
    boolean unloadTrackersResult;
    boolean deinitTrackersResult;
    
    // Destroy the tracking data set:
    unloadTrackersResult = m_appControl->DoUnloadTrackersData();

    // Deinitialize the trackers:
    deinitTrackersResult = m_appControl->DoDeinitTrackers();

    // Deinitialize Vuforia SDK:
    Vuforia::deinit();

    m_vuforiaInitialized = false;

    if (!unloadTrackersResult)
        throw ref new Platform::Exception(E_FAIL, "Failed to unload trackers data.");

    if (!deinitTrackersResult)
        throw ref new Platform::Exception(E_FAIL, "Failed to unload trackers data.");
}

void AppSession::StopCamera()
{
    if (m_cameraRunning)
    {
        if (!m_appControl->DoStopTrackers())
            throw ref new Platform::Exception(E_FAIL, "Failed to stop trackers.");

        if (!m_appControl->DoStopCamera())
            throw ref new Platform::Exception(E_FAIL, "Failed to stop camera.");

        m_cameraRunning = false;
    }
}

// Resumes Vuforia, restarts the trackers and the camera
void AppSession::ResumeAR()
{
    if (m_vuforiaInitialized)
    {
        // For an explanation of the following see the comment above
        // the declaration of m_resumeTask

        // Check for existing running resume task
        if (m_resumeTask == nullptr || m_resumeTask->Status != AsyncStatus::Started)
        {
            // To make sure we check the currently running pause task
            // (and not one created while we're running) we capture the
            // pointer here
            IAsyncAction^ localPauseTask = m_pauseTask;

            m_resumeTask = Concurrency::create_async([this, localPauseTask]() {
                if (localPauseTask != nullptr)
                {
                    while (localPauseTask->Status == AsyncStatus::Started)
                    {
                        Concurrency::wait(100);
                    }
                }

                // Only perform actions if we want to resume even after further events
                // Check that the same number or more pause events have been received
                if (m_ignoredResume >= m_ignoredPaused)
                {
                    Vuforia::onResume();
                    StartAR(m_cameraDirection);

                    // Notify app control
                    m_appControl->OnARStarted();
                }
                else
                {
                    m_ignoredPaused--;
                }
            });
        }
        else
        {
            m_ignoredResume++;
        }
    }
}

// Pauses Vuforia and stops the camera
void AppSession::PauseAR()
{
    if (m_vuforiaInitialized)
    {
        // For an explanation of the following see the comment above
        // the declaration of m_pauseTask

        // Check for existing running pause task
        if (m_pauseTask == nullptr || m_pauseTask->Status != AsyncStatus::Started)
        {
            // To make sure we check the currently running resume task
            // (and not one created while we're running) we capture the
            // pointer here
            IAsyncAction^ localResumeTask = m_resumeTask;

            // The camera must be stopped asynchronously
            m_pauseTask = Concurrency::create_async([this, localResumeTask]() {
                if (localResumeTask != nullptr)
                {
                    while (localResumeTask->Status == AsyncStatus::Started)
                    {
                        Concurrency::wait(100);
                    }
                }

                // Only perform actions if we want to pause even after further events
                // Check that the same number or more resume events have been received
                if (m_ignoredPaused >= m_ignoredResume)
                {
                    StopCamera();
                    Vuforia::onPause();
                }
                else
                {
                    m_ignoredResume--;
                }
            });
        }
        else
        {
            m_ignoredPaused++;
        }
    }
}

Concurrency::task<int> AppSession::InitVuforiaAsync()
{
    return Concurrency::create_task([]()
    {
        int progress = 0;
        while (progress >= 0 && progress < 100)
        {
            progress = Vuforia::init();
        }
        return progress;
    }, task_continuation_context::use_current());
}

void AppSession::ThrowInitError(int errorCode)
{
    if (errorCode >= 0) 
        return;// not an error, do nothing

    switch (errorCode)
    {
    case Vuforia::INIT_ERROR:
        throw ref new Platform::Exception(E_FAIL, 
            "Failed to initialize Vuforia.");
    case Vuforia::INIT_LICENSE_ERROR_INVALID_KEY:
        throw ref new Platform::Exception(E_FAIL, 
            "Invalid Key used. " +
            "Please make sure you are using a valid Vuforia App Key");
    case Vuforia::INIT_LICENSE_ERROR_CANCELED_KEY:
        throw ref new Platform::Exception(E_FAIL, 
            "This App license key has been cancelled " +
            "and may no longer be used. Please get a new license key.");
    case Vuforia::INIT_LICENSE_ERROR_MISSING_KEY:
        throw ref new Platform::Exception(E_FAIL,
            "Vuforia App key is missing. Please get a valid key, " +
            "by logging into your account at developer.vuforia.com " +
            "and creating a new project");
    case Vuforia::INIT_LICENSE_ERROR_PRODUCT_TYPE_MISMATCH:
        throw ref new Platform::Exception(E_FAIL, 
            "Vuforia App key is not valid for this product. Please get a valid key, " +
            "by logging into your account at developer.vuforia.com and choosing the " +
            "right product type during project creation");
    case Vuforia::INIT_LICENSE_ERROR_NO_NETWORK_TRANSIENT:
        throw ref new Platform::Exception(E_FAIL, 
            "Unable to contact server. Please try again later.");
    case Vuforia::INIT_LICENSE_ERROR_NO_NETWORK_PERMANENT:
        throw ref new Platform::Exception(E_FAIL, 
            "No network available. Please make sure you are connected to the internet.");
    case Vuforia::INIT_DEVICE_NOT_SUPPORTED:
        throw ref new Platform::Exception(E_FAIL, 
            "Failed to initialize Vuforia because this device is not supported.");
    case Vuforia::INIT_EXTERNAL_DEVICE_NOT_DETECTED:
        throw ref new Platform::Exception(E_FAIL, 
            "Failed to initialize Vuforia because this " +
            "device is not docked with required external hardware.");
    case Vuforia::INIT_NO_CAMERA_ACCESS:
        throw ref new Platform::Exception(E_FAIL, 
            "Camera Access was denied to this App. \n" +
            "When running on iOS8 devices, \n" +
            "users must explicitly allow the App to access the camera.\n" +
            "To restore camera access on your device, go to: \n" +
            "Settings > Privacy > Camera > [This App Name] and switch it ON.");
    default:
        throw ref new Platform::Exception(E_FAIL, "Vuforia init error. Unknown error.");
    }
}



