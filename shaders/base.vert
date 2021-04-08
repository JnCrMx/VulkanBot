#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec2 outCoord;

void main()
{
	gl_Position = vec4(inPosition, 1.0);
	outCoord = (inPosition.xy + vec2(1.0)) / 2.0;
}
