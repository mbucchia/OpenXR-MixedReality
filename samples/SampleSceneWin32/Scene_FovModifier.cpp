#include "pch.h"

#include <charconv>
#include <XrSceneLib/ProjectionLayer.h>
#include "MenuContextScene.h"

namespace {
    enum class FovMode : uint32_t {
        FovFull = 0,
        Fov75,  // 75%
        Fov50,  // 50%
        Fov25,  // 25%
        Fov150, // 150%

        // Note these flip modes are just applying a scale of -1.0 to the components
        // rather than doing frustum preserving flip (e.g as in the y-flip scene).
        // Expect to see slightly misaligned result when comparing to y-flip scene in these modes
        FovXFlip,
        FovYFlip,
        FovXYFlip,

        Count,
    };

    // Fov Modifier for layer 0
    struct FovModifierScene : public MenuContextScene {
        FovModifierScene(engine::Context& context, engine::ProjectionLayers& projectionLayers)
            : MenuContextScene(context, "FovModifier")
            , m_projectionLayers(projectionLayers) {
            MenuContext.MenuText = FovDescription();
        }

        void OnUpdate(const engine::FrameTime& frameTime) override {
            if (m_projectionLayers.Size() > 0) {
                // Update primary layer only
                m_projectionLayers.At(0).Config().Test_FovScale = FovScaleValue();
            }
        }

        void OnMenuClicked() override {
            m_fovMode = (FovMode)(((uint32_t)m_fovMode + 1) % (uint32_t)FovMode::Count);
            MenuContext.MenuText = FovDescription();
        }

    private:
        XrFovf FovScaleValue() const {
            switch (m_fovMode) {
            case FovMode::FovFull:
                return {1.0f, 1.0f, 1.0f, 1.0f};
            case FovMode::Fov75:
                return {0.75f, 0.75f, 0.75f, 0.75f};
            case FovMode::Fov50:
                return {0.5f, 0.5f, 0.5f, 0.5f};
            case FovMode::Fov25:
                return {0.25f, 0.25f, 0.25f, 0.25f};
            case FovMode::Fov150:
                return {1.5f, 1.5f, 1.5f, 1.5f};
            case FovMode::FovXFlip:
                return {-1.0f, -1.0f, 1.0f, 1.0f};
            case FovMode::FovYFlip:
                return {1.0f, 1.0f, -1.0f, -1.0f};
            case FovMode::FovXYFlip:
                return {-1.0f, -1.0f, -1.0f, -1.0f};
            default:
                throw std::logic_error("Invalid FovMode");
            }
        }

        std::string FovDescription() const {
            switch (m_fovMode) {
            case FovMode::FovFull:
                return "Fov: 100%";
            case FovMode::Fov75:
                return "Fov: 75%";
            case FovMode::Fov50:
                return "Fov: 50%";
            case FovMode::Fov25:
                return "Fov: 25%";
            case FovMode::Fov150:
                return "Fov: 150%";
            case FovMode::FovXFlip:
                return "Fov: XFlip";
            case FovMode::FovYFlip:
                return "Fov: YFlip";
            case FovMode::FovXYFlip:
                return "Fov: XYFlip";
            default:
                throw std::logic_error("Invalid FovMode");
            }
        }

        // Here we store the collection instead of an individual layer,
        // because the original individual layer may get deallocated in other scenes
        engine::ProjectionLayers& m_projectionLayers;
        FovMode m_fovMode{FovMode::FovFull};
    };
} // namespace

std::unique_ptr<engine::Scene> TryCreateFovModifierScene(engine::Context& context, engine::ProjectionLayers& projectionLayers) {
    return std::make_unique<FovModifierScene>(context, projectionLayers);
}
