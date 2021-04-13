#version 450

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D tex;

const vec3 light = normalize(vec3(1.0, -1.0, 1.0));

void main()
{
	vec3 normal = normalize(inNormal);

	float diff = max(dot(light, normal), 0.0);

	vec4 color = texture(tex, inTexCoord);
	outColor = color * vec4(vec3(diff), 1.0);
}
