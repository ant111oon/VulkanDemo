#pragma once

#include "core/math/math.h"
#include "core/math/frustum.h"


#include <bitset>


namespace eng
{
    class Camera
    {
        friend class CameraManager;

    public:
        Camera() = default;
        ~Camera();

        void Destroy() noexcept;

        void SetPerspProjection(float fovYDeg, float aspectRatio, float zNear, float zFar) noexcept;
        void SetOrthoProjection(float left, float right, float top, float bottom, float zNear, float zFar) noexcept;

        void SetFovY(float degrees) noexcept;
        void SetAspectRatio(float aspect) noexcept;
        void SetAspectRatio(uint32_t width, uint32_t height) noexcept;
        void SetZNear(float zNear) noexcept;
        void SetZFar(float zFar) noexcept;

        void SetOrthoLeft(float left) noexcept;
        void SetOrthoRight(float right) noexcept;
        void SetOrthoTop(float top) noexcept;
        void SetOrthoBottom(float bottom) noexcept;

        void Move(const glm::vec3& offset) noexcept;
        void MoveAlongDir(const glm::vec3& dir, float distance) noexcept;

        void SetRotation(const glm::quat& rotation) noexcept;
        void SetPosition(const glm::vec3& position) noexcept;

        float GetFovDeg() const noexcept { return m_fovDeg; }
        float GetAspectRatio() const noexcept { return m_aspectRatio; }
        float GetZNear() const noexcept { return m_zNear; }
        float GetZFar() const noexcept { return m_zFar; }

        float GetOrthoLeft() const noexcept { return m_left; }
        float GetOrthoRight() const noexcept { return m_right; }
        float GetOrthoTop() const noexcept { return m_top; }
        float GetOrthoBottom() const noexcept { return m_bottom; }

        glm::vec3 GetXDir() const noexcept { return glm::vec3(m_matView[0]); }
        glm::vec3 GetYDir() const noexcept { return glm::vec3(m_matView[1]); }
        glm::vec3 GetZDir() const noexcept { return glm::vec3(m_matView[2]); }

        glm::vec3 GetForwardDir() const noexcept { return -GetZDir(); }

        glm::vec3 GetPitchYawRollRadians() const noexcept { return glm::eulerAngles(m_rotation); }
        glm::vec3 GetPitchYawRollDegrees() const noexcept { return glm::degrees(GetPitchYawRollRadians()); }
        
        const glm::quat& GetRotation() const noexcept { return m_rotation; }
        const glm::vec3& GetPosition() const noexcept { return m_position; }

        const glm::mat4x4& GetViewMatrix() const noexcept { return m_matView; }
        const glm::mat4x4& GetProjMatrix() const noexcept { return m_matProj; }
        const glm::mat4x4& GetViewProjMatrix() const noexcept { return m_matViewProj; }

        const math::Frustum& GetFrustum() const noexcept { return m_frustum; }

        bool IsPerspProj() const noexcept { return !IsOrthoProj(); }
        bool IsOrthoProj() const noexcept { return m_flags.test(CameraFlagBits::FLAG_IS_ORTHO_PROJ); }

        bool IsProjMatrixRecalcRequested() const noexcept { return m_flags.test(CameraFlagBits::FLAG_NEED_RECALC_PROJ_MAT); }
        bool IsViewMatrixRecalcRequested() const noexcept { return m_flags.test(CameraFlagBits::FLAG_NEED_RECALC_VIEW_MAT); }
        
        bool IsNeedRecalcViewProjMatrix() const noexcept { return IsViewMatrixRecalcRequested() || IsProjMatrixRecalcRequested(); }

        void Update() noexcept;

    private:
        void RequestRecalcProjMatrix() noexcept
        { 
            m_flags.set(CameraFlagBits::FLAG_NEED_RECALC_PROJ_MAT);
            RequestRecalcFrustum();
        }

        void RequestRecalcViewMatrix() noexcept
        {
            m_flags.set(CameraFlagBits::FLAG_NEED_RECALC_VIEW_MAT);
            RequestRecalcFrustum();
        }

        void RequestRecalcFrustum() noexcept { m_flags.set(CameraFlagBits::FLAG_NEED_RECALC_FRUSTUM); }

        void ClearProjRecalcRequest() noexcept { m_flags.reset(CameraFlagBits::FLAG_NEED_RECALC_PROJ_MAT); }
        void ClearViewMatrixRecalcRequest() noexcept { m_flags.reset(CameraFlagBits::FLAG_NEED_RECALC_VIEW_MAT); }
        void ClearFrustumRecalcRequest() noexcept { m_flags.reset(CameraFlagBits::FLAG_NEED_RECALC_FRUSTUM); }

        void RecalcProjMatrix() noexcept;
        void RecalcViewMatrix() noexcept;
        void RecalcViewProjMatrix() noexcept;
        void RecalcFrustum() noexcept;

    private:
        enum CameraFlagBits
        {
            FLAG_IS_ORTHO_PROJ,
            FLAG_NEED_RECALC_PROJ_MAT,
            FLAG_NEED_RECALC_VIEW_MAT,
            FLAG_NEED_RECALC_FRUSTUM,

            FLAG_COUNT,
        };

        using CameraFlags = std::bitset<16>;
        static_assert(CameraFlagBits::FLAG_COUNT < CameraFlags().size());

        math::Frustum m_frustum;

        glm::mat4x4 m_matViewProj = M3D_MAT4_IDENTITY;
        glm::mat4x4 m_matProj     = M3D_MAT4_IDENTITY;
        glm::mat4x4 m_matView     = M3D_MAT4_IDENTITY;

        glm::quat m_rotation = M3D_QUAT_IDENTITY;
        glm::vec3 m_position = M3D_ZEROF3;

        // perspective
        float m_fovDeg = 0.f;
        float m_aspectRatio = 1.f;

        // ortho
        float m_left = 0.f;
        float m_right = 0.f;
        float m_top = 0.f;
        float m_bottom = 0.f;

        float m_zNear = 0.f;
        float m_zFar = 0.f;

        CameraFlags m_flags = {};
    };
}