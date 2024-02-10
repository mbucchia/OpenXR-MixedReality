#include "pch.h"
#include <pbr/GltfLoader.h>
#include <SampleShared/FileUtility.h>
#include <XrSceneLib/PbrModelObject.h>
#include <XrSceneLib/Scene.h>

using namespace DirectX;
using namespace std::chrono_literals;

namespace {
    struct AnimationScene : public engine::Scene {
        explicit AnimationScene(engine::Context& context)
            : Scene(context) {
            m_rootObject = AddObject(std::make_shared<engine::Object>());

            auto placeholderModel = std::make_shared<Pbr::Model>();
            placeholderModel->AddPrimitive(Pbr::Primitive(context.PbrResources,
                                                          Pbr::PrimitiveBuilder().AddSphere(0.25f, 10),
                                                          Pbr::Material::CreateFlat(context.PbrResources, Pbr::RGBA::White, 0.5f, 0.5f)));
            auto addAnimatingObject = [&](std::function<void(const std::shared_ptr<engine::PbrModelObject>& object,
                                                             const engine::FrameTime& frameTime)> updateObject) {
                auto engineObject = AddObject(std::make_shared<engine::PbrModelObject>(placeholderModel));
                auto update = [=](const engine::FrameTime& frameTime) { updateObject(engineObject, frameTime); };
                m_movingObjects.push_back(Animation{engineObject, update});
            };

            const XMVECTOR up = XMVectorSet(0, 1, 0, 1);

            // Rotate in place to your left, faster
            addAnimatingObject([=](const std::shared_ptr<engine::PbrModelObject>& object, const engine::FrameTime& frameTime) {
                XrPosef pose;
                xr::math::StoreXrQuaternion(&pose.orientation, XMQuaternionRotationAxis(up, frameTime.TotalElapsedSeconds * 2));
                pose.position = {-1, 0, 0};
                object->Pose() = pose;
            });

            // Rotate in place to your right, slower
            addAnimatingObject([=](const std::shared_ptr<engine::PbrModelObject>& object, const engine::FrameTime& frameTime) {
                XrPosef pose;
                xr::math::StoreXrQuaternion(&pose.orientation, XMQuaternionRotationAxis(up, frameTime.TotalElapsedSeconds));
                pose.position = {1, 0, 0};
                object->Pose() = pose;
            });

            // Upright circle in front, bigger and faster
            addAnimatingObject([=](const std::shared_ptr<engine::PbrModelObject>& object, const engine::FrameTime& frameTime) {
                XrPosef pose;
                const float angle = frameTime.TotalElapsedSeconds * 2;
                pose.orientation = xr::math::Quaternion::Identity();
                pose.position = {cos(angle), sin(angle), -2};
                object->Scale() = {2, 2, 2};
                object->Pose() = pose;
            });

            // Upright circle in back, closer, smaller and slower and spinning
            addAnimatingObject([=](const std::shared_ptr<engine::PbrModelObject>& object, const engine::FrameTime& frameTime) {
                XrPosef pose;
                const float angle = frameTime.TotalElapsedSeconds / 2;
                xr::math::StoreXrQuaternion(&pose.orientation, XMQuaternionRotationAxis(up, angle));
                pose.position = {cos(angle), sin(angle), 1};
                object->Pose() = pose;
            });

            // The GLB can take a while to load in debug builds so load it asynchronously.
            m_modelLoadOperation = engine::PbrModelLoadOperation::LoadGltfBinaryAsync(m_context.PbrResources, L"damagedhelmet.glb");

            XrReferenceSpaceCreateInfo spaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
            spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
            spaceCreateInfo.poseInReferenceSpace = xr::math::Pose::Identity();
            CHECK_XRCMD(xrCreateReferenceSpace(m_context.Session.Handle, &spaceCreateInfo, m_localSpace.Put(xrDestroySpace)));
        }

        void OnUpdate(const engine::FrameTime& frameTime) override {
            m_rootObject->SetVisible(false);
            XrSpaceLocation rootLocation{XR_TYPE_SPACE_LOCATION};
            CHECK_XRCMD(xrLocateSpace(m_localSpace.Get(), m_context.AppSpace, frameTime.PredictedDisplayTime, &rootLocation));
            if (xr::math::Pose::IsPoseValid(rootLocation)) {
                m_rootObject->Pose() = rootLocation.pose;
                m_rootObject->SetVisible(true);
            }

            // Update the object models when the real model is available.
            if (auto model = m_modelLoadOperation.TakeModelWhenReady()) {
                model->GetNode(Pbr::RootNodeIndex).SetTransform(XMMatrixScaling(0.25f, 0.25f, 0.25f));
                for (const Animation& obj : m_movingObjects) {
                    obj.Object->SetModel(model);
                }
            }

            if (frameTime.IsSessionFocused) {
                for (const Animation& obj : m_movingObjects) {
                    obj.Update(frameTime);
                }
            }
        }

    private:
        struct Animation {
            std::shared_ptr<engine::PbrModelObject> Object;
            std::function<void(const engine::FrameTime& frameTime)> Update;
        };
        engine::PbrModelLoadOperation m_modelLoadOperation;
        std::shared_ptr<engine::Object> m_rootObject;
        std::vector<Animation> m_movingObjects;
        xr::SpaceHandle m_localSpace;
    };
} // namespace

std::unique_ptr<engine::Scene> TryCreateAnimationScene(engine::Context& context) {
    return std::make_unique<AnimationScene>(context);
}
