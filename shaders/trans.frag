#version 450

layout(binding = 1) uniform UBO
{
	float time;
	float random;
} constants;
layout(binding = 0) uniform sampler2D tex;
layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

vec3 transcolors[5] = vec3[](
	vec3(0.357, 0.808, 0.961),
	vec3(0.961, 0.659, 0.706),
	vec3(1.0, 1.0, 1.0),
	vec3(0.961, 0.659, 0.706),
	vec3(0.357, 0.808, 0.961)
);

vec3 transcolor(float x)
{
	float px = x*transcolors.length();
	int a = int(floor(px))%transcolors.length();
	int b = int(ceil(px))%transcolors.length();
	return mix(transcolors[a], transcolors[b], fract(px));
}

void main()
{
	float x = fract(inTexCoord.x + constants.time);
	outColor = 0.5 * texture(tex, inTexCoord) + 0.6 * vec4(transcolor(x), 1.0);
}
