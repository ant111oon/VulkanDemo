#pragma once

#include "core/math/math.h"


namespace eng
{
    struct Camera
    {
        void Update();
    
        glm::mat4 GetViewMatrix() const;
        glm::mat4 GetRotationMatrix() const;
    
        glm::vec3 velocity;
        glm::vec3 position;
        float pitch = 0.f;
        float yaw = 0.f;
    };
}