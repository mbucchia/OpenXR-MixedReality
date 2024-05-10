// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include <windowsx.h>
#include <shellapi.h>
#include <XrSceneLib/XrApp.h>
#include "Resource.h"

std::unique_ptr<engine::Scene> TryCreateTitleScene(engine::Context& context);
std::unique_ptr<engine::Scene> TryCreateAnimationScene(engine::Context& context);
std::unique_ptr<engine::Scene> TryCreateVisibilityMaskScene(engine::Context& context);
std::unique_ptr<engine::Scene> TryCreateControllerActionsScene(engine::Context& context);
std::unique_ptr<engine::Scene> TryCreateHandTrackingScene(engine::Context& context);
std::unique_ptr<engine::Scene> TryCreateTrackingStateScene(engine::Context& context);
std::unique_ptr<engine::Scene> TryCreateQuadLayerScene(engine::Context& context);
std::unique_ptr<engine::Scene> TryCreateEyeGazeInteractionScene(engine::Context& context);
std::unique_ptr<engine::Scene> TryCreateProjectionConfigScene(engine::Context& context, engine::ProjectionLayers& projectionLayers);
std::unique_ptr<engine::Scene> TryCreateProjectionScaleScene(engine::Context& context, engine::ProjectionLayers& projectionLayers);
std::unique_ptr<engine::Scene> TryCreateProjectionSamplingScene(engine::Context& context, engine::ProjectionLayers& projectionLayers);
std::unique_ptr<engine::Scene> TryCreateFovModifierScene(engine::Context& context, engine::ProjectionLayers& projectionLayers);
std::unique_ptr<engine::Scene> TryCreateColorFormatScene(engine::Context& context, engine::ProjectionLayers& projectionLayers);
std::unique_ptr<engine::Scene> TryCreateDepthFormatScene(engine::Context& context,
                                                         engine::ProjectionLayers& projectionLayers,
                                                         bool preferD16);
std::unique_ptr<engine::Scene> TryCreatePauseScene(engine::Context& context, engine::ProjectionLayers& layers, engine::XrApp& app);
std::unique_ptr<engine::Scene> TryCreateMenuScene(engine::Context& context, engine::XrApp& app);

// Global Variables:
std::thread sceneThread;
std::unique_ptr<engine::XrApp> app;

void EnterVR();
INT_PTR CALLBACK DialogWinProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int) {
    int nArgs;
    LPWSTR* szArglist = ::CommandLineToArgvW(GetCommandLineW(), &nArgs);
    if (nArgs >= 2 && ::_wcsicmp(szArglist[1], L"-openxr") == 0) {
        EnterVR();
    }

    DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOGBOX), NULL, DialogWinProc);
    return 0;
}

void EnterVR() {
    sceneThread = std::thread([] {
        CHECK_HRCMD(::CoInitializeEx(nullptr, COINIT_MULTITHREADED));
        auto on_exit = MakeScopeGuard([] { ::CoUninitialize(); });

        engine::XrAppConfiguration appConfig({"SampleSceneWin32", 1});
        appConfig.RequestedExtensions.push_back(XR_KHR_VISIBILITY_MASK_EXTENSION_NAME);
        appConfig.RequestedExtensions.push_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
        appConfig.RequestedExtensions.push_back(XR_EXT_PALM_POSE_EXTENSION_NAME);
        appConfig.RequestedExtensions.push_back(XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME);
        appConfig.RequestedExtensions.push_back(XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME);

        // NOTE: Uncomment a filter below to test specific action binding of given profile.
        // appConfig.InteractionProfilesFilter.push_back("/interaction_profiles/microsoft/motion_controller");
        // appConfig.InteractionProfilesFilter.push_back("/interaction_profiles/oculus/touch_controller");
        // appConfig.InteractionProfilesFilter.push_back("/interaction_profiles/htc/vive_controller");
        // appConfig.InteractionProfilesFilter.push_back("/interaction_profiles/valve/index_controller");
        // appConfig.InteractionProfilesFilter.push_back("/interaction_profiles/khr/simple_controller");

        app = engine::CreateXrApp(appConfig);

        auto addScene = [&](bool defaultActive, std::unique_ptr<engine::Scene> scene) {
            if (scene != nullptr) {
                scene->SetActive(defaultActive);
                app->AddScene(std::move(scene));
            }
        };

        addScene(true, TryCreateColorFormatScene(app->Context(), app->ProjectionLayers()));
        addScene(true, TryCreateDepthFormatScene(app->Context(), app->ProjectionLayers(), false));
        addScene(true, TryCreateProjectionConfigScene(app->Context(), app->ProjectionLayers()));
        addScene(true, TryCreateProjectionScaleScene(app->Context(), app->ProjectionLayers()));
        addScene(true, TryCreateProjectionSamplingScene(app->Context(), app->ProjectionLayers()));
        addScene(true, TryCreateFovModifierScene(app->Context(), app->ProjectionLayers()));

        addScene(true, TryCreateAnimationScene(app->Context()));
        addScene(true, TryCreateVisibilityMaskScene(app->Context()));
        addScene(true, TryCreateControllerActionsScene(app->Context()));
        addScene(true, TryCreateHandTrackingScene(app->Context()));
        addScene(true, TryCreateTrackingStateScene(app->Context()));
        addScene(false, TryCreateQuadLayerScene(app->Context()));
        addScene(false, TryCreateEyeGazeInteractionScene(app->Context()));

        addScene(true, TryCreateTitleScene(app->Context()));
        addScene(false, TryCreatePauseScene(app->Context(), app->ProjectionLayers(), *app));

        addScene(true, TryCreateMenuScene(app->Context(), *app));

        app->Run();
        app = nullptr;
    });
}

void ExitVR() {
    if (sceneThread.joinable()) {
        if (app) {
            app->Stop();
        }
        sceneThread.join();
    }
}

// https://docs.microsoft.com/en-us/windows/win32/menurc/using-cursors#confining-a-cursor
// The effect of ClipCursor is restricted to this process.
// When the focus is switched to another window, the cursor is free to full screen.
// Therefore redo this operation when this window becomes active or being moved.
void ConfineCursor(HWND hwnd) {
    RECT rc;
    ::GetWindowRect(hwnd, &rc);
    ::ClipCursor(&rc);
}

INT_PTR CALLBACK DialogWinProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);

    switch (message) {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            ExitVR();
            EnterVR();
        } else if (LOWORD(wParam) == IDCANCEL) {
            ::EndDialog(hwnd, 0);
        }
        break;

    case WM_ACTIVATE:
        if (LOWORD(wParam) != WA_INACTIVE) {
            ConfineCursor(hwnd);
        }
        break;

    case WM_EXITSIZEMOVE:
        ConfineCursor(hwnd);
        break;

    case WM_MOUSEMOVE:
        ::SetWindowText(hwnd, fmt::format(L"Mouse pos: {}, {}", GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)).c_str());
        break;

    case WM_DESTROY:
        ExitVR();
        ::PostQuitMessage(0);
        break;
    }

    return (INT_PTR)FALSE;
}
