#version 450

layout(binding = 1) uniform UBO
{
    float time;
    float random;
} constants;
layout(binding = 0) uniform sampler2D tex;
layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main()
{
  vec3 c = vec3(inTexCoord.x, 1.0, 1.0);
  c.x = fract(c.x + constants.time);
  outColor = texture(tex, inTexCoord) * vec4(hsv2rgb(c), 1.0);
}
