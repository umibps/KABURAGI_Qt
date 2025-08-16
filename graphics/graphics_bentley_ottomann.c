#include <string.h>
#include <setjmp.h>
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

#define DEBUG_PRINT_STATE 0
#define DEBUG_EVENTS 0
#define DEBUG_TRAPS 0

typedef GRAPHICS_POINT GRAPHICS_BO_POINT32;

typedef struct _GRAPHICS_BO_INTERSECT_ORDOMATE
{
	int32 ordinate;
	enum
	{
		EXACT,
		INEXACT
	} exactness;
} GRAPHICS_BO_INTERSECT_ORDOMATE;

typedef struct _GRAPHICS_BO_INTERSECT_POINT
{
	GRAPHICS_BO_INTERSECT_ORDOMATE x;
	GRAPHICS_BO_INTERSECT_ORDOMATE y;
} GRAPHICS_BO_INTERSECT_POINT;

typedef struct _GRAPHICS_BO_EDGE GRAPHICS_BO_EDGE;
typedef struct _GRAPHICS_BO_TRAP GRAPHICS_BO_TRAP;

/* A deferred trapezoid of an edge */
struct _GRAPHICS_BO_TRAP
{
	GRAPHICS_BO_EDGE *right;
	int32 top;
};

struct _GRAPHICS_BO_EDGE
{
	GRAPHICS_EDGE edge;
	GRAPHICS_BO_EDGE *prev;
	GRAPHICS_BO_EDGE *next;
	GRAPHICS_BO_EDGE *colinear;
	GRAPHICS_BO_TRAP deferred_trap;
};

/* the parent is always given by index/2 */
#define PQ_PARENT_INDEX(i) ((i) >> 1)
#define PQ_FIRST_ENTRY 1

/* left and right children are index * 2 and (index * 2) +1 respectively */
#define PQ_LEFT_CHILD_INDEX(i) ((i) << 1)

typedef enum _eGRAPHICS_BO_EVENT_TYPE
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
	GRAPHICS_BO_EDGE *e1;
	GRAPHICS_BO_EDGE *e2;
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
	POINT_QUEUE pqueue;
	GRAPHICS_BO_EVENT **start_events;
} GRAPHICS_BO_EVENT_QUEUE;

typedef struct _GRAPHICS_BO_SWEEP_LINE {
	GRAPHICS_BO_EDGE *head;
	GRAPHICS_BO_EDGE *stopped;
	int32 current_y;
	GRAPHICS_BO_EDGE *current_edge;
} GRAPHICS_BO_SWEEP_LINE;

#if DEBUG_TRAPS
static void dump_traps(GRAPHICS_TRAPS* traps, const char* filename)
{
	FILE *file;
	cairo_box_t extents;
	int n;

	if (getenv ("CAIRO_DEBUG_TRAPS") == NULL)
		return;

#if 0
	if (traps->has_limits) {
		printf ("%s: limits=(%d, %d, %d, %d)\n",
			filename,
			traps->limits.p1.x, traps->limits.p1.y,
			traps->limits.p2.x, traps->limits.p2.y);
	}
#endif
	_cairo_traps_extents (traps, &extents);
	printf ("%s: extents=(%d, %d, %d, %d)\n",
		filename,
		extents.p1.x, extents.p1.y,
		extents.p2.x, extents.p2.y);

	file = fopen (filename, "a");
	if (file != NULL) {
		for (n = 0; n < traps->num_traps; n++) {
			fprintf (file, "%d %d L:(%d, %d), (%d, %d) R:(%d, %d), (%d, %d)\n",
				traps->traps[n].top,
				traps->traps[n].bottom,
				traps->traps[n].left.p1.x,
				traps->traps[n].left.p1.y,
				traps->traps[n].left.p2.x,
				traps->traps[n].left.p2.y,
				traps->traps[n].right.p1.x,
				traps->traps[n].right.p1.y,
				traps->traps[n].right.p2.x,
				traps->traps[n].right.p2.y);
		}
		fprintf (file, "\n");
		fclose (file);
	}
}

static void dump_edges(
	GRAPHICS_BO_START_EVENT* events,
	int num_edges,
	const char* filename
)
{
	FILE *file;
	int n;

	if (getenv ("CAIRO_DEBUG_TRAPS") == NULL)
		return;

	file = fopen (filename, "a");
	if (file != NULL) {
		for (n = 0; n < num_edges; n++) {
			fprintf (file, "(%d, %d), (%d, %d) %d %d %d\n",
				events[n].edge.edge.line.p1.x,
				events[n].edge.edge.line.p1.y,
				events[n].edge.edge.line.p2.x,
				events[n].edge.edge.line.p2.y,
				events[n].edge.edge.top,
				events[n].edge.edge.bottom,
				events[n].edge.edge.dir);
		}
		fprintf (file, "\n");
		fclose (file);
	}
}
#endif

static GRAPHICS_FLOAT_FIXED LineComputeIntersectionXforY(
	const GRAPHICS_LINE* line,
	GRAPHICS_FLOAT_FIXED y
)
{
	GRAPHICS_FLOAT_FIXED x, dy;

	if (y == line->point1.y)
		return line->point1.x;
	if (y == line->point2.y)
		return line->point2.x;

	x = line->point1.x;
	dy = line->point2.y - line->point1.y;
	if (dy != 0) {
		x += GraphicsFixedMultiDivideFloor(y - line->point1.y,
			line->point2.x - line->point1.x, dy);
	}

	return x;
}

static INLINE int GraphicsBoPoint32Compare (GRAPHICS_BO_POINT32 const* a, GRAPHICS_BO_POINT32 const* b)
{
	int cmp;

	cmp = a->y - b->y;
	if (cmp)
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
static INLINE int _slope_compare(const GRAPHICS_BO_EDGE* a, const GRAPHICS_BO_EDGE* b)
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
	if (adx == 0)
		return -bdx;
	if (bdx == 0)
		return adx;

	/* Then where the two edges point in different directions wrt x. */
	if ((adx ^ bdx) < 0)
		return adx;

	/* Finally we actually need to do the general comparison. */
	{
		int32 ady = a->edge.line.point2.y - a->edge.line.point1.y;
		int32 bdy = b->edge.line.point2.y - b->edge.line.point1.y;
		int64 adx_bdy = GRAPHICS_INT32x32_64_MULTI(adx, bdy);
		int64 bdx_ady = GRAPHICS_INT32x32_64_MULTI(bdx, ady);

		return GRAPHICS_INT64_COMPARE(adx_bdy, bdx_ady);
	}
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
static int
edge_compare_for_y_against_x (const GRAPHICS_BO_EDGE* a, int32 y, int32 x)
{
	int32 adx, ady;
	int32 dx, dy;
	int64 L, R;

	if (x < a->edge.line.point1.x && x < a->edge.line.point2.x)
		return 1;
	if (x > a->edge.line.point1.x && x > a->edge.line.point2.x)
		return -1;

	adx = a->edge.line.point2.x - a->edge.line.point1.x;
	dx = x - a->edge.line.point1.x;

	if (adx == 0)
		return -dx;
	if (dx == 0 || (adx ^ dx) < 0)
		return adx;

	dy = y - a->edge.line.point1.y;
	ady = a->edge.line.point2.y - a->edge.line.point1.y;

	L = GRAPHICS_INT32x32_64_MULTI(dy, adx);
	R = GRAPHICS_INT32x32_64_MULTI(dx, ady);

	return GRAPHICS_INT64_COMPARE(L, R);
}

static INLINE int GraphicsBoSweepLineCompareEdges(
	const GRAPHICS_BO_SWEEP_LINE* sweep_line,
	const GRAPHICS_BO_EDGE* a,
	const GRAPHICS_BO_EDGE* b
)
{
	int cmp;

	cmp =  GraphicsLinesCompareAtY(&a->edge.line,
		&b->edge.line,
		sweep_line->current_y);
	if (cmp)
		return cmp;

	/* We've got two collinear edges now. */
	return b->edge.bottom - a->edge.bottom;
}

static INLINE int64 det32_64(int32 a, int32 b, int32 c, int32 d)
{
	/* det = a * d - b * c */
	return INTEGER32x32_64_MULTI(a, d) - INTEGER32x32_64_MULTI(b, c);
}

static INLINE int128 det64x32_128(int64 a, int32 b, int64 c, int64 d)
{
	/* det = a * d - b * c */
	return INTEGER128_SUB(GRAPHICS_INT64x32_128_MULTI(a, d),
		GRAPHICS_INT64x32_128_MULTI(c, b));
}

/* Compute the intersection of two lines as defined by two edges. The
* result is provided as a coordinate pair of 128-bit integers.
*
* Returns %CAIRO_BO_STATUS_INTERSECTION if there is an intersection or
* %CAIRO_BO_STATUS_PARALLEL if the two lines are exactly parallel.
*/
static int intersect_lines(
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

	den_det = det32_64 (dx1, dy1, dx2, dy2);

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
	R = det32_64(dx2, dy2,
		b->edge.line.point1.x - a->edge.line.point1.x,
		b->edge.line.point1.y - a->edge.line.point1.y);
	if(den_det < 0)
	{
		if(den_det >= R)
		{
			return FALSE;
		}
	}
	else
	{
		if(den_det <= R)
		{
			return FALSE;
		}
	}

	R = det32_64(dy1, dx1,
		a->edge.line.point1.y - b->edge.line.point1.y,
		a->edge.line.point1.x - b->edge.line.point1.x);
	if(den_det < 0)
	{
		if(den_det >= R)
		{
			return FALSE;
		}
	}
	else
	{
		if(den_det <= R)
		{
			return FALSE;
		}
	}

	/* We now know that the two lines should intersect within range. */

	a_det = det32_64(a->edge.line.point1.x, a->edge.line.point1.y,
		a->edge.line.point2.x, a->edge.line.point2.y);
	b_det = det32_64(b->edge.line.point1.x, b->edge.line.point1.y,
		b->edge.line.point2.x, b->edge.line.point2.y);

	/* x = det (a_det, dx1, b_det, dx2) / den_det */
	qr = GraphicsInteger96by64_32x64_DivideRemain(det64x32_128(a_det, dx1, b_det, dx2), den_det);
	if(qr.rem == den_det)
	{
		return FALSE;
	}
#if 0
	intersection->x.exactness = _cairo_int64_is_zero (qr.rem) ? EXACT : INEXACT;
#else
	intersection->x.exactness = EXACT;
	if (qr.rem != 0)
	{
		if((den_det < 0) ^ (qr.rem < 0))
		{
			qr.rem = -qr.rem;
		}
		qr.rem =  qr.rem * 2;
		if(qr.rem >= den_det)
		{
			qr.quo = qr.quo + (qr.quo < 0 ? -1 : 1);
		}
		else
		{
			intersection->x.exactness = INEXACT;
		}
	}
#endif
	intersection->x.ordinate = (int32)qr.quo;

	/* y = det (a_det, dy1, b_det, dy2) / den_det */
	qr = GraphicsInteger96by64_32x64_DivideRemain(det64x32_128 (a_det, dy1, b_det, dy2), den_det);
	if(qr.rem == den_det)
	{
		return FALSE;
	}
#if 0
	intersection->y.exactness = _cairo_int64_is_zero (qr.rem) ? EXACT : INEXACT;
#else
	intersection->y.exactness = EXACT;
	if (qr.rem != 0)
	{
		if((den_det < 0) ^ (qr.rem < 0))
		{
			qr.rem = -qr.rem;
		}
		qr.rem = qr.rem * 2;
		if(qr.rem >= den_det)
		{
			qr.quo = qr.quo + (qr.quo < 0 ? -1 : 1);
		}
		else
		{
			intersection->y.exactness = INEXACT;
		}
	}
#endif
	intersection->y.ordinate = (int32)(qr.quo);

	return TRUE;
}

static int GraphicsBoIntersectOrdomate32Compare(GRAPHICS_BO_INTERSECT_ORDOMATE	a,
	int32			b)
{
	/* First compare the quotient */
	if (a.ordinate > b)
		return +1;
	if (a.ordinate < b)
		return -1;
	/* With quotient identical, if remainder is 0 then compare equal */
	/* Otherwise, the non-zero remainder makes a > b */
	return INEXACT == a.exactness;
}

/* Does the given edge contain the given point. The point must already
* be known to be contained within the line determined by the edge,
* (most LIKELYthe point results from an intersection of this edge
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
static int GraphicsBoEdgeContainsIntersectPoint(
	GRAPHICS_BO_EDGE* edge,
	GRAPHICS_BO_INTERSECT_POINT* point
)
{
	int cmp_top, cmp_bottom;

	/* XXX: When running the actual algorithm, we don't actually need to
	* compare against edge->top at all here, since any intersection above
	* top is eliminated early via a slope comparison. We're leaving these
	* here for now only for the sake of the quadratic-time intersection
	* finder which needs them.
	*/

	cmp_top = GraphicsBoIntersectOrdomate32Compare(point->y, edge->edge.top);
	cmp_bottom = GraphicsBoIntersectOrdomate32Compare(point->y, edge->edge.bottom);

	if (cmp_top < 0 || cmp_bottom > 0)
	{
		return FALSE;
	}

	if (cmp_top > 0 && cmp_bottom < 0)
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

	if (cmp_top == 0) {
		GRAPHICS_FLOAT_FIXED top_x;

		top_x = LineComputeIntersectionXforY (&edge->edge.line,
			edge->edge.top);
		return GraphicsBoIntersectOrdomate32Compare (point->x, top_x) > 0;
	} else { /* cmp_bottom == 0 */
		GRAPHICS_FLOAT_FIXED bot_x;

		bot_x = LineComputeIntersectionXforY (&edge->edge.line,
			edge->edge.bottom);
		return GraphicsBoIntersectOrdomate32Compare (point->x, bot_x) < 0;
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

	if (! intersect_lines (a, b, &quorem))
		return FALSE;

	if (! GraphicsBoEdgeContainsIntersectPoint (a, &quorem))
		return FALSE;

	if (! GraphicsBoEdgeContainsIntersectPoint (b, &quorem))
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

	cmp = GraphicsBoPoint32Compare (&a->point, &b->point);
	if (cmp)
		return cmp;

	cmp = a->type - b->type;
	if (cmp)
		return cmp;

	return (a == b) ? 0 : (a < 0) ? -1 : 1;
}

static INLINE void _pqueue_init(POINT_QUEUE* pq)
{
	pq->max_size = sizeof(pq->elements_embedded) / sizeof(pq->elements_embedded[0]);
	pq->size = 0;

	pq->elements = pq->elements_embedded;
}

static INLINE void _pqueue_fini(POINT_QUEUE* pq)
{
	if (pq->elements != pq->elements_embedded)
		free (pq->elements);
}

static eGRAPHICS_STATUS _pqueue_grow(POINT_QUEUE* pq)
{
	GRAPHICS_BO_EVENT **new_elements;
	pq->max_size *= 2;

	if (pq->elements == pq->elements_embedded)
	{
		new_elements = MEM_ALLOC_FUNC(pq->max_size * sizeof (GRAPHICS_BO_EVENT *));
		if(UNLIKELY(new_elements == NULL))
		{
			return GRAPHICS_STATUS_NO_MEMORY;
		}

		(void)memcpy(new_elements, pq->elements_embedded, sizeof(pq->elements_embedded));
	}
	else
	{
		new_elements = MEM_REALLOC_FUNC(pq->elements, pq->max_size * sizeof (GRAPHICS_BO_EVENT *));
		if(UNLIKELY(new_elements == NULL))
		{
			return GRAPHICS_STATUS_NO_MEMORY;
		}
	}

	pq->elements = new_elements;
	return GRAPHICS_STATUS_SUCCESS;
}

static INLINE eGRAPHICS_STATUS _pqueue_push(POINT_QUEUE* pq, GRAPHICS_BO_EVENT* event)
{
	GRAPHICS_BO_EVENT **elements;
	int i, parent;

	if (UNLIKELY (pq->size + 1 == pq->max_size)) {
		eGRAPHICS_STATUS status;

		status = _pqueue_grow (pq);
		if (UNLIKELY (status))
			return status;
	}

	elements = pq->elements;

	for (i = ++pq->size;
		i != PQ_FIRST_ENTRY &&
		GraphicsBoEventCompare (event,
			elements[parent = PQ_PARENT_INDEX (i)]) < 0;
		i = parent)
	{
		elements[i] = elements[parent];
	}

	elements[i] = event;

	return GRAPHICS_STATUS_SUCCESS;
}

static INLINE void _pqueue_pop(POINT_QUEUE* pq)
{
	GRAPHICS_BO_EVENT **elements = pq->elements;
	GRAPHICS_BO_EVENT *tail;
	int child, i;

	tail = elements[pq->size--];
	if (pq->size == 0) {
		elements[PQ_FIRST_ENTRY] = NULL;
		return;
	}

	for (i = PQ_FIRST_ENTRY;
		(child = PQ_LEFT_CHILD_INDEX (i)) <= pq->size;
		i = child)
	{
		if (child != pq->size &&
			GraphicsBoEventCompare (elements[child+1],
				elements[child]) < 0)
		{
			child++;
		}

		if (GraphicsBoEventCompare (elements[child], tail) >= 0)
			break;

		elements[i] = elements[child];
	}
	elements[i] = tail;
}

static INLINE eGRAPHICS_STATUS GraphicsBoEventQueueInsert(
	GRAPHICS_BO_EVENT_QUEUE* queue,
	eGRAPHICS_BO_EVENT_TYPE	 type,
	GRAPHICS_BO_EDGE* e1,
	GRAPHICS_BO_EDGE* e2,
	const GRAPHICS_POINT* point)
{
	GRAPHICS_BO_QUEUE_EVENT *event;

	event = GraphicsMemoryPoolAllocation(&queue->pool);
	if(UNLIKELY (event == NULL))
	{
		return GRAPHICS_STATUS_NO_MEMORY;
	}

	event->type = type;
	event->e1 = e1;
	event->e2 = e2;
	event->point = *point;

	return _pqueue_push(&queue->pqueue, (GRAPHICS_BO_EVENT*)event);
}

static void GraphicsBoEventQueueDelete(GRAPHICS_BO_EVENT_QUEUE* queue,	GRAPHICS_BO_EVENT* event)
{
	GraphicsMemoryPoolFree(&queue->pool, event);
}

static GRAPHICS_BO_EVENT* GraphicsBoEventDequeue(GRAPHICS_BO_EVENT_QUEUE* event_queue)
{
	GRAPHICS_BO_EVENT *event, *cmp;

	event = event_queue->pqueue.elements[PQ_FIRST_ENTRY];
	cmp = *event_queue->start_events;
	if (event == NULL ||
		(cmp != NULL && GraphicsBoEventCompare (cmp, event) < 0))
	{
		event = cmp;
		event_queue->start_events++;
	}
	else
	{
		_pqueue_pop (&event_queue->pqueue);
	}

	return event;
}

GRAPHICS_COMBSORT_DECLARE(
	GraphicsBoEventQueueSort,
	GRAPHICS_BO_EVENT*,
	GraphicsBoEventCompare
)

static void InitializeGraphicsBoEventQueue(
	GRAPHICS_BO_EVENT_QUEUE	* event_queue,
	GRAPHICS_BO_EVENT** start_events,
	int num_events
)
{
	event_queue->start_events = start_events;

	InitializeGraphicsMemoryPool(&event_queue->pool,
		sizeof (GRAPHICS_BO_QUEUE_EVENT));
	_pqueue_init (&event_queue->pqueue);
	event_queue->pqueue.elements[PQ_FIRST_ENTRY] = NULL;
}

static eGRAPHICS_STATUS GraphicsBoEventQueuensertStop(GRAPHICS_BO_EVENT_QUEUE* event_queue, GRAPHICS_BO_EDGE* edge)
{
	GRAPHICS_BO_POINT32 point;

	point.y = edge->edge.bottom;
	point.x = LineComputeIntersectionXforY (&edge->edge.line,
		point.y);
	return GraphicsBoEventQueueInsert (event_queue,
		GRAPHICS_BO_EVENT_TYPE_STOP,
		edge, NULL,
		&point);
}

static void GraphicsBoEventQueueFinish(GRAPHICS_BO_EVENT_QUEUE *event_queue)
{
	_pqueue_fini (&event_queue->pqueue);
	GraphicsMemoryPoolFinish(&event_queue->pool);
}

static INLINE eGRAPHICS_STATUS GraphicsBoEventQueueInsertIfIntersectBelowCurrentY(
	GRAPHICS_BO_EVENT_QUEUE* event_queue,
	GRAPHICS_BO_EDGE* left,
	GRAPHICS_BO_EDGE* right
)
{
	GRAPHICS_BO_POINT32 intersection;

	if(MAXIMUM(left->edge.line.point1.x, left->edge.line.point2.x) <=
		MINIMUM(right->edge.line.point1.x, right->edge.line.point2.x))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(GraphicsLinesEqual(&left->edge.line, &right->edge.line))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	/* The names "left" and "right" here are correct descriptions of
	* the order of the two edges within the active edge list. So if a
	* slope comparison also puts left less than right, then we know
	* that the intersection of these two segments has already
	* occurred before the current sweep line position. */
	if (_slope_compare (left, right) <= 0)
		return GRAPHICS_STATUS_SUCCESS;

	if (! GraphicsBoEdgeIntersect (left, right, &intersection))
		return GRAPHICS_STATUS_SUCCESS;

	return GraphicsBoEventQueueInsert (event_queue,
		GRAPHICS_BO_EVENT_TYPE_INTERSECTION,
		left, right,
		&intersection);
}

static void InitializeGraphicsBoSweepLine(GRAPHICS_BO_SWEEP_LINE* sweep_line)
{
	sweep_line->head = NULL;
	sweep_line->stopped = NULL;
	sweep_line->current_y = INT32_MIN;
	sweep_line->current_edge = NULL;
}

static void GraphicsBoSweepLineInsert(GRAPHICS_BO_SWEEP_LINE* sweep_line, GRAPHICS_BO_EDGE* edge)
{
	if (sweep_line->current_edge != NULL) {
		GRAPHICS_BO_EDGE *prev, *next;
		int cmp;

		cmp = GraphicsBoSweepLineCompareEdges (sweep_line,
			sweep_line->current_edge,
			edge);
		if (cmp < 0) {
			prev = sweep_line->current_edge;
			next = prev->next;
			while (next != NULL &&
				GraphicsBoSweepLineCompareEdges (sweep_line,
					next, edge) < 0)
			{
				prev = next, next = prev->next;
			}

			prev->next = edge;
			edge->prev = prev;
			edge->next = next;
			if (next != NULL)
				next->prev = edge;
		} else if (cmp > 0) {
			next = sweep_line->current_edge;
			prev = next->prev;
			while (prev != NULL &&
				GraphicsBoSweepLineCompareEdges (sweep_line,
					prev, edge) > 0)
			{
				next = prev, prev = next->prev;
			}

			next->prev = edge;
			edge->next = next;
			edge->prev = prev;
			if (prev != NULL)
				prev->next = edge;
			else
				sweep_line->head = edge;
		} else {
			prev = sweep_line->current_edge;
			edge->prev = prev;
			edge->next = prev->next;
			if (prev->next != NULL)
				prev->next->prev = edge;
			prev->next = edge;
		}
	} else {
		sweep_line->head = edge;
		edge->next = NULL;
	}

	sweep_line->current_edge = edge;
}

static void GraphicsBoSweepLineDelete(GRAPHICS_BO_SWEEP_LINE* sweep_line, GRAPHICS_BO_EDGE* edge)
{
	if (edge->prev != NULL)
		edge->prev->next = edge->next;
	else
		sweep_line->head = edge->next;

	if (edge->next != NULL)
		edge->next->prev = edge->prev;

	if (sweep_line->current_edge == edge)
		sweep_line->current_edge = edge->prev ? edge->prev : edge->next;
}

static void GraphicsBoSweepLineSwap(
	GRAPHICS_BO_SWEEP_LINE* sweep_line,
	GRAPHICS_BO_EDGE* left,
	GRAPHICS_BO_EDGE* right
)
{
	if (left->prev != NULL)
		left->prev->next = right;
	else
		sweep_line->head = right;

	if (right->next != NULL)
		right->next->prev = left;

	left->next = right->next;
	right->next = left;

	right->prev = left->prev;
	left->prev = right;
}

#if DEBUG_PRINT_STATE
static void
_GRAPHICS_BO_EDGE_print (GRAPHICS_BO_EDGE *edge)
{
	printf ("(0x%x, 0x%x)-(0x%x, 0x%x)",
		edge->edge.line.p1.x, edge->edge.line.p1.y,
		edge->edge.line.p2.x, edge->edge.line.p2.y);
}

static void
_GRAPHICS_BO_EVENT_print (GRAPHICS_BO_EVENT *event)
{
	switch (event->type) {
	case GRAPHICS_BO_EVENT_TYPE_START:
		printf ("Start: ");
		break;
	case GRAPHICS_BO_EVENT_TYPE_STOP:
		printf ("Stop: ");
		break;
	case GRAPHICS_BO_EVENT_TYPE_INTERSECTION:
		printf ("Intersection: ");
		break;
	}
	printf ("(%d, %d)\t", event->point.x, event->point.y);
	_GRAPHICS_BO_EDGE_print (event->e1);
	if (event->type == GRAPHICS_BO_EVENT_TYPE_INTERSECTION) {
		printf (" X ");
		_GRAPHICS_BO_EDGE_print (event->e2);
	}
	printf ("\n");
}

static void
_GRAPHICS_BO_EVENT_QUEUE_print (GRAPHICS_BO_EVENT_QUEUE *event_queue)
{
	/* XXX: fixme to print the start/stop array too. */
	printf ("Event queue:\n");
}

static void
_GRAPHICS_BO_SWEEP_LINE_print (GRAPHICS_BO_SWEEP_LINE *sweep_line)
{
	cairo_bool_t first = TRUE;
	GRAPHICS_BO_EDGE *edge;

	printf ("Sweep line from edge list: ");
	first = TRUE;
	for (edge = sweep_line->head;
		edge;
		edge = edge->next)
	{
		if (!first)
			printf (", ");
		_GRAPHICS_BO_EDGE_print (edge);
		first = FALSE;
	}
	printf ("\n");
}

static void
print_state (const char			*msg,
	GRAPHICS_BO_EVENT		*event,
	GRAPHICS_BO_EVENT_QUEUE	*event_queue,
	GRAPHICS_BO_SWEEP_LINE	*sweep_line)
{
	printf ("%s ", msg);
	_GRAPHICS_BO_EVENT_print (event);
	_GRAPHICS_BO_EVENT_QUEUE_print (event_queue);
	_GRAPHICS_BO_SWEEP_LINE_print (sweep_line);
	printf ("\n");
}
#endif

#if DEBUG_EVENTS
static void CAIRO_PRINTF_FORMAT (1, 2)
event_log (const char *fmt, ...)
{
	FILE *file;

	if (getenv ("CAIRO_DEBUG_EVENTS") == NULL)
		return;

	file = fopen ("bo-events.txt", "a");
	if (file != NULL) {
		va_list ap;

		va_start (ap, fmt);
		vfprintf (file, fmt, ap);
		va_end (ap);

		fclose (file);
	}
}
#endif

#define HAS_COLINEAR(a, b) ((GRAPHICS_BO_EDGE *)(((uintptr_t)(a))&~1) == (b))
#define IS_COLINEAR(e) (((uintptr_t)(e))&1)
#define MARK_COLINEAR(e, v) ((GRAPHICS_BO_EDGE *)(((uintptr_t)(e))|(v)))

static INLINE int edges_colinear(GRAPHICS_BO_EDGE* a, const GRAPHICS_BO_EDGE* b)
{
	unsigned p;

	if (HAS_COLINEAR(a->colinear, b))
		return IS_COLINEAR(a->colinear);

	if (HAS_COLINEAR(b->colinear, a)) {
		p = IS_COLINEAR(b->colinear);
		a->colinear = MARK_COLINEAR(b, p);
		return p;
	}

	p = 0;
	p |= (a->edge.line.point1.x == b->edge.line.point1.x) << 0;
	p |= (a->edge.line.point1.y == b->edge.line.point1.y) << 1;
	p |= (a->edge.line.point2.x == b->edge.line.point2.x) << 3;
	p |= (a->edge.line.point2.y == b->edge.line.point2.y) << 4;
	if (p == ((1 << 0) | (1 << 1) | (1 << 3) | (1 << 4))) {
		a->colinear = MARK_COLINEAR(b, 1);
		return TRUE;
	}

	if (_slope_compare (a, b)) {
		a->colinear = MARK_COLINEAR(b, 0);
		return FALSE;
	}

	/* The choice of y is not truly arbitrary since we must guarantee that it
	* is greater than the start of either line.
	*/
	if (p != 0) {
		/* colinear if either end-point are coincident */
		p = (((p >> 1) & p) & 5) != 0;
	} else if (a->edge.line.point1.y < b->edge.line.point1.y) {
		p = edge_compare_for_y_against_x (b,
			a->edge.line.point1.y,
			a->edge.line.point1.x) == 0;
	} else {
		p = edge_compare_for_y_against_x (a,
			b->edge.line.point1.y,
			b->edge.line.point1.x) == 0;
	}

	a->colinear = MARK_COLINEAR(b, p);
	return p;
}

/* Adds the trapezoid, if any, of the left edge to the #GRAPHICS_TRAPS */
static void GraphicsBoEdgeEndTrap(
	GRAPHICS_BO_EDGE* left,
	int32 bot,
	GRAPHICS_TRAPS* traps
)
{
	GRAPHICS_BO_TRAP *trap = &left->deferred_trap;

	/* Only emit (trivial) non-degenerate trapezoids with positive height. */
	if(LIKELY(trap->top < bot))
	{
		GraphicsTrapsAddTrap (traps,
			trap->top, bot,
			&left->edge.line, &trap->right->edge.line);

#if DEBUG_PRINT_STATE
		printf ("Deferred trap: left=(%x, %x)-(%x,%x) "
			"right=(%x,%x)-(%x,%x) top=%x, bot=%x\n",
			left->edge.line.p1.x, left->edge.line.p1.y,
			left->edge.line.p2.x, left->edge.line.p2.y,
			trap->right->edge.line.p1.x, trap->right->edge.line.p1.y,
			trap->right->edge.line.p2.x, trap->right->edge.line.p2.y,
			trap->top, bot);
#endif
#if DEBUG_EVENTS
		event_log ("end trap: %lu %lu %d %d\n",
			(long) left,
			(long) trap->right,
			trap->top,
			bot);
#endif
	}

	trap->right = NULL;
}


/* Start a new trapezoid at the given top y coordinate, whose edges
* are `edge' and `edge->next'. If `edge' already has a trapezoid,
* then either add it to the traps in `traps', if the trapezoid's
* right edge differs from `edge->next', or do nothing if the new
* trapezoid would be a continuation of the existing one. */
static INLINE void GraphicsBoEdgeStartOrContinueTrap(
	GRAPHICS_BO_EDGE* left,
	GRAPHICS_BO_EDGE* right,
	int top,
	GRAPHICS_TRAPS* traps
)
{
	if (left->deferred_trap.right == right)
		return;

	ASSERT(right);
	if (left->deferred_trap.right != NULL) {
		if (edges_colinear (left->deferred_trap.right, right))
		{
			/* continuation on right, so just swap edges */
			left->deferred_trap.right = right;
			return;
		}

		GraphicsBoEdgeEndTrap (left, top, traps);
	}

	if (! edges_colinear (left, right)) {
		left->deferred_trap.top = top;
		left->deferred_trap.right = right;

#if DEBUG_EVENTS
		event_log ("begin trap: %lu %lu %d\n",
			(long) left,
			(long) right,
			top);
#endif
	}
}

static INLINE void _active_edges_to_traps(
	GRAPHICS_BO_EDGE* pos,
	int32 top,
	unsigned mask,
	GRAPHICS_TRAPS* traps
)
{
	GRAPHICS_BO_EDGE *left;
	int in_out;


#if DEBUG_PRINT_STATE
	printf ("Processing active edges for %x\n", top);
#endif

	in_out = 0;
	left = pos;
	while (pos != NULL) {
		if (pos != left && pos->deferred_trap.right) {
			/* XXX It shouldn't be possible to here with 2 deferred traps
			* on colinear edges... See bug-bo-rictoz.
			*/
			if (left->deferred_trap.right == NULL &&
				edges_colinear (left, pos))
			{
				/* continuation on left */
				left->deferred_trap = pos->deferred_trap;
				pos->deferred_trap.right = NULL;
			}
			else
			{
				GraphicsBoEdgeEndTrap (pos, top, traps);
			}
		}

		in_out += pos->edge.direction;
		if ((in_out & mask) == 0) {
			/* skip co-linear edges */
			if (pos->next == NULL || ! edges_colinear (pos, pos->next)) {
				GraphicsBoEdgeStartOrContinueTrap (left, pos, top, traps);
				left = pos->next;
			}
		}

		pos = pos->next;
	}
}

/* Execute a single pass of the Bentley-Ottmann algorithm on edges,
* generating trapezoids according to the fill_rule and appending them
* to traps. */
static eGRAPHICS_STATUS
_cairo_bentley_ottmann_tessellate_bo_edges (GRAPHICS_BO_EVENT   **start_events,
	int			 num_events,
	unsigned		 fill_rule,
	GRAPHICS_TRAPS	*traps,
	int			*num_intersections)
{
	eGRAPHICS_STATUS status;
	int intersection_count = 0;
	GRAPHICS_BO_EVENT_QUEUE event_queue;
	GRAPHICS_BO_SWEEP_LINE sweep_line;
	GRAPHICS_BO_EVENT *event;
	GRAPHICS_BO_EDGE *left, *right;
	GRAPHICS_BO_EDGE *e1, *e2;

	/* convert the fill_rule into a winding mask */
	if (fill_rule == GRAPHICS_FILL_RULE_WINDING)
		fill_rule = (unsigned) -1;
	else
		fill_rule = 1;

#if DEBUG_EVENTS
	{
		int i;

		for (i = 0; i < num_events; i++) {
			GRAPHICS_BO_START_EVENT *event =
				((GRAPHICS_BO_START_EVENT **) start_events)[i];
			event_log ("edge: %lu (%d, %d) (%d, %d) (%d, %d) %d\n",
				(long) &events[i].edge,
				event->edge.edge.line.p1.x,
				event->edge.edge.line.p1.y,
				event->edge.edge.line.p2.x,
				event->edge.edge.line.p2.y,
				event->edge.top,
				event->edge.bottom,
				event->edge.edge.dir);
		}
	}
#endif

	InitializeGraphicsBoEventQueue (&event_queue, start_events, num_events);
	InitializeGraphicsBoSweepLine (&sweep_line);

	while ((event = GraphicsBoEventDequeue (&event_queue))) {
		if (event->point.y != sweep_line.current_y) {
			for (e1 = sweep_line.stopped; e1; e1 = e1->next) {
				if (e1->deferred_trap.right != NULL) {
					GraphicsBoEdgeEndTrap (e1,
						e1->edge.bottom,
						traps);
				}
			}
			sweep_line.stopped = NULL;

			_active_edges_to_traps (sweep_line.head,
				sweep_line.current_y,
				fill_rule, traps);

			sweep_line.current_y = event->point.y;
		}

#if DEBUG_EVENTS
		event_log ("event: %d (%ld, %ld) %lu, %lu\n",
			event->type,
			(long) event->point.x,
			(long) event->point.y,
			(long) event->e1,
			(long) event->e2);
#endif

		switch (event->type) {
		case GRAPHICS_BO_EVENT_TYPE_START:
			e1 = &((GRAPHICS_BO_START_EVENT *) event)->edge;

			GraphicsBoSweepLineInsert (&sweep_line, e1);

			status = GraphicsBoEventQueuensertStop (&event_queue, e1);
			if (UNLIKELY (status))
				goto unwind;

			/* check to see if this is a continuation of a stopped edge */
			/* XXX change to an infinitesimal lengthening rule */
			for (left = sweep_line.stopped; left; left = left->next) {
				if (e1->edge.top <= left->edge.bottom &&
					edges_colinear (e1, left))
				{
					e1->deferred_trap = left->deferred_trap;
					if (left->prev != NULL)
						left->prev = left->next;
					else
						sweep_line.stopped = left->next;
					if (left->next != NULL)
						left->next->prev = left->prev;
					break;
				}
			}

			left = e1->prev;
			right = e1->next;

			if (left != NULL) {
				status = GraphicsBoEventQueueInsertIfIntersectBelowCurrentY (&event_queue, left, e1);
				if (UNLIKELY (status))
					goto unwind;
			}

			if (right != NULL) {
				status = GraphicsBoEventQueueInsertIfIntersectBelowCurrentY (&event_queue, e1, right);
				if (UNLIKELY (status))
					goto unwind;
			}

			break;

		case GRAPHICS_BO_EVENT_TYPE_STOP:
			e1 = ((GRAPHICS_BO_QUEUE_EVENT *) event)->e1;
			GraphicsBoEventQueueDelete (&event_queue, event);

			left = e1->prev;
			right = e1->next;

			GraphicsBoSweepLineDelete (&sweep_line, e1);

			/* first, check to see if we have a continuation via a fresh edge */
			if (e1->deferred_trap.right != NULL) {
				e1->next = sweep_line.stopped;
				if (sweep_line.stopped != NULL)
					sweep_line.stopped->prev = e1;
				sweep_line.stopped = e1;
				e1->prev = NULL;
			}

			if (left != NULL && right != NULL) {
				status = GraphicsBoEventQueueInsertIfIntersectBelowCurrentY (&event_queue, left, right);
				if (UNLIKELY (status))
					goto unwind;
			}

			break;

		case GRAPHICS_BO_EVENT_TYPE_INTERSECTION:
			e1 = ((GRAPHICS_BO_QUEUE_EVENT *) event)->e1;
			e2 = ((GRAPHICS_BO_QUEUE_EVENT *) event)->e2;
			GraphicsBoEventQueueDelete (&event_queue, event);

			/* skip this intersection if its edges are not adjacent */
			if (e2 != e1->next)
				break;

			intersection_count++;

			left = e1->prev;
			right = e2->next;

			GraphicsBoSweepLineSwap (&sweep_line, e1, e2);

			/* after the swap e2 is left of e1 */

			if (left != NULL) {
				status = GraphicsBoEventQueueInsertIfIntersectBelowCurrentY (&event_queue, left, e2);
				if (UNLIKELY (status))
					goto unwind;
			}

			if (right != NULL) {
				status = GraphicsBoEventQueueInsertIfIntersectBelowCurrentY (&event_queue, e1, right);
				if (UNLIKELY (status))
					goto unwind;
			}

			break;
		}
	}

	*num_intersections = intersection_count;
	for (e1 = sweep_line.stopped; e1; e1 = e1->next) {
		if (e1->deferred_trap.right != NULL) {
			GraphicsBoEdgeEndTrap (e1, e1->edge.bottom, traps);
		}
	}
	status = traps->status;
unwind:
	GraphicsBoEventQueueFinish (&event_queue);

#if DEBUG_EVENTS
	event_log ("\n");
#endif

	return status;
}

eGRAPHICS_STATUS GraphicsBentleyOttmannTessellatePolygon(
	GRAPHICS_TRAPS* traps,
	const GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE	fill_rule
)
{
	int intersections;
	GRAPHICS_BO_START_EVENT stack_events[GRAPHICS_STACK_ARRAY_LENGTH(GRAPHICS_BO_START_EVENT)];
	GRAPHICS_BO_START_EVENT *events;
	GRAPHICS_BO_EVENT *stack_event_ptrs[(sizeof(stack_events) / sizeof(stack_events[0])) + 1];
	GRAPHICS_BO_EVENT **event_ptrs;
	GRAPHICS_BO_START_EVENT *stack_event_y[64];
	GRAPHICS_BO_START_EVENT **event_y = NULL;
	int i, num_events, y, ymin, ymax;
	eGRAPHICS_STATUS status;

	num_events = polygon->num_edges;
	if (UNLIKELY (0 == num_events))
		return GRAPHICS_STATUS_SUCCESS;

	if (polygon->num_limits) {
		ymin = GraphicsFixedIntegerFloor(polygon->limit.point1.y);
		ymax = GraphicsFixedIngegerCeil(polygon->limit.point2.y) - ymin;

		if (ymax > 64)
		{
			event_y = MEM_ALLOC_FUNC(sizeof(GRAPHICS_BO_EVENT*) * ymax);
			if(UNLIKELY (event_y == NULL))
			{
				return GRAPHICS_STATUS_NO_MEMORY;
			}
		}
		else
		{
			event_y = stack_event_y;
		}
		(void)memset(event_y, 0, ymax * sizeof(GRAPHICS_BO_EVENT *));
	}

	events = stack_events;
	event_ptrs = stack_event_ptrs;
	if(num_events > (sizeof(stack_events) / sizeof(stack_events[0])))
	{
		events = MEM_ALLOC_FUNC(num_events * (sizeof(GRAPHICS_BO_START_EVENT) +
			sizeof(GRAPHICS_BO_EVENT *)) +	sizeof(GRAPHICS_BO_EVENT *));
		if (UNLIKELY (events == NULL))
		{
			if(event_y != stack_event_y)
			{
				MEM_FREE_FUNC(event_y);
			}
			return GRAPHICS_STATUS_NO_MEMORY;
		}

		event_ptrs = (GRAPHICS_BO_EVENT**)(events + num_events);
	}

	for (i = 0; i < num_events; i++)
	{
		events[i].type = GRAPHICS_BO_EVENT_TYPE_START;
		events[i].point.y = polygon->edges[i].top;
		events[i].point.x =
			LineComputeIntersectionXforY (&polygon->edges[i].line,
				events[i].point.y);

		events[i].edge.edge = polygon->edges[i];
		events[i].edge.deferred_trap.right = NULL;
		events[i].edge.prev = NULL;
		events[i].edge.next = NULL;
		events[i].edge.colinear = NULL;

		if(event_y)
		{
			y = GraphicsFixedIntegerFloor(events[i].point.y) - ymin;
			events[i].edge.next = (GRAPHICS_BO_EDGE*)event_y[y];
			event_y[y] = (GRAPHICS_BO_START_EVENT*)&events[i];
		}
		else
		{
			event_ptrs[i] = (GRAPHICS_BO_EVENT*)& events[i];
		}
	}

	if(event_y)
	{
		for(y = i = 0; y < ymax && i < num_events; y++)
		{
			GRAPHICS_BO_START_EVENT *e;
			int j = i;
			for(e = event_y[y]; e; e = (GRAPHICS_BO_START_EVENT*)e->edge.next)
			{
				event_ptrs[i++] = (GRAPHICS_BO_EVENT*)e;
			}
			if(i > j + 1)
			{
				GraphicsBoEventQueueSort(event_ptrs + j, i - j);
			}
		}
		if(event_y != stack_event_y)
		{
			MEM_FREE_FUNC(event_y);
		}
	}
	else
	{
		GraphicsBoEventQueueSort (event_ptrs, i);
	}
	event_ptrs[i] = NULL;

#if DEBUG_TRAPS
	dump_edges (events, num_events, "bo-polygon-edges.txt");
#endif

	/* XXX: This would be the convenient place to throw in multiple
	* passes of the Bentley-Ottmann algorithm. It would merely
	* require storing the results of each pass into a temporary
	* GRAPHICS_TRAPS. */
	status = _cairo_bentley_ottmann_tessellate_bo_edges (event_ptrs, num_events,
		fill_rule, traps,
		&intersections);
#if DEBUG_TRAPS
	dump_traps (traps, "bo-polygon-out.txt");
#endif

	if (events != stack_events)
		free (events);

	return status;
}

eGRAPHICS_STATUS GraphicsBentleyOttmannTessellateTraps(
	GRAPHICS_TRAPS* traps,
	eGRAPHICS_FILL_RULE fill_rule
)
{
	eGRAPHICS_STATUS status;
	GRAPHICS_POLYGON polygon;
	int i;

	if (UNLIKELY (0 == traps->num_traps))
		return GRAPHICS_STATUS_SUCCESS;

#if DEBUG_TRAPS
	dump_traps (traps, "bo-traps-in.txt");
#endif

	InitializeGraphicsPolygon(&polygon, traps->limits, traps->num_limits);

	for(i = 0; i < traps->num_traps; i++)
	{
		status = GraphicsPolygonAddLine(&polygon,
			&traps->traps[i].left,
			traps->traps[i].top,
			traps->traps[i].bottom,
			1);
		if(UNLIKELY (status))
		{
			goto CLEANUP;
		}

		status = GraphicsPolygonAddLine (&polygon,
			&traps->traps[i].right,
			traps->traps[i].top,
			traps->traps[i].bottom,
			-1);
		if(UNLIKELY (status))
		{
			goto CLEANUP;
		}
	}

	GraphicsTrapsClear(traps);
	status = GraphicsBentleyOttmannTessellatePolygon (traps,
		&polygon, fill_rule);

#if DEBUG_TRAPS
	dump_traps (traps, "bo-traps-out.txt");
#endif

CLEANUP:
	GraphicsPolygonFinish(&polygon);

	return status;
}

#if 0
static cairo_bool_t
edges_have_an_intersection_quadratic (GRAPHICS_BO_EDGE	*edges,
	int		 num_edges)

{
	int i, j;
	GRAPHICS_BO_EDGE *a, *b;
	GRAPHICS_BO_POINT32 intersection;

	/* We must not be given any upside-down edges. */
	for (i = 0; i < num_edges; i++) {
		ASSERT(GraphicsBoPoint32Compare (&edges[i].top, &edges[i].bottom) < 0);
		edges[i].line.p1.x <<= CAIRO_BO_GUARD_BITS;
		edges[i].line.p1.y <<= CAIRO_BO_GUARD_BITS;
		edges[i].line.p2.x <<= CAIRO_BO_GUARD_BITS;
		edges[i].line.p2.y <<= CAIRO_BO_GUARD_BITS;
	}

	for (i = 0; i < num_edges; i++) {
		for (j = 0; j < num_edges; j++) {
			if (i == j)
				continue;

			a = &edges[i];
			b = &edges[j];

			if (! GraphicsBoEdgeIntersect (a, b, &intersection))
				continue;

			printf ("Found intersection (%d,%d) between (%d,%d)-(%d,%d) and (%d,%d)-(%d,%d)\n",
				intersection.x,
				intersection.y,
				a->line.p1.x, a->line.p1.y,
				a->line.p2.x, a->line.p2.y,
				b->line.p1.x, b->line.p1.y,
				b->line.p2.x, b->line.p2.y);

			return TRUE;
		}
	}
	return FALSE;
}

#define TEST_MAX_EDGES 10

typedef struct test {
	const char *name;
	const char *description;
	int num_edges;
	GRAPHICS_BO_EDGE edges[TEST_MAX_EDGES];
} test_t;

static test_t
tests[] = {
	{
		"3 near misses",
		"3 edges all intersecting very close to each other",
		3,
{
	{ { 4, 2}, {0, 0}, { 9, 9}, NULL, NULL },
{ { 7, 2}, {0, 0}, { 2, 3}, NULL, NULL },
{ { 5, 2}, {0, 0}, { 1, 7}, NULL, NULL }
}
	},
	{
		"inconsistent data",
		"Derived from random testing---was leading to skip list and edge list disagreeing.",
		2,
{
	{ { 2, 3}, {0, 0}, { 8, 9}, NULL, NULL },
{ { 2, 3}, {0, 0}, { 6, 7}, NULL, NULL }
}
	},
	{
		"failed sort",
		"A test derived from random testing that leads to an inconsistent sort --- looks like we just can't attempt to validate the sweep line with edge_compare?",
		3,
{
	{ { 6, 2}, {0, 0}, { 6, 5}, NULL, NULL },
{ { 3, 5}, {0, 0}, { 5, 6}, NULL, NULL },
{ { 9, 2}, {0, 0}, { 5, 6}, NULL, NULL },
}
	},
	{
		"minimal-intersection",
		"Intersection of a two from among the smallest possible edges.",
		2,
{
	{ { 0, 0}, {0, 0}, { 1, 1}, NULL, NULL },
{ { 1, 0}, {0, 0}, { 0, 1}, NULL, NULL }
}
	},
	{
		"simple",
		"A simple intersection of two edges at an integer (2,2).",
		2,
{
	{ { 1, 1}, {0, 0}, { 3, 3}, NULL, NULL },
{ { 2, 1}, {0, 0}, { 2, 3}, NULL, NULL }
}
	},
	{
		"bend-to-horizontal",
		"With intersection truncation one edge bends to horizontal",
		2,
{
	{ { 9, 1}, {0, 0}, {3, 7}, NULL, NULL },
{ { 3, 5}, {0, 0}, {9, 9}, NULL, NULL }
}
	}
};

/*
{
"endpoint",
"An intersection that occurs at the endpoint of a segment.",
{
{ { 4, 6}, { 5, 6}, NULL, { { NULL }} },
{ { 4, 5}, { 5, 7}, NULL, { { NULL }} },
{ { 0, 0}, { 0, 0}, NULL, { { NULL }} },
}
}
{
name = "overlapping",
desc = "Parallel segments that share an endpoint, with different slopes.",
edges = {
{ top = { x = 2, y = 0}, bottom = { x = 1, y = 1}},
{ top = { x = 2, y = 0}, bottom = { x = 0, y = 2}},
{ top = { x = 0, y = 3}, bottom = { x = 1, y = 3}},
{ top = { x = 0, y = 3}, bottom = { x = 2, y = 3}},
{ top = { x = 0, y = 4}, bottom = { x = 0, y = 6}},
{ top = { x = 0, y = 5}, bottom = { x = 0, y = 6}}
}
},
{
name = "hobby_stage_3",
desc = "A particularly tricky part of the 3rd stage of the 'hobby' test below.",
edges = {
{ top = { x = -1, y = -2}, bottom = { x =  4, y = 2}},
{ top = { x =  5, y =  3}, bottom = { x =  9, y = 5}},
{ top = { x =  5, y =  3}, bottom = { x =  6, y = 3}},
}
},
{
name = "hobby",
desc = "Example from John Hobby's paper. Requires 3 passes of the iterative algorithm.",
edges = {
{ top = { x =   0, y =   0}, bottom = { x =   9, y =   5}},
{ top = { x =   0, y =   0}, bottom = { x =  13, y =   6}},
{ top = { x =  -1, y =  -2}, bottom = { x =   9, y =   5}}
}
},
{
name = "slope",
desc = "Edges with same start/stop points but different slopes",
edges = {
{ top = { x = 4, y = 1}, bottom = { x = 6, y = 3}},
{ top = { x = 4, y = 1}, bottom = { x = 2, y = 3}},
{ top = { x = 2, y = 4}, bottom = { x = 4, y = 6}},
{ top = { x = 6, y = 4}, bottom = { x = 4, y = 6}}
}
},
{
name = "horizontal",
desc = "Test of a horizontal edge",
edges = {
{ top = { x = 1, y = 1}, bottom = { x = 6, y = 6}},
{ top = { x = 2, y = 3}, bottom = { x = 5, y = 3}}
}
},
{
name = "vertical",
desc = "Test of a vertical edge",
edges = {
{ top = { x = 5, y = 1}, bottom = { x = 5, y = 7}},
{ top = { x = 2, y = 4}, bottom = { x = 8, y = 5}}
}
},
{
name = "congruent",
desc = "Two overlapping edges with the same slope",
edges = {
{ top = { x = 5, y = 1}, bottom = { x = 5, y = 7}},
{ top = { x = 5, y = 2}, bottom = { x = 5, y = 6}},
{ top = { x = 2, y = 4}, bottom = { x = 8, y = 5}}
}
},
{
name = "multi",
desc = "Several segments with a common intersection point",
edges = {
{ top = { x = 1, y = 2}, bottom = { x = 5, y = 4} },
{ top = { x = 1, y = 1}, bottom = { x = 5, y = 5} },
{ top = { x = 2, y = 1}, bottom = { x = 4, y = 5} },
{ top = { x = 4, y = 1}, bottom = { x = 2, y = 5} },
{ top = { x = 5, y = 1}, bottom = { x = 1, y = 5} },
{ top = { x = 5, y = 2}, bottom = { x = 1, y = 4} }
}
}
};
*/

static int
run_test (const char		*test_name,
	GRAPHICS_BO_EDGE	*test_edges,
	int			 num_edges)
{
	int i, intersections, passes;
	GRAPHICS_BO_EDGE *edges;
	cairo_array_t intersected_edges;

	printf ("Testing: %s\n", test_name);

	_cairo_array_init (&intersected_edges, sizeof (GRAPHICS_BO_EDGE));

	intersections = _cairo_bentley_ottmann_intersect_edges (test_edges, num_edges, &intersected_edges);
	if (intersections)
		printf ("Pass 1 found %d intersections:\n", intersections);


	/* XXX: Multi-pass Bentley-Ottmmann. Preferable would be to add a
	* pass of Hobby's tolerance-square algorithm instead. */
	passes = 1;
	while (intersections) {
		int num_edges = _cairo_array_num_elements (&intersected_edges);
		passes++;
		edges = _cairo_malloc_ab (num_edges, sizeof (GRAPHICS_BO_EDGE));
		ASSERT(edges != NULL);
		memcpy (edges, _cairo_array_index (&intersected_edges, 0), num_edges * sizeof (GRAPHICS_BO_EDGE));
		_cairo_array_fini (&intersected_edges);
		_cairo_array_init (&intersected_edges, sizeof (GRAPHICS_BO_EDGE));
		intersections = _cairo_bentley_ottmann_intersect_edges (edges, num_edges, &intersected_edges);
		free (edges);

		if (intersections){
			printf ("Pass %d found %d remaining intersections:\n", passes, intersections);
		} else {
			if (passes > 3)
				for (i = 0; i < passes; i++)
					printf ("*");
			printf ("No remainining intersections found after pass %d\n", passes);
		}
	}

	if (edges_have_an_intersection_quadratic (_cairo_array_index (&intersected_edges, 0),
		_cairo_array_num_elements (&intersected_edges)))
		printf ("*** FAIL ***\n");
	else
		printf ("PASS\n");

	_cairo_array_fini (&intersected_edges);

	return 0;
}

#define MAX_RANDOM 300

int
main (void)
{
	char random_name[] = "random-XX";
	GRAPHICS_BO_EDGE random_edges[MAX_RANDOM], *edge;
	unsigned int i, num_random;
	test_t *test;

	for (i = 0; i < ARRAY_LENGTH (tests); i++) {
		test = &tests[i];
		run_test (test->name, test->edges, test->num_edges);
	}

	for (num_random = 0; num_random < MAX_RANDOM; num_random++) {
		srand (0);
		for (i = 0; i < num_random; i++) {
			do {
				edge = &random_edges[i];
				edge->line.p1.x = (int32) (10.0 * (rand() / (RAND_MAX + 1.0)));
				edge->line.p1.y = (int32) (10.0 * (rand() / (RAND_MAX + 1.0)));
				edge->line.p2.x = (int32) (10.0 * (rand() / (RAND_MAX + 1.0)));
				edge->line.p2.y = (int32) (10.0 * (rand() / (RAND_MAX + 1.0)));
				if (edge->line.p1.y > edge->line.p2.y) {
					int32 tmp = edge->line.p1.y;
					edge->line.p1.y = edge->line.p2.y;
					edge->line.p2.y = tmp;
				}
			} while (edge->line.p1.y == edge->line.p2.y);
		}

		sprintf (random_name, "random-%02d", num_random);

		run_test (random_name, random_edges, num_random);
	}

	return 0;
}
#endif
