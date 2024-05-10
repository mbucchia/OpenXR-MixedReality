#include "pch.h"

#include <chrono>

#include <XrSceneLib/ProjectionLayer.h>
#include <XrSceneLib/XrApp.h>
#include "MenuContextScene.h"

using namespace std::chrono_literals;

namespace {
    enum class PauseMode {
        None,

        // Add extra 11,22,33,44ms after xrWaitFrame() on cpu to simulate workload
        ThrottleExtraOneFrame,
        ThrottleExtraTwoFrames,
        ThrottleExtraThreeFrames,
        ThrottleExtraFourFrames,
        ThrottleExtra5s,

        // Pause rendering new frames
        // Keep submitting previously rendered frame for 5 secs
        Pause5s,

        // Pause on every other frame to simulate half fps on rendering, but full fps submission
        PauseSkipOneFrame,

        Count,
    };

    class PauseScene : public MenuContextScene {
    public:
        PauseScene(engine::Context& context, engine::ProjectionLayers& projectionLayers, engine::XrApp& app)
            : MenuContextScene(context, "Pause")
            , m_projectionLayers(projectionLayers)
            , m_app(app) {
            ApplyPauseModeConfiguration();
        }

        void OnUpdate(const engine::FrameTime& frameTime) override {
            if (m_pauseMode == PauseMode::PauseSkipOneFrame) {
                if (frameTime.FrameIndex % 2 == 0) {
                    SetScenePause(false);
                } else {
                    SetScenePause(true);
                }
            } else if (m_pauseMode == PauseMode::Pause5s) {
                if (!m_pause5sSkippedFirstFrame) {
                    // The first frame we don't pause, but just update the description
                    m_pause5sSkippedFirstFrame = true;
                } else if (!m_pauseStartTime.has_value()) {
                    m_pauseStartTime = std::chrono::high_resolution_clock::now();
                    SetScenePause(true);
                } else {
                    auto pauseDuration = std::chrono::high_resolution_clock::now() - m_pauseStartTime.value();
                    if (!m_5sWaitDone && pauseDuration >= 5s) {
                        SetScenePause(false);
                        MenuContext.MenuText = "Pause done";
                        m_5sWaitDone = true;
                    }
                }
            }
        }

        void ApplyPauseModeConfiguration() {
            std::string description;

            SetScenePause(false);
            SetFrameLoopThrottle(0);

            switch (m_pauseMode) {
            case PauseMode::None:
                description = "Frame throttle (None)";
                break;
            case PauseMode::Pause5s:
                description = "Pause 5 secs (submit)";
                m_5sWaitDone = false;
                m_pause5sSkippedFirstFrame = false;
                m_pauseStartTime = std::nullopt;
                break;
            case PauseMode::PauseSkipOneFrame:
                description = "Skip one frame (submit)";
                break;
            case PauseMode::ThrottleExtraOneFrame:
                description = "Extra 11ms (no submit)";
                SetFrameLoopThrottle(1);
                break;
            case PauseMode::ThrottleExtraTwoFrames:
                description = "Extra 22ms (no submit)";
                SetFrameLoopThrottle(2);
                break;
            case PauseMode::ThrottleExtraThreeFrames:
                description = "Extra 33ms (no submit)";
                SetFrameLoopThrottle(3);
                break;
            case PauseMode::ThrottleExtraFourFrames:
                description = "Extra 44ms (no submit)";
                SetFrameLoopThrottle(4);
                break;
            case PauseMode::ThrottleExtra5s:
                description = "Extra 5s (no submit)";
                SetFrameLoopThrottle(455);
                break;
            default:
                throw std::logic_error("Invalid pause mode");
            }
            MenuContext.MenuText = description;
        }

        void OnMenuClicked() override {
            m_pauseMode = (PauseMode)(((uint32_t)m_pauseMode + 1) % (uint32_t)PauseMode::Count);
            ApplyPauseModeConfiguration();
        }

    private:
        void SetScenePause(bool pause) {
            for (uint32_t i = 0; i < m_projectionLayers.Size(); i++) {
                m_projectionLayers.At(i).Test_Pause = pause;
            }
        }

        void SetFrameLoopThrottle(int factor) {
            m_app.Test_ThrottleFrameLoop(factor);
        }

        engine::ProjectionLayers& m_projectionLayers;
        engine::XrApp& m_app;

        PauseMode m_pauseMode{PauseMode::None};

        bool m_5sWaitDone{false};
        bool m_pause5sSkippedFirstFrame{false};
        std::optional<std::chrono::high_resolution_clock::time_point> m_pauseStartTime;
    };
} // namespace

std::unique_ptr<engine::Scene> TryCreatePauseScene(engine::Context& context, engine::ProjectionLayers& layers, engine::XrApp& app) {
    return std::make_unique<PauseScene>(context, layers, app);
}
