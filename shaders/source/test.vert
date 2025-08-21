#version 450

#include "test_incl.fx"


layout(location = 0) out vec2 oUV;
layout(location = 1) out vec4 oColor;


void main()
{
    TestVertex inputData = TEST_VERTECIES[gl_VertexIndex];

    oUV = inputData.uv;
    oColor = inputData.color;
    gl_Position = vec4(inputData.ndc, 0.5f, 1.f);
}