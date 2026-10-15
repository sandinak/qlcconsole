#version 440
// A triangle that covers the screen, for a pass over the light buffer. Its
// corners come from the vertex buffer (0,0), (2,0), (0,2): GLSL 1.20, which
// some OpenGL drivers still get, has neither gl_VertexID nor bit operations.
layout(location = 0) in vec4 position;
layout(location = 0) out vec2 v_uv;
void main()
{
    vec2 p = position.xy;
    v_uv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
