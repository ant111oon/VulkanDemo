#pragma once

#ifdef ENG_GFX_API_VULKAN
    #define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>


#include "core/utils/assert.h"


#define MATH_LOG_TRACE(FMT, ...)        ENG_LOG_TRACE("MATH", FMT, __VA_ARGS__)
#define MATH_LOG_INFO(FMT, ...)         ENG_LOG_INFO("MATH",  FMT, __VA_ARGS__)
#define MATH_LOG_WARN(FMT, ...)         ENG_LOG_WARN("MATH",  FMT, __VA_ARGS__)
#define MATH_LOG_ERROR(FMT, ...)        ENG_LOG_ERROR("MATH", FMT, __VA_ARGS__)
#define MATH_ASSERT_MSG(COND, FMT, ...) ENG_ASSERT_MSG(COND, "MATH", FMT, __VA_ARGS__)
#define MATH_ASSERT(COND)               MATH_ASSERT_MSG(COND, #COND)
#define MATH_ASSERT_FAIL(FMT, ...)      MATH_ASSERT_MSG(false, FMT, __VA_ARGS__)


constexpr inline float M3D_EPS     = 0.000001f;
constexpr inline float M3D_2_EPS   = 2.f * M3D_EPS;
constexpr inline float M3D_PI      = glm::pi<float>();
constexpr inline float M3D_2_PI    = 2.0f * M3D_PI;
constexpr inline float M3D_HALF_PI = 0.5f * M3D_PI;


constexpr inline glm::vec2 M3D_ZEROF2 = glm::vec2(0.f);
constexpr inline glm::vec3 M3D_ZEROF3 = glm::vec3(0.f);
constexpr inline glm::vec4 M3D_ZEROF4 = glm::vec4(0.f);
constexpr inline glm::vec2 M3D_ONEF2  = glm::vec2(1.f);
constexpr inline glm::vec3 M3D_ONEF3  = glm::vec3(1.f);
constexpr inline glm::vec4 M3D_ONEF4  = glm::vec4(1.f);


constexpr inline glm::vec3 M3D_AXIS_X = glm::vec3(1.f, 0.f, 0.f);
constexpr inline glm::vec3 M3D_AXIS_Y = glm::vec3(0.f, 1.f, 0.f);
constexpr inline glm::vec3 M3D_AXIS_Z = glm::vec3(0.f, 0.f, 1.f);


constexpr inline glm::mat3 M3D_MAT3_IDENTITY = glm::identity<glm::mat3>();
constexpr inline glm::mat4 M3D_MAT4_IDENTITY = glm::identity<glm::mat4>();
constexpr inline glm::quat M3D_QUAT_IDENTITY = glm::identity<glm::quat>();


namespace math
{
    template <typename T>
    constexpr inline bool IsZero(const T& value) noexcept
    {
        return glm::length(value) < M3D_EPS;
    }

    constexpr inline bool IsZero(float value) noexcept
    {
        return glm::abs(value) < M3D_EPS;
    }


    template <typename T>
    constexpr inline bool IsNormalized(const T& value) noexcept
    {
        return glm::abs(glm::length(value) - 1.f) < M3D_EPS;
    }


    constexpr inline bool IsNormalized(const glm::quat& quat) noexcept
    {
        return std::abs(glm::length(quat) - 1.f) < M3D_EPS;
    }


    template <typename T>
    constexpr inline bool IsEqual(const T& left, const T& right) noexcept
    {
        return glm::all(glm::epsilonEqual(left, right, M3D_EPS));
    }


    constexpr inline bool IsEqual(float left, float right) noexcept
    {
        return glm::abs(left - right) < M3D_EPS;
    }
}
