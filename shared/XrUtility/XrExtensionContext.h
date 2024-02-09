// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "XrEnumerate.h"

namespace xr {
    struct ExtensionContext {
        bool SupportsD3D11{false};
        bool SupportsD3D12{false};
        bool SupportsDepthInfo{false};
        bool SupportsVisibilityMask{false};
        bool SupportsHandInteraction{false};
        bool SupportsEyeGazeInteraction{false};
        bool SupportsHandJointTracking{false};
        bool SupportsSecondaryViewConfiguration{false};
        bool SupportsAppContainer{false};
        bool SupportsColorScaleBias{false};
        bool SupportsPalmPose{false};
        bool SupportsViveTrackers{false};

        std::vector<const char*> EnabledExtensions;

        inline bool IsEnabled(const char* extensionName) const {
            const auto it = std::find_if(
                EnabledExtensions.begin(), EnabledExtensions.end(), [&extensionName](auto&& i) { return 0 == strcmp(i, extensionName); });
            return it != EnabledExtensions.end();
        }
    };

    inline ExtensionContext CreateExtensionContext(const std::vector<const char*>& requestedExtensions) {
        const std::vector<XrExtensionProperties> runtimeSupportedExtensions = xr::EnumerateInstanceExtensionProperties();

        // Filter requested extensions to make sure enabled extensions are supported by current runtime
        xr::ExtensionContext extensions{};
        for (auto& requestedExtension : requestedExtensions) {
            for (const auto& supportedExtension : runtimeSupportedExtensions) {
                if (strcmp(supportedExtension.extensionName, requestedExtension) == 0) {
                    extensions.EnabledExtensions.push_back(requestedExtension);
                    break;
                }
            }
        }

        // Record enabled extensions in extension context as bool for easy usage.
#ifdef XR_USE_GRAPHICS_API_D3D11
        extensions.SupportsD3D11 = extensions.IsEnabled(XR_KHR_D3D11_ENABLE_EXTENSION_NAME);
#endif
#ifdef XR_USE_GRAPHICS_API_D3D12
        extensions.SupportsD3D12 = extensions.IsEnabled(XR_KHR_D3D12_ENABLE_EXTENSION_NAME);
#endif
#ifdef XR_USE_PLATFORM_WIN32
        extensions.SupportsAppContainer = extensions.IsEnabled(XR_EXT_WIN32_APPCONTAINER_COMPATIBLE_EXTENSION_NAME);
#endif
        extensions.SupportsDepthInfo = extensions.IsEnabled(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
        extensions.SupportsVisibilityMask = extensions.IsEnabled(XR_KHR_VISIBILITY_MASK_EXTENSION_NAME);
        extensions.SupportsHandInteraction = extensions.IsEnabled(XR_MSFT_HAND_INTERACTION_EXTENSION_NAME);
        extensions.SupportsEyeGazeInteraction = extensions.IsEnabled(XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME);
        extensions.SupportsSecondaryViewConfiguration = extensions.IsEnabled(XR_MSFT_SECONDARY_VIEW_CONFIGURATION_EXTENSION_NAME);
        extensions.SupportsHandJointTracking = extensions.IsEnabled(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
        extensions.SupportsColorScaleBias = extensions.IsEnabled(XR_KHR_COMPOSITION_LAYER_COLOR_SCALE_BIAS_EXTENSION_NAME);
        extensions.SupportsPalmPose = extensions.IsEnabled(XR_EXT_PALM_POSE_EXTENSION_NAME);
        extensions.SupportsViveTrackers = extensions.IsEnabled(XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME);

        return extensions;
    }
} // namespace xr

