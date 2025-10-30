#include "pch.h"

#include "camera.h"


namespace eng
{
    void Camera::Update()
    {
        position += glm::vec3(GetRotationMatrix() * glm::vec4(velocity, 0.f));
    }
    
    
    glm::mat4 Camera::GetViewMatrix() const
    {
        const glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
        const glm::mat4 cameraRotation = GetRotationMatrix();
        
        return glm::inverse(cameraTranslation * cameraRotation);
    }
    
    
    glm::mat4 Camera::GetRotationMatrix() const
    {
        const glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3 { 1.f, 0.f, 0.f });
        const glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3 { 0.f, -1.f, 0.f });
    
        return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
    }
}