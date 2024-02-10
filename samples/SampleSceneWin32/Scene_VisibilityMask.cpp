#include "pch.h"

#include <XrSceneLib/ProjectionLayer.h>
#include <XrSceneLib/PbrModelObject.h>

namespace {
    void InvertWindingOrder(std::vector<uint32_t>& indices) {
        assert(indices.size() % 3 == 0);

        for (size_t i = 0; i < indices.size(); i += 3) {
            std::swap(indices[i], indices[i + 1]);
        }
    }

    Pbr::PrimitiveBuilder CreateMeshPrimitiveBuilder(const std::vector<XrVector2f>& vertices, std::vector<uint32_t> indices) {
        Pbr::PrimitiveBuilder builder;
        builder.Vertices.resize(vertices.size());
        builder.Indices = std::move(indices);

        for (uint32_t i = 0; i < vertices.size(); i++) {
            const XrVector2f& vertex = vertices[i];

            Pbr::Vertex& pbrVertex = builder.Vertices[i];
            pbrVertex.Position.x = vertex.x;
            pbrVertex.Position.y = vertex.y;
            pbrVertex.Position.z = -1;

            // normal facing viewer
            pbrVertex.Normal = {0, 0, 1};
            pbrVertex.Color0 = {1, 1, 1, 1};
            pbrVertex.Tangent = {1, 0, 0, 0};
            pbrVertex.TexCoord0 = {0, 0};
            pbrVertex.ModelTransformIndex = Pbr::RootNodeIndex; // Index into the node transforms
        }

        return builder;
    }

    struct VisibilityMaskScene : public engine::Scene {
        VisibilityMaskScene(engine::Context& context)
            : Scene(context) {
            m_meshMaterial = Pbr::Material::CreateFlat(m_context.PbrResources, Pbr::RGBA::White, 1, 0);

            XrReferenceSpaceCreateInfo viewSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
            viewSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
            viewSpaceCreateInfo.poseInReferenceSpace = xr::math::Pose::Identity();
            CHECK_XRCMD(xrCreateReferenceSpace(context.Session.Handle, &viewSpaceCreateInfo, m_viewSpace.Put(xrDestroySpace)));

            const size_t viewCount = xr::EnumerateViewConfigurationViews(
                                         context.Instance.Handle, context.System.Id, context.Session.PrimaryViewConfigurationType)
                                         .size();

            m_visibleMaskObjects.resize(viewCount);
            for (uint32_t viewIndex = 0; viewIndex < viewCount; viewIndex++) {
                UpdateVisibleMaskAtViewIndex(context.Session.Handle, context.Session.PrimaryViewConfigurationType, viewIndex);
            }
        }

        void OnActiveChanged() override {
            m_visibleMaskEnabled = IsActive();
            sample::Trace("VisibilityMaskEnabled: {}", IsActive());
        }

        void OnEvent(const XrEventDataBuffer& eventData) override {
            if (auto* maskChangedEvent = xr::event_cast<XrEventDataVisibilityMaskChangedKHR>(&eventData)) {
                if (maskChangedEvent->session == m_context.Session.Handle &&
                    maskChangedEvent->viewConfigurationType == m_context.Session.PrimaryViewConfigurationType) {
                    UpdateVisibleMaskAtViewIndex(
                        maskChangedEvent->session, maskChangedEvent->viewConfigurationType, maskChangedEvent->viewIndex);
                }
            }
        }

        void OnUpdate(const engine::FrameTime& frameTime) override {
            XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
            locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            locateInfo.displayTime = frameTime.PredictedDisplayTime;
            locateInfo.space = m_context.AppSpace;

            XrViewState viewState{XR_TYPE_VIEW_STATE};

            uint32_t viewCount = 0;
            CHECK_XRCMD(xrLocateViews(m_context.Session.Handle, &locateInfo, &viewState, 0, &viewCount, nullptr));

            std::vector<XrView> views(viewCount, {XR_TYPE_VIEW});
            CHECK_XRCMD(xrLocateViews(m_context.Session.Handle, &locateInfo, &viewState, viewCount, &viewCount, views.data()));

            assert(viewCount == xr::StereoView::Count);
            assert(m_visibleMaskObjects.size() >= xr::StereoView::Count);

            if (xr::math::Pose::IsPoseValid(viewState)) {
                for (uint32_t viewIndex : {xr::StereoView::Left, xr::StereoView::Right}) {
                    if (m_visibleMaskObjects[viewIndex]) {
                        m_visibleMaskObjects[viewIndex]->Pose() = views[viewIndex].pose;
                    }
                }
            }
        }

    private:
        void UpdateVisibleMaskAtViewIndex(XrSession session, XrViewConfigurationType type, uint32_t viewIndex) {
            XrVisibilityMaskKHR mask{XR_TYPE_VISIBILITY_MASK_KHR};
            CHECK_XRCMD(xrGetVisibilityMaskKHR(session, type, viewIndex, XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR, &mask));

            if (mask.vertexCountOutput != 0 && mask.indexCountOutput != 0) {
                mask.vertexCapacityInput = mask.vertexCountOutput;
                mask.indexCapacityInput = mask.indexCountOutput;
                std::vector<XrVector2f> visibleMaskVertices(mask.vertexCountOutput);
                std::vector<uint32_t> visibleMaskIndices(mask.indexCountOutput);

                mask.vertices = visibleMaskVertices.data();
                mask.indices = visibleMaskIndices.data();

                CHECK_XRCMD(xrGetVisibilityMaskKHR(session, type, viewIndex, XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR, &mask));

                // xrGetVisibilityMaskKHR returns indices in counter-clockwise winding order
                // The pbr renderer expects clockwise winding order; thus the winding order inversion is needed
                InvertWindingOrder(visibleMaskIndices);

                const Pbr::PrimitiveBuilder meshBuilder = CreateMeshPrimitiveBuilder(visibleMaskVertices, std::move(visibleMaskIndices));
                if (!m_visibleMaskObjects.at(viewIndex)) {
                    // The hand mesh scene object doesn't exist yet and must be created.
                    Pbr::Primitive surfacePrimitive(m_context.PbrResources, meshBuilder, m_meshMaterial, false /* updatableBuffers */);

                    auto surfaceModel = std::make_shared<Pbr::Model>();
                    surfaceModel->AddPrimitive(std::move(surfacePrimitive));

                    m_visibleMaskObjects.at(viewIndex) = AddObject(
                        std::make_shared<engine::PbrModelObject>(surfaceModel, Pbr::ShadingMode::Regular, Pbr::FillMode::Wireframe));
                    m_visibleMaskObjects.at(viewIndex)->SetOnlyVisibleForViewIndex(viewIndex);
                } else {
                    // Update vertices and indices of the existing hand mesh scene object's primitive.
                    m_visibleMaskObjects.at(viewIndex)
                        ->GetModel()
                        ->GetPrimitive(Pbr::RootNodeIndex)
                        .UpdateBuffers(m_context.Device.get(), m_context.DeviceContext.get(), meshBuilder);
                }
            } else {
                sample::Trace("VisibleMaskUnavailable: viewIndex: {}, vertexCount: {}, indexCount: {}",
                              viewIndex,
                              mask.vertexCountOutput,
                              mask.indexCountOutput);
            }
        }

    private:
        std::vector<std::shared_ptr<engine::PbrModelObject>> m_visibleMaskObjects;
        std::shared_ptr<Pbr::Material> m_meshMaterial;

        xr::SpaceHandle m_viewSpace;

        bool m_visibleMaskEnabled{false};
    };
} // namespace

std::unique_ptr<engine::Scene> TryCreateVisibilityMaskScene(engine::Context& context) {
    return context.Extensions.SupportsVisibilityMask ? std::make_unique<VisibilityMaskScene>(context) : nullptr;
}
