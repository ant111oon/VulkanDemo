#ifndef SKYBOX_VS_IN_H
#define SKYBOX_VS_IN_H

#include "registers/common_registers.slang"
#include "registers/skybox_registers.slang"

#include "common/math/half.slang"


namespace utils
{
    static const uint SKYBOX_VERT_SIZE_F = 3;

    static const float SKYBOX_VERTS[36 * SKYBOX_VERT_SIZE_F] = 
    {
        // +X face
        1.0f, -1.0f, -1.0f,
        1.0f,  1.0f, -1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f, -1.0f,  1.0f,

        // -X face
        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,

        // +Y face
        -1.0f, 1.0f, -1.0f,
        -1.0f, 1.0f,  1.0f,
         1.0f, 1.0f,  1.0f,
        -1.0f, 1.0f, -1.0f,
         1.0f, 1.0f,  1.0f,
         1.0f, 1.0f, -1.0f,

        // -Y face
        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,

        // +Z face
        -1.0f, -1.0f, 1.0f,
         1.0f, -1.0f, 1.0f,
         1.0f,  1.0f, 1.0f,
        -1.0f, -1.0f, 1.0f,
         1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f, 1.0f,

        // -Z face
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
    };
}


struct SKYBOX_INPUT_DATA
{
    float3 position;
};


SKYBOX_INPUT_DATA PrepareSkyboxVSInputData(uint vertID)
{
    SKYBOX_INPUT_DATA inputData = (SKYBOX_INPUT_DATA)0;

    const uint offset = vertID * utils::SKYBOX_VERT_SIZE_F;

    inputData.position.x = utils::SKYBOX_VERTS[offset + 0];
    inputData.position.y = utils::SKYBOX_VERTS[offset + 1];
    inputData.position.z = utils::SKYBOX_VERTS[offset + 2];

    return inputData;
}

#endif