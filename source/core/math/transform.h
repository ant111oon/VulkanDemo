#pragma once

#include "math.h"


namespace math
{
    inline glm::float3 GetTranslation(const glm::float4x4& trs) noexcept
    {
        return glm::float3(trs[3]);
    }


    inline glm::float3 GetScale(const glm::float4x4& trs) noexcept
    {
        return glm::float3(
            glm::max(glm::length(glm::float3(trs[0])), M3D_EPS),
            glm::max(glm::length(glm::float3(trs[1])), M3D_EPS),
            glm::max(glm::length(glm::float3(trs[2])), M3D_EPS)
        );
    }


    inline glm::quat GetRotation(const glm::float4x4& trs) noexcept
    {
        const glm::float3 scale = GetScale(trs);

        const glm::float3x3 rotMat = glm::float3x3(
            glm::float3(trs[0]) / scale.x,
            glm::float3(trs[1]) / scale.y,
            glm::float3(trs[2]) / scale.z
        );

        return glm::quat_cast(rotMat);
    }


    inline void GetTRSComponents(const glm::float4x4& trs, glm::float3& translation, glm::quat& rotation, glm::float3& scale) noexcept
    {
        translation = GetTranslation(trs);
        scale = GetScale(trs);

        const glm::float3x3 rotMat = glm::float3x3(
            glm::float3(trs[0]) / scale.x,
            glm::float3(trs[1]) / scale.y,
            glm::float3(trs[2]) / scale.z
        );

        rotation = GetRotation(trs);
    }


    constexpr inline glm::float4x4 MakeTRS(const glm::float3& translation, const glm::quat& rotation, const glm::float3& scale) noexcept
    {
        const glm::float4x4 T = glm::translate(M3D_MAT4X4_IDENTITY, translation);
        const glm::float4x4 R = glm::mat4_cast(rotation);
        const glm::float4x4 S = glm::scale(M3D_MAT4X4_IDENTITY, scale);

        return T * R * S;
    }


    constexpr inline glm::float4x4 MakeTR(const glm::float3& translation, const glm::quat& rotation) noexcept
    {
        const glm::float4x4 T = glm::translate(M3D_MAT4X4_IDENTITY, translation);
        const glm::float4x4 R = glm::mat4_cast(rotation);

        return T * R;
    }


    constexpr inline glm::float4x4 MakeTS(const glm::float3& translation, const glm::float3& scale) noexcept
    {
        const glm::float4x4 T = glm::translate(M3D_MAT4X4_IDENTITY, translation);
        const glm::float4x4 S = glm::scale(M3D_MAT4X4_IDENTITY, scale);

        return T * S;
    }
}