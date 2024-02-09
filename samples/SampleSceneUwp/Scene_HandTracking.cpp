// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include <XrSceneLib/PbrModelObject.h>
#include <XrSceneLib/Scene.h>

using namespace DirectX;
using namespace xr::math;

namespace {

    //
    // This sample displays hand tracking inputs appears as joint axes.
    //
    struct HandTrackingScene : public engine::Scene {
        HandTrackingScene(engine::Context& context)
            : Scene(context) {
            sample::ActionSet& actionSet = ActionContext().CreateActionSet("hand_tracking_scene_actions", "Hand Tracking Scene Actions");

            m_motionRangeModeChangeAction = actionSet.CreateAction(
                "motion_range_mode_change_action", "Motion Range Mode Change Action", XR_ACTION_TYPE_BOOLEAN_INPUT, {});

            ActionContext().SuggestInteractionProfileBindings("/interaction_profiles/microsoft/motion_controller",
                                                              {
                                                                  {m_motionRangeModeChangeAction, "/user/hand/right/input/trigger"},
                                                                  {m_motionRangeModeChangeAction, "/user/hand/left/input/trigger"},
                                                              });

            ActionContext().SuggestInteractionProfileBindings("/interaction_profiles/khr/simple_controller",
                                                              {
                                                                  {m_motionRangeModeChangeAction, "/user/hand/right/input/select/click"},
                                                                  {m_motionRangeModeChangeAction, "/user/hand/left/input/select/click"},
                                                              });

            if (context.Extensions.SupportsHandInteraction) {
                ActionContext().SuggestInteractionProfileBindings("/interaction_profiles/microsoft/hand_interaction",
                                                                  {
                                                                      {m_motionRangeModeChangeAction, "/user/hand/left/input/select"},
                                                                      {m_motionRangeModeChangeAction, "/user/hand/right/input/select"},
                                                                  });
            }

            const std::tuple<XrHandEXT, HandData&> hands[] = {{XrHandEXT::XR_HAND_LEFT_EXT, m_leftHandData},
                                                              {XrHandEXT::XR_HAND_RIGHT_EXT, m_rightHandData}};

            m_jointMaterial = Pbr::Material::CreateFlat(m_context.PbrResources, Pbr::RGBA::White, 0.85f, 0.01f);

            auto createJointObjects = [&](HandData& handData) {
                auto jointModel = std::make_shared<Pbr::Model>();
                Pbr::PrimitiveBuilder primitiveBuilder;

                // Create a axis object attached to each joint location
                for (uint32_t k = 0; k < std::size(handData.PbrNodeIndices); k++) {
                    handData.PbrNodeIndices[k] = jointModel->AddNode(DirectX::XMMatrixIdentity(), Pbr::RootNodeIndex, "joint");
                    primitiveBuilder.AddAxis(1.0f, 0.5f, handData.PbrNodeIndices[k]);
                }

                // Now that the axis have been added for each joint into the primitive builder,
                // it can be baked into the model as a single primitive.
                jointModel->AddPrimitive(Pbr::Primitive(m_context.PbrResources, primitiveBuilder, m_jointMaterial));
                handData.JointModel = AddObject(std::make_shared<engine::PbrModelObject>(std::move(jointModel)));
                handData.JointModel->SetVisible(false);
            };

            // For each hand, initialize the joint objects and corresponding space.
            for (const auto& [hand, handData] : hands) {
                XrHandTrackerCreateInfoEXT createInfo{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
                createInfo.hand = hand;
                createInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
                CHECK_XRCMD(
                    xrCreateHandTrackerEXT(m_context.Session.Handle, &createInfo, handData.TrackerHandle.Put(xrDestroyHandTrackerEXT)));

                createJointObjects(handData);
            }

            // Set a clap detector that will toggle the display mode.
            m_clapDetector = std::make_unique<StateChangeDetector>(
                [this](XrTime time) {
                    const XrHandJointLocationEXT& leftPalmLocation = m_leftHandData.JointLocations[XR_HAND_JOINT_PALM_EXT];
                    const XrHandJointLocationEXT& rightPalmLocation = m_rightHandData.JointLocations[XR_HAND_JOINT_PALM_EXT];

                    if (xr::math::Pose::IsPoseValid(leftPalmLocation) && xr::math::Pose::IsPoseValid(rightPalmLocation)) {
                        const XMVECTOR leftPalmPosition = xr::math::LoadXrVector3(leftPalmLocation.pose.position);
                        const XMVECTOR rightPalmPosition = xr::math::LoadXrVector3(rightPalmLocation.pose.position);
                        const float distance = XMVectorGetX(XMVector3Length(XMVectorSubtract(leftPalmPosition, rightPalmPosition)));
                        return distance - leftPalmLocation.radius - rightPalmLocation.radius < 0.02f /*meter*/;
                    }

                    return false;
                },
                [this]() {
                });
        }

        void OnUpdate(const engine::FrameTime& frameTime) override {
            XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
            XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
            getInfo.action = m_motionRangeModeChangeAction;
            CHECK_XRCMD(xrGetActionStateBoolean(m_context.Session.Handle, &getInfo, &state));
            const bool isMotionRangeModeChangeButtonPressed = state.isActive && state.changedSinceLastSync && state.currentState;
            if (isMotionRangeModeChangeButtonPressed) {
                m_motionRangeMode = (MotionRangeMode)(((uint32_t)m_motionRangeMode + 1) % (uint32_t)MotionRangeMode::Count);
            }
            for (HandData& handData : {std::ref(m_leftHandData), std::ref(m_rightHandData)}) {
                XrHandJointsMotionRangeInfoEXT motionRangeInfo{XR_TYPE_HAND_JOINTS_MOTION_RANGE_INFO_EXT};
                motionRangeInfo.handJointsMotionRange = m_motionRangeMode == MotionRangeMode::Unobstructed
                                                            ? XR_HAND_JOINTS_MOTION_RANGE_UNOBSTRUCTED_EXT
                                                            : XR_HAND_JOINTS_MOTION_RANGE_CONFORMING_TO_CONTROLLER_EXT;
                XrHandJointsLocateInfoEXT locateInfo{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT, &motionRangeInfo};
                locateInfo.baseSpace = m_context.AppSpace;
                locateInfo.time = frameTime.PredictedDisplayTime;

                XrHandJointLocationsEXT locations{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
                locations.jointCount = (uint32_t)handData.JointLocations.size();
                locations.jointLocations = handData.JointLocations.data();
                CHECK_XRCMD(xrLocateHandJointsEXT(handData.TrackerHandle.Get(), &locateInfo, &locations));

                bool jointsVisible = m_mode == HandDisplayMode::Joints;

                if (jointsVisible) {
                    jointsVisible = UpdateJoints(handData, m_context.AppSpace, frameTime.PredictedDisplayTime);
                }

                handData.JointModel->SetVisible(jointsVisible);
            }

            // Detect hand clap to toggle hand display mode.
            m_clapDetector->Update(frameTime.PredictedDisplayTime);
        }

        struct HandData {
            xr::HandTrackerHandle TrackerHandle;

            // Data to display hand joints tracking
            std::shared_ptr<engine::PbrModelObject> JointModel;
            std::array<Pbr::NodeIndex_t, XR_HAND_JOINT_COUNT_EXT> PbrNodeIndices{};
            std::array<XrHandJointLocationEXT, XR_HAND_JOINT_COUNT_EXT> JointLocations{};

            HandData() = default;
            HandData(HandData&&) = delete;
            HandData(const HandData&) = delete;
        };

        bool UpdateJoints(HandData& handData, XrSpace referenceSpace, XrTime time) {
            bool jointsVisible = false;

            for (uint32_t k = 0; k < XR_HAND_JOINT_COUNT_EXT; k++) {
                if (xr::math::Pose::IsPoseValid(handData.JointLocations[k])) {
                    Pbr::Node& jointNode = handData.JointModel->GetModel()->GetNode(handData.PbrNodeIndices[k]);

                    const float radius = handData.JointLocations[k].radius;
                    jointNode.SetTransform(XMMatrixScaling(radius, radius, radius) * xr::math::LoadXrPose(handData.JointLocations[k].pose));

                    jointsVisible = true;
                }
            }

            return jointsVisible;
        }

        // Detects two spaces collide to each other
        class StateChangeDetector {
        public:
            StateChangeDetector(std::function<bool(XrTime)> getState, std::function<void()> callback)
                : m_getState(std::move(getState))
                , m_callback(std::move(callback)) {
            }

            void Update(XrTime time) {
                bool state = m_getState(time);
                if (m_lastState != state) {
                    m_lastState = state;
                    if (state) { // trigger on rising edge
                        m_callback();
                    }
                }
            }

        private:
            const std::function<bool(XrTime)> m_getState;
            const std::function<void()> m_callback;
            std::optional<bool> m_lastState{};
        };

        enum class HandDisplayMode { Joints, Count };
        HandDisplayMode m_mode{HandDisplayMode::Joints};

        enum class MotionRangeMode { Unobstructed, Controller, Count };
        MotionRangeMode m_motionRangeMode{MotionRangeMode::Unobstructed};
        XrAction m_motionRangeModeChangeAction{XR_NULL_HANDLE};

        std::shared_ptr<Pbr::Material> m_jointMaterial;

        HandData m_leftHandData;
        HandData m_rightHandData;
        std::unique_ptr<StateChangeDetector> m_clapDetector;
    };
} // namespace

std::unique_ptr<engine::Scene> TryCreateHandTrackingScene(engine::Context& context) {
    if (!context.Extensions.SupportsHandJointTracking || !context.System.HandTrackingProperties.supportsHandTracking) {
        return nullptr;
    }

    return std::make_unique<HandTrackingScene>(context);
}
