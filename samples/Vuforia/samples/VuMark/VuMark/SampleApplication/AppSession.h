/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#pragma once

#include "AppControl.h"

#include <memory>
#include <wrl.h>
#include <ppltasks.h>

#include <Vuforia\CameraDevice.h>
#include <Vuforia\UpdateCallback.h>

namespace VuMark
{
    class AppSession : public Vuforia::UpdateCallback
    {
    public:
        AppSession(AppControl^ appControl);

        void InitAR();
        void StartAR(Vuforia::CameraDevice::CAMERA_DIRECTION cameraDirection);
        void StopAR();
        void StopCamera();
        void PauseAR();
        void ResumeAR();

        bool VuforiaInitialized() const { return m_vuforiaInitialized; }
        bool CameraRunning() const { return m_cameraRunning; }
        Vuforia::CameraDevice::CAMERA_DIRECTION CameraDirection() const {
            return m_cameraDirection;
        }

        // Vuforia UpdateCallback interface
        virtual void Vuforia_onUpdate(Vuforia::State& state) override;

        void ConfigureVideoBackground(
            float screenWidth, float screenHeight,
            Windows::Graphics::Display::DisplayOrientations orientation
        );

    private:
        AppControl^ m_appControl;

        Concurrency::task<int> InitVuforiaAsync();
        void ThrowInitError(int errorCode);
        
        Vuforia::CameraDevice::CAMERA_DIRECTION m_cameraDirection;
        std::atomic<bool> m_cameraRunning;
        std::atomic<bool> m_vuforiaInitialized;

        /* For suspending and resuming we create an async task.
        * This is necessary because stopping and starting the camera takes
        * too long to run on the UI thread and the SDK doesn't allow this.
        * We need to ensure that these don't end up running concurrently and
        * that we always end up in the desired state even if the user
        * repeatedly suspend/resumes the app.
        */

        /// Handle to the suspend action
        Windows::Foundation::IAsyncAction^ m_pauseTask;
        /// Count the times we have ignored a suspend notification
        int m_ignoredPaused = 0;
        /// Handle to the resume action
        Windows::Foundation::IAsyncAction^ m_resumeTask;
        /// Count the times we have ignored the resume notification
        int m_ignoredResume = 0;
    };
};
