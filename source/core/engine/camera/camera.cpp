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
        m_prevState = {};
        m_currState = {};
    }


    void Camera::SetPerspProjection(float fovY, float aspectRatio, float zNear, float zFar) noexcept
    {
        SetFovY(fovY);
        SetAspectRatio(aspectRatio);
        SetZNear(zNear);
        SetZFar(zFar);

        RequestRecalcProjMatrix();
    }


    void Camera::SetOrthoProjection(float left, float right, float bottom, float top, float zNear, float zFar) noexcept
    {
        SwitchToOrthoProjection();

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
        CORE_ASSERT(IsFovValid(radians));

        m_prevState.fovY = m_currState.fovY;
        m_currState.fovY = radians;

        SwitchToPerspProjection();
        RequestRecalcProjMatrix();
    }


    void Camera::SetAspectRatio(float aspect) noexcept
    {
        CORE_ASSERT_MSG(aspect > M3D_EPS, "Aspect can't be less or equal to zero");

        m_prevState.aspectRatio = m_currState.aspectRatio;
        m_currState.aspectRatio = aspect;

        SwitchToPerspProjection();
        RequestRecalcProjMatrix();
    }


    void Camera::SetAspectRatio(uint32_t width, uint32_t height) noexcept
    {
        CORE_ASSERT_MSG(height != 0, "Height can't be equal to zero");

        const float aspectRatio = float(width) / float(height);
        SetAspectRatio(aspectRatio);
    }


    void Camera::SetZNear(float zNear) noexcept
    {
        const float currZNear = m_currState.zNear;
        const float currZFar = m_currState.zFar;

        if (!math::IsEqual(currZNear, zNear)) {
            CORE_ASSERT_MSG(abs(currZFar - zNear) > M3D_EPS, "Can't set Z Near equal to Z Far");
        
            m_prevState.zNear = m_currState.zNear;
            m_currState.zNear = zNear;
            
            RequestRecalcProjMatrix();
        }
    }


    void Camera::SetZFar(float zFar) noexcept
    {
        const float currZNear = m_currState.zNear;
        const float currZFar = m_currState.zFar;

        if (!math::IsEqual(currZFar, zFar)) {
            CORE_ASSERT_MSG(abs(zFar - currZNear) > M3D_EPS, "Can't set Z Far equal to Z Near");
        
            m_prevState.zFar = m_currState.zFar;
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
        const float currRight = m_currState.right;

        CORE_ASSERT_MSG(abs(currRight - left) > M3D_EPS, "Can't set left equal to right");
        
        m_prevState.left = m_currState.left;
        m_currState.left = left;

        SwitchToOrthoProjection();
        RequestRecalcProjMatrix();
    }


    void Camera::SetOrthoRight(float right) noexcept
    {
        const float currLeft = m_currState.left;

        CORE_ASSERT_MSG(abs(right - currLeft) > M3D_EPS, "Can't set right equal to left");
        
        m_prevState.right = m_currState.right;
        m_currState.right = right;

        SwitchToOrthoProjection();
        RequestRecalcProjMatrix();
    }


    void Camera::SetOrthoTop(float top) noexcept
    {
        const float currBottom = m_currState.bottom;

        CORE_ASSERT_MSG(abs(top - currBottom) > M3D_EPS, "Can't set top equal to bottom");
    
        m_prevState.top = m_currState.top;
        m_currState.top = top;

        SwitchToOrthoProjection();
        RequestRecalcProjMatrix();
    }


    void Camera::SetOrthoBottom(float bottom) noexcept
    {
        const float currTop = m_currState.top;

        CORE_ASSERT_MSG(abs(currTop - bottom) > M3D_EPS, "Can't set bottom equal to top");
    
        m_prevState.bottom = m_currState.bottom;
        m_currState.bottom = bottom;

        SwitchToOrthoProjection();
        RequestRecalcProjMatrix();
    }


    void Camera::Move(const glm::float3& offset) noexcept
    {
        if (!math::IsZero(offset)) {
            m_prevState.position = m_currState.position;
            m_currState.position += offset;

            RequestRecalcViewMatrix();
        }
    }


    void Camera::MoveAlongDir(const glm::float3& dir, float distance) noexcept
    {
        if (!math::IsZero(distance)) {
            CORE_ASSERT_MSG(math::IsNormalized(dir), "Direction must be a normalized vector");
        
            m_prevState.position = m_currState.position;
            m_currState.position += dir * distance;

            RequestRecalcViewMatrix();
        }
    }


    void Camera::SetLookAt(const glm::float3& target, const glm::float3& up) noexcept
    {
        const glm::float3& currPos = m_currState.position;

        CORE_ASSERT(!math::IsEqual(currPos, target));

        const glm::float3 direction = glm::normalize(target - currPos);
        CORE_ASSERT(!math::IsEqual(glm::abs(glm::dot(target, up)), 1.f));

        SetRotation(glm::quatLookAt(direction, up));
    }


    void Camera::SetRotation(const glm::quat& rotation) noexcept
    {
        CORE_ASSERT_MSG(math::IsNormalized(rotation), "Rotation quaternion must be normalized");

        const glm::quat& currRot = m_currState.rotation;

        if (!math::IsEqual(currRot, rotation)) {
            m_prevState.rotation = m_currState.rotation;
            m_currState.rotation = rotation;

            RequestRecalcViewMatrix();
        }
    }


    void Camera::SetPosition(const glm::float3& position) noexcept
    {
        if (!math::IsEqual(m_currState.position, position)) {
            m_prevState.position = m_currState.position;
            m_currState.position += position;

            RequestRecalcViewMatrix();
        }
    }


    void Camera::SetTransform(const glm::float4x4& transform) noexcept
    {
        SetPosition(math::GetTranslation(transform));
        SetRotation(math::GetRotation(transform));
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

        m_prevState.matProj = m_currState.matProj;
        m_prevState.invMatProj = m_currState.invMatProj;

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
        m_prevState.matView = m_currState.matView;
        m_prevState.invMatView = m_currState.invMatView;

        m_currState.matView = glm::lookAt(m_currState.position, m_currState.position + GetForwardDir(), GetYDir());
        m_currState.invMatView = glm::inverse(m_currState.matView);
    }


    void Camera::RecalcViewProjMatrix() noexcept
    {
        m_prevState.matViewProj = m_currState.matViewProj;
        m_prevState.invMatViewProj = m_currState.invMatViewProj;

        m_currState.matViewProj = m_currState.matProj * m_currState.matView;
        m_currState.invMatViewProj = glm::inverse(m_currState.matViewProj);
    }


    void Camera::RecalcFrustum() noexcept
    {
        m_prevState.frustum = m_currState.frustum;
        m_currState.frustum.Construct(m_currState.matViewProj);
    }


    void Camera::SwitchToPerspProjection() noexcept
    {
        GetFlagsPrev().set(FLAG_IS_ORTHO_PROJ, IsOrthoProj());
        GetFlags().set(FLAG_IS_ORTHO_PROJ, false);
    }


    void Camera::SwitchToOrthoProjection() noexcept
    {
        GetFlagsPrev().set(FLAG_IS_ORTHO_PROJ, IsOrthoProj());
        GetFlags().set(FLAG_IS_ORTHO_PROJ, true);
    }
}