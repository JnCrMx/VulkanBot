#version 450

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec4 outColor;

const vec3 color = vec3(0.7, 0.7, 0.7);
const vec3 light = normalize(vec3(1.0, -1.0, 1.0));

void main()
{
	vec3 normal = normalize(inNormal);

	float diff = max(dot(light, normal), 0.0);
	outColor = vec4(color * diff, 1.0);
}
