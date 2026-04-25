#pragma once

#include "aabb.h"


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
    
    
    struct Frustum
    {
        Frustum() = default;
        Frustum(const glm::float3& position, const glm::float3& rightDir, const glm::float3& upDir, float fovY, float aspectRatio, float zNear, float zFar);

        void Recalculate(const glm::float3& position, const glm::float3& rightDir, const glm::float3& upDir, float fovY, float aspectRatio, float zNear, float zFar);
        bool IsIntersect(const AABB& aabb) const;

        const Plane& GetPlane(size_t index) const;

        enum PlaneIdx : size_t
        {
            PLANE_IDX_LEFT,
            PLANE_IDX_TOP,
            PLANE_IDX_RIGHT,
            PLANE_IDX_BOTTOM,
            PLANE_IDX_NEAR,
            PLANE_IDX_FAR,
            PLANE_COUNT
        };

        std::array<Plane, PLANE_COUNT> planes;
    };
}
