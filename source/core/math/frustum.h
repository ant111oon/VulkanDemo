#pragma once

#include "aabb.h"

#include <span>


namespace math
{
    struct Plane
    {
        Plane() = default;
        Plane(const glm::float3& norm, float dist);

        constexpr operator glm::float4() const { return glm::float4(normal, distance); }

        glm::float3 normal = { 0.f, 1.f, 0.f };
        float distance = 0.f;
    };
    
    
    class Frustum
    {
    public:
        Frustum() = default;
        Frustum(const glm::float3& position, const glm::float3& rightDir, const glm::float3& upDir, float fovY, float aspectRatio, float zNear, float zFar);
        Frustum(const glm::float3& position, const glm::quat& rotation, float fovY, float aspectRatio, float zNear, float zFar);
        Frustum(const glm::float4x4& transform, float fovY, float aspectRatio, float zNear, float zFar);

        void Construct(const glm::float3& position, const glm::float3& rightDir, const glm::float3& upDir, float fovY, float aspectRatio, float zNear, float zFar);
        void Construct(const glm::float3& position, const glm::quat& rotation, float fovY, float aspectRatio, float zNear, float zFar);
        void Construct(const glm::float4x4& transform, float fovY, float aspectRatio, float zNear, float zFar);

        bool IsIntersect(const AABB& aabb) const;

        const Plane& GetPlane(size_t index) const;

        std::span<const Plane> GetPlanes() const { return planes; }

    private:
        enum PlaneIdx : size_t
        {
            PLANE_IDX_LEFT,
            PLANE_IDX_TOP,
            PLANE_IDX_RIGHT,
            PLANE_IDX_BOTTOM,
            PLANE_IDX_NEAR,
            PLANE_IDX_FAR,
            PLANE_IDX_COUNT,
        };

        static_assert(M3D_FRUSTUM_PLANE_COUNT == PLANE_IDX_COUNT);

    private:
        std::array<Plane, M3D_FRUSTUM_PLANE_COUNT> planes;
    };
}
