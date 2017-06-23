/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#pragma once

using namespace Windows::UI::Xaml::Controls;

#include "AboutScreen.g.h"
#include "Features\VuMark\VuMarkView.xaml.h"

namespace VuMark
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public ref class AboutScreen sealed
    {
    public:
        AboutScreen();

    private:
        void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ e);
        void OnResuming(Platform::Object ^sender, Platform::Object ^args);
        VuMarkView^ m_vuMarkView;

        void Start_Button_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
    };
}
