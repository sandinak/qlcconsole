#version 440
// The rig view's geometry, already projected to the screen on the CPU: a
// screen position, a depth (0 nearest .. 1 farthest) and a premultiplied
// colour per corner (RhiRaster).
layout(location = 0) in vec4 position;
layout(location = 1) in vec4 colour;
layout(location = 0) out vec4 v_colour;
layout(std140, binding = 0) uniform buf {
    mat4 mvp;
};
void main()
{
    v_colour = colour;
    gl_Position = mvp * vec4(position.xyz, 1.0);
}
