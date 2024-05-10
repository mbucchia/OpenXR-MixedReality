#include "pch.h"

#include <XrSceneLib/TextTexture.h>
#include <XrSceneLib/PbrModelObject.h>
#include <XrSceneLib/XrApp.h>
#include <XrSceneLib/Scene.h>
#include "MenuContextScene.h"

using namespace DirectX;

static constexpr uint32_t cMenuNumColumns = 3;
static constexpr float cMenuHorizontalSpacingInMeters = 0.15f;
static constexpr float cMenuVerticalSpacingInMeters = 0.02f;
static constexpr float cMenuObjectSize = 0.05f;
static constexpr XMFLOAT3 cMenuObjectDim{cMenuObjectSize * 3, cMenuObjectSize, cMenuObjectSize / 5};
static constexpr float cMenuDistance = 0.6f;

static constexpr float cMenuItemDisableScale = 0.6f;
static constexpr float cMenuItemHoverScale = 1.25f;

static const DirectX::XMVECTORF32 cMenuObjectColors[] = {Colors::Salmon, Colors::Khaki, Colors::SeaGreen, Colors::DodgerBlue};

namespace scenes::priorities {
    constexpr uint32_t Default = 0;
    constexpr uint32_t Menu = 1;
} // namespace scenes::priorities

namespace {
    struct MenuScene : public engine::Scene {
        // Creates spaces for menu items.
        // Menu item spaces are represented in local space and are created at the current menu plane location
        // with an offset based on the index of the menu item.
        XrSpace CreateMenuItemSpace(XrTime time, uint32_t index) {
            const float xPosition = index % cMenuNumColumns * (cMenuHorizontalSpacingInMeters + cMenuObjectSize) - cMenuObjectSize / 2.0f -
                                    cMenuHorizontalSpacingInMeters * cMenuNumColumns / 2;
            const float yPosition = -(index / cMenuNumColumns * (cMenuVerticalSpacingInMeters + cMenuObjectSize) - cMenuObjectSize / 2.0f);

            // We position the menu plane such that it is vertically aligned with the view and facing it
            XrSpaceLocation menuPlaneToLocal{XR_TYPE_SPACE_LOCATION};
            XrSpaceLocation viewToLocal{XR_TYPE_SPACE_LOCATION};
            CHECK_XRCMD(xrLocateSpace(m_menuPlaneSpace, m_localSpace, time, &menuPlaneToLocal));
            CHECK_XRCMD(xrLocateSpace(m_viewSpace, m_localSpace, time, &viewToLocal));

            float rotationAngle = std::atan2f(viewToLocal.pose.position.x - menuPlaneToLocal.pose.position.x,
                                              viewToLocal.pose.position.z - menuPlaneToLocal.pose.position.z);

            auto menuPlaneToAppSpace =
                DirectX::XMMatrixTranslation(0, 0, -cMenuDistance) * DirectX::XMMatrixRotationY(rotationAngle) *
                DirectX::XMMatrixTranslation(viewToLocal.pose.position.x, viewToLocal.pose.position.y, viewToLocal.pose.position.z);

            XrSpace space;
            XrReferenceSpaceCreateInfo spaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
            spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
            XrPosef menuPlaneTranslation = xr::math::Pose::Translation({xPosition, yPosition, 0});
            xr::math::StoreXrPose(&spaceCreateInfo.poseInReferenceSpace, xr::math::LoadXrPose(menuPlaneTranslation) * menuPlaneToAppSpace);

            CHECK_XRCMD(xrCreateReferenceSpace(m_context.Session.Handle, &spaceCreateInfo, &space));

            return space;
        }

        void DestroyMenuSpaces() {
            for (auto& menuItem : m_menuItems) {
                if (menuItem.Space != XR_NULL_HANDLE) {
                    CHECK_XRCMD(xrDestroySpace(menuItem.Space));
                    menuItem.Space = XR_NULL_HANDLE;
                }
            }
        }

        MenuScene(engine::Context& context, engine::XrApp& app)
            : Scene(context) {
            XrReferenceSpaceCreateInfo spaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
            spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
            spaceCreateInfo.poseInReferenceSpace = xr::math::Pose::Identity();
            CHECK_XRCMD(xrCreateReferenceSpace(m_context.Session.Handle, &spaceCreateInfo, &m_viewSpace));

            spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
            spaceCreateInfo.poseInReferenceSpace = xr::math::Pose::Translation({0, 0, -1});
            CHECK_XRCMD(xrCreateReferenceSpace(m_context.Session.Handle, &spaceCreateInfo, &m_menuPlaneSpace));

            spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
            spaceCreateInfo.poseInReferenceSpace = xr::math::Pose::Identity();
            CHECK_XRCMD(xrCreateReferenceSpace(m_context.Session.Handle, &spaceCreateInfo, &m_localSpace));

            engine::TextTextureInfo textInfo{256, 256 / 3};
            textInfo.FontName = L"Arial";
            textInfo.FontSize = 24.0f;
            textInfo.Foreground = Pbr::FromSRGB(DirectX::Colors::Black);

            auto CreateNextMenuItem = [&](const char* menuText, Pbr::RGBAColor color) -> MenuItem& {
                auto buttonObject = AddObject(engine::CreateCube(m_context.PbrResources, cMenuObjectDim, color));

                std::unique_ptr<engine::TextTexture> textTexture = std::make_unique<engine::TextTexture>(m_context, textInfo);
                textTexture->Draw(menuText);

                auto textObject = AddObject(engine::CreateQuad(m_context.PbrResources,
                                                               XMFLOAT2{cMenuObjectDim.x, cMenuObjectDim.y},
                                                               textTexture->CreatePbrMaterial(m_context.PbrResources)));
                textObject->SetParent(buttonObject);
                textObject->Pose().position.z = cMenuObjectDim.z / 2 + 0.01f; // 1cm in front of button.

                MenuItem menuItem{};
                menuItem.Text = menuText;
                menuItem.Object = buttonObject;
                menuItem.TextTexture = std::move(textTexture);
                m_menuItems.push_back(std::move(menuItem));
                return m_menuItems.back();
            };

            int i = 0;
            for (const auto& scene : app.Scenes()) {
                const DirectX::XMVECTORF32 color = cMenuObjectColors[(i++) % std::size(cMenuObjectColors)];
                auto menuContextScene = dynamic_cast<MenuContextScene*>(scene.get());
                if (menuContextScene) {
                    auto menuContext = menuContextScene->GetMenuContext();
                    MenuItem& menuItem = CreateNextMenuItem(menuContext->MenuText.c_str(), Pbr::FromSRGB(color));
                    menuItem.Scene = scene.get();
                    menuItem.Context = menuContext;
                } else {
                    std::string sceneName = typeid(*scene.get()).name();
                    const size_t namespacePlace = sceneName.rfind("::");
                    if (namespacePlace != std::string::npos) {            // remove the namespace if there's any.
                        sceneName = sceneName.substr(namespacePlace + 2); // pick the class name
                    }
                    MenuItem& menuItem = CreateNextMenuItem(sceneName.c_str(), Pbr::FromSRGB(color));
                    menuItem.Scene = scene.get();
                    menuItem.Callback = [scene = scene.get()] { scene->ToggleActive(); };
                }
            }

            // Non-scene menu items
            {
                MenuItem& exitMenuItem = CreateNextMenuItem("Exit", Pbr::FromSRGB(DirectX::Colors::DarkRed));
                exitMenuItem.Callback = [session = m_context.Session.Handle] { CHECK_XRCMD(xrRequestExitSession(session)); };

                // Force stop will cause the render and update loops to exit and everything will be destroyed without the session being
                // cleanly ended.
                MenuItem& hardExitMenuItem = CreateNextMenuItem("Hard Exit", Pbr::FromSRGB(DirectX::Colors::Red));
                hardExitMenuItem.Callback = [&app] { app.Test_ForceStop(); };

                MenuItem& recreateSwapchainMenuItem = CreateNextMenuItem("New Swapchain", Pbr::FromSRGB(DirectX::Colors::Indigo));
                recreateSwapchainMenuItem.Callback = [&app] { app.Test_RecreateProjectionSwapchains(); };
            }

            // m_menuToggleActionSet is always active, m_menuActionSet is only active when the menu is open
            m_menuActionSet = &ActionContext().CreateActionSet("menu_actions", "Menu Actions", scenes::priorities::Menu);

            // Always active, needed to determine the hand orientation and bring up the menu if it is facing up
            // The priority is default as if it were higher, this action would stomp on other grip pose actions
            // And prevent their spaces from locating (As they would be inactive)
            m_menuToggleActionSet =
                &ActionContext().CreateActionSet("menu_toggle_actions", "Menu toggle actions", scenes::priorities::Default);

            std::vector<std::string> subactionPaths = {"/user/hand/right", "/user/hand/left"};

            m_menuToggleAction =
                m_menuToggleActionSet->CreateAction("menu_toggle", "Menu Toggle", XR_ACTION_TYPE_BOOLEAN_INPUT, subactionPaths);

            m_menuGripPoseAction = m_menuToggleActionSet->CreateAction("menu_grip", "Menu Grip", XR_ACTION_TYPE_POSE_INPUT, subactionPaths);

            m_menuAimPoseAction = m_menuActionSet->CreateAction("menu_pointer", "Menu Pointer", XR_ACTION_TYPE_POSE_INPUT, subactionPaths);

            m_menuSelectAction = m_menuActionSet->CreateAction("menu_select", "Menu Select", XR_ACTION_TYPE_BOOLEAN_INPUT, subactionPaths);

            m_hapticAction = m_menuActionSet->CreateAction("menu_haptics", "Menu Haptics", XR_ACTION_TYPE_VIBRATION_OUTPUT, subactionPaths);

            ActionContext().SuggestInteractionProfileBindings("/interaction_profiles/microsoft/motion_controller",
                                                              {
                                                                  {m_menuToggleAction, "/user/hand/right/input/menu/click"},
                                                                  {m_menuToggleAction, "/user/hand/left/input/menu/click"},
                                                                  {m_menuAimPoseAction, "/user/hand/right/input/aim/pose"},
                                                                  {m_menuAimPoseAction, "/user/hand/left/input/aim/pose"},
                                                                  {m_menuGripPoseAction, "/user/hand/right/input/grip/pose"},
                                                                  {m_menuGripPoseAction, "/user/hand/left/input/grip/pose"},
                                                                  {m_menuSelectAction, "/user/hand/right/input/trigger/value"},
                                                                  {m_menuSelectAction, "/user/hand/left/input/trigger/value"},
                                                                  {m_hapticAction, "/user/hand/right/output/haptic"},
                                                                  {m_hapticAction, "/user/hand/left/output/haptic"},
                                                              });

            ActionContext().SuggestInteractionProfileBindings("/interaction_profiles/khr/simple_controller",
                                                              {
                                                                  {m_menuToggleAction, "/user/hand/right/input/menu/click"},
                                                                  {m_menuToggleAction, "/user/hand/left/input/menu/click"},
                                                                  {m_menuAimPoseAction, "/user/hand/right/input/aim/pose"},
                                                                  {m_menuAimPoseAction, "/user/hand/left/input/aim/pose"},
                                                                  {m_menuGripPoseAction, "/user/hand/right/input/grip/pose"},
                                                                  {m_menuGripPoseAction, "/user/hand/left/input/grip/pose"},
                                                                  {m_menuSelectAction, "/user/hand/right/input/select/click"},
                                                                  {m_menuSelectAction, "/user/hand/left/input/select/click"},
                                                                  {m_hapticAction, "/user/hand/right/output/haptic"},
                                                                  {m_hapticAction, "/user/hand/left/output/haptic"},
                                                              });

            ActionContext().SuggestInteractionProfileBindings("/interaction_profiles/oculus/touch_controller",
                                                              {
                                                                  {m_menuToggleAction, "/user/hand/left/input/menu/click"},
                                                                  {m_menuAimPoseAction, "/user/hand/right/input/aim/pose"},
                                                                  {m_menuAimPoseAction, "/user/hand/left/input/aim/pose"},
                                                                  {m_menuGripPoseAction, "/user/hand/right/input/grip/pose"},
                                                                  {m_menuGripPoseAction, "/user/hand/left/input/grip/pose"},
                                                                  {m_menuSelectAction, "/user/hand/right/input/a/click"},
                                                                  {m_menuSelectAction, "/user/hand/right/input/b/click"},
                                                                  {m_menuSelectAction, "/user/hand/left/input/x/click"},
                                                                  {m_menuSelectAction, "/user/hand/left/input/y/click"},
                                                                  {m_hapticAction, "/user/hand/right/output/haptic"},
                                                                  {m_hapticAction, "/user/hand/left/output/haptic"},
                                                              });

            // Aim space
            XrActionSpaceCreateInfo actionSpaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
            actionSpaceCreateInfo.action = m_menuAimPoseAction;
            actionSpaceCreateInfo.poseInActionSpace = xr::math::Pose::Translation({0, 0, -cMenuDistance});

            actionSpaceCreateInfo.subactionPath = m_context.Instance.LeftHandPath;
            CHECK_XRCMD(xrCreateActionSpace(m_context.Session.Handle, &actionSpaceCreateInfo, &m_leftPointerSpace));

            actionSpaceCreateInfo.subactionPath = m_context.Instance.RightHandPath;
            CHECK_XRCMD(xrCreateActionSpace(m_context.Session.Handle, &actionSpaceCreateInfo, &m_rightPointerSpace));

            // Grip space
            actionSpaceCreateInfo.action = m_menuGripPoseAction;
            actionSpaceCreateInfo.poseInActionSpace = xr::math::Pose::Identity();

            actionSpaceCreateInfo.subactionPath = m_context.Instance.LeftHandPath;
            CHECK_XRCMD(xrCreateActionSpace(m_context.Session.Handle, &actionSpaceCreateInfo, &m_leftGripSpace));

            actionSpaceCreateInfo.subactionPath = m_context.Instance.RightHandPath;
            CHECK_XRCMD(xrCreateActionSpace(m_context.Session.Handle, &actionSpaceCreateInfo, &m_rightGripSpace));

            auto createPointerRay = [&](std::shared_ptr<engine::PbrModelObject> parent, const Pbr::RGBAColor& color) {
                auto aimRay = AddObject(engine::CreateCube(m_context.PbrResources, {1.0f, 1.0f, 1.0f}, color));
                aimRay->SetParent(parent);
                aimRay->Pose().position.z = -5.0f + cMenuDistance;
                aimRay->Scale() = {0.006f, 0.006f, 10.01f};
            };

            m_leftPointerObject = AddObject(std::make_shared<engine::PbrModelObject>(std::make_shared<Pbr::Model>()));
            m_rightPointerObject = AddObject(std::make_shared<engine::PbrModelObject>(std::make_shared<Pbr::Model>()));

            createPointerRay(m_leftPointerObject, Pbr::FromSRGB(Colors::HotPink));
            createPointerRay(m_rightPointerObject, Pbr::FromSRGB(Colors::Cyan));
        }

        ~MenuScene() {
            DestroyMenuSpaces();

            CHECK_XRCMD(xrDestroySpace(m_leftPointerSpace));
            CHECK_XRCMD(xrDestroySpace(m_rightPointerSpace));
            CHECK_XRCMD(xrDestroySpace(m_leftGripSpace));
            CHECK_XRCMD(xrDestroySpace(m_rightGripSpace));

            CHECK_XRCMD(xrDestroySpace(m_menuPlaneSpace));
            CHECK_XRCMD(xrDestroySpace(m_localSpace));
            CHECK_XRCMD(xrDestroySpace(m_viewSpace));
        }

        bool HandFacingUp(XrSpace handSpace, XrTime time) {
            XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
            xrLocateSpace(handSpace, m_viewSpace, time, &location);

            if (xr::math::Pose::IsPoseValid(location)) {
                // Palms have different orientation based on handedness
                const DirectX::XMFLOAT3 handPalmNormal =
                    handSpace == m_rightGripSpace ? DirectX::XMFLOAT3{-1, 0, 0} : DirectX::XMFLOAT3{1, 0, 0};

                const DirectX::XMVECTOR handVector =
                    DirectX::XMVector3TransformCoord(XMLoadFloat3(&handPalmNormal), xr::math::LoadXrPose(location.pose));

                const DirectX::XMVECTORF32 headUpVector{0, -1, 0};

                // Dot the hand palm normal with head forward to get palm direction. Virtual root space is gravity aligned.
                const DirectX::XMVECTOR dp = DirectX::XMVector3Dot(headUpVector, handVector);

                return DirectX::XMVectorGetX(dp) < -0.707f; // True if the palm normal is no more than 45 degrees from head up.
            }

            return false;
        }

        void OnUpdate(const engine::FrameTime& frameTime) override {
            XrInteractionProfileState rightHandInteractionProfileInfo{XR_TYPE_INTERACTION_PROFILE_STATE};
            XrInteractionProfileState leftHandInteractionProfileInfo{XR_TYPE_INTERACTION_PROFILE_STATE};
            CHECK_XRCMD(xrGetCurrentInteractionProfile(
                m_context.Session.Handle, m_context.Instance.RightHandPath, &rightHandInteractionProfileInfo));
            CHECK_XRCMD(
                xrGetCurrentInteractionProfile(m_context.Session.Handle, m_context.Instance.LeftHandPath, &leftHandInteractionProfileInfo));

            const std::set<XrPath> handInteractionProfiles = {
                xr::StringToPath(m_context.Instance.Handle, "/interaction_profiles/microsoft/hand_interaction"),
                xr::StringToPath(m_context.Instance.Handle, "/interaction_profiles/ext/hand_interaction_ext")};
            const bool usingHandInteraction = handInteractionProfiles.count(rightHandInteractionProfileInfo.interactionProfile) > 0 ||
                                              handInteractionProfiles.count(leftHandInteractionProfileInfo.interactionProfile) > 0;

            XrActionStateBoolean menuToggleActionState{XR_TYPE_ACTION_STATE_BOOLEAN};
            XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
            bool toggleMenu = false;
            if (usingHandInteraction) {
                getInfo.action = m_menuToggleAction;
                getInfo.subactionPath = xr::StringToPath(m_context.Instance.Handle, "/user/hand/left");
                CHECK_XRCMD(xrGetActionStateBoolean(m_context.Session.Handle, &getInfo, &menuToggleActionState));
                toggleMenu = menuToggleActionState.changedSinceLastSync && !menuToggleActionState.currentState &&
                             HandFacingUp(m_leftGripSpace, frameTime.PredictedDisplayTime);

                if (!toggleMenu) {
                    getInfo.subactionPath = xr::StringToPath(m_context.Instance.Handle, "/user/hand/right");
                    CHECK_XRCMD(xrGetActionStateBoolean(m_context.Session.Handle, &getInfo, &menuToggleActionState));
                    toggleMenu = menuToggleActionState.changedSinceLastSync && !menuToggleActionState.currentState &&
                                 HandFacingUp(m_rightGripSpace, frameTime.PredictedDisplayTime);
                }
            } else {
                getInfo.action = m_menuToggleAction;
                CHECK_XRCMD(xrGetActionStateBoolean(m_context.Session.Handle, &getInfo, &menuToggleActionState));
                toggleMenu = menuToggleActionState.changedSinceLastSync && !menuToggleActionState.currentState;
            }

            if (toggleMenu) {
                m_menuActionSet->SetActive(!m_menuActionSet->Active());
                DestroyMenuSpaces();
            }

            if (m_menuActionSet->Active()) {
                for (uint32_t i = 0; i < static_cast<uint32_t>(m_menuItems.size()); i++) {
                    if (m_menuItems.at(i).Space == XR_NULL_HANDLE) {
                        m_menuItems.at(i).Space = CreateMenuItemSpace(frameTime.PredictedDisplayTime, i);
                    }
                }
            }

            float leftDistance = std::numeric_limits<float>::max();
            float rightDistance = std::numeric_limits<float>::max();
            const MenuItem* leftClosestMenuItem = nullptr;
            const MenuItem* rightClosestMenuItem = nullptr;
            for (MenuItem& menuItem : m_menuItems) {
                menuItem.Object->SetVisible(m_menuActionSet->Active());

                if (menuItem.Space == XR_NULL_HANDLE) {
                    continue; // this menu item doesn't have placement yet
                }

                // Re-render the button text if it has changed.
                if (menuItem.Context != nullptr) {
                    if (menuItem.Text != menuItem.Context->MenuText) {
                        menuItem.Text = menuItem.Context->MenuText;
                        menuItem.TextTexture->Draw(menuItem.Text.c_str());
                    }
                }

                float scale = menuItem.Scene == nullptr || menuItem.Scene->IsActive() ? 1 : cMenuItemDisableScale;
                menuItem.Object->Scale() = {scale, scale, scale};

                XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION};
                CHECK_XRCMD(xrLocateSpace(menuItem.Space, m_context.AppSpace, frameTime.PredictedDisplayTime, &spaceLocation));
                if (xr::math::Pose::IsPoseValid(spaceLocation)) {
                    menuItem.Object->Pose() = spaceLocation.pose;
                }

                // We detect menu interaction by projecting the pointer space to the plane of the menu and taking the closest menu item
                // that is within a threshold (size of menu item).
                auto MenuDistance = [](XrVector3f vec) { return std::sqrtf(vec.x * vec.x + vec.y * vec.y); };

                CHECK_XRCMD(xrLocateSpace(menuItem.Space, m_leftPointerSpace, frameTime.PredictedDisplayTime, &spaceLocation));
                const float leftDist = MenuDistance(spaceLocation.pose.position);
                if (xr::math::Pose::IsPoseValid(spaceLocation) != 0 && leftDist < leftDistance) {
                    leftDistance = leftDist;
                    if (leftDist < cMenuObjectSize / 2) {
                        menuItem.Object->Scale() = {cMenuItemHoverScale, cMenuItemHoverScale, cMenuItemHoverScale};
                        leftClosestMenuItem = &menuItem;
                    }
                }

                CHECK_XRCMD(xrLocateSpace(menuItem.Space, m_rightPointerSpace, frameTime.PredictedDisplayTime, &spaceLocation));
                const float rightDist = MenuDistance(spaceLocation.pose.position);
                if (xr::math::Pose::IsPoseValid(spaceLocation) && rightDist < rightDistance) {
                    rightDistance = rightDist;
                    if (rightDist < cMenuObjectSize / 2) {
                        menuItem.Object->Scale() = {cMenuItemHoverScale, cMenuItemHoverScale, cMenuItemHoverScale};
                        rightClosestMenuItem = &menuItem;
                    }
                }
            }

            auto CheckMenuSelection = [&](const MenuItem* menuItem, XrPath hand) {
                XrActionStateBoolean menuSelectActionState{XR_TYPE_ACTION_STATE_BOOLEAN};
                XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                getInfo.action = m_menuSelectAction;
                getInfo.subactionPath = hand;
                CHECK_XRCMD(xrGetActionStateBoolean(m_context.Session.Handle, &getInfo, &menuSelectActionState));
                if (menuSelectActionState.changedSinceLastSync && menuSelectActionState.currentState) {
                    if (menuItem->Callback) {
                        menuItem->Callback();
                    } else if (menuItem->Context->Callback) {
                        menuItem->Context->Callback();
                    } else if (menuItem->Scene) {
                        menuItem->Scene->SetActive(!menuItem->Scene->IsActive());
                    }

                    XrHapticVibration hapticVibration{XR_TYPE_HAPTIC_VIBRATION};
                    hapticVibration.amplitude = 1;
                    hapticVibration.duration = XR_MIN_HAPTIC_DURATION;
                    hapticVibration.frequency = XR_FREQUENCY_UNSPECIFIED;

                    XrHapticActionInfo hapticInfo{XR_TYPE_HAPTIC_ACTION_INFO};
                    hapticInfo.action = m_hapticAction;
                    hapticInfo.subactionPath = hand;

                    CHECK_XRCMD(xrApplyHapticFeedback(
                        m_context.Session.Handle, &hapticInfo, reinterpret_cast<const XrHapticBaseHeader*>(&hapticVibration)));
                }
            };

            XrPath leftHand = m_context.Instance.LeftHandPath;
            if (leftClosestMenuItem) {
                CheckMenuSelection(leftClosestMenuItem, leftHand);
            }

            XrPath rightHand = m_context.Instance.RightHandPath;
            if (rightClosestMenuItem) {
                CheckMenuSelection(rightClosestMenuItem, rightHand);
            }

            auto SetPointerVisibilityAndLocation = [&](std::shared_ptr<engine::PbrModelObject> object, XrSpace space) {
                object->SetVisible(false);

                XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION};
                CHECK_XRCMD(xrLocateSpace(space, m_context.AppSpace, frameTime.PredictedDisplayTime, &spaceLocation));
                if (xr::math::Pose::IsPoseValid(spaceLocation)) {
                    object->Pose() = spaceLocation.pose;
                    object->SetVisible(m_menuActionSet->Active());
                }
            };

            SetPointerVisibilityAndLocation(m_leftPointerObject, m_leftPointerSpace);
            SetPointerVisibilityAndLocation(m_rightPointerObject, m_rightPointerSpace);
        }

    private:
        XrSpace m_menuPlaneSpace;
        XrSpace m_localSpace;
        XrSpace m_viewSpace;

        struct MenuItem {
            std::string Text;
            XrSpace Space = {XR_NULL_HANDLE};
            std::shared_ptr<engine::PbrModelObject> Object;
            std::unique_ptr<engine::TextTexture> TextTexture;
            std::function<void()> Callback;
            Scene* Scene = nullptr;
            const MenuContext* Context = nullptr;
        };

        std::vector<MenuItem> m_menuItems;

        sample::ActionSet* m_menuActionSet;
        sample::ActionSet* m_menuToggleActionSet;
        XrAction m_menuToggleAction;
        XrAction m_menuAimPoseAction;
        XrAction m_menuGripPoseAction;
        XrAction m_menuSelectAction;
        XrAction m_hapticAction;

        XrSpace m_leftPointerSpace;
        XrSpace m_rightPointerSpace;
        XrSpace m_leftGripSpace;
        XrSpace m_rightGripSpace;
        std::shared_ptr<engine::PbrModelObject> m_leftPointerObject;
        std::shared_ptr<engine::PbrModelObject> m_rightPointerObject;
    };
} // namespace

std::unique_ptr<engine::Scene> TryCreateMenuScene(engine::Context& context, engine::XrApp& app) {
    return std::make_unique<MenuScene>(context, app);
}
