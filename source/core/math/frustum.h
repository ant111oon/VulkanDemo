#pragma once

#include "aabb.h"

#include <span>


namespace math
{
    struct Plane
    {
        Plane() = default;
        Plane(const glm::float3& norm, float dist);

        void Construct(const glm::float3& A, const glm::float3& B, const glm::float3& C);

        float DistanceTo(const glm::float3& pt) const;

        constexpr operator glm::float4() const { return glm::float4(normal, distance); }

        glm::float3 normal = { 0.f, 1.f, 0.f };
        float distance = 0.f;
    };

    
    class Frustum
    {
    public:
        enum PointID
        {
            POINT_NEAR_LEFT_BOTTOM,
            POINT_NEAR_RIGHT_BOTTOM,
            POINT_NEAR_RIGHT_TOP,
            POINT_NEAR_LEFT_TOP,

            POINT_FAR_LEFT_BOTTOM,
            POINT_FAR_RIGHT_BOTTOM,
            POINT_FAR_RIGHT_TOP,
            POINT_FAR_LEFT_TOP,

            POINT_COUNT
        };

        static_assert(POINT_COUNT == M3D_FRUSTUM_CORNER_COUNT);

        enum PlaneID
        {
            PLANE_LEFT,
            PLANE_TOP,
            PLANE_RIGHT,
            PLANE_BOTTOM,
            PLANE_NEAR,
            PLANE_FAR,

            PLANE_COUNT
        };

        static_assert(PLANE_COUNT == M3D_FRUSTUM_PLANE_COUNT);  
        
    public:
        Frustum() = default;

        Frustum(const glm::float3& position, const glm::quat& rotation, const glm::float4x4& proj);
        Frustum(const glm::float4x4& view, const glm::float4x4& proj);
        Frustum(const glm::float4x4& viewProj);
        
        Frustum(const glm::float3& position, const glm::quat& rotation, float fovY, float aspectRatio, float zNear, float zFar);
        Frustum(const glm::float3& position, const glm::quat& rotation, float left, float right, float bottom, float top, float zNear, float zFar);
        
        void Construct(const glm::float3& position, const glm::quat& rotation, const glm::float4x4& proj);
        void Construct(const glm::float4x4& view, const glm::float4x4& proj);
        void Construct(const glm::float4x4& viewProj);

        void Construct(const glm::float3& position, const glm::quat& rotation, float fovY, float aspectRatio, float zNear, float zFar);
        void Construct(const glm::float3& position, const glm::quat& rotation, float left, float right, float bottom, float top, float zNear, float zFar);
        
        bool IsIntersect(const AABB& aabb) const;

        const Plane& GetPlane(uint32_t index) const;
        const glm::float3& GetPoint(uint32_t index) const;

        const glm::float3& GetCenter() const { return m_center; }

        std::span<const Plane> GetPlanes() const { return m_planes; }
        std::span<const glm::float3> GetPoints() const { return m_points; }

    private:
        std::array<Plane, PLANE_COUNT> m_planes;
        std::array<glm::float3, POINT_COUNT> m_points;

        glm::float3 m_center;
    };
}
