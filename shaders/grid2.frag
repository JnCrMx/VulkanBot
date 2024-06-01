#version 450
layout(binding = 0) uniform sampler2D tex;
layout(location = 0) in vec2 inCoord;
layout(location = 0) out vec4 outColor;

void main()
{
	vec2 coord = inCoord * 10;
	vec2 d = abs(round(coord)-coord);

	vec3 color = vec3(0.0);
	if(d.x < 0.1)
	{
		color += vec3(1.0, 0.0, 0.0) * (0.1-d.x)*10;
	}
	if(d.y < 0.1)
	{
		color = vec3(0.0, 1.0, 0.0) * (0.1-d.y)*10;
	}
	outColor = vec4(color, 1.0);
}
