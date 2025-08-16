#include <string.h>
#include "graphics_types.h"
#include "graphics_list.h"
#include "graphics_memory_pool.h"
#include "graphics_region.h"
#include "graphics_matrix.h"
#include "graphics_private.h"
#include "../memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STACK_BUFFER_SIZE (512 * sizeof(int))

#ifndef GRAPHICS_COMBSORT_DECLARE
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

# define GRAPHICS_COMBSORT_DECLARE(NAME, TYPE, CMP) \
static void \
NAME (TYPE *base, unsigned int nmemb) \
{ \
  unsigned int gap = nmemb; \
  unsigned int i, j; \
  int swapped; \
  do { \
	  gap = GraphicsCombsortNewgap(gap); \
	  swapped = gap > 1; \
	  for (i = 0; i < nmemb-gap ; i++) { \
	  j = i + gap; \
	  if (CMP (base[i], base[j]) > 0 ) { \
		  TYPE tmp; \
		  tmp = base[i]; \
		  base[i] = base[j]; \
		  base[j] = tmp; \
		  swapped = 1; \
	  } \
	  } \
  } while (swapped); \
}

#endif

typedef GRAPHICS_POINT GRAPHICS_BO_POINT32;

typedef struct _GRAPHICS_BO_INTERSECT_ORDINATE
{
	int32 ordinate;
	enum { EXACT, INEXACT } exactness;
} GRAPHICS_BO_INTERSECT_ORDINATE;

typedef struct _GRAPHICS_BO_INTERSECT_POINT
{
	GRAPHICS_BO_INTERSECT_ORDINATE x;
	GRAPHICS_BO_INTERSECT_ORDINATE y;
} GRAPHICS_BO_INTERSECT_POINT;

typedef struct _GRAPHICS_BO_EDGE GRAPHICS_BO_EDGE;

typedef struct _GRAPHICS_BO_DEFERRED
{
	GRAPHICS_BO_EDGE *right;
	int32 top;
} GRAPHICS_BO_DEFERRED;

struct _GRAPHICS_BO_EDGE
{
	GRAPHICS_EDGE edge;
	GRAPHICS_BO_EDGE *prev;
	GRAPHICS_BO_EDGE *next;
	GRAPHICS_BO_DEFERRED deferred;
};

/* the parent is always given by index/2 */
#define POINT_QUEUE_PARENT_INDEX(i) ((i) >> 1)
#define POINT_QUEUE_FIRST_ENTRY 1

/* left and right children are index * 2 and (index * 2) +1 respectively */
#define POINT_QUEUE_LEFT_CHILD_INDEX(i) ((i) << 1)

typedef enum _eeGRAPHICS_BO_EVENT_TYPE
{
	GRAPHICS_BO_EVENT_TYPE_STOP,
	GRAPHICS_BO_EVENT_TYPE_INTERSECTION,
	GRAPHICS_BO_EVENT_TYPE_START
} eGRAPHICS_BO_EVENT_TYPE;

typedef struct _GRAPHICS_BO_EVENT
{
	eGRAPHICS_BO_EVENT_TYPE type;
	GRAPHICS_POINT point;
} GRAPHICS_BO_EVENT;

typedef struct _GRAPHICS_BO_START_EVENT
{
	eGRAPHICS_BO_EVENT_TYPE type;
	GRAPHICS_POINT point;
	GRAPHICS_BO_EDGE edge;
} GRAPHICS_BO_START_EVENT;

typedef struct _GRAPHICS_BO_QUEUE_EVENT
{
	eGRAPHICS_BO_EVENT_TYPE type;
	GRAPHICS_POINT point;
	GRAPHICS_BO_EDGE *edge1;
	GRAPHICS_BO_EDGE *edge2;
} GRAPHICS_BO_QUEUE_EVENT;

typedef struct _POINT_QUEUE
{
	int size, max_size;

	GRAPHICS_BO_EVENT **elements;
	GRAPHICS_BO_EVENT *elements_embedded[1024];
} POINT_QUEUE;

typedef struct _GRAPHICS_BO_EVENT_QUEUE
{
	GRAPHICS_MEMORY_POOL pool;
	POINT_QUEUE point_queue;
	GRAPHICS_BO_EVENT **start_events;
} GRAPHICS_BO_EVENT_QUEUE;

typedef struct _GRAPHICS_BO_SWEEP_LINE
{
	GRAPHICS_BO_EDGE *head;
	int32 current_y;
	GRAPHICS_BO_EDGE *current_edge;
} GRAPHICS_BO_SWEEP_LINE;

static GRAPHICS_FLOAT_FIXED LineComputeIntersectionXforY(const GRAPHICS_LINE* line, GRAPHICS_FLOAT_FIXED y)
{
	GRAPHICS_FLOAT_FIXED x, dy;

	if(y == line->point1.y)
		return line->point1.x;
	if(y == line->point2.y)
		return line->point2.x;

	x = line->point1.x;
	dy = line->point2.y - line->point1.y;
	if(dy != 0)
	{
		x += GraphicsFixedMultiDivideFloor(y - line->point1.y,
					 line->point2.x - line->point1.x, dy);
	}

	return x;
}

static INLINE int GraphicsBoPoint32Compare(GRAPHICS_BO_POINT32 const* a, GRAPHICS_BO_POINT32 const* b)
{
	int cmp;

	cmp = a->y - b->y;
	if(cmp)
		return cmp;

	return a->x - b->x;
}

/* Compare the slope of a to the slope of b, returning 1, 0, -1 if the
 * slope a is respectively greater than, equal to, or less than the
 * slope of b.
 *
 * For each edge, consider the direction vector formed from:
 *
 *	top -> bottom
 *
 * which is:
 *
 *	(dx, dy) = (line.p2.x - line.p1.x, line.p2.y - line.p1.y)
 *
 * We then define the slope of each edge as dx/dy, (which is the
 * inverse of the slope typically used in math instruction). We never
 * compute a slope directly as the value approaches infinity, but we
 * can derive a slope comparison without division as follows, (where
 * the ? represents our compare operator).
 *
 * 1.	   slope(a) ? slope(b)
 * 2.		adx/ady ? bdx/bdy
 * 3.	(adx * bdy) ? (bdx * ady)
 *
 * Note that from step 2 to step 3 there is no change needed in the
 * sign of the result since both ady and bdy are guaranteed to be
 * greater than or equal to 0.
 *
 * When using this slope comparison to sort edges, some care is needed
 * when interpreting the results. Since the slope compare operates on
 * distance vectors from top to bottom it gives a correct left to
 * right sort for edges that have a common top point, (such as two
 * edges with start events at the same location). On the other hand,
 * the sense of the result will be exactly reversed for two edges that
 * have a common stop point.
 */
static INLINE int SlopeCompare(const GRAPHICS_BO_EDGE* a, const GRAPHICS_BO_EDGE* b)
{
	/* XXX: We're assuming here that dx and dy will still fit in 32
	 * bits. That's not true in general as there could be overflow. We
	 * should prevent that before the tessellation algorithm
	 * begins.
	 */
	int32 adx = a->edge.line.point2.x - a->edge.line.point1.x;
	int32 bdx = b->edge.line.point2.x - b->edge.line.point1.x;

	/* Since the dy's are all positive by construction we can fast
	 * path several common cases.
	 */

	/* First check for vertical lines. */
	if(adx == 0)
		return -bdx;
	if(bdx == 0)
		return adx;

	/* Then where the two edges point in different directions wrt x. */
	if((adx ^ bdx) < 0)
		return adx;

	/* Finally we actually need to do the general comparison. */
	{
		int32 ady = a->edge.line.point2.y - a->edge.line.point1.y;
		int32 bdy = b->edge.line.point2.y - b->edge.line.point1.y;
		int64 adx_bdy = GRAPHICS_INT32x32_64_MULTI(adx, bdy);
		int64 bdx_ady = GRAPHICS_INT32x32_64_MULTI(bdx, ady);

		return adx_bdy == bdx_ady ? 0 : adx_bdy < bdx_ady ? -1 : 1;
	}
}

/*
 * We need to compare the x-coordinates of a pair of lines for a particular y,
 * without loss of precision.
 *
 * The x-coordinate along an edge for a given y is:
 *   X = A_x + (Y - A_y) * A_dx / A_dy
 *
 * So the inequality we wish to test is:
 *   A_x + (Y - A_y) * A_dx / A_dy ∘ B_x + (Y - B_y) * B_dx / B_dy,
 * where ∘ is our inequality operator.
 *
 * By construction, we know that A_dy and B_dy (and (Y - A_y), (Y - B_y)) are
 * all positive, so we can rearrange it thus without causing a sign change:
 *   A_dy * B_dy * (A_x - B_x) ∘ (Y - B_y) * B_dx * A_dy
 *								 - (Y - A_y) * A_dx * B_dy
 *
 * Given the assumption that all the deltas fit within 32 bits, we can compute
 * this comparison directly using 128 bit arithmetic. For certain, but common,
 * input we can reduce this down to a single 32 bit compare by inspecting the
 * deltas.
 *
 * (And put the burden of the work on developing fast 128 bit ops, which are
 * required throughout the tessellator.)
 *
 * See the similar discussion for _slope_compare().
 */
static int EdgesCompareXforY_General(
	const GRAPHICS_BO_EDGE* a,
	const GRAPHICS_BO_EDGE* b,
	int32 y
)
{
	/* XXX: We're assuming here that dx and dy will still fit in 32
	 * bits. That's not true in general as there could be overflow. We
	 * should prevent that before the tessellation algorithm
	 * begins.
	 */
	int32 dx;
	int32 adx, ady;
	int32 bdx, bdy;
	enum
	{
		HAVE_NONE	= 0x0,
		HAVE_DX	  = 0x1,
		HAVE_ADX	 = 0x2,
		HAVE_DX_ADX  = HAVE_DX | HAVE_ADX,
		HAVE_BDX	 = 0x4,
		HAVE_DX_BDX  = HAVE_DX | HAVE_BDX,
		HAVE_ADX_BDX = HAVE_ADX | HAVE_BDX,
		HAVE_ALL	 = HAVE_DX | HAVE_ADX | HAVE_BDX
	} have_dx_adx_bdx = HAVE_ALL;

	/* don't bother solving for abscissa if the edges' bounding boxes
	 * can be used to order them. */
	{
		int32 amin, amax;
		int32 bmin, bmax;
		if (a->edge.line.point1.x < a->edge.line.point2.x)
		{
			amin = a->edge.line.point1.x;
			amax = a->edge.line.point2.x;
		}
		else
		{
			amin = a->edge.line.point2.x;
			amax = a->edge.line.point1.x;
		}
		if(b->edge.line.point1.x < b->edge.line.point2.x)
		{
			bmin = b->edge.line.point1.x;
			bmax = b->edge.line.point2.x;
		}
		else
		{
			bmin = b->edge.line.point2.x;
			bmax = b->edge.line.point1.x;
		}
		if(amax < bmin) return -1;
		if(amin > bmax) return +1;
	}

	ady = a->edge.line.point2.y - a->edge.line.point1.y;
	adx = a->edge.line.point2.x - a->edge.line.point1.x;
	if(adx == 0)
		have_dx_adx_bdx &= ~HAVE_ADX;

	bdy = b->edge.line.point2.y - b->edge.line.point1.y;
	bdx = b->edge.line.point2.x - b->edge.line.point1.x;
	if(bdx == 0)
		have_dx_adx_bdx &= ~HAVE_BDX;

	dx = a->edge.line.point1.x - b->edge.line.point1.x;
	if(dx == 0)
		have_dx_adx_bdx &= ~HAVE_DX;

#define L GRAPHICS_INT64x32_128_MULTI(GRAPHICS_INT32x32_64_MULTI(ady, bdy), dx)
#define A GRAPHICS_INT64x32_128_MULTI(GRAPHICS_INT32x32_64_MULTI(adx, bdy), y - a->edge.line.point1.y)
#define B GRAPHICS_INT64x32_128_MULTI(GRAPHICS_INT32x32_64_MULTI(bdx, ady), y - b->edge.line.point1.y)
	switch(have_dx_adx_bdx)
	{
	default:
	case HAVE_NONE:
		return 0;
	case HAVE_DX:
		/* A_dy * B_dy * (A_x - B_x) ∘ 0 */
		return dx; /* ady * bdy is positive definite */
	case HAVE_ADX:
		/* 0 ∘  - (Y - A_y) * A_dx * B_dy */
		return adx; /* bdy * (y - a->top.y) is positive definite */
	case HAVE_BDX:
		/* 0 ∘ (Y - B_y) * B_dx * A_dy */
		return -bdx; /* ady * (y - b->top.y) is positive definite */
	case HAVE_ADX_BDX:
		/*  0 ∘ (Y - B_y) * B_dx * A_dy - (Y - A_y) * A_dx * B_dy */
		if((adx ^ bdx) < 0)
		{
			return adx;
		}
		else if(a->edge.line.point1.y == b->edge.line.point1.y)
		{ /* common origin */
			int64 adx_bdy, bdx_ady;

			/* ∴ A_dx * B_dy ∘ B_dx * A_dy */

			adx_bdy = GRAPHICS_INT32x32_64_MULTI(adx, bdy);
			bdx_ady = GRAPHICS_INT32x32_64_MULTI(bdx, ady);

			return adx_bdy == bdx_ady ? 0 : adx_bdy < bdx_ady ? -1 : 1;
		}
		else
			return Integer128Compare(A, B);
	case HAVE_DX_ADX:
		/* A_dy * (A_x - B_x) ∘ - (Y - A_y) * A_dx */
		if((-adx ^ dx) < 0)
		{
			return dx;
		}
		else
		{
			int64 ady_dx, dy_adx;

			ady_dx = GRAPHICS_INT32x32_64_MULTI(ady, dx);
			dy_adx = GRAPHICS_INT32x32_64_MULTI(a->edge.line.point1.y - y, adx);

			return ady_dx == dy_adx ? 0 : ady_dx < dy_adx ? -1 : 1;
		}
	case HAVE_DX_BDX:
		/* B_dy * (A_x - B_x) ∘ (Y - B_y) * B_dx */
		if((bdx ^ dx) < 0)
		{
			return dx;
		}
		else
		{
			int64 bdy_dx, dy_bdx;

			bdy_dx = GRAPHICS_INT32x32_64_MULTI(bdy, dx);
			dy_bdx = GRAPHICS_INT32x32_64_MULTI(y - b->edge.line.point1.y, bdx);

			return bdy_dx == dy_bdx ? 0 : bdy_dx < dy_bdx ? -1 : 1;
		}
	case HAVE_ALL:
		/* XXX try comparing (a->edge.line.p2.x - b->edge.line.p2.x) et al */
		return Integer128Compare(L, INTEGER128_SUB(B, A));
	}
#undef B
#undef A
#undef L
}

/*
 * We need to compare the x-coordinate of a line for a particular y wrt to a
 * given x, without loss of precision.
 *
 * The x-coordinate along an edge for a given y is:
 *   X = A_x + (Y - A_y) * A_dx / A_dy
 *
 * So the inequality we wish to test is:
 *   A_x + (Y - A_y) * A_dx / A_dy ∘ X
 * where ∘ is our inequality operator.
 *
 * By construction, we know that A_dy (and (Y - A_y)) are
 * all positive, so we can rearrange it thus without causing a sign change:
 *   (Y - A_y) * A_dx ∘ (X - A_x) * A_dy
 *
 * Given the assumption that all the deltas fit within 32 bits, we can compute
 * this comparison directly using 64 bit arithmetic.
 *
 * See the similar discussion for _slope_compare() and
 * edges_compare_x_for_y_general().
 */
static int EdgeCompareForYagainstX(
	const GRAPHICS_BO_EDGE* a,
	int32 y,
	int32 x
)
{
	int32 adx, ady;
	int32 dx, dy;
	int64 L, R;

	if(x < a->edge.line.point1.x && x < a->edge.line.point2.x)
		return 1;
	if(x > a->edge.line.point1.x && x > a->edge.line.point2.x)
		return -1;

	adx = a->edge.line.point2.x - a->edge.line.point1.x;
	dx = x - a->edge.line.point1.x;

	if(adx == 0)
		return -dx;
	if(dx == 0 || (adx ^ dx) < 0)
		return adx;

	dy = y - a->edge.line.point1.y;
	ady = a->edge.line.point2.y - a->edge.line.point1.y;

	L = GRAPHICS_INT32x32_64_MULTI(dy, adx);
	R = GRAPHICS_INT32x32_64_MULTI(dx, ady);

	return L == R ? 0 : L < R ? -1 : 1;
}

static int EdgesCompareXforY(
	const GRAPHICS_BO_EDGE* a,
	const GRAPHICS_BO_EDGE* b,
	int32 y
)
{
	/* If the sweep-line is currently on an end-point of a line,
	 * then we know its precise x value (and considering that we often need to
	 * compare events at end-points, this happens frequently enough to warrant
	 * special casing).
	 */
	enum
	{
		HAVE_NEITHER = 0x0,
		HAVE_AX	  = 0x1,
		HAVE_BX	  = 0x2,
		HAVE_BOTH	= HAVE_AX | HAVE_BX
	} have_ax_bx = HAVE_BOTH;
	int32 ax = 0, bx = 0;

	if(y == a->edge.line.point1.y)
		ax = a->edge.line.point1.x;
	else if(y == a->edge.line.point2.y)
		ax = a->edge.line.point2.x;
	else
		have_ax_bx &= ~HAVE_AX;

	if(y == b->edge.line.point1.y)
		bx = b->edge.line.point1.x;
	else if(y == b->edge.line.point2.y)
		bx = b->edge.line.point2.x;
	else
		have_ax_bx &= ~HAVE_BX;

	switch(have_ax_bx)
	{
	default:
	case HAVE_NEITHER:
		return EdgesCompareXforY_General(a, b, y);
	case HAVE_AX:
		return -EdgeCompareForYagainstX(b, y, ax);
	case HAVE_BX:
		return EdgeCompareForYagainstX(a, y, bx);
	case HAVE_BOTH:
		return ax - bx;
	}
}

static INLINE int LineEqual(const GRAPHICS_LINE* a, const GRAPHICS_LINE* b)
{
	return (a->point1.x == b->point1.x && a->point1.y == b->point1.y &&
		a->point2.x == b->point2.x && a->point2.y == b->point2.y);
}

static int GraphicsBoSweepLineCompareEdges(
	GRAPHICS_BO_SWEEP_LINE* sweep_line,
	const GRAPHICS_BO_EDGE* a,
	const GRAPHICS_BO_EDGE* b
)
{
	int cmp;

	/* compare the edges if not identical */
	if(FALSE == LineEqual(&a->edge.line, &b->edge.line))
	{
		cmp = EdgesCompareXforY(a, b, sweep_line->current_y);
		if(cmp)
			return cmp;

		/* The two edges intersect exactly at y, so fall back on slope
		 * comparison. We know that this compare_edges function will be
		 * called only when starting a new edge, (not when stopping an
		 * edge), so we don't have to worry about conditionally inverting
		 * the sense of _slope_compare. */
		cmp = SlopeCompare(a, b);
		if(cmp)
			return cmp;
	}

	/* We've got two collinear edges now. */
	return b->edge.bottom - a->edge.bottom;
}

static INLINE int64 Determin32_64(int32 a, int32 b, int32 c, int32 d)
{
	/* det = a * d - b * c */
	return  GRAPHICS_INT32x32_64_MULTI(a, d) - GRAPHICS_INT32x32_64_MULTI(b, c);
}

static INLINE int128 Determin64x32_128(int64 a, int32 b, int64 c, int32 d)
{
	/* det = a * d - b * c */
	return INTEGER128_SUB(GRAPHICS_INT64x32_128_MULTI(a, d), GRAPHICS_INT64x32_128_MULTI(c, b));
}

/* Compute the intersection of two lines as defined by two edges. The
 * result is provided as a coordinate pair of 128-bit integers.
 *
 * Returns %CAIRO_BO_STATUS_INTERSECTION if there is an intersection or
 * %CAIRO_BO_STATUS_PARALLEL if the two lines are exactly parallel.
 */
static int IntersectLines(
	GRAPHICS_BO_EDGE* a,
	GRAPHICS_BO_EDGE* b,
	GRAPHICS_BO_INTERSECT_POINT* intersection
)
{
	int64 a_det, b_det;

	/* XXX: We're assuming here that dx and dy will still fit in 32
	 * bits. That's not true in general as there could be overflow. We
	 * should prevent that before the tessellation algorithm begins.
	 * What we're doing to mitigate this is to perform clamping in
	 * cairo_bo_tessellate_polygon().
	 */
	int32 dx1 = a->edge.line.point1.x - a->edge.line.point2.x;
	int32 dy1 = a->edge.line.point1.y - a->edge.line.point2.y;

	int32 dx2 = b->edge.line.point1.x - b->edge.line.point2.x;
	int32 dy2 = b->edge.line.point1.y - b->edge.line.point2.y;

	int64 den_det;
	int64 R;
	GRAPHICS_QUOREM64 qr;

	den_det = Determin32_64(dx1, dy1, dx2, dy2);

	/* Q: Can we determine that the lines do not intersect (within range)
	  * much more cheaply than computing the intersection point i.e. by
	  * avoiding the division?
	  *
	  *   X = ax + t * adx = bx + s * bdx;
	  *   Y = ay + t * ady = by + s * bdy;
	  *   ∴ t * (ady*bdx - bdy*adx) = bdx * (by - ay) + bdy * (ax - bx)
	  *   => t * L = R
	  *
	  * Therefore we can reject any intersection (under the criteria for
	  * valid intersection events) if:
	  *   L^R < 0 => t < 0, or
	  *   L<R => t > 1
	  *
	  * (where top/bottom must at least extend to the line endpoints).
	  *
	  * A similar substitution can be performed for s, yielding:
	  *   s * (ady*bdx - bdy*adx) = ady * (ax - bx) - adx * (ay - by)
	  */
	R = Determin32_64(dx2, dy2, b->edge.line.point1.x - a->edge.line.point1.x,
		  b->edge.line.point1.y - a->edge.line.point1.y);
	if(den_det < 0)
	{
		if(den_det >= R)
			return FALSE;
	}
	else
	{
		if (den_det <= R)
			return FALSE;
	}

	R = Determin32_64(dy1, dx1, a->edge.line.point1.y - b->edge.line.point1.y,
						a->edge.line.point1.x - b->edge.line.point1.x);
	if(den_det < 0)
	{
		if(den_det >= R)
			return FALSE;
	}
	else
	{
		if(den_det <= R)
			return FALSE;
	}

	/* We now know that the two lines should intersect within range. */

	a_det = Determin32_64(a->edge.line.point1.x, a->edge.line.point1.y,
				a->edge.line.point2.x, a->edge.line.point2.y);
	b_det = Determin32_64(b->edge.line.point1.x, b->edge.line.point1.y,
				b->edge.line.point2.x, b->edge.line.point2.y);

	/* x = det (a_det, dx1, b_det, dx2) / den_det */
	qr = GraphicsInteger96by64_32x64_DivideRemain(Determin64x32_128(a_det, dx1,
								b_det, dx2), den_det);
	if(qr.rem == den_det)
		return FALSE;
#if 0
	intersection->x.exactness = _cairo_int64_is_zero (qr.rem) ? EXACT : INEXACT;
#else
	intersection->x.exactness = EXACT;
	if(qr.rem != 0)
	{
		if((den_det < 0) ^ (qr.rem < 0))
			qr.rem = - qr.rem;
		qr.rem = qr.rem * 2;
		if(qr.rem >= den_det)
		{
			qr.quo = qr.quo + qr.quo < 0 ? -1 : 1;
		}
		else
			intersection->x.exactness = INEXACT;
	}
#endif
	intersection->x.ordinate = (int32)qr.quo;

	/* y = det (a_det, dy1, b_det, dy2) / den_det */
	qr = GraphicsInteger96by64_32x64_DivideRemain(Determin64x32_128(a_det, dy1,
								b_det, dy2), den_det);
	if(qr.rem == den_det)
		return FALSE;
#if 0
	intersection->y.exactness = _cairo_int64_is_zero (qr.rem) ? EXACT : INEXACT;
#else
	intersection->y.exactness = EXACT;
	if(qr.rem != 0)
	{
		if((den_det < 0) ^ (qr.rem < 0))
			qr.rem = - qr.rem;
		qr.rem = qr.rem * 2;
		if(qr.rem >= den_det)
		{
			qr.quo = qr.quo + qr.quo < 0 ? -1 : 1;
		}
		else
			intersection->y.exactness = INEXACT;
	}
#endif
	intersection->y.ordinate = (int32)qr.quo;

	return TRUE;
}

static int GraphicsBoIntersectOrdinate32Compare(GRAPHICS_BO_INTERSECT_ORDINATE a, int32	b)
{
	/* First compare the quotient */
	if(a.ordinate > b)
		return +1;
	if(a.ordinate < b)
		return -1;
	/* With quotient identical, if remainder is 0 then compare equal */
	/* Otherwise, the non-zero remainder makes a > b */
	return INEXACT == a.exactness;
}

/* Does the given edge contain the given point. The point must already
 * be known to be contained within the line determined by the edge,
 * (most likely the point results from an intersection of this edge
 * with another).
 *
 * If we had exact arithmetic, then this function would simply be a
 * matter of examining whether the y value of the point lies within
 * the range of y values of the edge. But since intersection points
 * are not exact due to being rounded to the nearest integer within
 * the available precision, we must also examine the x value of the
 * point.
 *
 * The definition of "contains" here is that the given intersection
 * point will be seen by the sweep line after the start event for the
 * given edge and before the stop event for the edge. See the comments
 * in the implementation for more details.
 */
static int GraphicsBoEdgeContainsIntersectPoint(GRAPHICS_BO_EDGE* edge, GRAPHICS_BO_INTERSECT_POINT* point)
{
	int cmp_top, cmp_bottom;

	/* XXX: When running the actual algorithm, we don't actually need to
	 * compare against edge->top at all here, since any intersection above
	 * top is eliminated early via a slope comparison. We're leaving these
	 * here for now only for the sake of the quadratic-time intersection
	 * finder which needs them.
	 */

	cmp_top = GraphicsBoIntersectOrdinate32Compare(point->y, edge->edge.top);
	cmp_bottom = GraphicsBoIntersectOrdinate32Compare(point->y, edge->edge.bottom);

	if(cmp_top < 0 || cmp_bottom > 0)
	{
		return FALSE;
	}

	if(cmp_top > 0 && cmp_bottom < 0)
	{
		return TRUE;
	}

	/* At this stage, the point lies on the same y value as either
	 * edge->top or edge->bottom, so we have to examine the x value in
	 * order to properly determine containment. */

	/* If the y value of the point is the same as the y value of the
	 * top of the edge, then the x value of the point must be greater
	 * to be considered as inside the edge. Similarly, if the y value
	 * of the point is the same as the y value of the bottom of the
	 * edge, then the x value of the point must be less to be
	 * considered as inside. */

	if(cmp_top == 0)
	{
		GRAPHICS_FLOAT_FIXED top_x;

		top_x = LineComputeIntersectionXforY(&edge->edge.line,
								edge->edge.top);
		return GraphicsBoIntersectOrdinate32Compare(point->x, top_x) > 0;
	}
	else
	{ /* cmp_bottom == 0 */
		GRAPHICS_FLOAT_FIXED bot_x;

		bot_x = LineComputeIntersectionXforY(&edge->edge.line,
								edge->edge.bottom);
		return GraphicsBoIntersectOrdinate32Compare(point->x, bot_x) < 0;
	}
}

/* Compute the intersection of two edges. The result is provided as a
 * coordinate pair of 128-bit integers.
 *
 * Returns %CAIRO_BO_STATUS_INTERSECTION if there is an intersection
 * that is within both edges, %CAIRO_BO_STATUS_NO_INTERSECTION if the
 * intersection of the lines defined by the edges occurs outside of
 * one or both edges, and %CAIRO_BO_STATUS_PARALLEL if the two edges
 * are exactly parallel.
 *
 * Note that when determining if a candidate intersection is "inside"
 * an edge, we consider both the infinitesimal shortening and the
 * infinitesimal tilt rules described by John Hobby. Specifically, if
 * the intersection is exactly the same as an edge point, it is
 * effectively outside (no intersection is returned). Also, if the
 * intersection point has the same
 */
static int GraphicsBoEdgeIntersect(
	GRAPHICS_BO_EDGE* a,
	GRAPHICS_BO_EDGE* b,
	GRAPHICS_BO_POINT32* intersection
)
{
	GRAPHICS_BO_INTERSECT_POINT quorem;

	if(FALSE == IntersectLines(a, b, &quorem))
		return FALSE;

	if(FALSE == GraphicsBoEdgeContainsIntersectPoint(a, &quorem))
		return FALSE;

	if(FALSE == GraphicsBoEdgeContainsIntersectPoint(b, &quorem))
		return FALSE;

	/* Now that we've correctly compared the intersection point and
	 * determined that it lies within the edge, then we know that we
	 * no longer need any more bits of storage for the intersection
	 * than we do for our edge coordinates. We also no longer need the
	 * remainder from the division. */
	intersection->x = quorem.x.ordinate;
	intersection->y = quorem.y.ordinate;

	return TRUE;
}

static INLINE int GraphicsBoEventCompare(
	const GRAPHICS_BO_EVENT* a,
	const GRAPHICS_BO_EVENT* b
)
{
	int cmp;

	cmp = GraphicsBoPoint32Compare(&a->point, &b->point);
	if(cmp)
		return cmp;

	cmp = a->type - b->type;
	if(cmp)
		return cmp;

	return (int)(a - b);
}

static INLINE void InitializePointQueue(POINT_QUEUE* pq)
{
	pq->max_size = sizeof(pq->elements_embedded) / sizeof(pq->elements_embedded[0]);
	pq->size = 0;

	pq->elements = pq->elements_embedded;
}

static INLINE void PointQueueFinish(POINT_QUEUE* pq)
{
	if(pq->elements != pq->elements_embedded)
		MEM_FREE_FUNC(pq->elements);
}

static eGRAPHICS_STATUS PointQueueGrow(POINT_QUEUE* pq)
{
	GRAPHICS_BO_EVENT **new_elements;
	pq->max_size *= 2;

	if(pq->elements == pq->elements_embedded)
	{
		new_elements = (GRAPHICS_BO_EVENT**)MEM_ALLOC_FUNC(
			pq->max_size * sizeof(GRAPHICS_BO_EVENT*));
		if(UNLIKELY(new_elements == NULL))
			return GRAPHICS_STATUS_NO_MEMORY;

		(void)memcpy(new_elements, pq->elements_embedded,
			sizeof (pq->elements_embedded));
	}
	else
	{
		new_elements = MEM_REALLOC_FUNC(pq->elements, pq->max_size * sizeof(GRAPHICS_BO_EVENT));
		if(UNLIKELY(new_elements == NULL))
			return GRAPHICS_STATUS_NO_MEMORY;
	}

	pq->elements = new_elements;
	return GRAPHICS_STATUS_SUCCESS;
}

static INLINE eGRAPHICS_STATUS PointQueuePush(POINT_QUEUE* pq, GRAPHICS_BO_EVENT* event)
{
	GRAPHICS_BO_EVENT **elements;
	int i, parent;

	if(UNLIKELY(pq->size + 1 == pq->max_size))
	{
		eGRAPHICS_STATUS status;

		status = PointQueueGrow(pq);
		if(UNLIKELY(status))
			return status;
	}

	elements = pq->elements;

	for(i = ++pq->size;
		i != POINT_QUEUE_FIRST_ENTRY &&
			GraphicsBoEventCompare(event,
					elements[parent = POINT_QUEUE_PARENT_INDEX (i)]) < 0;
		i = parent
	)
	{
		elements[i] = elements[parent];
	}

	elements[i] = event;

	return GRAPHICS_STATUS_SUCCESS;
}

static INLINE void PointQueuePop (POINT_QUEUE* pq)
{
	GRAPHICS_BO_EVENT **elements = pq->elements;
	GRAPHICS_BO_EVENT *tail;
	int child, i;

	tail = elements[pq->size--];
	if(pq->size == 0)
	{
		elements[POINT_QUEUE_FIRST_ENTRY] = NULL;
		return;
	}

	for(i = POINT_QUEUE_FIRST_ENTRY;
		(child = POINT_QUEUE_LEFT_CHILD_INDEX (i)) <= pq->size;
			i = child
	)
	{
		if(child != pq->size &&
			GraphicsBoEventCompare(elements[child+1], elements[child]) < 0)
		{
			child++;
		}

		if(GraphicsBoEventCompare(elements[child], tail) >= 0)
			break;

		elements[i] = elements[child];
	}
	elements[i] = tail;
}

static INLINE eGRAPHICS_STATUS GraphicsBoEventQueueInsert(
	GRAPHICS_BO_EVENT_QUEUE* queue,
	eGRAPHICS_BO_EVENT_TYPE type,
	GRAPHICS_BO_EDGE* edge1,
	GRAPHICS_BO_EDGE* edge2,
	const GRAPHICS_POINT* point
)
{
	GRAPHICS_BO_QUEUE_EVENT *event;

	event = GraphicsMemoryPoolAllocation(&queue->pool);
	if(UNLIKELY(event == NULL))
		return GRAPHICS_STATUS_NO_MEMORY;

	event->type = type;
	event->edge1 = edge1;
	event->edge2 = edge2;
	event->point = *point;

	return PointQueuePush(&queue->point_queue, (GRAPHICS_BO_EVENT*)event);
}

static void GraphicsBoEventQueueDelete(GRAPHICS_BO_EVENT_QUEUE* queue, GRAPHICS_BO_EVENT* event)
{
	GraphicsMemoryPoolFree(&queue->pool, event);
}

static GRAPHICS_BO_EVENT* GraphicsBoEventDequeue(GRAPHICS_BO_EVENT_QUEUE* event_queue)
{
	GRAPHICS_BO_EVENT *event, *cmp;

	event = event_queue->point_queue.elements[POINT_QUEUE_FIRST_ENTRY];
	cmp = *event_queue->start_events;
	if(event == NULL ||
		(cmp != NULL && GraphicsBoEventCompare(cmp, event) < 0))
	{
		event = cmp;
		event_queue->start_events++;
	}
	else
	{
		PointQueuePop(&event_queue->point_queue);
	}

	return event;
}

GRAPHICS_COMBSORT_DECLARE(GraphicsBoEventQueueSort,
			GRAPHICS_BO_EVENT*,	GraphicsBoEventCompare)

static void InitializeGraphicsBoEventQueue(
	GRAPHICS_BO_EVENT_QUEUE* event_queue,
	GRAPHICS_BO_EVENT **start_events,
	int num_events
)
{
	GraphicsBoEventQueueSort(start_events, num_events);
	start_events[num_events] = NULL;

	event_queue->start_events = start_events;

	InitializeGraphicsMemoryPool(&event_queue->pool, sizeof(GRAPHICS_BO_QUEUE_EVENT));
	InitializePointQueue(&event_queue->point_queue);
	event_queue->point_queue.elements[POINT_QUEUE_FIRST_ENTRY] = NULL;
}

static eGRAPHICS_STATUS GraphicsBoEventQueueInsertStop(
	GRAPHICS_BO_EVENT_QUEUE* event_queue,
	GRAPHICS_BO_EDGE* edge
)
{
	GRAPHICS_BO_POINT32 point;

	point.y = edge->edge.bottom;
	point.x = LineComputeIntersectionXforY(&edge->edge.line, point.y);
	return GraphicsBoEventQueueInsert(event_queue, GRAPHICS_BO_EVENT_TYPE_STOP,
						edge, NULL, &point);
}

static void GraphicsBoEventQueueFinish(GRAPHICS_BO_EVENT_QUEUE* event_queue)
{
	PointQueueFinish(&event_queue->point_queue);
	GraphicsMemoryPoolFinish(&event_queue->pool);
}

static INLINE eGRAPHICS_STATUS GraphicsBoEventQueueInsertIfIntersectBelowCurrentY(
	GRAPHICS_BO_EVENT_QUEUE* event_queue,
	GRAPHICS_BO_EDGE* left,
	GRAPHICS_BO_EDGE* right
)
{
	GRAPHICS_BO_POINT32 intersection;

	if(LineEqual(&left->edge.line, &right->edge.line))
		return GRAPHICS_STATUS_SUCCESS;

	/* The names "left" and "right" here are correct descriptions of
	 * the order of the two edges within the active edge list. So if a
	 * slope comparison also puts left less than right, then we know
	 * that the intersection of these two segments has already
	 * occurred before the current sweep line position. */
	if(SlopeCompare(left, right) <= 0)
		return GRAPHICS_STATUS_SUCCESS;

	if(FALSE == GraphicsBoEdgeIntersect(left, right, &intersection))
		return GRAPHICS_STATUS_SUCCESS;

	return GraphicsBoEventQueueInsert(event_queue,
				GRAPHICS_BO_EVENT_TYPE_INTERSECTION, left, right, &intersection);
}

static void InitializeGraphicsBoSweepLine(GRAPHICS_BO_SWEEP_LINE* sweep_line)
{
	sweep_line->head = NULL;
	sweep_line->current_y = INT32_MIN;
	sweep_line->current_edge = NULL;
}

static eGRAPHICS_STATUS GraphicsBoSweepLineInsert(
	GRAPHICS_BO_SWEEP_LINE* sweep_line,
	GRAPHICS_BO_EDGE* edge
)
{
	if(sweep_line->current_edge != NULL)
	{
		GRAPHICS_BO_EDGE *prev, *next;
		int cmp;

		cmp = GraphicsBoSweepLineCompareEdges(sweep_line,
							sweep_line->current_edge, edge);
		if(cmp < 0)
		{
			prev = sweep_line->current_edge;
			next = prev->next;
			while(next != NULL &&
					GraphicsBoSweepLineCompareEdges(sweep_line, next, edge) < 0)
			{
				prev = next, next = prev->next;
			}

			prev->next = edge;
			edge->prev = prev;
			edge->next = next;
			if(next != NULL)
				next->prev = edge;
		}
		else if(cmp > 0)
		{
			next = sweep_line->current_edge;
			prev = next->prev;
			while(prev != NULL &&
					GraphicsBoSweepLineCompareEdges(sweep_line, prev, edge) > 0)
			{
				next = prev, prev = next->prev;
			}

			next->prev = edge;
			edge->next = next;
			edge->prev = prev;
			if(prev != NULL)
				prev->next = edge;
			else
				sweep_line->head = edge;
		}
		else
		{
			prev = sweep_line->current_edge;
			edge->prev = prev;
			edge->next = prev->next;
			if(prev->next != NULL)
				prev->next->prev = edge;
			prev->next = edge;
		}
	}
	else
	{
		sweep_line->head = edge;
	}

	sweep_line->current_edge = edge;

	return GRAPHICS_STATUS_SUCCESS;
}

static void GraphicsBoSweepLineDelete(
	GRAPHICS_BO_SWEEP_LINE* sweep_line,
	GRAPHICS_BO_EDGE* edge
)
{
	if(edge->prev != NULL)
		edge->prev->next = edge->next;
	else
		sweep_line->head = edge->next;

	if(edge->next != NULL)
		edge->next->prev = edge->prev;

	if(sweep_line->current_edge == edge)
		sweep_line->current_edge = edge->prev ? edge->prev : edge->next;
}

static void GraphicsBoSweepLineSwap(
	GRAPHICS_BO_SWEEP_LINE* sweep_line,
	GRAPHICS_BO_EDGE* left,
	GRAPHICS_BO_EDGE* right
)
{
	if(left->prev != NULL)
		left->prev->next = right;
	else
		sweep_line->head = right;

	if(right->next != NULL)
		right->next->prev = left;

	left->next = right->next;
	right->next = left;

	right->prev = left->prev;
	left->prev = right;
}

static INLINE int EdgesColinear(const GRAPHICS_BO_EDGE* a, const GRAPHICS_BO_EDGE* b)
{
	if(LineEqual(&a->edge.line, &b->edge.line))
		return TRUE;

	if(SlopeCompare(a, b))
		return FALSE;

	/* The choice of y is not truly arbitrary since we must guarantee that it
	 * is greater than the start of either line.
	 */
	if(a->edge.line.point1.y == b->edge.line.point1.y)
	{
		return a->edge.line.point1.x == b->edge.line.point1.x;
	}
	else if(a->edge.line.point2.y == b->edge.line.point2.y)
	{
		return a->edge.line.point2.x == b->edge.line.point2.x;
	}
	else if(a->edge.line.point1.y < b->edge.line.point1.y)
	{
		return EdgeCompareForYagainstX(b, a->edge.line.point1.y,
					a->edge.line.point1.x) == 0;
	}
	else
	{
		return EdgeCompareForYagainstX(a, b->edge.line.point1.y,
					b->edge.line.point1.x) == 0;
	}
}

static void GraphicsBoEdgeEnd(
	GRAPHICS_BO_EDGE* left,

	int32 bot,
	GRAPHICS_POLYGON* polygon
)
{
	GRAPHICS_BO_DEFERRED *d = &left->deferred;

	if(LIKELY(d->top < bot))
	{
		GraphicsPolygonAddLine(polygon,
				 &left->edge.line, d->top, bot, 1);
		GraphicsPolygonAddLine(polygon,
				 &d->right->edge.line, d->top, bot, -1);
	}

	d->right = NULL;
}


static INLINE void GraphicsBoEdgeStartOrContinue(
	GRAPHICS_BO_EDGE* left,
	GRAPHICS_BO_EDGE* right,
	int top,
	GRAPHICS_POLYGON* polygon
)
{
	if(left->deferred.right == right)
		return;

	if(left->deferred.right != NULL)
	{
		if(right != NULL && EdgesColinear(left->deferred.right, right))
		{
			/* continuation on right, so just swap edges */
			left->deferred.right = right;
			return;
		}

		GraphicsBoEdgeEnd(left, top, polygon);
	}

	if(right != NULL && FALSE == EdgesColinear(left, right))
	{
		left->deferred.top = top;
		left->deferred.right = right;
	}
}

static INLINE void ActiveEdgesToPolygon(
	GRAPHICS_BO_EDGE* left,
	int32 top,
	eGRAPHICS_FILL_RULE fill_rule,
	GRAPHICS_POLYGON* polygon
)
{
	GRAPHICS_BO_EDGE *right;
	unsigned int mask;

	if(fill_rule == GRAPHICS_FILL_RULE_WINDING)
		mask = ~0;
	else
		mask = 1;

	while(left != NULL)
	{
		int in_out = left->edge.direction;

		right = left->next;
		if(left->deferred.right == NULL)
		{
			while(right != NULL && right->deferred.right == NULL)
				right = right->next;

			if(right != NULL && EdgesColinear (left, right))
			{
				/* continuation on left */
				left->deferred = right->deferred;
				right->deferred.right = NULL;
			}
		}

		right = left->next;
		while(right != NULL)
		{
			if(right->deferred.right != NULL)
				GraphicsBoEdgeEnd(right, top, polygon);

			in_out += right->edge.direction;
			if((in_out & mask) == 0)
			{
				/* skip co-linear edges */
				if(right->next == NULL || FALSE == EdgesColinear(right, right->next))
					break;
			}

			right = right->next;
		}

		GraphicsBoEdgeStartOrContinue(left, right, top, polygon);

		left = right;
		if(left != NULL)
			left = left->next;
	}
}

static eGRAPHICS_STATUS GraphicsBentleyOttmannTessellateBoEdges(
	GRAPHICS_BO_EVENT** start_events,
	int num_events,
	eGRAPHICS_FILL_RULE fill_rule,
	GRAPHICS_POLYGON* polygon
)
{
	eGRAPHICS_STATUS status = GRAPHICS_STATUS_SUCCESS; /* silence compiler */
	GRAPHICS_BO_EVENT_QUEUE event_queue;
	GRAPHICS_BO_SWEEP_LINE sweep_line;
	GRAPHICS_BO_EVENT *event;
	GRAPHICS_BO_EDGE *left, *right;
	GRAPHICS_BO_EDGE *edge1, *edge2;

	InitializeGraphicsBoEventQueue(&event_queue, start_events, num_events);
	InitializeGraphicsBoSweepLine(&sweep_line);

	while((event = GraphicsBoEventDequeue(&event_queue)))
	{
		if(event->point.y != sweep_line.current_y)
		{
			ActiveEdgesToPolygon(sweep_line.head,
						sweep_line.current_y, fill_rule, polygon);

			sweep_line.current_y = event->point.y;
		}

		switch(event->type)
		{
		case GRAPHICS_BO_EVENT_TYPE_START:
			edge1 = &((GRAPHICS_BO_START_EVENT*) event)->edge;

			status = GraphicsBoSweepLineInsert(&sweep_line, edge1);
			if(UNLIKELY(status))
				goto unwind;

			status = GraphicsBoEventQueueInsertStop(&event_queue, edge1);
			if(UNLIKELY(status))
				goto unwind;

			left = edge1->prev;
			right = edge1->next;

			if(left != NULL)
			{
				status = GraphicsBoEventQueueInsertIfIntersectBelowCurrentY(&event_queue, left, edge1);
				if(UNLIKELY(status))
					goto unwind;
			}

			if(right != NULL)
			{
				status = GraphicsBoEventQueueInsertIfIntersectBelowCurrentY(&event_queue, edge1, right);
				if(UNLIKELY(status))
					goto unwind;
			}

			break;

		case GRAPHICS_BO_EVENT_TYPE_STOP:
			edge1 = ((GRAPHICS_BO_QUEUE_EVENT*) event)->edge1;
			GraphicsBoEventQueueDelete(&event_queue, event);

			left = edge1->prev;
			right = edge1->next;

			GraphicsBoSweepLineDelete(&sweep_line, edge1);

			if(edge1->deferred.right != NULL)
				GraphicsBoEdgeEnd(edge1, edge1->edge.bottom, polygon);

			if(left != NULL && right != NULL)
			{
				status = GraphicsBoEventQueueInsertIfIntersectBelowCurrentY(&event_queue, left, right);
				if(UNLIKELY(status))
					goto unwind;
			}

			break;

		case GRAPHICS_BO_EVENT_TYPE_INTERSECTION:
			edge1 = ((GRAPHICS_BO_QUEUE_EVENT*) event)->edge1;
			edge2 = ((GRAPHICS_BO_QUEUE_EVENT*) event)->edge2;
			GraphicsBoEventQueueDelete(&event_queue, event);

			/* skip this intersection if its edges are not adjacent */
			if(edge2 != edge1->next)
				break;

			left = edge1->prev;
			right = edge2->next;

			GraphicsBoSweepLineSwap(&sweep_line, edge1, edge2);

			/* after the swap e2 is left of e1 */

			if(left != NULL)
			{
				status = GraphicsBoEventQueueInsertIfIntersectBelowCurrentY(&event_queue, left, edge2);
				if(UNLIKELY(status))
					goto unwind;
			}

			if(right != NULL)
			{
				status = GraphicsBoEventQueueInsertIfIntersectBelowCurrentY(&event_queue, edge1, right);
				if(UNLIKELY(status))
					goto unwind;
			}

			break;
		}
	}

 unwind:
	GraphicsBoEventQueueFinish(&event_queue);

	return status;
}

eGRAPHICS_STATUS GraphicsPolygonReduce(
	GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE fill_rule
)
{
	eGRAPHICS_STATUS status;
	GRAPHICS_BO_START_EVENT stack_events[STACK_BUFFER_SIZE / sizeof(GRAPHICS_BO_START_EVENT)];
	GRAPHICS_BO_START_EVENT *events;
	GRAPHICS_BO_EVENT *stack_event_ptrs[sizeof(stack_events) / sizeof(stack_events[0]) + 1];
	GRAPHICS_BO_EVENT **event_ptrs;
	int num_limits;
	int num_events;
	int i;

	num_events = polygon->num_edges;
	if(UNLIKELY(0 == num_events))
		return GRAPHICS_STATUS_SUCCESS;

	events = stack_events;
	event_ptrs = stack_event_ptrs;
	if(num_events > sizeof(stack_events) / sizeof(stack_events[0]))
	{
		events = (GRAPHICS_BO_START_EVENT*)MEM_ALLOC_FUNC(
			num_events * (sizeof(GRAPHICS_BO_START_EVENT) + sizeof(GRAPHICS_BO_EVENT*))
				+ sizeof(GRAPHICS_BO_EVENT));
		if(events == NULL)
			return GRAPHICS_STATUS_NO_MEMORY;

		event_ptrs = (GRAPHICS_BO_EVENT**)(events + num_events);
	}

	for(i = 0; i < num_events; i++)
	{
		event_ptrs[i] = (GRAPHICS_BO_EVENT*) &events[i];

		events[i].type = GRAPHICS_BO_EVENT_TYPE_START;
		events[i].point.y = polygon->edges[i].top;
		events[i].point.x =
			LineComputeIntersectionXforY(&polygon->edges[i].line,
											events[i].point.y);

		events[i].edge.edge = polygon->edges[i];
		events[i].edge.deferred.right = NULL;
		events[i].edge.prev = NULL;
		events[i].edge.next = NULL;
	}

	num_limits = polygon->num_limits; polygon->num_limits = 0;
	polygon->num_edges = 0;

	status = GraphicsBentleyOttmannTessellateBoEdges(event_ptrs,
				num_events, fill_rule, polygon);
	polygon->num_limits = num_limits;

	if(events != stack_events)
		MEM_FREE_FUNC(events);

	return status;
}

#ifdef __cplusplus
}
#endif
