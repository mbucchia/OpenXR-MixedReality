#include "pch.h"
#include <SampleShared/DxUtility.h>

#include <XrSceneLib/Scene.h>
#include <XrSceneLib/QuadLayerObject.h>

using namespace DirectX;
using namespace std::chrono_literals;

namespace {
    std::vector<int32_t>
    GenerateCheckboardData(uint32_t widthInPixels, uint32_t heightInPixels, uint32_t columnWidthInPixels, uint32_t rowHeightInPixels) {
        std::vector<int32_t> buffer(widthInPixels * heightInPixels);

        bool filled = true;

        for (uint32_t y = 0; y < heightInPixels; ++y) {
            for (uint32_t x = 0; x < widthInPixels; ++x) {
                buffer[y * widthInPixels + x] = (filled) ? 0xFF000000 : 0xFFFFFFFF;

                if ((x + 1) % columnWidthInPixels == 0) {
                    filled = !filled;
                }
            }

            if ((y + 1) % rowHeightInPixels == 0) {
                filled = !filled;
            }
        }

        return buffer;
    }

    void GenerateMipmaps(ID3D11Texture2D* texture) {
        winrt::com_ptr<ID3D11Device> device;
        texture->GetDevice(device.put());

        winrt::com_ptr<ID3D11DeviceContext> context;
        device->GetImmediateContext(context.put());

        D3D11_TEXTURE2D_DESC textureDesc;
        texture->GetDesc(&textureDesc);

        const CD3D11_SHADER_RESOURCE_VIEW_DESC desc{texture, D3D11_SRV_DIMENSION_TEXTURE2D, textureDesc.Format};

        winrt::com_ptr<ID3D11ShaderResourceView> result;
        CHECK_HRCMD(device->CreateShaderResourceView(texture, &desc, result.put()));
        context->GenerateMips(result.get());
    }

    winrt::com_ptr<ID3D11Texture2D> CreateCheckerboard(ID3D11Device* device,
                                                       uint32_t widthInPixels = 512,
                                                       uint32_t heightInPixels = 512,
                                                       uint32_t columnWidthInPixels = 32,
                                                       uint32_t rowHeightInPixels = 32,
                                                       D3D11_USAGE usage = D3D11_USAGE_DEFAULT,
                                                       uint32_t cpuAccessflags = 0) {
        auto colorBuffer = GenerateCheckboardData(widthInPixels, heightInPixels, columnWidthInPixels, rowHeightInPixels);

        uint32_t bindFlags = D3D11_BIND_SHADER_RESOURCE;
        uint32_t miscFlags = 0;

        if (usage == D3D11_USAGE_DEFAULT) {
            bindFlags |= D3D11_BIND_RENDER_TARGET;
            miscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
        }

        const auto desc = CD3D11_TEXTURE2D_DESC(
            DXGI_FORMAT_R8G8B8A8_UNORM, widthInPixels, heightInPixels, 1, 1, bindFlags, usage, cpuAccessflags, 1, 0, miscFlags);

        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = colorBuffer.data();
        data.SysMemPitch = sizeof(int32_t) * widthInPixels;

        winrt::com_ptr<ID3D11Texture2D> result;
        CHECK_HRCMD(device->CreateTexture2D(&desc, &data, result.put()));

        if (usage == D3D11_USAGE_DEFAULT) {
            GenerateMipmaps(result.get());
        }

        return result;
    }

    struct QuadLayerScene : public engine::Scene {
        explicit QuadLayerScene(engine::Context& context)
            : Scene(context) {
            auto device = m_context.Device;
            auto deviceContext = m_context.DeviceContext;

            auto checkerBoard = CreateCheckerboard(device.get(), m_staticCheckboardImageSize, m_staticCheckboardImageSize, 32, 32);

            m_staticCheckboardSwapchain = sample::dx::CreateSwapchainD3D11(m_context.Session.Handle,
                                                                           DXGI_FORMAT_R8G8B8A8_UNORM,
                                                                           m_staticCheckboardImageSize,
                                                                           m_staticCheckboardImageSize,
                                                                           /*arrayLength*/ 1,
                                                                           /*sampleCount*/ 1,
                                                                           /*createFlags*/ 0,
                                                                           /*usage Flags*/ 0);

            {
                uint32_t index;

                const XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                CHECK_XRCMD(xrAcquireSwapchainImage(m_staticCheckboardSwapchain.Handle.Get(), &acquireInfo, &index));

                XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                waitInfo.timeout = XR_INFINITE_DURATION;
                CHECK_XRCMD(xrWaitSwapchainImage(m_staticCheckboardSwapchain.Handle.Get(), &waitInfo));

                deviceContext->CopyResource(m_staticCheckboardSwapchain.Images[index].texture, checkerBoard.get());

                const XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                CHECK_XRCMD(xrReleaseSwapchainImage(m_staticCheckboardSwapchain.Handle.Get(), &releaseInfo));
            }

            m_dynamicCheckboardImage = CreateCheckerboard(device.get(),
                                                          m_dynamicCheckboardImageSize,
                                                          m_dynamicCheckboardImageSize,
                                                          32,
                                                          32,
                                                          D3D11_USAGE_DYNAMIC,
                                                          D3D11_CPU_ACCESS_WRITE);

            m_dynamicCheckboardSwapchain = sample::dx::CreateSwapchainD3D11(m_context.Session.Handle,
                                                                            DXGI_FORMAT_R8G8B8A8_UNORM,
                                                                            m_dynamicCheckboardImageSize,
                                                                            m_dynamicCheckboardImageSize,
                                                                            /*arrayLength*/ 1,
                                                                            /*sampleCount*/ 1,
                                                                            /*createFlags*/ 0,
                                                                            /*usage Flags*/ 0);

            {
                uint32_t index;

                XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                CHECK_XRCMD(xrAcquireSwapchainImage(m_dynamicCheckboardSwapchain.Handle.Get(), &acquireInfo, &index));

                XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                waitInfo.timeout = XR_INFINITE_DURATION;
                CHECK_XRCMD(xrWaitSwapchainImage(m_dynamicCheckboardSwapchain.Handle.Get(), &waitInfo));

                deviceContext->CopyResource(m_dynamicCheckboardSwapchain.Images[index].texture, m_dynamicCheckboardImage.get());

                const XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                CHECK_XRCMD(xrReleaseSwapchainImage(m_dynamicCheckboardSwapchain.Handle.Get(), &releaseInfo));
            }

            XrSwapchainSubImage staticImageData{};
            staticImageData.swapchain = m_staticCheckboardSwapchain.Handle.Get();
            staticImageData.imageRect.extent = {m_staticCheckboardImageSize, m_staticCheckboardImageSize};

            XrSwapchainSubImage dynamicImageData{};
            dynamicImageData.swapchain = m_dynamicCheckboardSwapchain.Handle.Get();
            dynamicImageData.imageRect.extent = {m_dynamicCheckboardImageSize, m_dynamicCheckboardImageSize};

            XrReferenceSpaceCreateInfo spaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};

            {
                spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
                spaceCreateInfo.poseInReferenceSpace = xr::math::Pose::Translation({0, 0, -100});
                CHECK_XRCMD(xrCreateReferenceSpace(m_context.Session.Handle, &spaceCreateInfo, &m_viewSpace));

                m_viewQuad = AddQuadLayerObject(engine::CreateQuadLayerObject(m_viewSpace, dynamicImageData));
                m_viewQuad->Scale() = {50, 50, 50};
                m_viewQuad->LayerGroup = engine::LayerGrouping::Underlay;
            }

            if (m_context.Session.SupportsStageSpace) {
                spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
                spaceCreateInfo.poseInReferenceSpace = xr::math::Pose::Translation({0, 0, -2});
                CHECK_XRCMD(xrCreateReferenceSpace(m_context.Session.Handle, &spaceCreateInfo, &m_stageSpace));

                m_stageQuad = AddQuadLayerObject(engine::CreateQuadLayerObject(m_stageSpace, staticImageData));
                m_stageQuad->Scale() = {0.75f, 0.75f, 0.75f};
            }
        }

        ~QuadLayerScene() {
            CHECK_XRCMD(xrDestroySpace(m_viewSpace));
            if (m_stageSpace != XR_NULL_HANDLE) {
                CHECK_XRCMD(xrDestroySpace(m_stageSpace));
            }
        }

        void OnUpdate(const engine::FrameTime& frameTime) override {
            const float dt = std::chrono::duration<float>(frameTime.Elapsed).count();

            // Rotate quad around Y
            if (m_stageQuad) {
                auto orientation = m_stageQuad->Pose().orientation;
                auto orientationXM =
                    XMQuaternionMultiply(xr::math::LoadXrQuaternion(orientation), XMQuaternionRotationRollPitchYaw(0.0f, dt * 0.25f, 0.0f));
                xr::math::StoreXrQuaternion(&m_stageQuad->Pose().orientation, orientationXM);
            }

            // Generate a new image for the headQuad. This swapchain image has arrayLength = 1, we only need to fill the data once
            // per frame, instead of per eye in OnRender
            if (((frameTime.FrameIndex + 1) % 30) == 0) {
                const DWORD fillColor = (rand() & 0xFF) | (rand() & 0xFF) << 8 | (rand() & 0xFF) << 16 | (rand() & 0xFF) << 24;

                bool filled = true;

                auto context = m_context.DeviceContext;

                D3D11_MAPPED_SUBRESOURCE subresource;
                if (SUCCEEDED(context->Map(m_dynamicCheckboardImage.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &subresource))) {
                    uint32_t* dynamicCheckboardData = static_cast<uint32_t*>(subresource.pData);
                    const uint32_t pitch = subresource.RowPitch / sizeof(uint32_t);
                    for (int32_t y = 0; y < m_dynamicCheckboardImageSize; ++y) {
                        for (int32_t x = 0; x < m_dynamicCheckboardImageSize; ++x) {
                            dynamicCheckboardData[y * pitch + x] = (filled) ? fillColor : 0xFFFFFFFF;

                            if ((x + 1) % 32 == 0) {
                                filled = !filled;
                            }
                        }

                        if ((y + 1) % 32 == 0) {
                            filled = !filled;
                        }
                    }

                    context->Unmap(m_dynamicCheckboardImage.get(), 0);
                }

                uint32_t index;

                XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                CHECK_XRCMD(xrAcquireSwapchainImage(m_dynamicCheckboardSwapchain.Handle.Get(), &acquireInfo, &index));

                XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                waitInfo.timeout = XR_INFINITE_DURATION;
                CHECK_XRCMD(xrWaitSwapchainImage(m_dynamicCheckboardSwapchain.Handle.Get(), &waitInfo));

                context->CopyResource(m_dynamicCheckboardSwapchain.Images[index].texture, m_dynamicCheckboardImage.get());

                const XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                CHECK_XRCMD(xrReleaseSwapchainImage(m_dynamicCheckboardSwapchain.Handle.Get(), &releaseInfo));
            }
        }

    private:
        XrSpace m_viewSpace{XR_NULL_HANDLE};
        std::shared_ptr<engine::QuadLayerObject> m_viewQuad;
        XrSpace m_stageSpace{XR_NULL_HANDLE};
        std::shared_ptr<engine::QuadLayerObject> m_stageQuad;

        const int32_t m_staticCheckboardImageSize = 512;
        sample::dx::SwapchainD3D11 m_staticCheckboardSwapchain;

        const int32_t m_dynamicCheckboardImageSize = 256;
        winrt::com_ptr<ID3D11Texture2D> m_dynamicCheckboardImage;
        sample::dx::SwapchainD3D11 m_dynamicCheckboardSwapchain;
    };
} // namespace

std::unique_ptr<engine::Scene> TryCreateQuadLayerScene(engine::Context& context) {
    return std::make_unique<QuadLayerScene>(context);
}
