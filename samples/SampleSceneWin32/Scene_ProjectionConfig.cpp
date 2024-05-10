#include "pch.h"

#include <XrSceneLib/ProjectionLayer.h>
#include "MenuContextScene.h"

namespace {
    enum class ProjectionMode : uint32_t {
        // Single layer, texture array, alpha blended.
        TextureArray,

        // Single layer, texture array, not layer alpha blended.
        NoAlphaBlend,

        // Single layer, layer alpha blended, double-wide, with depth
        DoubleWide,

        // Offset left and right eye to the side by 5 cm, to validate view pose override
        PoseOverride,

        // Two projection layers to simulate foveated rendering
        Foveated,

        Count,
    };

    // Cycle through atypical projection layer configurations.  This scene doesn't change pixel formats
    struct ProjectionConfigScene : public MenuContextScene {
        ProjectionConfigScene(engine::Context& context, engine::ProjectionLayers& projectionLayers)
            : MenuContextScene(context, "Projection Config")
            , m_projectionLayers(projectionLayers) {
            SetProjectionConfiguration();
        }

        void SetProjectionConfiguration() {
            std::string description;

            // Ensure defaults
            m_projectionLayers.Resize(1, m_context);
            m_projectionLayers.At(0).Config().SwapchainSizeScale = {1, 1};
            m_projectionLayers.At(0).Config().DoubleWideMode = false;
            m_projectionLayers.At(0).Config().SubmitDepthInfo = true;
            m_projectionLayers.At(0).Config().LayerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            m_projectionLayers.At(0).Config().Test_ViewPoseOffsets[xr::StereoView::Left] = {{0, 0, 0, 1}, {0, 0, 0}};
            m_projectionLayers.At(0).Config().Test_ViewPoseOffsets[xr::StereoView::Right] = {{0, 0, 0, 1}, {0, 0, 0}};

            switch (m_projectionMode) {
            case ProjectionMode::TextureArray:
                description = "TextureArray";
                break;
            case ProjectionMode::NoAlphaBlend:
                description = "NoAlphaBlend";
                m_projectionLayers.At(0).Config().LayerFlags = 0;
                break;
            case ProjectionMode::DoubleWide:
                description = "DoubleWide";
                m_projectionLayers.At(0).Config().DoubleWideMode = true;
                break;
            case ProjectionMode::Foveated:
                description = "Foveated";
                m_projectionLayers.Resize(2, m_context);
                m_projectionLayers.At(0).Config().SwapchainSizeScale = {0.5, 0.5};

                // Inner layer, 200% resolution, down sampling to half resolution
                m_projectionLayers.At(1).Config().ColorSwapchainFormat = m_projectionLayers.At(0).Config().ColorSwapchainFormat;
                m_projectionLayers.At(1).Config().DepthSwapchainFormat = m_projectionLayers.At(0).Config().DepthSwapchainFormat;
                m_projectionLayers.At(1).Config().Test_FovScale = {0.5f, 0.5f, 0.5f, 0.5f};
                m_projectionLayers.At(1).Config().SwapchainSizeScale = {2.0, 2.0};
                m_projectionLayers.At(1).Config().LayerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;

                break;

            case ProjectionMode::PoseOverride:
                description = "Pose Override";
                m_projectionLayers.At(0).Config().Test_ViewPoseOffsets[xr::StereoView::Left].position = {0.05f, 0, 0};
                m_projectionLayers.At(0).Config().Test_ViewPoseOffsets[xr::StereoView::Right].position = {-0.05f, 0, 0};

                break;
            default:
                throw std::logic_error("Invalid Projection Config");
            }

            MenuContext.MenuText = description;
        }

        void OnMenuClicked() override {
            m_projectionMode = (ProjectionMode)(((uint32_t)m_projectionMode + 1) % (uint32_t)ProjectionMode::Count);
            SetProjectionConfiguration();
        }

    private:
        engine::ProjectionLayers& m_projectionLayers;
        ProjectionMode m_projectionMode{ProjectionMode::TextureArray};
    };
} // namespace

std::unique_ptr<engine::Scene> TryCreateProjectionConfigScene(engine::Context& context, engine::ProjectionLayers& projectionLayers) {
    return std::make_unique<ProjectionConfigScene>(context, projectionLayers);
}
