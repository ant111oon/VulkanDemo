#include "pch.h"

#include "camera.h"
#include "core/core.h"


namespace eng
{
    constexpr inline bool IsFovValid(float radians) noexcept
    {
        return radians > glm::radians(M3D_EPS) && radians < glm::radians(180.f);
    }


    Camera::~Camera()
    {
        Destroy();
    }


    void Camera::Destroy() noexcept
    {
        m_matViewProj = M3D_MAT4X4_IDENTITY;
        m_matProj     = M3D_MAT4X4_IDENTITY;
        m_matView     = M3D_MAT4X4_IDENTITY;
        
        m_rotation = M3D_QUAT_IDENTITY;
        m_position = M3D_ZEROF3;
        
        m_fovY = 0.f;
        m_aspectRatio = 0.f;
        
        m_left = 0.f;
        m_right = 0.f;
        m_top = 0.f;
        m_bottom = 0.f;
        
        m_zNear = 0.f;
        m_zFar = 0.f;
    }


    void Camera::SetPerspProjection(float fovY, float aspectRatio, float zNear, float zFar) noexcept
    {
        m_flags.set(FLAG_IS_ORTHO_PROJ, false);

        SetFovY(fovY);
        SetAspectRatio(aspectRatio);
        SetZNear(zNear);
        SetZFar(zFar);

        RequestRecalcProjMatrix();
    }


    void Camera::SetOrthoProjection(float left, float right, float top, float bottom, float zNear, float zFar) noexcept
    {
        m_flags.set(FLAG_IS_ORTHO_PROJ, true);

        SetOrthoLeft(left);
        SetOrthoRight(right);
        SetOrthoTop(top);
        SetOrthoBottom(bottom);
        SetZNear(zNear);
        SetZFar(zFar);

        RequestRecalcProjMatrix();
    }


    void Camera::SetFovY(float radians) noexcept
    {
        if (!math::IsEqual(m_fovY, radians)) {
            CORE_ASSERT(IsFovValid(radians));

            m_fovY = radians;
            RequestRecalcProjMatrix();
        }
    }


    void Camera::SetAspectRatio(float aspect) noexcept
    {
        if (!math::IsEqual(m_aspectRatio, aspect)) {
            CORE_ASSERT_MSG(aspect > M3D_EPS, "Aspect can't be less or equal to zero");

            m_aspectRatio = aspect;
            RequestRecalcProjMatrix();
        }
    }


    void Camera::SetAspectRatio(uint32_t width, uint32_t height) noexcept
    {
        CORE_ASSERT_MSG(height != 0, "Height can't be equal to zero");

        const float aspectRatio = float(width) / float(height);
        SetAspectRatio(aspectRatio);
    }


    void Camera::SetZNear(float zNear) noexcept
    {
        CORE_ASSERT_MSG(zNear > 0.f, "zNear must be positive");

        if (!math::IsEqual(m_zNear, zNear)) {
            CORE_ASSERT_MSG(abs(m_zFar - zNear) > M3D_EPS, "Can't set Z Near equal to Z Far");
        
            m_zNear = zNear;
            RequestRecalcProjMatrix();
        }
    }


    void Camera::SetZFar(float zFar) noexcept
    {
        CORE_ASSERT_MSG(zFar > 0.f, "zFar must be positive");

        if (!math::IsEqual(m_zFar, zFar)) {
            CORE_ASSERT_MSG(abs(zFar - m_zNear) > M3D_EPS, "Can't set Z Far equal to Z Near");
        
            m_zFar = zFar;
            RequestRecalcProjMatrix();
        }
    }


    void Camera::SetOrthoLeft(float left) noexcept
    {
        if (!math::IsEqual(m_left, left)) {
            CORE_ASSERT_MSG(abs(m_right - left) > M3D_EPS, "Can't set left equal to right");
        
            m_left = left;
            RequestRecalcProjMatrix();
        }
    }


    void Camera::SetOrthoRight(float right) noexcept
    {
        if (!math::IsEqual(m_right, right)) {
            CORE_ASSERT_MSG(abs(right - m_left) > M3D_EPS, "Can't set right equal to left");
        
            m_right = right;
            RequestRecalcProjMatrix();
        }
    }


    void Camera::SetOrthoTop(float top) noexcept
    {
        if (!math::IsEqual(m_top, top)) {
            CORE_ASSERT_MSG(abs(top - m_bottom) > M3D_EPS, "Can't set top equal to bottom");
        
            m_top = top;
            RequestRecalcProjMatrix();
        }
    }


    void Camera::SetOrthoBottom(float bottom) noexcept
    {
        if (!math::IsEqual(m_bottom, bottom)) {
            CORE_ASSERT_MSG(abs(m_top - bottom) > M3D_EPS, "Can't set bottom equal to top");
        
            m_bottom = bottom;
            RequestRecalcProjMatrix();
        }
    }


    void Camera::Move(const glm::vec3& offset) noexcept
    {
        if (!math::IsZero(offset)) {
            m_position += offset;
            RequestRecalcViewMatrix();
        }
    }


    void Camera::MoveAlongDir(const glm::vec3& dir, float distance) noexcept
    {
        if (!math::IsZero(distance)) {
            CORE_ASSERT_MSG(math::IsNormalized(dir), "Direction must be a normalized vector");
        
            m_position += dir * distance;
            RequestRecalcViewMatrix();
        }
    }


    void Camera::SetRotation(const glm::quat& rotation) noexcept
    {
        CORE_ASSERT_MSG(math::IsNormalized(rotation), "Rotation quaternion must be normalized");

        if (!math::IsEqual(m_rotation, rotation)) {
            m_rotation = rotation;
            RequestRecalcViewMatrix();
        }
    }


    void Camera::SetPosition(const glm::vec3& position) noexcept
    {
        if (!math::IsEqual(m_position, position)) {
            m_position = position;
            RequestRecalcViewMatrix();
        }
    }


    void Camera::Update() noexcept
    {
        bool shouldRecalcViewProjMat = false;

        if (IsViewMatrixRecalcRequested()) {
            RecalcViewMatrix();
            ClearViewMatrixRecalcRequest();
            shouldRecalcViewProjMat = true;
        }

        if (IsProjMatrixRecalcRequested()) {
            RecalcProjMatrix();
            ClearProjRecalcRequest();
            shouldRecalcViewProjMat = true;
        }

        if (shouldRecalcViewProjMat) {
            RecalcViewProjMatrix();

            RecalcFrustum();
            ClearFrustumRecalcRequest();
        }
    }


    void Camera::RecalcProjMatrix() noexcept
    {
    #if defined(ENG_REVERSED_Z)
        const float zNear = m_zFar;
        const float zFar = m_zNear;
    #else
        const float zNear = m_zNear;
        const float zFar = m_zFar;
    #endif

        if (IsPerspProj()) {
            m_matProj = glm::perspective(m_fovY, m_aspectRatio, zNear, zFar);

        #ifdef ENG_GFX_API_VULKAN
            m_matProj[1][1] *= -1.f;
        #endif
        } else if (IsOrthoProj()) {
            m_matProj = glm::ortho(m_left, m_right, m_bottom, m_top, zNear, zFar);
        }   
    }


    void Camera::RecalcViewMatrix() noexcept
    {
        m_matView = glm::inverse(glm::mat4_cast(m_rotation)) * glm::translate(M3D_MAT4X4_IDENTITY, -m_position);
    }


    void Camera::RecalcViewProjMatrix() noexcept
    {
        m_matViewProj = m_matProj * m_matView;
    }


    void Camera::RecalcFrustum() noexcept
    {
        using namespace math;

        const glm::vec3 farVec = GetForwardDir() * m_zFar;
        const float halfH = m_zFar * glm::tan(m_fovY * 0.5f);
        const float halfW = halfH * m_aspectRatio;

        std::array<glm::vec3, Frustum::PLANE_COUNT> planeNormals = {};

        planeNormals[Frustum::PLANE_IDX_LEFT] = glm::normalize(glm::cross(farVec - GetXDir() * halfW, GetYDir()));
        planeNormals[Frustum::PLANE_IDX_TOP] = glm::normalize(glm::cross(farVec + GetYDir() * halfH, -GetXDir()));
        planeNormals[Frustum::PLANE_IDX_RIGHT] = glm::normalize(glm::cross(farVec + GetXDir() * halfW, -GetYDir()));
        planeNormals[Frustum::PLANE_IDX_BOTTOM] = glm::normalize(glm::cross(farVec - GetYDir() * halfH, GetXDir()));
        planeNormals[Frustum::PLANE_IDX_NEAR] = GetForwardDir();
        planeNormals[Frustum::PLANE_IDX_FAR] = -GetForwardDir();

        for (size_t i = 0; i < Frustum::PLANE_COUNT; ++i) {
            m_frustum.planes[Frustum::PLANE_IDX_LEFT] = Plane(planeNormals[i], -glm::dot(planeNormals[i], m_position));
        }
    }
}