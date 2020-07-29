//*********************************************************
//    Copyright (c) Microsoft. All rights reserved.
//
//    Apache 2.0 License
//
//    You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
//    implied. See the License for the specific language governing
//    permissions and limitations under the License.
//
//*********************************************************
#include "pch.h"
#include "PbrModelObject.h"
#include "TextTexture.h"
#include "Scene.h"

using namespace DirectX;
using namespace xr::math;
using namespace std::chrono;
using namespace std::chrono_literals;

namespace {
    //
    // This sample displays a floating title text block following the user.
    // When the user moves or looks around, the text slowly ease into it's position in front of the user.
    //
    struct TitleScene : public engine::Scene {
        TitleScene(engine::Context& context)
            : Scene(context) {
            XrReferenceSpaceCreateInfo createInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
            createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
            createInfo.poseInReferenceSpace = Pose::Identity();
            CHECK_XRCMD(xrCreateReferenceSpace(m_context.Session.Handle, &createInfo, m_viewSpace.Put()));

            constexpr float margin = 0.01f;
            constexpr float titleWidth = 0.5f;
            constexpr float titleHeight = titleWidth / 3;
            const auto& material = Pbr::Material::CreateFlat(m_context.PbrResources, Pbr::FromSRGB(Colors::DarkGray));
            m_background = AddObject(engine::CreateQuad(m_context.PbrResources, {titleWidth, titleHeight}, material));
            m_background->SetVisible(false);

            engine::TextTextureInfo textInfo{256, 128}; // pixels
            textInfo.Foreground = Pbr::RGBA::White;
            textInfo.Background = Pbr::FromSRGB(Colors::DarkSlateBlue);
            textInfo.Margin = 5; // pixels
            textInfo.TextAlignment = DWRITE_TEXT_ALIGNMENT_LEADING;
            textInfo.ParagraphAlignment = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;

            auto placeTextBlock = [&](TextBlock& block, float top, float blockHeight) {
                textInfo.Height = (uint32_t)std::floor(blockHeight * textInfo.Width / titleWidth); // Keep texture aspect ratio
                auto textTexture = std::make_unique<engine::TextTexture>(m_context, textInfo);
                textTexture->Draw(block.Text.c_str());
                const auto& material = textTexture->CreatePbrMaterial(m_context.PbrResources);
                block.Object = AddObject(engine::CreateQuad(m_context.PbrResources, {titleWidth, blockHeight}, material));
                block.Object->Pose() = Pose::Translation({0, (titleHeight / 2) - top - (blockHeight / 2), margin});
                block.Object->SetParent(m_background);
            };

            m_title.Text =
                fmt::format(L"{}, v{}", xr::utf8_to_wide(m_context.Instance.AppInfo.Name), m_context.Instance.AppInfo.Version)
                    .c_str();
            textInfo.FontSize = 16.0f;
            placeTextBlock(m_title, margin, titleHeight / 2 - margin * 2);

            m_subtitle.Text = fmt::format(L"OpenXR API version: {}.{}.{}\n{}, v{}.{}.{}",
                                          XR_VERSION_MAJOR(XR_CURRENT_API_VERSION),
                                          XR_VERSION_MINOR(XR_CURRENT_API_VERSION),
                                          XR_VERSION_PATCH(XR_CURRENT_API_VERSION),
                                          xr::utf8_to_wide(m_context.Instance.Properties.runtimeName),
                                          XR_VERSION_MAJOR(m_context.Instance.Properties.runtimeVersion),
                                          XR_VERSION_MINOR(m_context.Instance.Properties.runtimeVersion),
                                          XR_VERSION_PATCH(m_context.Instance.Properties.runtimeVersion))
                                  .c_str();
            textInfo.FontSize = 10.0f;
            placeTextBlock(m_subtitle, titleHeight / 2, titleHeight / 2 - margin);
        }

        void OnUpdate(const engine::FrameTime& frameTime) override {
            XrSpaceLocation viewInScene = {XR_TYPE_SPACE_LOCATION};
            CHECK_XRCMD(xrLocateSpace(m_viewSpace.Get(), m_context.SceneSpace, frameTime.PredictedDisplayTime, &viewInScene));
            if (Pose::IsPoseValid(viewInScene)) {
                XrPosef titleInView = {{0, 0, 0, 1}, {0, 0, -1.f}}; // 1 meter in front
                XrPosef titleInScene = titleInView * viewInScene.pose;
                titleInScene.position.y = 0.5f; // floating in the top of user's view
                XrVector3f forward = titleInScene.position - viewInScene.pose.position;
                m_targetPose = Pose::LookAt(titleInScene.position, forward, {0, 1, 0});

                if (!m_background->IsVisible()) {
                    m_background->SetVisible(true);
                    m_background->Pose() = m_targetPose;
                } else {
                    // Ease the object to the target pose slowly. 98%^90hz~=16%
                    m_background->Pose() = Pose::Slerp(m_background->Pose(), m_targetPose, 0.02f);
                }
            }
        }

    private:
        struct TextBlock {
            std::wstring Text;
            std::shared_ptr<engine::PbrModelObject> Object;
        };

        xr::SpaceHandle m_viewSpace;
        std::shared_ptr<engine::PbrModelObject> m_background;
        TextBlock m_title;
        TextBlock m_subtitle;
        XrPosef m_targetPose;
    };
} // namespace

std::unique_ptr<engine::Scene> TryCreateTitleScene(engine::Context& context) {
    return std::make_unique<TitleScene>(context);
}