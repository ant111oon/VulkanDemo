#pragma once

#include "aabb.h"

#include <span>


namespace math
{
    struct Plane
    {
        Plane() = default;
        Plane(const glm::float3& norm, float dist);

        float DistanceTo(const glm::float3& pt) const;

        constexpr operator glm::float4() const { return glm::float4(normal, distance); }

        glm::float3 normal = { 0.f, 1.f, 0.f };
        float distance = 0.f;
    };


    enum FrustumCornerIndex
    {
        FR_CORNER_LEFT_BOTTOM_NEAR,
        FR_CORNER_RIGHT_BOTTOM_NEAR,
        FR_CORNER_RIGHT_TOP_NEAR,
        FR_CORNER_LEFT_TOP_NEAR,

        FR_CORNER_LEFT_BOTTOM_FAR,
        FR_CORNER_RIGHT_BOTTOM_FAR,
        FR_CORNER_RIGHT_TOP_FAR,
        FR_CORNER_LEFT_TOP_FAR,

        FR_CORNER_COUNT
    };

    static_assert(FR_CORNER_COUNT == M3D_FRUSTUM_CORNER_COUNT);

    enum FrustumPlaneIndex
    {
        FR_PLANE_LEFT,
        FR_PLANE_TOP,
        FR_PLANE_RIGHT,
        FR_PLANE_BOTTOM,
        FR_PLANE_NEAR,
        FR_PLANE_FAR,
        FR_PLANE_COUNT,
    };

    static_assert(FR_PLANE_COUNT == M3D_FRUSTUM_PLANE_COUNT);


    using FrustumCorners = std::array<glm::float3, FR_CORNER_COUNT>;

    
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

        const Plane& GetPlane(FrustumPlaneIndex index) const;

        std::span<const Plane> GetPlanes() const { return planes; }

    private:
        std::array<Plane, M3D_FRUSTUM_PLANE_COUNT> planes;
    };


    FrustumCorners GetFrustumCornersWCS(const glm::float4x4& view, const glm::float4x4& proj);
    FrustumCorners GetFrustumCornersWCS(const glm::float4x4& viewProj);
    FrustumCorners GetFrustumCornersWCS_Inv(const glm::float4x4& invViewProj);

    glm::float3 GetFrustumCenterWCS(const FrustumCorners& corners);
}
