/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#include "pch.h"
#include "SplashScreen.xaml.h"

using namespace VuMark;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=234238

SplashScreen::SplashScreen()
{
    InitializeComponent();
    Application^ app = Application::Current;
    app->Suspending += ref new SuspendingEventHandler(this, &SplashScreen::OnSuspending);
    app->Resuming += ref new EventHandler<Object^>(this, &SplashScreen::OnResuming);

    if (m_aboutPage == nullptr)
    {
        m_aboutPage = ref new AboutScreen();
    }

    dispatcherTimer = ref new DispatcherTimer();
    dispatcherTimer->Tick += ref new EventHandler<Object^>(this, &SplashScreen::SplashTimerTick);

    TimeSpan splashTime;
    splashTime.Duration = SPLASH_DURATION;
    dispatcherTimer->Interval = splashTime;
    dispatcherTimer->Start();
}

void SplashScreen::OnSuspending(Platform::Object ^ sender, Windows::ApplicationModel::SuspendingEventArgs ^ e)
{
    OutputDebugString(L"Splash Suspend");
}

void SplashScreen::OnResuming(Platform::Object ^ sender, Platform::Object ^ args)
{
    OutputDebugString(L"Splash Resume");
}

void SplashScreen::SplashTimerTick(Platform::Object ^ sender, Platform::Object ^ e)
{
    dispatcherTimer->Stop();
    Window::Current->Content = m_aboutPage;
    //((Windows::UI::Xaml::Controls::Frame ^)(Window::Current->Content))->Navigate(AboutScreen::typeid);
}