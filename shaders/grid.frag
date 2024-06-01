#version 450
layout(binding = 0) uniform sampler2D tex;
layout(location = 0) in vec2 inCoord;
layout(location = 0) out vec4 outColor;

void main()
{
	vec2 coord = inCoord * 10.0 - vec2(0.5);
	vec2 d = abs(floor(coord)-coord);

	if(d.x < 0.01)
	{
		outColor = vec4(1.0, 0.0, 0.0, 1.0);
	}
	else if(d.y < 0.01)
	{
		outColor = vec4(0.0, 1.0, 0.0, 1.0);
	}
	else
	{
		outColor = vec4(0.0, 0.0, 0.0, 1.0);
	}
}
