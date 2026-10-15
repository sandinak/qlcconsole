#version 440
// The light buffer, rolled off as ZRaster::lightImage() does it: linear for a
// lone beam, easing toward a ceiling short of white as light piles up. The
// buffer holds light on a 0..1-per-255 scale; the result is premultiplied
// with alpha covering the brightest channel, for a Screen composite.
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D lightTex;
layout(std140, binding = 0) uniform buf {
    mat4 unused;
    vec4 flip;          // x: 1 when the texture's rows run bottom-up
};
void main()
{
    vec2 uv = v_uv;
    if (flip.x > 0.5)
        uv.y = 1.0 - uv.y;
    vec3 v = max(texture(lightTex, uv).rgb * 255.0, vec3(0.0));
    vec3 rolled = floor(225.0 * (vec3(1.0) - exp(-v / 225.0))) / 255.0;
    float a = max(rolled.r, max(rolled.g, rolled.b));
    fragColor = vec4(rolled, a);
}
