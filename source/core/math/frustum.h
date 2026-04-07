#pragma once

#include "aabb.h"


namespace math
{
    struct Plane
    {
        Plane() = default;
        Plane(const glm::vec3& norm, float dist);

        glm::vec3 normal = { 0.f, 1.f, 0.f };
        float distance = 0.f;
    };
    
    
    struct Frustum
    {
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

        Frustum() = default;
        Frustum(const glm::float3& position, const glm::float3& rightDir, const glm::float3& upDir, float fovY, float aspectRatio, float zNear, float zFar);

        void Recalculate(const glm::float3& position, const glm::float3& rightDir, const glm::float3& upDir, float fovY, float aspectRatio, float zNear, float zFar);
        bool IsIntersect(const AABB& aabb) const;

        const Plane& GetPlane(PlaneIdx index) const;

        std::array<Plane, PLANE_COUNT> planes;
    };
}
