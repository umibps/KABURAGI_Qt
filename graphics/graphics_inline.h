#ifndef _INCLUDED_GRAPHICS_INLINE_H_
#define _INCLUDED_GRAPHICS_INLINE_H_

#include "graphics.h"
#include "graphics_types.h"
#include "graphics_private.h"

#define GRAPHICS_MAGIC_NUMBER_FIXED_16_16 (103079215104.0)

static INLINE GRAPHICS_SURFACE* GraphicsPatternGetSource(
	const GRAPHICS_SURFACE_PATTERN* pattern,
	GRAPHICS_RECTANGLE_INT* extents
)
{
	return GraphicsSurfaceGetSource(pattern->surface, extents);
}

static INLINE int GraphicsRectangleContainsRectangle(const GRAPHICS_RECTANGLE_INT* a, const GRAPHICS_RECTANGLE_INT* b)
{
	return (a->x <= b->x && a->x + (int)a->width >= b->x + (int)b->width
		&& a->y <= b->y && a->y + (int)a->height >= b->y + (int)b->height);
}

static INLINE int GraphicsMatrixIsTranslation(const GRAPHICS_MATRIX* matrix)
{
	return (matrix->xx == 1.0 && matrix->yx == 0.0 && matrix->xy == 0.0 && matrix->yy == 1.0);
}

static INLINE int GraphicsFixedIntegerPart(GRAPHICS_FLOAT_FIXED f)
{
	return f >> GRAPHICS_FIXED_FRAC_BITS;
}

static INLINE int GraphicsFixedFractionalPart(GRAPHICS_FLOAT_FIXED f)
{
	return f & GRAPHICS_FIXED_FRAC_MASK;
}

static INLINE int GraphicsFixedIntegerRoundDown(GRAPHICS_FLOAT_FIXED f)
{
	return GraphicsFixedIntegerPart(f + GRAPHICS_FIXED_FRAC_MASK/2);
}

static INLINE GRAPHICS_FIXED_16_16 GraphicsFixed16_16FromFloat(FLOAT_T f)
{
	union{FLOAT_T f; int32 i[2];} u;
	u.f = f + GRAPHICS_MAGIC_NUMBER_FIXED_16_16;

#if defined(BIG_ENDIAN) && BIG_ENDIAN
	return u.i[1];
#else
	return u.i[0];
#endif
}

static INLINE GRAPHICS_FIXED_16_16 GraphicsFixedTo16_16(GRAPHICS_FLOAT_FIXED f)
{
#if (GRAPHICS_FIXED_FRAC_BITS == 16) // && (GRAPHICS_FIXED_BITS == 32)
	return f;
#else
	GRAPHICS_FIXED_16_16 x;

	if((f >> GRAPHICS_FIXED_FRAC_BITS) < INT16_MIN)
	{
		x = INT32_MIN;
	}
	else if((f >> GRAPHICS_FIXED_FRAC_BITS) > INT16_MAX)
	{
		x = INT32_MAX;
	}
	else
	{
		x = f << (16 - GRAPHICS_FIXED_FRAC_BITS);
	}

	return x;
#endif
}

static INLINE void GraphicsBoxFromIntegers(GRAPHICS_BOX* box, int x, int y, int width, int height)
{
	box->point1.x = GraphicsFixedFromInteger(x);
	box->point1.y = GraphicsFixedFromInteger(y);
	box->point2.x = GraphicsFixedFromInteger(x + width);
	box->point2.y = GraphicsFixedFromInteger(y + height);
}

static INLINE int GraphicsPatternIsOpaqueSolid(const GRAPHICS_PATTERN* pattern)
{
	GRAPHICS_SOLID_PATTERN *solid;

	if(pattern->type != GRAPHICS_PATTERN_TYPE_SOLID)
	{
		return FALSE;
	}

	solid = (GRAPHICS_SOLID_PATTERN*)pattern;

	return GRAPHICS_COLOR_IS_OPAQUE(&solid->color);
}

#define GRAPHICS_COLOR_FLOAT_TO_SHORT(d) ((FLOAT_T)(d) * 65535.0 + 0.5)

static INLINE GRAPHICS_POINT* GraphicsContourLastPoint(GRAPHICS_CONTOUR* contour)
{
	return &contour->tail->points[contour->tail->num_points-1];
}

#endif	// #ifndef _INCLUDED_GRAPHICS_INLINE_H_
