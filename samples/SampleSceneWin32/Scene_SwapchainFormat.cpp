#include "pch.h"

#include <XrSceneLib/ProjectionLayer.h>
#include "MenuContextScene.h"

namespace {
    // Cycle through all runtime supported color/depth formats
    struct SwapchainFormatScene : public MenuContextScene {
        SwapchainFormatScene(engine::Context& context,
                             engine::ProjectionLayers& projectionLayers,
                             std::unordered_map<DXGI_FORMAT, std::string> swapchainFormatNameMap,
                             std::function<void(engine::ProjectionLayerConfig&, DXGI_FORMAT)> changeSwapchainFormatAction)
            : MenuContextScene(context, "SwapchainFormatScene")
            , m_projectionLayers(projectionLayers)
            , m_swapchainFormatNameMap(std::move(swapchainFormatNameMap))
            , m_changeSwapchainFormatAction(std::move(changeSwapchainFormatAction)) {
            const std::vector<int64_t> systemSupportedFormat = xr::EnumerateSwapchainFormats(context.Session.Handle);

            // Add swapchain formats in runtime-preferred order.
            for (int64_t format : systemSupportedFormat) {
                // Only add it as a candidate if the engine supports it and we have it in our name map.
                if (xr::Contains(systemSupportedFormat, (int64_t)format) &&
                    m_swapchainFormatNameMap.find((DXGI_FORMAT)format) != m_swapchainFormatNameMap.end()) {
                    m_candidateFormats.push_back((DXGI_FORMAT)format);
                }
            }

            // Add unknown last so that it isn't the default. Use "UNKNOWN" format as "submit no depth info at xrEndFrame".
            if (m_swapchainFormatNameMap.find(DXGI_FORMAT_UNKNOWN) != m_swapchainFormatNameMap.end()) {
                m_candidateFormats.push_back(DXGI_FORMAT_UNKNOWN);
            }

            assert(m_candidateFormats.size() >= 1);

            SetProjectionConfiguration();
        }

        void SetProjectionConfiguration() {
            assert(m_currentFormatIndex < (uint32_t)m_candidateFormats.size());
            const auto currentFormat = (DXGI_FORMAT)m_candidateFormats.at(m_currentFormatIndex);
            for (uint32_t i = 0; i < m_projectionLayers.Size(); ++i) {
                m_changeSwapchainFormatAction(m_projectionLayers.At(i).Config(), currentFormat);
            }
            MenuContext.MenuText = m_swapchainFormatNameMap.at(currentFormat);
        }

        void OnMenuClicked() override {
            m_currentFormatIndex = (m_currentFormatIndex + 1) % (uint32_t)m_candidateFormats.size();
            SetProjectionConfiguration();
        }

    private:
        engine::ProjectionLayers& m_projectionLayers;
        const std::unordered_map<DXGI_FORMAT, std::string> m_swapchainFormatNameMap;
        const std::function<void(engine::ProjectionLayerConfig&, DXGI_FORMAT)> m_changeSwapchainFormatAction;
        std::vector<DXGI_FORMAT> m_candidateFormats;
        uint32_t m_currentFormatIndex = 0;
    };
} // namespace

std::unique_ptr<engine::Scene> TryCreateColorFormatScene(engine::Context& context, engine::ProjectionLayers& projectionLayers) {
    const std::unordered_map<DXGI_FORMAT, std::string> swapchainFormatNameMap = {
        {DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, "BGRA-8-SRGB"},
        {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, "RGBA-8-SRGB"},
        {DXGI_FORMAT_R8G8B8A8_UNORM, "RGBA-8"},
        {DXGI_FORMAT_B8G8R8A8_UNORM, "BGRA-8"},
    };

    return std::make_unique<SwapchainFormatScene>(
        context, projectionLayers, swapchainFormatNameMap, [](engine::ProjectionLayerConfig& config, DXGI_FORMAT format) {
            config.ColorSwapchainFormat = format;
        });
}

std::unique_ptr<engine::Scene> TryCreateDepthFormatScene(engine::Context& context,
                                                         engine::ProjectionLayers& projectionLayers,
                                                         bool preferD16) {
    const std::unordered_map<DXGI_FORMAT, std::string> swapchainFormatNameMap = {
        {DXGI_FORMAT_D16_UNORM, "D16_UNORM"},
        {DXGI_FORMAT_UNKNOWN, "No Depth"},
        {DXGI_FORMAT_D32_FLOAT, "D32_FLOAT"},
        {DXGI_FORMAT_D24_UNORM_S8_UINT, "D24_S8_UINT"},
        {DXGI_FORMAT_D32_FLOAT_S8X24_UINT, "D32_FLOAT_S8X24"},
    };

    return std::make_unique<SwapchainFormatScene>(
        context, projectionLayers, swapchainFormatNameMap, [](engine::ProjectionLayerConfig& config, DXGI_FORMAT format) {
            if (format == DXGI_FORMAT_UNKNOWN) {
                config.SubmitDepthInfo = false;
                config.DepthSwapchainFormat = DXGI_FORMAT_D16_UNORM;
            } else {
                config.SubmitDepthInfo = true;
                config.DepthSwapchainFormat = format;
            }
        });
}
