/*===============================================================================
Copyright (c) 2016 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/
#pragma once

#include <iostream>
#include <string>
#include <vccorlib.h>
#include <windows.h>

#define MAX_STRLEN 1024

namespace SampleCommon
{
    class SampleUtil
    {
    public:
        static inline void Log(Platform::String^ tag, Platform::String^ message)
        {
            Platform::String^ msg = tag + " : " + message + "\n";
            OutputDebugString(msg->Data());
        }

        static inline void ToWString(const char *cStr, std::wstring & wStr)
        {
            size_t strLen = strlen(cStr) + 1;
            if (strLen > MAX_STRLEN) strLen = MAX_STRLEN;
            wchar_t wcstr[MAX_STRLEN];
            size_t numChars;
            mbstowcs_s(&numChars, wcstr, cStr, strLen);
            wStr.assign(wcstr);
        }

        static inline Platform::String^ ToPlatformString(const char *cStr)
        {
            size_t strLen = strlen(cStr) + 1;
            if (strLen > MAX_STRLEN) strLen = MAX_STRLEN;
            wchar_t wcstr[MAX_STRLEN];
            size_t numChars;
            mbstowcs_s(&numChars, wcstr, cStr, strLen);
            return ref new Platform::String(wcstr);
        }

        static inline void ToStdString(Platform::String^ pStr, std::string & stdStr)
        {
            if (pStr == nullptr) return;
            std::wstring wstr(pStr->Data());
            stdStr.assign(wstr.begin(), wstr.end());
        }

        static inline void ToCString(Platform::String^ pStr, char* cStr)
        {
            if (pStr == nullptr) return;
            std::wstring wstr(pStr->Data());
            std::string str(wstr.begin(), wstr.end());
            strcpy_s(cStr, str.length(), str.c_str());
        }

        static void ExitApp(Windows::UI::Popups::IUICommand^ command)
        {
            Windows::ApplicationModel::Core::CoreApplication::Exit();
        }

        static void ShowError(Platform::String^ title, Platform::String^ message)
        {
            auto dispatcher = Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher;
            dispatcher->RunAsync(
                Windows::UI::Core::CoreDispatcherPriority::Normal,
                ref new Windows::UI::Core::DispatchedHandler([title, message]()
            {
                auto initErrorDialog = ref new Windows::UI::Popups::MessageDialog(message, title);

                auto closeCommand = ref new Windows::UI::Popups::UICommand(
                    "Close",
                    ref new Windows::UI::Popups::UICommandInvokedHandler(&ExitApp));

                initErrorDialog->Commands->Append(closeCommand);
                initErrorDialog->DefaultCommandIndex = 0;
                initErrorDialog->CancelCommandIndex = 0;
                initErrorDialog->ShowAsync();
            }));
        }
    };
} // namespace SampleCommon

