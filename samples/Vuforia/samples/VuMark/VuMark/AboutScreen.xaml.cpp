/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#include "pch.h"
#include "AboutScreen.xaml.h"

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

AboutScreen::AboutScreen()
{
    InitializeComponent();
    Application^ app = Application::Current;
    app->Suspending += ref new SuspendingEventHandler(this, &AboutScreen::OnSuspending);
    app->Resuming += ref new EventHandler<Object^>(this, &AboutScreen::OnResuming);

    Uri^ aboutHtmlPage = ref new Uri("ms-appx-web:///Assets/VuMark/VM_about.html");
    aboutText->Navigate(aboutHtmlPage);
}

void AboutScreen::OnSuspending(Platform::Object ^ sender, Windows::ApplicationModel::SuspendingEventArgs ^ e)
{
    OutputDebugString(L"AboutPage Suspend");
}

void AboutScreen::OnResuming(Platform::Object ^ sender, Platform::Object ^ args)
{
    OutputDebugString(L"AboutPage Resume");
}

void AboutScreen::Start_Button_Click(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e)
{
    m_vuMarkView = ref new VuMarkView();
    Window::Current->Content = m_vuMarkView;
}
