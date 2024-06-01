#version 450

layout(binding = 1) uniform UBO
{
	float time;
	float random;
} constants;
layout(location = 0) in vec3 vEC;
layout(location = 0) out vec4 FragColor;

void main()
{
	vec3 x = dFdx(vEC);
	vec3 y = dFdy(vEC);
	vec3 normal = normalize(cross(x, y));
	float c = 1.0 - dot(normal, vec3(0.0, 0.0, 1.0));
	c = (1.0 - cos(c * c)) / 3.0;
	FragColor = vec4(c, c, c, 1.0);
}
