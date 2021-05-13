#include "math/constants.glsl"

// computes the color to visualize the complex value
vec3 ccolor(vec2 y, bool checkerboard, bool origin, bool unit_circle)
{
	if(origin &&
		distance(y, vec2(0.0)) <= 0.1)
	{
		return vec3(y.x*y.y > 0.0 ? 1.0 : 0.0);
	}

	float dc = distance(y, vec2(0.0));
	if(unit_circle &&
		distance(1.0, dc) <= 0.1 &&
		distance(y, vec2(+1.0,  0.0)) > 0.1 &&
		distance(y, vec2(-1.0,  0.0)) > 0.1 &&
		distance(y, vec2( 0.0, +1.0)) > 0.1 &&
		distance(y, vec2( 0.0, -1.0)) > 0.1)
	{
		return vec3(dc<1.0 ? 0.5 : 0.75);
	}

	if(checkerboard &&
		( int(floor(y.x*10))%2 == 1 ^^ int(floor(y.y*10))%2 == 0 ) )
	{
		return vec3(0.0);
	}

	vec3 c = vec3(atan(y.y, y.x) / (2*PI) + 0.5, 1.0, 1.0/log(length(y)+E));

	// HSV to RGB
	vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
	return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 ccolor(vec2 y)
{
	return ccolor(y, true, true, true);
}

// product of two complex numbers
vec2 cmul(vec2 self, vec2 other) {
	return vec2(self.x * other.x - self.y * other.y,
				self.x * other.y + self.y * other.x);
}

// computes c^e / e!
vec2 cfpow(vec2 c, int e)
{
	vec2 r = vec2(1.0, 0.0);
	for(int i=1; i<=e; i++)
	{
		r = cmul(r, (c / float(i)));
	}
	return r;
}

// computes c^e
vec2 cpow(vec2 c, int e)
{
	vec2 r = vec2(1.0, 0.0);
	for(int i=1; i<=e; i++)
	{
		r = cmul(r, c);
	}
	return r;
}
