#include "pch.h"

#include "camera.h"
#include "core/core.h"

#include "core/math/transform.h"


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
        m_currState = {};
        m_prevState = {};
    }


    void Camera::SetPerspProjection(float fovY, float aspectRatio, float zNear, float zFar) noexcept
    {
        m_currState.flags.set(FLAG_IS_ORTHO_PROJ, false);

        SetFovY(fovY);
        SetAspectRatio(aspectRatio);
        SetZNear(zNear);
        SetZFar(zFar);

        RequestRecalcProjMatrix();
    }


    void Camera::SetOrthoProjection(float left, float right, float bottom, float top, float zNear, float zFar) noexcept
    {
        m_currState.flags.set(FLAG_IS_ORTHO_PROJ, true);

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
        if (!math::IsEqual(m_currState.fovY, radians)) {
            CORE_ASSERT(IsFovValid(radians));

            m_currState.fovY = radians;
            RequestRecalcProjMatrix();
        }
    }


    void Camera::SetAspectRatio(float aspect) noexcept
    {
        if (!math::IsEqual(m_currState.aspectRatio, aspect)) {
            CORE_ASSERT_MSG(aspect > M3D_EPS, "Aspect can't be less or equal to zero");

            m_currState.aspectRatio = aspect;
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
        if (!math::IsEqual(m_currState.zNear, zNear)) {
            CORE_ASSERT_MSG(abs(m_currState.zFar - zNear) > M3D_EPS, "Can't set Z Near equal to Z Far");
        
            m_currState.zNear = zNear;
            RequestRecalcProjMatrix();
        }
    }


    void Camera::SetZFar(float zFar) noexcept
    {
        if (!math::IsEqual(m_currState.zFar, zFar)) {
            CORE_ASSERT_MSG(abs(zFar - m_currState.zNear) > M3D_EPS, "Can't set Z Far equal to Z Near");
        
            m_currState.zFar = zFar;
            RequestRecalcProjMatrix();
        }
    }


    void Camera::SetZNearFar(float zNear, float zFar) noexcept
    {
        SetZFar(zFar);
        SetZNear(zNear);
    }


    void Camera::SetOrthoLeft(float left) noexcept
    {
        if (!math::IsEqual(m_currState.left, left)) {
            CORE_ASSERT_MSG(abs(m_currState.right - left) > M3D_EPS, "Can't set left equal to right");
        
            m_currState.left = left;
            RequestRecalcProjMatrix();
        }
    }


    void Camera::SetOrthoRight(float right) noexcept
    {
        if (!math::IsEqual(m_currState.right, right)) {
            CORE_ASSERT_MSG(abs(right - m_currState.left) > M3D_EPS, "Can't set right equal to left");
        
            m_currState.right = right;
            RequestRecalcProjMatrix();
        }
    }


    void Camera::SetOrthoTop(float top) noexcept
    {
        if (!math::IsEqual(m_currState.top, top)) {
            CORE_ASSERT_MSG(abs(top - m_currState.bottom) > M3D_EPS, "Can't set top equal to bottom");
        
            m_currState.top = top;
            RequestRecalcProjMatrix();
        }
    }


    void Camera::SetOrthoBottom(float bottom) noexcept
    {
        if (!math::IsEqual(m_currState.bottom, bottom)) {
            CORE_ASSERT_MSG(abs(m_currState.top - bottom) > M3D_EPS, "Can't set bottom equal to top");
        
            m_currState.bottom = bottom;
            RequestRecalcProjMatrix();
        }
    }


    void Camera::Move(const glm::float3& offset) noexcept
    {
        if (!math::IsZero(offset)) {
            m_currState.position += offset;
            RequestRecalcViewMatrix();
        }
    }


    void Camera::MoveAlongDir(const glm::float3& dir, float distance) noexcept
    {
        if (!math::IsZero(distance)) {
            CORE_ASSERT_MSG(math::IsNormalized(dir), "Direction must be a normalized vector");
        
            m_currState.position += dir * distance;
            RequestRecalcViewMatrix();
        }
    }


    void Camera::SetLookAt(const glm::float3& target, const glm::float3& up) noexcept
    {
        CORE_ASSERT(!math::IsEqual(m_currState.position, target));

        const glm::float3 direction = glm::normalize(target - m_currState.position);
        CORE_ASSERT(!math::IsEqual(glm::abs(glm::dot(target, up)), 1.f));

        SetRotation(glm::quatLookAt(direction, up));
    }


    void Camera::SetRotation(const glm::quat& rotation) noexcept
    {
        CORE_ASSERT_MSG(math::IsNormalized(rotation), "Rotation quaternion must be normalized");

        if (!math::IsEqual(m_currState.rotation, rotation)) {
            m_currState.rotation = rotation;
            RequestRecalcViewMatrix();
        }
    }


    void Camera::SetPosition(const glm::float3& position) noexcept
    {
        if (!math::IsEqual(m_currState.position, position)) {
            m_currState.position = position;
            RequestRecalcViewMatrix();
        }
    }


    void Camera::SetTransform(const glm::float4x4& transform) noexcept
    {
        SetPosition(math::GetTranslation(transform));
        SetRotation(math::GetRotation(transform));
    }


    void Camera::PushPrevState() noexcept
    {
        m_prevState = m_currState;
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
        const float zNear = m_currState.zFar;
        const float zFar = m_currState.zNear;
    #else
        const float zNear = m_currState.zNear;
        const float zFar = m_currState.zFar;
    #endif

        if (IsPerspProj()) {
            m_currState.matProj = glm::perspective(m_currState.fovY, m_currState.aspectRatio, zNear, zFar);
        } else if (IsOrthoProj()) {
            m_currState.matProj = glm::ortho(m_currState.left, m_currState.right, m_currState.bottom, m_currState.top, zNear, zFar);
        }

        #ifdef ENG_GFX_API_VULKAN
            m_currState.matProj[1][1] *= -1.f;
        #endif

        m_currState.invMatProj = glm::inverse(m_currState.matProj);
    }


    void Camera::RecalcViewMatrix() noexcept
    {
        m_currState.matView = glm::lookAt(m_currState.position, m_currState.position + GetForwardDir(), GetYDir());
        m_currState.invMatView = glm::inverse(m_currState.matView);
    }


    void Camera::RecalcViewProjMatrix() noexcept
    {
        m_currState.matViewProj = m_currState.matProj * m_currState.matView;
        m_currState.invMatViewProj = glm::inverse(m_currState.matViewProj);
    }


    void Camera::RecalcFrustum() noexcept
    {
        m_currState.frustum.Construct(m_currState.matViewProj);
    }
}