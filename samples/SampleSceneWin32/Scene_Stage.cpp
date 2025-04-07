#include "pch.h"
#include <pbr/GltfLoader.h>
#include <SampleShared/FileUtility.h>
#include <XrSceneLib/PbrModelObject.h>
#include <XrSceneLib/Scene.h>

using namespace DirectX;
using namespace std::chrono_literals;

namespace {
    struct StageScene : public engine::Scene {
        explicit StageScene(engine::Context& context)
            : Scene(context) {
            // The GLB can take several seconds to load in debug builds so load it asynchronously.
            m_loadLevelOperation = engine::PbrModelLoadOperation::LoadGltfBinaryAsync(m_context.PbrResources, L"level.glb");

            OnSpaceChanging(XR_REFERENCE_SPACE_TYPE_STAGE, 0, std::nullopt);
        }

        void OnEvent(const XrEventDataBuffer& eventData) override {
            if (auto* spaceChangingEvent = xr::event_cast<XrEventDataReferenceSpaceChangePending>(&eventData)) {
                std::optional<XrPosef> pose{};
                if (spaceChangingEvent->poseValid) {
                    pose = spaceChangingEvent->poseInPreviousSpace;
                }

                OnSpaceChanging(spaceChangingEvent->referenceSpaceType, spaceChangingEvent->changeTime, pose);
            }
        }

        void OnSpaceChanging(XrReferenceSpaceType referenceSpaceType,
                             XrTime changeTime,
                             const std::optional<XrPosef>& poseInPreviousSpace) {
            if (referenceSpaceType != XR_REFERENCE_SPACE_TYPE_STAGE) {
                return;
            }

            // Create a new space with the updated offset
            XMMATRIX currentOffsetMatrix = DirectX::XMMatrixIdentity();
            if (poseInPreviousSpace) {
                XMMATRIX poseMatrix = xr::math::LoadInvertedXrPose(poseInPreviousSpace.value());
                currentOffsetMatrix *= xr::math::LoadXrPose(m_stageOffset) * poseMatrix;

                xr::math::StoreXrPose(&m_stageOffset, currentOffsetMatrix);
            } else {
                // We have a disconnection, in this case we reset to the new space
                m_stageOffset = xr::math::Pose::Identity();
            }

            XrReferenceSpaceCreateInfo spaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
            spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
            xr::math::StoreXrPose(&spaceCreateInfo.poseInReferenceSpace, currentOffsetMatrix);

            CHECK_XRCMD(xrCreateReferenceSpace(m_context.Session.Handle, &spaceCreateInfo, m_stageSpace.Put(xrDestroySpace)));
        }

        void OnUpdate(const engine::FrameTime& frameTime) override {
            if (auto levelModel = m_loadLevelOperation.TakeModelWhenReady()) {
                // Rotate the level so that the user placed looking down the Sponza courtyard rather than facing the wall.
                levelModel->GetNode(Pbr::RootNodeIndex).SetTransform(XMMatrixRotationRollPitchYaw(0, DirectX::XMConvertToRadians(90), 0));
                m_stageObject = AddObject(std::make_shared<engine::PbrModelObject>(levelModel));
            }

            if (m_stageObject) {
                m_stageObject->SetVisible(false);
                XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION};
                CHECK_XRCMD(xrLocateSpace(m_stageSpace.Get(), m_context.AppSpace, frameTime.PredictedDisplayTime, &spaceLocation));
                if (xr::math::Pose::IsPoseValid(spaceLocation)) {
                    XrPosef& pose = spaceLocation.pose;
                    m_stageObject->Pose() = pose;
                    m_stageObject->SetVisible(true);
                }
            }
        }

    private:
        XrPosef m_stageOffset;
        std::shared_ptr<engine::Object> m_stageObject;
        xr::SpaceHandle m_stageSpace;
        engine::PbrModelLoadOperation m_loadLevelOperation;
    };
} // namespace

std::unique_ptr<engine::Scene> TryCreateStageScene(engine::Context& context) {
    const bool supportsStage = xr::Contains(context.Session.SupportedReferenceSpaces, XR_REFERENCE_SPACE_TYPE_STAGE);
    return supportsStage ? std::make_unique<StageScene>(context) : nullptr;
}
