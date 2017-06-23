﻿#pragma once
//------------------------------------------------------------------------------
//     This code was generated by a tool.
//
//     Changes to this file may cause incorrect behavior and will be lost if
//     the code is regenerated.
//------------------------------------------------------------------------------


namespace Windows {
    namespace UI {
        namespace Xaml {
            namespace Controls {
                ref class SwapChainPanel;
                ref class ProgressRing;
                ref class Grid;
                ref class Image;
                ref class TextBlock;
                ref class StackPanel;
                ref class ToggleSwitch;
            }
        }
    }
}
namespace Windows {
    namespace UI {
        namespace Xaml {
            namespace Media {
                ref class TranslateTransform;
            }
        }
    }
}
namespace Windows {
    namespace UI {
        namespace Xaml {
            namespace Media {
                namespace Animation {
                    ref class Storyboard;
                }
            }
        }
    }
}

namespace VuMark
{
    [::Windows::Foundation::Metadata::WebHostHidden]
    partial ref class VuMarkView : public ::Windows::UI::Xaml::Controls::Page, 
        public ::Windows::UI::Xaml::Markup::IComponentConnector,
        public ::Windows::UI::Xaml::Markup::IComponentConnector2
    {
    public:
        void InitializeComponent();
        virtual void Connect(int connectionId, ::Platform::Object^ target);
        virtual ::Windows::UI::Xaml::Markup::IComponentConnector^ GetBindingConnector(int connectionId, ::Platform::Object^ target);
    
    private:
        bool _contentLoaded;
    
        private: ::Windows::UI::Xaml::Controls::SwapChainPanel^ swapChainPanel;
        private: ::Windows::UI::Xaml::Controls::ProgressRing^ InitProgressRing;
        private: ::Windows::UI::Xaml::Controls::Grid^ VuMarkCard;
        private: ::Windows::UI::Xaml::Media::TranslateTransform^ VuMarkCardTranslate;
        private: ::Windows::UI::Xaml::Media::Animation::Storyboard^ ShowVuMarkCardStoryboard;
        private: ::Windows::UI::Xaml::Media::Animation::Storyboard^ HideVuMarkCardStoryboard;
        private: ::Windows::UI::Xaml::Controls::Image^ VuMarkImage;
        private: ::Windows::UI::Xaml::Controls::TextBlock^ VuMarkTypeTextBlock;
        private: ::Windows::UI::Xaml::Controls::TextBlock^ VuMarkIdTextBlock;
        private: ::Windows::UI::Xaml::Controls::StackPanel^ OptionsMenu;
        private: ::Windows::UI::Xaml::Media::TranslateTransform^ OptionsMenuTranslate;
        private: ::Windows::UI::Xaml::Media::Animation::Storyboard^ ShowMenuStoryboard;
        private: ::Windows::UI::Xaml::Controls::ToggleSwitch^ AutofocusToggleSwitch;
    };
}

