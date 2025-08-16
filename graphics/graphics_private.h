#ifndef _INCLUDE_GRAPHICS_PRIVATE_H_
#define _INCLUDE_GRAPHICS_PRIVATE_H_

#include <math.h>
#include <limits.h>
#include "graphics_compositor.h"
#include "graphics_types.h"
#include "graphics_surface.h"
#include "../pixel_manipulate/pixel_manipulate.h"

#if defined(__GNUC__) && (__GNUC__ > 2) && defined(__OPTIMIZE__)
# define LIKELY(expr) (__builtin_expect (!!(expr), 1))
# define UNLIKELY(expr) (__builtin_expect (!!(expr), 0))
#else
# define LIKELY(expr) (expr)
# define UNLIKELY(expr) (expr)
#endif

#ifndef ASSERT
# if defined(_DEBUG)
#  include <assert.h>
#  define ASSERT(expr) assert(expr)
# else
#  define ASSERT(expr)
# endif
#endif

#define IS_FINITE(x) ((x) * (x) >= 0.)

#define GRAPHICS_REFERENCE_COUNT_DEC_AND_TEST(SURFACE) ((SURFACE)->reference_count == 1)
#define GRAPHICS_REFERENCE_COUNT_VALID_VALUE (-1)
#define GRAPHICS_REFERENCE_COUNT_VALID {GRAPHICS_REFERENCE_COUNT_IS_VALID_VALUE}
#define GRAPHICS_REFERENCE_COUNT_GET_VALUE(SURFACE) (((SURFACE))->reference_count)
#define GRAPHICS_REFERENCE_COUNT_IS_INVALID(SURFACE) (GRAPHICS_REFERENCE_COUNT_GET_VALUE((SURFACE)) > 0)
#define GRAPHICS_REFERENCE_COUNT_HAS_REFERENCE(SURFACE) (GRAPHICS_REFERENCE_COUNT_GET_VALUE((SURFACE)) > 0)

#define GRAPHICS_COLOR_ALPHA_SHORT_IS_OPAQUE(alpha) ((alpha) >= 0xFF00)
#define GRAPHICS_COLOR_IS_OPAQUE(color) GRAPHICS_COLOR_ALPHA_SHORT_IS_OPAQUE((color)->alpha_short)

#define GRAPHICS_RECTANGLE_INT_MAX (INT_MAX >> 8)
#define GRAPHICS_RECTANGLE_INT_MIN (INT_MIN >> 8)

#define GRAPHICS_FIXED_BITS (sizeof(int32) * 8)
#define GRAPHICS_FIXED_FRAC_BITS 8
#define GRAPHICS_FIXED_ONE ((GRAPHICS_FLOAT_FIXED)1 << GRAPHICS_FIXED_FRAC_BITS)
#define GRAPHICS_FIXED_ONE_FLOAT ((FLOAT_T)(1 << GRAPHICS_FIXED_FRAC_BITS))
#define GRAPHICS_FIXED_TO_FLOAT(int_value) ((FLOAT_T)(int_value) / GRAPHICS_FIXED_ONE_FLOAT)
#define GRAPHICS_FIXED_FRAC_MASK ((int32)(((uint32)(-1)) >> (GRAPHICS_FIXED_BITS - GRAPHICS_FIXED_FRAC_BITS)))
#define GRAPHICS_MAGIC_NUMBER_FIXED ((1LL << (52 - GRAPHICS_FIXED_FRAC_BITS)) * 1.5)
#define GRAPHICS_FIXED_IS_INTEGER(int_value) (((int_value) & GRAPHICS_FIXED_FRAC_MASK) == 0)
#define GRAPHICS_FIXED_EPXILON ((int32)(1))
#define GRAPHICS_FIXED_ERROR_FLOAT (1.0 / (2 * GRAPHICS_FIXED_ONE_FLOAT))
#define GRAPHICS_FIXED_INTEGER_FLOOR(f) (((f) >= 0) ? ((f) >> GRAPHICS_FIXED_FRAC_BITS) : (- ((-(f) - 1) >> GRAPHICS_FIXED_FRAC_BITS) - 1))
#define GRAPHICS_FIXED_INTEGER_CEIL(f) (((f) > 0) ? (((f) - 1) >> GRAPHICS_FIXED_FRAC_BITS + 1) : (- ((GRAPHICS_FLOAT_FIXED)(-(GRAPHICS_FLOAT_FIXED_UNSIGNED)(f)) >> GRAPHICS_FIXED_FRAC_BITS)))
#define GRAPHICS_FIXED_FLOOR(f) ((f) & ~(GRAPHICS_FIXED_FRAC_MASK))
#define GRAPHICS_FIXED_CEIL(f) (GRAPHICS_FIXED_FLOOR((f) + GRAPHICS_FIXED_FRAC_MASK))
#define GRAPHICS_FIXED_ROUND_DOWN(f) (GRAPHICS_FIXED_FLOOR((f) + (GRAPHICS_FIXED_FRAC_MASK+1)/2))

#define GRAPHICS_FIXED(X) ((X) << 16)
#define PIXEL_MANIPULATE_MAX_INT (((GRAPHICS_FIXED(1)) >> 1) - 1)

typedef struct _uint128
{
	uint64 lo, hi;
} uint128, int128;

typedef int32 GRAPHICS_FIXED_16_16;

#define GRAPHICS_INT32x32_64_MULTI(a, b) ((int64)(a) * (b))
#define GRAPHICS_INT64_32_DIVIDE(num, den) ((num) / (den))
#define GRAPHICS_INT64x32_128_MULTI(a, b) GraphicsInteger64x64_128Multi((a), (uint64)(b))
#define GRAPHICS_UINT32x32_64_MULTI(a, b) ((uint64) (a) * (b))
#define GRAPHICS_UINT64x32_128_MULTI(a, b) GraphicsUnsignedInteger64x64_128Multi((a), (b))
#define GRAPHICS_INT64_COMPARE(a, b) ((a) == (b) ? 0 : (a) < (b) ? -1 : 1)

#ifndef M_SQRT1_2
# define M_SQRT1_2  0.707106781186547524401
#endif

#ifndef M_SQRT2
# define M_SQRT2	1.41421356237309504880
#endif

#define GRAPHICS_TOLERANCE_MINIMUM GRAPHICS_FIXED_TO_FLOAT(1)

#define INTEGER64_ADD(a, b) ((a) + (b))
#define INTEGER32x32_64_MULTI(a, b) ((int64) (a) * (b))
#define INTEGER64_EQUAL(a, b) ((a) == (b))
#define INTEGER64_NEGATIVE(a) ((a) < 0)
#define UNSIGNED_INTEGER64_NEGATIVE(a) ((int64)(a) < 0)
#define UNSIGNED_INTEGER128_NEGATIVE(a) UNSIGNED_INTEGER64_NEGATIVE((a).hi)
#define INTEGER128_NEGATIVE(a) UNSIGNED_INTEGER128_NEGATIVE((a))
#define INTEGER128_SUB(a, b) UnsignedInteger128Sub((a), (b))
#define INT32_MAX	(2147483647)
#define INT32_MIN	(-2147483647-1)
#define INT16_MIN	(-32767-1)
#define INT16_MAX	(32767)

#ifndef GRAPHICS_STACK_BUFFER_SIZE
# define GRAPHICS_STACK_BUFFER_SIZE (512 * sizeof(int))
#endif

#define GRAPHICS_STACK_ARRAY_LENGTH(T) (GRAPHICS_STACK_BUFFER_SIZE / sizeof(T))

#if defined (__GNUC__)
#define GRAPHICS_CONTAINER_OF(ptr, type, member) ({ \
	const __typeof__ (((type *) 0)->member) *mptr__ = (ptr); \
	(type *) ((char *) mptr__ - offsetof (type, member)); \
})
#elif !defined(GRAPHICS_CONTAINER_OF)
#define GRAPHICS_CONTAINER_OF(ptr, type, member) \
	((type *)((char *) (ptr) - (char *) &((type *)0)->member))
#endif

typedef enum _eGRAPHICS_DIRECTION
{
	GRAPHICS_DIRECTION_FORWARD,
	GRAPHICS_DIRECTION_REVERSE
} eGRAPHICS_DIRECTION;

typedef struct _GRAPHICS_QUOREM64
{
	int64 quo;
	int64 rem;
} GRAPHICS_QUOREM64;

typedef enum _eGRAPHICS_INTERNAL_SURFACE_TYPE
{
	GRAPHICS_INTERNAL_SURFACE_TYPE_SNAPSHOT = 0x1000,
	GRAPHICS_INTERNAL_SURFACE_TYPE_FALLBACK,
	GRAPHICS_INTERNAL_SURFACE_TYPEWRAPPING,
	GRAPHICS_INTERNAL_SURFACE_TYPE_NULL
} eGRAPHICS_INTERNAL_SURFACE_TYPE;

typedef struct _GRAPHICS_UNSIGNED_QUOREM64
{
	uint64 quo;
	uint64 rem;
} GRAPHICS_UNSIGNED_QUOREM64;

static INLINE int32 GraphicsFixedFromFloat(FLOAT_T d)
{
	union {
		FLOAT_T d;
		int32 i[2];
	} u;

	u.d = d + GRAPHICS_MAGIC_NUMBER_FIXED;
#ifdef GRAPHICS_LITTLE_ENDIAN
	return u.i[0];
#else
	return u.i[1];
#endif
}

static INLINE int32 GraphicsFixedFromInteger(int i)
{
	return i << GRAPHICS_FIXED_FRAC_BITS;
}

static INLINE void GraphicsBoxFromRectangleInt(GRAPHICS_BOX* box, const GRAPHICS_RECTANGLE_INT* rect)
{
	box->point1.x = (rect->x << GRAPHICS_FIXED_FRAC_BITS);
	box->point1.y = (rect->y << GRAPHICS_FIXED_FRAC_BITS);
	box->point2.x = ((rect->x + rect->width) << GRAPHICS_FIXED_FRAC_BITS);
	box->point2.y = ((rect->y + rect->height) << GRAPHICS_FIXED_FRAC_BITS);
}

#define GRAPHICS_RECTANGLE_CONTAINS_RECTANGLE(A, B) \
	((A)->x <= (B)->x && (A)->x + (int)((A)->width) >= (B)->x + (int)((B)->width) \
	&& (A)->y <= (B)->y && (A)->y + (int)((A)->height) >= (B)->y + (int)((B)->height))

static INLINE int GraphicsBoxIsPixelAligned(const GRAPHICS_BOX* box)
{
	int32 f;

	f = 0;
	f |= box->point1.x & GRAPHICS_FIXED_FRAC_MASK;
	f |= box->point1.y & GRAPHICS_FIXED_FRAC_MASK;
	f |= box->point2.x & GRAPHICS_FIXED_FRAC_MASK;
	f |= box->point2.y & GRAPHICS_FIXED_FRAC_MASK;

	return f == 0;
}

static INLINE int GraphicsLinesEqual(const GRAPHICS_LINE* a, const GRAPHICS_LINE* b)
{
	return (a->point1.x == b->point1.x && a->point1.y == b->point1.y
			&& a->point2.x == b->point2.x && a->point2.y == b->point2.y);
}

static INLINE void GraphicsBoxSet(GRAPHICS_BOX* box, const GRAPHICS_POINT* point1, const GRAPHICS_POINT* point2)
{
	box->point1 = *point1;
	box->point2 = *point2;
}

static INLINE void GraphicsBoxAddPoint(GRAPHICS_BOX* box, const GRAPHICS_POINT* point)
{
	if(point->x < box->point1.x)
	{
		box->point1.x = point->x;
	}
	else if(point->x > box->point2.x)
	{
		box->point2.x = point->x;
	}
	
	if(point->y < box->point1.y)
	{
		box->point1.y = point->y;
	}
	else if(point->y > box->point2.y)
	{
		box->point2.y = point->y;
	}
}

static INLINE void GraphicsBoxAddBox(GRAPHICS_BOX* box, const GRAPHICS_BOX* add)
{
	if(add->point1.x < box->point1.x)
	{
		box->point1.x = add->point1.x;
	}
	if(add->point2.x > box->point2.x)
	{
		box->point2.x = add->point2.x;
	}

	if(add->point1.y < box->point1.y)
	{
		box->point1.y = add->point1.y;
	}
	if(add->point2.y > box->point2.y)
	{
		box->point2.y = add->point2.y;
	}
}

static INLINE int GraphicsFixedIntegerFloor(int32 f)
{
	if(f >= 0)
	{
		return f >> GRAPHICS_FIXED_FRAC_BITS;
	}
	return -((-f - 1) >> GRAPHICS_FIXED_FRAC_BITS) - 1;
}

static INLINE int GraphicsFixedIngegerCeil(int32 f)
{
	if(f > 0)
	{
		return ((f - 1) >> GRAPHICS_FIXED_FRAC_BITS) + 1;
	}
	return - ((int32)(-(uint32)f) >> GRAPHICS_FIXED_FRAC_BITS);
}


static INLINE int32 GraphicsFixedMultiDivideFloor(int32 a, int32 b, int32 c)
{
	return GRAPHICS_INT64_32_DIVIDE(GRAPHICS_INT32x32_64_MULTI(a, b), c);
}

static INLINE GRAPHICS_FLOAT_FIXED GraphicsEdgeComputeIntersectionXforY(
	const GRAPHICS_POINT* point1,
	const GRAPHICS_POINT* point2,
	GRAPHICS_FLOAT_FIXED y
)
{
	GRAPHICS_FLOAT_FIXED x, dy;

	if(y == point1->y)
	{
		return point1->x;
	}
	if(y == point2->y)
	{
		return point2->x;
	}

	x = point1->x;
	dy = point2->y - point1->y;
	if(dy != 0)
	{
		x += GraphicsFixedMultiDivideFloor(y - point1->y, point2->x - point1->x, dy);
	}

	return x;
}

static INLINE GRAPHICS_FLOAT_FIXED GraphicsEdgeComputeIntersectionYforX(const GRAPHICS_POINT* point1, const GRAPHICS_POINT* point2, int32 x)
{
	GRAPHICS_FLOAT_FIXED y, dx;

	if(x == point1->x)
	{
		return point1->y;
	}
	if(x == point2->x)
	{
		return point2->y;
	}

	y = point1->y;
	dx = point2->x - point1->x;
	if(dx != 0)
	{
		y += GraphicsFixedMultiDivideFloor(x - point1->x, point2->y - point1->y, dx);
	}

	return y;
}

static INLINE FLOAT_T GraphicsRestrictValue(FLOAT_T value, FLOAT_T minimum, FLOAT_T maximum)
{
	if(value < minimum)
	{
		return minimum;
	}
	else if(value > maximum)
	{
		return maximum;
	}
	return value;
}

static INLINE int GraphicsBoxContainsPoint(const GRAPHICS_BOX* box, const GRAPHICS_POINT* point)
{
	return box->point1.x <= point->x && point->x <= box->point2.x
		&& box->point1.y <= point->y && point->y <= box->point2.y;
}

static INLINE int SlowSegmentIntersection(
	const GRAPHICS_POINT* segment1_point1,
	const GRAPHICS_POINT* segment1_point2,
	const GRAPHICS_POINT* segment2_point1,
	const GRAPHICS_POINT* segment2_point2,
	GRAPHICS_POINT* intersection
)
{
	FLOAT_T denomirator, u_a, u_b;
	FLOAT_T segment1_dx, segment1_dy, segment2_dx, segment2_dy,
			segment_start_dx, segment_start_dy;

	segment1_dx = GRAPHICS_FIXED_TO_FLOAT(segment1_point2->x - segment1_point1->x);
	segment1_dy = GRAPHICS_FIXED_TO_FLOAT(segment1_point2->y - segment1_point1->y);
	segment2_dx = GRAPHICS_FIXED_TO_FLOAT(segment2_point2->x - segment2_point1->x);
	segment2_dy = GRAPHICS_FIXED_TO_FLOAT(segment2_point2->y - segment2_point1->y);
	denomirator = (segment2_dy * segment1_dx) - (segment2_dx * segment1_dy);
	if(denomirator == 0)
	{
		return FALSE;
	}

	segment_start_dx = GRAPHICS_FIXED_TO_FLOAT(segment1_point1->x - segment2_point1->x);
	segment_start_dy = GRAPHICS_FIXED_TO_FLOAT(segment1_point1->y - segment2_point1->y);
	u_a = ((segment2_dx * segment_start_dy) - (segment2_dy * segment_start_dx)) / denomirator;
	u_b = ((segment1_dx * segment_start_dy) - (segment1_dy * segment_start_dx)) / denomirator;

	if(u_a <= 0 || u_a >= 1 || u_b <= 0 || u_b >= 1)
	{
		return FALSE;
	}

	intersection->x = segment1_point1->x + GraphicsFixedFromFloat((u_a * segment1_dx));
	intersection->y = segment1_point1->x + GraphicsFixedFromFloat((u_a * segment1_dy));
	return TRUE;
}

static INLINE unsigned int GraphicsCombsortNewGap(unsigned int gap)
{
	gap = 10 + gap / 13;
	if(gap == 9  || gap == 10)
	{
		gap = 11;
	}
	if(gap < 1)
	{
		gap = 1;
	}
	return gap;
}

static INLINE int UnsignedInteger128Compare(const uint128 a, const uint128 b)
{
	if(a.hi == b.hi)
	{
		return (a.lo == b.lo ? 0 : a.lo < b.lo ? -1 : 1);
	}
	return a.hi < b.hi ? -1 : 1;
}

static INLINE int Integer128Compare(const int128 a, const int128 b)
{
	if(INTEGER128_NEGATIVE(a) && FALSE == INTEGER128_NEGATIVE(b))
	{
		return -1;
	}
	if(FALSE == INTEGER128_NEGATIVE(a) && INTEGER128_NEGATIVE(b))
	{
		return 1;
	}

	return UnsignedInteger128Compare(a, b);
}

static INLINE uint128 UnsignedInteger128Add(const uint128 a, const uint128 b)
{
	uint128 sum;

	sum.hi = a.hi + b.hi;
	sum.lo = a.lo + b.lo;
	if(sum.lo < a.lo)
	{
		sum.hi = sum.hi + 1;
	}
	return sum;
}

static INLINE uint128 UnsignedInteger128Sub(const uint128 a, const uint128 b)
{
	uint128 sub;

	sub.hi = a.hi - b.hi;
	sub.lo = a.lo - b.lo;
	if(sub.lo > a.lo)
	{
		sub.hi = sub.hi - 1;
	}
	return sub;
}

static INLINE GRAPHICS_UNSIGNED_QUOREM64 GraphicsUnsignedQuorem64DivideRemain(uint64 num, uint64 den)
{
	GRAPHICS_UNSIGNED_QUOREM64 qr;

	qr.quo = num / den;
	qr.rem = num % den;
	return  qr;
}

static INLINE int GraphicsMatrixIsScale(const GRAPHICS_MATRIX* matrix)
{
	return matrix->yx == 0.0 && matrix->xy == 0.0;
}

static INLINE int GraphicsClipIsAllClipped(const GRAPHICS_CLIP* clip)
{
	if(clip == NULL)
	{
		return FALSE;
	}
	return clip->clip_all;
}

static INLINE unsigned int GraphicsCombsortNewgap(unsigned int gap)
{
	gap = 10 * gap / 13;
	if(gap == 9 || gap == 10)
	{
		gap = 11;
	}
	if(gap < 1)
	{
		gap = 1;
	}
	return gap;
}

#define GRAPHICS_COMBSORT_DECLARE(NAME, TYPE, CMP) \
static void \
NAME(TYPE *base, unsigned int nmemb) \
{ \
  unsigned int gap = nmemb; \
  unsigned int i, j; \
  int swapped; \
  do { \
	  gap = GraphicsCombsortNewgap(gap); \
	  swapped = gap > 1; \
	  for (i = 0; i < nmemb-gap ; i++) \
	  { \
		j = i + gap; \
		if(CMP(base[i], base[j]) > 0) \
		{ \
		  TYPE tmp; \
		  tmp = base[i]; \
		  base[i] = base[j]; \
		  base[j] = tmp; \
		  swapped = 1; \
		} \
		} \
	} while (swapped); \
}

#define TRACE(X) 

#ifdef __cplusplus
extern "C" {
#endif

static INLINE GRAPHICS_SURFACE* GraphicsSurfaceGetSource(GRAPHICS_SURFACE* surface, GRAPHICS_RECTANGLE_INT* extents)
{
	return surface->backend->source(surface, extents);
}

extern uint128 GraphicsInteger64x64_128Multi(int64 a, int64 b);
extern uint128 GraphicsUnsignedInteger64x64_128Multi(uint64 a, uint64 b);
extern uint128 GraphicsUnsignedInteger128Negate(uint128 a);
extern GRAPHICS_QUOREM64 GraphicsInteger96by64_32x64_DivideRemain(int128 num, int64 den);
extern GRAPHICS_UNSIGNED_QUOREM64 GraphicsUnsignedInteger96by64_32x64_DivideRemain(uint128 num, uint64 den);
extern uint32 GraphicsOperatorBoundedByEither(eGRAPHICS_OPERATOR op);
extern int GraphicsOperatorBoundedBySource(eGRAPHICS_OPERATOR op);
extern int GraphicsOperatorBoundedByMask(eGRAPHICS_OPERATOR op);
extern void _InitializeGraphicsContext(struct _GRAPHICS_CONTEXT* context, struct _GRAPHICS_BACKEND* backend);
extern struct _GRAPHICS_PATH* CreateGraphicsPath(struct _GRAPHICS_PATH_FIXED* path, struct _GRAPHICS_CONTEXT* context);
extern struct _GRAPHICS_PATH* CreateGraphicsPathFlat(struct _GRAPHICS_PATH_FIXED* path, struct _GRAPHICS_CONTEXT* context);
extern eGRAPHICS_STATUS GraphicsPathAppendToContext(const struct _GRAPHICS_PATH* path, struct _GRAPHICS_CONTEXT* context);
extern eGRAPHICS_STATUS GraphicsBentleyOttmannTessellatePolygon(
	GRAPHICS_TRAPS* traps,
	const GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE fill_rule
);
extern eGRAPHICS_STATUS GraphicsBentleyOttmannTessellateTraps(
	GRAPHICS_TRAPS* traps,
	eGRAPHICS_FILL_RULE fill_rule
);
extern int GraphicsContentDepth(eGRAPHICS_CONTENT content);
extern PIXEL_MANIPULATE_IMAGE* PixelManipulateImageForColor(const GRAPHICS_COLOR* cairo_color, void* graphics);

#ifdef __cplusplus
}
#endif

#endif // _INCLUDE_GRAPHICS_PRIVATE_H_
