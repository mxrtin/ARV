/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#pragma once

#include "SplashScreen.g.h"
#include "AboutScreen.xaml.h"

using namespace Windows::UI::Xaml;

namespace VuMark
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public ref class SplashScreen sealed
    {
    public:
        SplashScreen();

    private:
        void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ e);
        void OnResuming(Platform::Object ^sender, Platform::Object ^args);
        void SplashTimerTick(Platform::Object ^ sender, Platform::Object ^ e);
        DispatcherTimer^ dispatcherTimer;
        AboutScreen^ m_aboutPage;

        long long SECONDS_TO_HNANO = 10000000;
        long long SPLASH_DURATION = SECONDS_TO_HNANO * 2;


    };
}
