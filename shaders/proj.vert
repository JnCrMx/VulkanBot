#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outNormal;

void main()
{
	mat4 projection = mat4(  2.414,  0.000,  0.000,  0.000,
							 0.000,  2.414,  0.000,  0.000,
							 0.000,  0.000, -1.020,  4.899,
							 0.000,  0.000, -1.000,  5.000);
	vec4 projected = vec4(inPosition, 1.0) * projection;
	
	gl_Position = projected;
	outTexCoord = inTexCoord;
	outNormal = inNormal;
}
