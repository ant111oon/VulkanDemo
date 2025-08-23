#version 450

#extension GL_EXT_buffer_reference : require


layout(buffer_reference, std430) readonly buffer VERTEX_DATA_REF
{ 
	vec4 DATA[];
};


layout(push_constant) uniform PER_DRAW_DATA
{
    VERTEX_DATA_REF VERTEXES;
};


struct INPUT_DATA
{
    vec2 position;
    vec2 uv;
    vec4 color;
};


const uint INPUT_DATA_SIZE_F4 = 2;


INPUT_DATA PrepareInputData(uint vertIdx)
{
    INPUT_DATA inputData;

    const uint offset = vertIdx * INPUT_DATA_SIZE_F4;

    inputData.position = VERTEXES.DATA[offset + 0].xy;
    inputData.uv       = VERTEXES.DATA[offset + 0].zw;
    inputData.color    = VERTEXES.DATA[offset + 1].rgba;

    return inputData;
}


layout(location = 0) out vec2 oUV;
layout(location = 1) out vec4 oColor;


void main()
{
    INPUT_DATA inputData = PrepareInputData(gl_VertexIndex);

    oUV = inputData.uv;
    oColor = inputData.color;
    gl_Position = vec4(inputData.position, 0.5f, 1.f);
}