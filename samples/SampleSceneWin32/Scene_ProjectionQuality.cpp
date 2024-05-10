#include "pch.h"

#include <XrSceneLib/ProjectionLayer.h>
#include "MenuContextScene.h"

namespace {
    static const float ScaleFactors[] = {sqrt(1.0f),  // 100%
                                         sqrt(1.5f),  // 150%
                                         sqrt(2.0f),  // 200%
                                         sqrt(0.5f)}; // 50%

    // Cycle through different projection scale factors.
    struct ProjectionScaleScene : public MenuContextScene {
        ProjectionScaleScene(engine::Context& context, engine::ProjectionLayers& projectionLayers)
            : MenuContextScene(context, "Projection Scale")
            , m_projectionLayers(projectionLayers) {
            OnMenuClicked();
        }

        void OnMenuClicked() override {
            const float scaleFactor = ScaleFactors[m_scaleIndex];
            m_projectionLayers.At(0).Config().SwapchainSizeScale = {scaleFactor, scaleFactor};
            MenuContext.MenuText = fmt::format("{:.1f} Scale", scaleFactor);

            m_scaleIndex = (m_scaleIndex + 1) % (int)_countof(ScaleFactors);
        }

    private:
        engine::ProjectionLayers& m_projectionLayers;
        int m_scaleIndex{0};
    };

    // Cycle through different projection scale factors.
    struct ProjectionSamplingScene : public MenuContextScene {
        ProjectionSamplingScene(engine::Context& context, engine::ProjectionLayers& projectionLayers)
            : MenuContextScene(context, "Projection MSAA")
            , m_projectionLayers(projectionLayers) {
            const auto viewConfigViews = xr::EnumerateViewConfigurationViews(
                context.Instance.Handle, context.System.Id, context.Session.PrimaryViewConfigurationType);
            const uint32_t maxSampleCount = viewConfigViews[0].maxSwapchainSampleCount;

            for (uint32_t sampleCount = 1; sampleCount <= maxSampleCount; sampleCount *= 2) {
                m_sampleCounts.push_back(sampleCount);
            }

            OnMenuClicked();
        }

        void OnMenuClicked() override {
            const uint32_t sampleCount = m_sampleCounts[m_sampleCountIndex];
            m_projectionLayers.At(0).Config().SwapchainSampleCount = sampleCount;
            MenuContext.MenuText = fmt::format("{} MSAA", sampleCount);

            m_sampleCountIndex = (m_sampleCountIndex + 1) % (int)m_sampleCounts.size();
        }

    private:
        engine::ProjectionLayers& m_projectionLayers;
        int m_sampleCountIndex{0};
        std::vector<uint32_t> m_sampleCounts;
    };
} // namespace

std::unique_ptr<engine::Scene> TryCreateProjectionScaleScene(engine::Context& context, engine::ProjectionLayers& projectionLayers) {
    return std::make_unique<ProjectionScaleScene>(context, projectionLayers);
}

std::unique_ptr<engine::Scene> TryCreateProjectionSamplingScene(engine::Context& context, engine::ProjectionLayers& projectionLayers) {
    return std::make_unique<ProjectionSamplingScene>(context, projectionLayers);
}
