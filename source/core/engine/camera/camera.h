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

        void PushPrevState() noexcept;
        void Update() noexcept;

        void SetPerspProjection(float fovY, float aspectRatio, float zNear, float zFar) noexcept;
        void SetOrthoProjection(float left, float right, float bottom, float top, float zNear, float zFar) noexcept;

        // This functions switch camera to perspective projection
        void SetFovY(float radians) noexcept;
        void SetAspectRatio(float aspect) noexcept;
        void SetAspectRatio(uint32_t width, uint32_t height) noexcept;
        void SetZNear(float zNear) noexcept;
        void SetZFar(float zFar) noexcept;
        void SetZNearFar(float zNear, float zFar) noexcept;

        // This functions switch camera to ortho projection
        void SetOrthoLeft(float left) noexcept;
        void SetOrthoRight(float right) noexcept;
        void SetOrthoTop(float top) noexcept;
        void SetOrthoBottom(float bottom) noexcept;

        void Move(const glm::float3& offset) noexcept;
        void MoveAlongDir(const glm::float3& dir, float distance) noexcept;

        void SetLookAt(const glm::float3& target, const glm::float3& up) noexcept;
        void SetRotation(const glm::quat& rotation) noexcept;
        void SetPosition(const glm::float3& position) noexcept;
        void SetTransform(const glm::float4x4& transform) noexcept;

        float GetFovY() const noexcept  { return m_currState.fovY; }
        float GetFovYPrev() const noexcept  { return m_prevState.fovY; }
        
        float GetAspectRatio() const noexcept { return m_currState.aspectRatio; }
        float GetAspectRatioPrev() const noexcept { return m_prevState.aspectRatio; }
        
        float GetZNear() const noexcept { return m_currState.zNear; }
        float GetZNearPrev() const noexcept { return m_prevState.zNear; }
        
        float GetZFar() const noexcept { return m_currState.zFar; }
        float GetZFarPrev() const noexcept { return m_prevState.zFar; }

        float GetOrthoLeft() const noexcept { return m_currState.left; }
        float GetOrthoLeftPrev() const noexcept { return m_prevState.left; }
        
        float GetOrthoRight() const noexcept { return m_currState.right; }
        float GetOrthoRightPrev() const noexcept { return m_prevState.right; }
        
        float GetOrthoTop() const noexcept { return m_currState.top; }
        float GetOrthoTopPrev() const noexcept { return m_prevState.top; }
        
        float GetOrthoBottom() const noexcept { return m_currState.bottom; }
        float GetOrthoBottomPrev() const noexcept { return m_prevState.bottom; }

        glm::float3 GetXDir() const noexcept { return glm::normalize(m_currState.rotation * M3D_AXIS_X); }
        glm::float3 GetXDirPrev() const noexcept { return glm::normalize(m_prevState.rotation * M3D_AXIS_X); }
        
        glm::float3 GetYDir() const noexcept { return glm::normalize(m_currState.rotation * M3D_AXIS_Y); }
        glm::float3 GetYDirPrev() const noexcept { return glm::normalize(m_prevState.rotation * M3D_AXIS_Y); }
        
        glm::float3 GetZDir() const noexcept { return glm::normalize(m_currState.rotation * M3D_AXIS_Z); }
        glm::float3 GetZDirPrev() const noexcept { return glm::normalize(m_prevState.rotation * M3D_AXIS_Z); }

        glm::float3 GetForwardDir() const noexcept { return -GetZDir(); }
        glm::float3 GetForwardDirPrev() const noexcept { return -GetZDirPrev(); }

        glm::float3 GetPitchYawRollRadians() const noexcept { return glm::eulerAngles(m_currState.rotation); }
        glm::float3 GetPitchYawRollRadiansPrev() const noexcept { return glm::eulerAngles(m_prevState.rotation); }
        
        glm::float3 GetPitchYawRollDegrees() const noexcept { return glm::degrees(GetPitchYawRollRadians()); }
        glm::float3 GetPitchYawRollDegreesPrev() const noexcept { return glm::degrees(GetPitchYawRollRadiansPrev()); }

        const glm::quat& GetRotation() const noexcept { return m_currState.rotation; }
        const glm::quat& GetRotationPrev() const noexcept { return m_prevState.rotation; }

        const glm::float3& GetPosition() const noexcept { return m_currState.position; }
        const glm::float3& GetPositionPrev() const noexcept { return m_prevState.position; }

        const glm::float4x4& GetViewMatrix() const noexcept { return m_currState.matView; }
        const glm::float4x4& GetViewMatrixPrev() const noexcept { return m_prevState.matView; }
        
        const glm::float4x4& GetProjMatrix() const noexcept { return m_currState.matProj; }
        const glm::float4x4& GetProjMatrixPrev() const noexcept { return m_prevState.matProj; }
        
        const glm::float4x4& GetViewProjMatrix() const noexcept { return m_currState.matViewProj; }
        const glm::float4x4& GetViewProjMatrixPrev() const noexcept { return m_prevState.matViewProj; }

        const glm::float4x4& GetInvViewMatrix() const noexcept { return m_currState.invMatView; }
        const glm::float4x4& GetInvViewMatrixPrev() const noexcept { return m_prevState.invMatView; }
        
        const glm::float4x4& GetInvProjMatrix() const noexcept { return m_currState.invMatProj; }
        const glm::float4x4& GetInvProjMatrixPrev() const noexcept { return m_prevState.invMatProj; }
        
        const glm::float4x4& GetInvViewProjMatrix() const noexcept { return m_currState.invMatViewProj; }
        const glm::float4x4& GetInvViewProjMatrixPrev() const noexcept { return m_prevState.invMatViewProj; }

        const math::Frustum& GetFrustum() const noexcept { return m_currState.frustum; }
        const math::Frustum& GetFrustumPrev() const noexcept { return m_prevState.frustum; }

        bool IsPerspProj() const noexcept { return !IsOrthoProj(); }
        bool IsPerspProjPrev() const noexcept { return !IsOrthoProjPrev(); }
        
        bool IsOrthoProj() const noexcept { return GetFlags().test(FLAG_IS_ORTHO_PROJ); }
        bool IsOrthoProjPrev() const noexcept { return GetFlagsPrev().test(FLAG_IS_ORTHO_PROJ); }

        bool IsProjMatrixRecalcRequested() const noexcept { return GetFlags().test(FLAG_NEED_RECALC_PROJ_MAT); }
        bool IsViewMatrixRecalcRequested() const noexcept { return GetFlags().test(FLAG_NEED_RECALC_VIEW_MAT); }
        
        bool IsNeedRecalcViewProjMatrix() const noexcept { return IsViewMatrixRecalcRequested() || IsProjMatrixRecalcRequested(); }

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
    
    private:
        CameraFlags& GetFlags() { return m_currState.flags; } 
        const CameraFlags& GetFlags() const { return m_currState.flags; } 

        CameraFlags& GetFlagsPrev() { return m_prevState.flags; } 
        const CameraFlags& GetFlagsPrev() const { return m_prevState.flags; } 

        void RequestRecalcProjMatrix() noexcept
        {
            GetFlags().set(FLAG_NEED_RECALC_PROJ_MAT);
            RequestRecalcFrustum();
        }

        void RequestRecalcViewMatrix() noexcept
        {
            GetFlags().set(FLAG_NEED_RECALC_VIEW_MAT);
            RequestRecalcFrustum();
        }

        void RequestRecalcFrustum() noexcept { GetFlags().set(FLAG_NEED_RECALC_FRUSTUM); }

        void ClearProjRecalcRequest() noexcept { GetFlags().reset(FLAG_NEED_RECALC_PROJ_MAT); }
        void ClearViewMatrixRecalcRequest() noexcept { GetFlags().reset(FLAG_NEED_RECALC_VIEW_MAT); }
        void ClearFrustumRecalcRequest() noexcept { GetFlags().reset(FLAG_NEED_RECALC_FRUSTUM); }

        void RecalcProjMatrix() noexcept;
        void RecalcViewMatrix() noexcept;
        void RecalcViewProjMatrix() noexcept;
        void RecalcFrustum() noexcept;

    private:
        struct State
        {
            math::Frustum frustum;
            
            glm::float4x4 matViewProj = M3D_MAT4X4_IDENTITY;
            glm::float4x4 matProj     = M3D_MAT4X4_IDENTITY;
            glm::float4x4 matView     = M3D_MAT4X4_IDENTITY;

            glm::float4x4 invMatViewProj = M3D_MAT4X4_IDENTITY;
            glm::float4x4 invMatProj     = M3D_MAT4X4_IDENTITY;
            glm::float4x4 invMatView     = M3D_MAT4X4_IDENTITY;

            glm::quat rotation   = M3D_QUAT_IDENTITY;
            glm::float3 position = ZEROF3;

            // perspective
            float fovY = 0.f;
            float aspectRatio = 0.f;

            // ortho
            float left = 0.f;
            float right = 0.f;
            float top = 0.f;
            float bottom = 0.f;

            float zNear = 0.f;
            float zFar = 0.f;

            CameraFlags flags = {};
        };

        State m_prevState;
        State m_currState;
    };
}