/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#pragma once

#include <Vuforia\State.h>
#include <Vuforia\CameraDevice.h>

namespace VuMark
{
    public ref class VuforiaState sealed
    {
    public:
        VuforiaState() {
            m_nativeState = nullptr;
        }

    internal:
        Vuforia::State *m_nativeState;
    };

    public interface class AppControl
    {
        // To be called to initialize the trackers
        bool DoInitTrackers();

        // To be called to load the trackers' data
        bool DoLoadTrackersData();

        // To be called to start the camera
        bool DoStartCamera(int direction);

        // To be called to stop the camera
        bool DoStopCamera();

        // To be called to start the trackers
        bool DoStartTrackers();

        // To be called to stop the trackers
        bool DoStopTrackers();

        // To be called to destroy the trackers' data
        bool DoUnloadTrackersData();

        // To be called to deinitialize the trackers
        bool DoDeinitTrackers();

        // This callback is called after the Vuforia initialization is complete,
        // the trackers are initialized, their data loaded and
        // tracking is ready to start
        void OnInitARDone();

        // This callback is called after the Vuforia Camera and Trackers have started
        void OnARStarted();

        // This callback is called every cycle
        // Here we use an int because we cannot use native type Vuforia::State
        // in managed classes or interface method signatures
        void OnVuforiaUpdate(VuforiaState^ vuforiaState);
    };
};