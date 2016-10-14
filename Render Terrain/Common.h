/*
Common.h

Author:			Chris Serson
Last Edited:	October 14, 2016

Description:	Common constants, functions, etc.
*/
#pragma once

// linearly interpolate between a and b by amount t.
inline float lerp(float a, float b, float t) {
	return a + t * (b - a);
}

// bilinear interpolation between four values.
// performed as linear interpolation between a and b by u,
// then between c and d by u,
// then between ab and cd by v.
inline float bilerp(float a, float b, float c, float d, float u, float v) {
	return lerp(lerp(a, b, u), lerp(c, d, u), v);
}