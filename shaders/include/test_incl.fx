#ifndef TEST_H
#define TEST_H

struct TestVertex
{
    vec2 ndc;
    vec2 uv;
    vec4 color;
};

const TestVertex TEST_VERTECIES[3] = {
    TestVertex(vec2(-0.5f, 0.5f), vec2(0.f, 0.f), vec4(1.f, 0.f, 0.f, 1.f)),
    TestVertex(vec2( 0.5f, 0.5f), vec2(1.f, 0.f), vec4(0.f, 1.f, 0.f, 1.f)),
    TestVertex(vec2( 0.f, -0.5f), vec2(0.5f, 1.f), vec4(0.f, 0.f, 1.f, 1.f)),
};

#endif