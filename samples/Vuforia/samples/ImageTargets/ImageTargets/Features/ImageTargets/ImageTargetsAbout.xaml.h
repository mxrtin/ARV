/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#pragma once

using namespace Windows::UI::Xaml::Controls;

#include "Features/ImageTargets/ImageTargetsAbout.g.h"
#include "ImageTargetsView.xaml.h"

namespace ImageTargets
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public ref class ImageTargetsAbout sealed
    {
    public:
        ImageTargetsAbout();

    private:
        void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ e);
        void OnResuming(Platform::Object ^sender, Platform::Object ^args);
        ImageTargetsView^ m_imageTargetsView;

        void Start_Button_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
    };
}
