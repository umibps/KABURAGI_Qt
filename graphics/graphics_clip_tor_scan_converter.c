#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <setjmp.h>
#include "graphics_types.h"
#include "graphics_compositor.h"
#include "graphics_surface.h"
#include "graphics.h"
#include "graphics_flags.h"
#include "graphics_matrix.h"
#include "graphics_pen.h"
#include "graphics_memory_pool.h"
#include "graphics_clip_tor_scan_converter.h"
#include "graphics_private.h"
#include "graphics_inline.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The input coordinate scale and the rasterisation grid scales. */
#define GLITTER_INPUT_BITS GRAPHICS_FIXED_FRAC_BITS
#define GRID_X_BITS GRAPHICS_FIXED_FRAC_BITS
#define GRID_Y 15

/*-------------------------------------------------------------------------
 * glitter-paths.h
 */

/* "Input scaled" numbers are fixed precision reals with multiplier
 * 2**GLITTER_INPUT_BITS.  Input coordinates are given to glitter as
 * pixel scaled numbers.  These get converted to the internal grid
 * scaled numbers as soon as possible. Internal overflow is possible
 * if GRID_X/Y inside glitter-paths.c is larger than
 * 1<<GLITTER_INPUT_BITS. */
#ifndef GLITTER_INPUT_BITS
#  define GLITTER_INPUT_BITS 8
#endif
#define GLITTER_INPUT_SCALE (1<<GLITTER_INPUT_BITS)

/* Default x/y scale factors.
 *  You can either define GRID_X/Y_BITS to get a power-of-two scale
 *  or define GRID_X/Y separately. */
#if !defined(GRID_X) && !defined(GRID_X_BITS)
#  define GRID_X_BITS 8
#endif
#if !defined(GRID_Y) && !defined(GRID_Y_BITS)
#  define GRID_Y 15
#endif

/* Use GRID_X/Y_BITS to define GRID_X/Y if they're available. */
#ifdef GRID_X_BITS
#  define GRID_X (1 << GRID_X_BITS)
#endif
#ifdef GRID_Y_BITS
#  define GRID_Y (1 << GRID_Y_BITS)
#endif

/* The GRID_X_TO_INT_FRAC macro splits a grid scaled coordinate into
 * integer and fractional parts. The integer part is floored. */
#if defined(GRID_X_TO_INT_FRAC)
  /* do nothing */
#elif defined(GRID_X_BITS)
#  define GRID_X_TO_INT_FRAC(x, i, f) \
	_GRID_TO_INT_FRAC_SHIFT(x, i, f, GRID_X_BITS)
#else
#  define GRID_X_TO_INT_FRAC(x, i, f) \
	_GRID_TO_INT_FRAC_GENERAL(x, i, f, GRID_X)
#endif

#define _GRID_TO_INT_FRAC_GENERAL(t, i, f, m) do {	\
	(i) = (t) / (m);					\
	(f) = (t) % (m);					\
	if ((f) < 0) {					\
	--(i);						\
	(f) += (m);					\
	}							\
} while (0)

#define _GRID_TO_INT_FRAC_SHIFT(t, i, f, b) do {	\
	(f) = (t) & ((1 << (b)) - 1);			\
	(i) = (t) >> (b);					\
} while (0)

/* A grid area is a real in [0,1] scaled by 2*GRID_X*GRID_Y.  We want
 * to be able to represent exactly areas of subpixel trapezoids whose
 * vertices are given in grid scaled coordinates.  The scale factor
 * comes from needing to accurately represent the area 0.5*dx*dy of a
 * triangle with base dx and height dy in grid scaled numbers. */

#define GRID_XY (2*GRID_X*GRID_Y) /* Unit area on the grid. */

/* GRID_AREA_TO_ALPHA(area): map [0,GRID_XY] to [0,255]. */
#if GRID_XY == 510
#  define GRID_AREA_TO_ALPHA(c)	  (((c)+1) >> 1)
#elif GRID_XY == 255
#  define  GRID_AREA_TO_ALPHA(c)  (c)
#elif GRID_XY == 64
#  define  GRID_AREA_TO_ALPHA(c)  (((c) << 2) | -(((c) & 0x40) >> 6))
#elif GRID_XY == 128
#  define  GRID_AREA_TO_ALPHA(c)  ((((c) << 1) | -((c) >> 7)) & 255)
#elif GRID_XY == 256
#  define  GRID_AREA_TO_ALPHA(c)  (((c) | -((c) >> 8)) & 255)
#elif GRID_XY == 15
#  define  GRID_AREA_TO_ALPHA(c)  (((c) << 4) + (c))
#elif GRID_XY == 2*256*15
#  define  GRID_AREA_TO_ALPHA(c)  (((c) + ((c)<<4) + 256) >> 9)
#else
#  define  GRID_AREA_TO_ALPHA(c)  (((c)*255 + GRID_XY/2) / GRID_XY)
#endif

#define UNROLL3(x) x x x

/* Compute the floored division a/b. Assumes / and % perform symmetric
 * division. */
static INLINE QUOREM FlooredDivrem(int a, int b)
{
	QUOREM qr;
	qr.quo = a/b;
	qr.rem = a%b;
	if((a^b)<0 && qr.rem)
	{
		qr.quo -= 1;
		qr.rem += b;
	}
	return qr;
}

/* Compute the floored division (x*a)/b. Assumes / and % perform symmetric
 * division. */
static QUOREM FlooredMuldivrem(int x, int a, int b)
{
	QUOREM qr;
	long long xa = (long long)x*a;
	qr.quo = xa/b;
	qr.rem = xa%b;
	if((xa>=0) != (b>=0) && qr.rem)
	{
		qr.quo -= 1;
		qr.rem += b;
	}
	return qr;
}

static POOL_CHUNK* InitializePoolChunk(
	POOL_CHUNK* p,
	POOL_CHUNK* prev_chunk,
	size_t capacity
)
{
	p->prev_chunk = prev_chunk;
	p->size = 0;
	p->capacity = capacity;
	return p;
}

static POOL_CHUNK* PoolChunkCreate(POOL* pool, size_t size)
{
	POOL_CHUNK *p;

	p = MEM_ALLOC_FUNC((size + sizeof(POOL_CHUNK)));
	if(NULL == p)
	{
		longjmp (*pool->jmp, GRAPHICS_STATUS_NO_MEMORY);
	}

	return InitializePoolChunk(p, pool->current, size);
}

static void InitializePool(
	POOL* pool,
	jmp_buf* jmp,
	size_t default_capacity,
	size_t embedded_capacity
)
{
	pool->jmp = jmp;
	pool->current = pool->sentinel;
	pool->first_free = NULL;
	pool->default_capacity = default_capacity;
	InitializePoolChunk(pool->sentinel, NULL, embedded_capacity);
}

static void PoolFinish(POOL* pool)
{
	POOL_CHUNK *p = pool->current;
	do
	{
		while(NULL != p)
		{
			POOL_CHUNK *prev = p->prev_chunk;
			if (p != pool->sentinel)
			{
				MEM_FREE_FUNC(p);
			}
			p = prev;
		}
		p = pool->first_free;
		pool->first_free = NULL;
	} while(NULL != p);
}

/* Satisfy an allocation by first allocating a new large enough chunk
 * and adding it to the head of the pool's chunk list. This function
 * is called as a fallback if pool_alloc() couldn't do a quick
 * allocation from the current chunk in the pool. */
static void* PoolAllocFromNewChunk(
	POOL* pool,
	size_t size
)
{
	POOL_CHUNK *chunk;
	void *obj;
	size_t capacity;

	/* If the allocation is smaller than the default chunk size then
	 * try getting a chunk off the free list.  Force alloc of a new
	 * chunk for large requests. */
	capacity = size;
	chunk = NULL;
	if(size < pool->default_capacity)
	{
		capacity = pool->default_capacity;
		chunk = pool->first_free;
		if(chunk)
		{
			pool->first_free = chunk->prev_chunk;
			InitializePoolChunk(chunk, pool->current, chunk->capacity);
		}
	}

	if(NULL == chunk)
	{
		chunk = PoolChunkCreate(pool, capacity);
	}
	pool->current = chunk;

	obj = ((unsigned char*)chunk + sizeof(*chunk) + chunk->size);
	chunk->size += size;
	return obj;
}

/* Allocate size bytes from the pool.  The first allocated address
 * returned from a pool is aligned to sizeof(void*).  Subsequent
 * addresses will maintain alignment as long as multiples of void* are
 * allocated.  Returns the address of a new memory area or %NULL on
 * allocation failures.	 The pool retains ownership of the returned
 * memory. */
static INLINE void* PoolAlloc(POOL* pool, size_t size)
{
	POOL_CHUNK *chunk = pool->current;

	if(size <= chunk->capacity - chunk->size)
	{
		void *obj = ((unsigned char*)chunk + sizeof(*chunk) + chunk->size);
		chunk->size += size;
		return obj;
	}
	else
	{
		return PoolAllocFromNewChunk(pool, size);
	}
}

/* Relinquish all pool_alloced memory back to the pool. */
static void PoolReset(POOL* pool)
{
	/* Transfer all used chunks to the chunk free list. */
	POOL_CHUNK *chunk = pool->current;
	if(chunk != pool->sentinel)
	{
		while(chunk->prev_chunk != pool->sentinel)
		{
			chunk = chunk->prev_chunk;
		}
		chunk->prev_chunk = pool->first_free;
		pool->first_free = pool->current;
	}
	/* Reset the sentinel as the current chunk. */
	pool->current = pool->sentinel;
	pool->sentinel->size = 0;
}

/* Rewinds the cell list's cursor to the beginning.  After rewinding
 * we're good to cell_list_find() the cell any x coordinate. */
static INLINE void CellListRewind(CELL_LIST* cells)
{
	cells->cursor = &cells->head;
}

/* Rewind the cell list if its cursor has been advanced past x. */
static INLINE void CellListMaybeRewind(CELL_LIST* cells, int x)
{
	CELL *tail = cells->cursor;
	if(tail->x > x)
	{
		CellListRewind (cells);
	}
}

static void InitializeCellList(CELL_LIST* cells, jmp_buf* jmp)
{
	InitializePool(cells->cell_pool.base, jmp,
			 256*sizeof(CELL), sizeof(cells->cell_pool.embedded));
	cells->tail.next = NULL;
	cells->tail.x = INT_MAX;
	cells->head.x = INT_MIN;
	cells->head.next = &cells->tail;
	CellListRewind(cells);
}

static void CellListFinish(CELL_LIST* cells)
{
	PoolFinish(cells->cell_pool.base);
}

/* Empty the cell list.  This is called at the start of every pixel
 * row. */
static INLINE void CellListReset(CELL_LIST* cells)
{
	CellListRewind(cells);
	cells->head.next = &cells->tail;
	PoolReset(cells->cell_pool.base);
}

static CELL* CellListAlloc(CELL_LIST* cells, CELL* tail, int x)
{
	CELL *cell;

	cell = PoolAlloc(cells->cell_pool.base, sizeof(CELL));
	cell->next = tail->next;
	tail->next = cell;
	cell->x = x;
	cell->uncovered_area = 0;
	cell->covered_height = 0;
	cell->clipped_height = 0;
	return cell;
}

/* Find a cell at the given x-coordinate.  Returns %NULL if a new cell
 * needed to be allocated but couldn't be.  Cells must be found with
 * non-decreasing x-coordinate until the cell list is rewound using
 * cell_list_rewind(). Ownership of the returned cell is retained by
 * the cell list. */
static INLINE CELL* CellListFind(CELL_LIST* cells, int x)
{
	CELL *tail = cells->cursor;

	while(1)
	{
		UNROLL3({
			if(tail->next->x > x)
				break;
			tail = tail->next;
		});
	}

	if(tail->x != x)
	{
		tail = CellListAlloc(cells, tail, x);
	}
	return cells->cursor = tail;
}

/* Find two cells at x1 and x2.	 This is exactly equivalent
 * to
 *
 *   pair.cell1 = cell_list_find(cells, x1);
 *   pair.cell2 = cell_list_find(cells, x2);
 *
 * except with less function call overhead. */
static INLINE CELL_PAIR CellListFindPair(CELL_LIST* cells, int x1, int x2)
{
	CELL_PAIR pair;

	pair.cell1 = cells->cursor;
	while(1)
	{
		UNROLL3({
			if(pair.cell1->next->x > x1)
				break;
			pair.cell1 = pair.cell1->next;
		});
	}
	if(pair.cell1->x != x1)
	{
		CELL *cell = PoolAlloc(cells->cell_pool.base, sizeof(CELL));
		cell->x = x1;
		cell->uncovered_area = 0;
		cell->covered_height = 0;
		cell->clipped_height = 0;
		cell->next = pair.cell1->next;
		pair.cell1->next = cell;
		pair.cell1 = cell;
	}

	pair.cell2 = pair.cell1;
	while(1)
	{
		UNROLL3({
			if(pair.cell2->next->x > x2)
				break;
			pair.cell2 = pair.cell2->next;
		});
	}
	if(pair.cell2->x != x2)
	{
		CELL *cell = PoolAlloc(cells->cell_pool.base, sizeof(CELL));
		cell->uncovered_area = 0;
		cell->covered_height = 0;
		cell->clipped_height = 0;
		cell->x = x2;
		cell->next = pair.cell2->next;
		pair.cell2->next = cell;
		pair.cell2 = cell;
	}

	cells->cursor = pair.cell2;
	return pair;
}

/* Add a subpixel span covering [x1, x2) to the coverage cells. */
static INLINE void CellListAddSubspan(
	CELL_LIST* cells,
	GRID_SCALED_X x1,
	GRID_SCALED_X x2
)
{
	int ix1, fx1;
	int ix2, fx2;

	GRID_X_TO_INT_FRAC(x1, ix1, fx1);
	GRID_X_TO_INT_FRAC(x2, ix2, fx2);

	if(ix1 != ix2)
	{
		CELL_PAIR p;
		p = CellListFindPair(cells, ix1, ix2);
		p.cell1->uncovered_area += 2*fx1;
		++p.cell1->covered_height;
		p.cell2->uncovered_area -= 2*fx2;
		--p.cell2->covered_height;
	}
	else
	{
		CELL *cell = CellListFind(cells, ix1);
		cell->uncovered_area += 2*(fx1-fx2);
	}
}

/* Adds the analytical coverage of an edge crossing the current pixel
 * row to the coverage cells and advances the edge's x position to the
 * following row.
 *
 * This function is only called when we know that during this pixel row:
 *
 * 1) The relative order of all edges on the active list doesn't
 * change.  In particular, no edges intersect within this row to pixel
 * precision.
 *
 * 2) No new edges start in this row.
 *
 * 3) No existing edges end mid-row.
 *
 * This function depends on being called with all edges from the
 * active list in the order they appear on the list (i.e. with
 * non-decreasing x-coordinate.)  */
static void CellListRenderEdge(
	CELL_LIST* cells,
	EDGE* edge,
	int sign
)
{
	GRID_SCALED_Y y1, y2, dy;
	GRID_SCALED_X dx;
	int ix1, ix2;
	GRID_SCALED_X fx1, fx2;

	QUOREM x1 = edge->x;
	QUOREM x2 = x1;

	if(FALSE == edge->vertical)
	{
		x2.quo += edge->dxdy_full.quo;
		x2.rem += edge->dxdy_full.rem;
		if(x2.rem >= 0)
		{
			++x2.quo;
			x2.rem -= edge->dy;
		}

		edge->x = x2;
	}

	GRID_X_TO_INT_FRAC(x1.quo, ix1, fx1);
	GRID_X_TO_INT_FRAC(x2.quo, ix2, fx2);

	/* Edge is entirely within a column? */
	if(ix1 == ix2)
	{
		/* We always know that ix1 is >= the cell list cursor in this
		 * case due to the no-intersections precondition.  */
		CELL *cell = CellListFind(cells, ix1);
		cell->covered_height += sign*GRID_Y;
		cell->uncovered_area += sign*(fx1 + fx2)*GRID_Y;
		return;
	}

	/* Orient the edge left-to-right. */
	dx = x2.quo - x1.quo;
	if(dx >= 0)
	{
		y1 = 0;
		y2 = GRID_Y;
	}
	else
	{
		int tmp;
		tmp = ix1; ix1 = ix2; ix2 = tmp;
		tmp = fx1; fx1 = fx2; fx2 = tmp;
		dx = -dx;
		sign = -sign;
		y1 = GRID_Y;
		y2 = 0;
	}
	dy = y2 - y1;

	/* Add coverage for all pixels [ix1,ix2] on this row crossed
	 * by the edge. */
	{
		CELL_PAIR pair;
		QUOREM y = FlooredDivrem((GRID_X - fx1)*dy, dx);

		/* When rendering a previous edge on the active list we may
		 * advance the cell list cursor past the leftmost pixel of the
		 * current edge even though the two edges don't intersect.
		 * e.g. consider two edges going down and rightwards:
		 *
		 *  --\_+---\_+-----+-----+----
		 *	  \_	\_	|	 |
		 *	  | \_  | \_  |	 |
		 *	  |   \_|   \_|	 |
		 *	  |	 \_	\_	|
		 *  ----+-----+-\---+-\---+----
		 *
		 * The left edge touches cells past the starting cell of the
		 * right edge.  Fortunately such cases are rare.
		 *
		 * The rewinding is never necessary if the current edge stays
		 * within a single column because we've checked before calling
		 * this function that the active list order won't change. */
		CellListMaybeRewind(cells, ix1);

		pair = CellListFindPair(cells, ix1, ix1+1);
		pair.cell1->uncovered_area += sign*y.quo*(GRID_X + fx1);
		pair.cell1->covered_height += sign*y.quo;
		y.quo += y1;

		if(ix1+1 < ix2)
		{
			QUOREM dydx_full = FlooredDivrem(GRID_X*dy, dx);
			CELL *cell = pair.cell2;

			++ix1;
			do
			{
				GRID_SCALED_Y y_skip = dydx_full.quo;
				y.rem += dydx_full.rem;
				if(y.rem >= dx)
				{
					++y_skip;
					y.rem -= dx;
				}

				y.quo += y_skip;

				y_skip *= sign;
				cell->uncovered_area += y_skip*GRID_X;
				cell->covered_height += y_skip;

				++ix1;
				cell = CellListFind(cells, ix1);
			} while(ix1 != ix2);

			pair.cell2 = cell;
		}
		pair.cell2->uncovered_area += sign*(y2 - y.quo)*fx2;
		pair.cell2->covered_height += sign*(y2 - y.quo);
	}
}

static void InitializePolygon(POLYGON* polygon, jmp_buf* jmp)
{
	polygon->ymin = polygon->ymax = 0;
	polygon->y_buckets = polygon->y_buckets_embedded;
	InitializePool(polygon->edge_pool.base, jmp,
		   8192 - sizeof(POOL_CHUNK), sizeof(polygon->edge_pool.embedded));
}

static void PolygonFinish(POLYGON *polygon)
{
	if(polygon->y_buckets != polygon->y_buckets_embedded)
	{
		MEM_FREE_FUNC(polygon->y_buckets);
	}
	PoolFinish(polygon->edge_pool.base);
}

/* Empties the polygon of all edges. The polygon is then prepared to
 * receive new edges and clip them to the vertical range
 * [ymin,ymax). */
static eGRAPHICS_STATUS PolygonReset(
	POLYGON* polygon,
	GRID_SCALED_Y ymin,
	GRID_SCALED_Y ymax
)
{
	unsigned h = ymax - ymin;
	unsigned num_buckets = EDGE_Y_BUCKET_INDEX(ymax + EDGE_Y_BUCKET_HEIGHT-1, ymin);

	PoolReset(polygon->edge_pool.base);

	if(UNLIKELY(h > 0x7FFFFFFFU - EDGE_Y_BUCKET_HEIGHT))
	{
		goto bail_no_mem; /* even if you could, you wouldn't want to. */
	}

	if(polygon->y_buckets != polygon->y_buckets_embedded)
	{
		MEM_FREE_FUNC(polygon->y_buckets);
	}

	polygon->y_buckets =  polygon->y_buckets_embedded;
	if(num_buckets > sizeof(polygon->y_buckets_embedded) / sizeof(polygon->y_buckets_embedded[0]))
	{
		polygon->y_buckets = (EDGE**)MEM_ALLOC_FUNC(num_buckets * sizeof(EDGE*));
		if(polygon->y_buckets == NULL)
		{
			goto bail_no_mem;
		}
	}
	(void)memset(polygon->y_buckets, 0, num_buckets * sizeof(EDGE*));

	polygon->ymin = ymin;
	polygon->ymax = ymax;
	return GRAPHICS_STATUS_SUCCESS;

 bail_no_mem:
	polygon->ymin = 0;
	polygon->ymax = 0;
	return GRAPHICS_STATUS_NO_MEMORY;
}

static void PolygonInsertEdgeIntoItsY_Bucket(
	POLYGON* polygon,
	EDGE* e
)
{
	unsigned ix = EDGE_Y_BUCKET_INDEX(e->ytop, polygon->ymin);
	EDGE **ptail = &polygon->y_buckets[ix];
	e->next = *ptail;
	*ptail = e;
}

static INLINE void PolygonAddEdge(
	POLYGON* polygon,
	const GRAPHICS_EDGE* edge,
	int clip
)
{
	EDGE *e;
	GRID_SCALED_X dx;
	GRID_SCALED_Y dy;
	GRID_SCALED_Y ytop, ybot;
	GRID_SCALED_Y ymin = polygon->ymin;
	GRID_SCALED_Y ymax = polygon->ymax;

	ASSERT(edge->bottom > edge->top);

	if(UNLIKELY(edge->top >= ymax || edge->bottom <= ymin))
	{
		return;
	}

	e = PoolAlloc(polygon->edge_pool.base, sizeof(EDGE));

	dx = edge->line.point2.x - edge->line.point1.x;
	dy = edge->line.point2.y - edge->line.point1.y;
	e->dy = dy;
	e->direction = edge->direction;
	e->clip = clip;

	ytop = edge->top >= ymin ? edge->top : ymin;
	ybot = edge->bottom <= ymax ? edge->bottom : ymax;
	e->ytop = ytop;
	e->height_left = ybot - ytop;

	if(dx == 0)
	{
		e->vertical = TRUE;
		e->x.quo = edge->line.point1.x;
		e->x.rem = 0;
		e->dxdy.quo = 0;
		e->dxdy.rem = 0;
		e->dxdy_full.quo = 0;
		e->dxdy_full.rem = 0;
	}
	else
	{
		e->vertical = FALSE;
		e->dxdy = FlooredDivrem(dx, dy);
		if(ytop == edge->line.point1.y)
		{
			e->x.quo = edge->line.point1.x;
			e->x.rem = 0;
		}
		else
		{
			e->x = FlooredMuldivrem(ytop - edge->line.point1.y, dx, dy);
			e->x.quo += edge->line.point1.x;
		}

		if(e->height_left >= GRID_Y)
		{
			e->dxdy_full = FlooredMuldivrem(GRID_Y, dx, dy);
		}
		else
		{
			e->dxdy_full.quo = 0;
			e->dxdy_full.rem = 0;
		}
	}

	PolygonInsertEdgeIntoItsY_Bucket(polygon, e);

	e->x.rem -= dy;		/* Bias the remainder for faster
				 * edge advancement. */
}

static void ActiveListReset(ACTIVE_LIST* active)
{
	active->head = NULL;
	active->min_height = 0;
}

static void InitializeActiveList(ACTIVE_LIST* active)
{
	ActiveListReset(active);
}

/*
 * Merge two sorted edge lists.
 * Input:
 *  - head_a: The head of the first list.
 *  - head_b: The head of the second list; head_b cannot be NULL.
 * Output:
 * Returns the head of the merged list.
 *
 * Implementation notes:
 * To make it fast (in particular, to reduce to an insertion sort whenever
 * one of the two input lists only has a single element) we iterate through
 * a list until its head becomes greater than the head of the other list,
 * then we switch their roles. As soon as one of the two lists is empty, we
 * just attach the other one to the current list and exit.
 * Writes to memory are only needed to "switch" lists (as it also requires
 * attaching to the output list the list which we will be iterating next) and
 * to attach the last non-empty list.
 */
static EDGE* MergeSortedEdges(EDGE* head_a, EDGE* head_b)
{
	EDGE *head, **next;
	int32 x;

	if(head_a == NULL)
	{
		return head_b;
	}

		next = &head;
	if(head_a->x.quo <= head_b->x.quo)
	{
		head = head_a;
	}
	else
	{
		head = head_b;
		goto start_with_b;
	}

	do
	{
		x = head_b->x.quo;
		while(head_a != NULL && head_a->x.quo <= x)
		{
			next = &head_a->next;
			head_a = head_a->next;
		}

		*next = head_b;
		if(head_a == NULL)
		{
			return head;
		}

start_with_b:
		x = head_a->x.quo;
		while(head_b != NULL && head_b->x.quo <= x)
		{
			next = &head_b->next;
			head_b = head_b->next;
		}

		*next = head_a;
		if(head_b == NULL)
		{
			return head;
		}
	} while(1);
}

/*
 * Sort (part of) a list.
 * Input:
 *  - list: The list to be sorted; list cannot be NULL.
 *  - limit: Recursion limit.
 * Output:
 *  - head_out: The head of the sorted list containing the first 2^(level+1) elements of the
 *			  input list; if the input list has fewer elements, head_out be a sorted list
 *			  containing all the elements of the input list.
 * Returns the head of the list of unprocessed elements (NULL if the sorted list contains
 * all the elements of the input list).
 *
 * Implementation notes:
 * Special case single element list, unroll/inline the sorting of the first two elements.
 * Some tail recursion is used since we iterate on the bottom-up solution of the problem
 * (we start with a small sorted list and keep merging other lists of the same size to it).
 */
static EDGE* SortEdges(
	EDGE* list,
	unsigned int level,
	EDGE** head_out
)
{
	EDGE *head_other, *remaining;
	unsigned int i;

	head_other = list->next;

	/* Single element list -> return */
	if (head_other == NULL) {
	*head_out = list;
	return NULL;
	}

	/* Unroll the first iteration of the following loop (halves the number of calls to merge_sorted_edges):
	 *  - Initialize remaining to be the list containing the elements after the second in the input list.
	 *  - Initialize *head_out to be the sorted list containing the first two element.
	 */
	remaining = head_other->next;
	if (list->x.quo <= head_other->x.quo) {
	*head_out = list;
	/* list->next = head_other; */ /* The input list is already like this. */
	head_other->next = NULL;
	} else {
	*head_out = head_other;
	head_other->next = list;
	list->next = NULL;
	}

	for (i = 0; i < level && remaining; i++) {
	/* Extract a sorted list of the same size as *head_out
	 * (2^(i+1) elements) from the list of remaining elements. */
	remaining = SortEdges(remaining, i, &head_other);
	*head_out = MergeSortedEdges(*head_out, head_other);
	}

	/* *head_out now contains (at most) 2^(level+1) elements. */

	return remaining;
}

/* Test if the edges on the active list can be safely advanced by a
 * full row without intersections or any edges ending. */
INLINE static int ActiveListCanStepFullRow(ACTIVE_LIST* active)
{
	const EDGE *e;
	int prev_x = INT_MIN;

	/* Recomputes the minimum height of all edges on the active
	 * list if we have been dropping edges. */
	if(active->min_height <= 0)
	{
		int min_height = INT_MAX;

		e = active->head;
		while (NULL != e)
		{
			if(e->height_left < min_height)
			{
				min_height = e->height_left;
			}
			e = e->next;
		}

		active->min_height = min_height;
	}

	if(active->min_height < GRID_Y)
	{
		return 0;
	}

	/* Check for intersections as no edges end during the next row. */
	e = active->head;
	while(NULL != e)
	{
		QUOREM x = e->x;

		if(!e->vertical)
		{
			x.quo += e->dxdy_full.quo;
			x.rem += e->dxdy_full.rem;
			if(x.rem >= 0)
			{
				++x.quo;
			}
		}

		if (x.quo <= prev_x)
		{
			return 0;
		}

		prev_x = x.quo;
		e = e->next;
	}

	return 1;
}

/* Merges edges on the given subpixel row from the polygon to the
 * active_list. */
INLINE static void ActiveListMergeEdgesFromPolygon(
	ACTIVE_LIST* active,
	EDGE** ptail,
	GRID_SCALED_Y y,
	POLYGON* polygon
)
{
	/* Split off the edges on the current subrow and merge them into
	 * the active list. */
	int min_height = active->min_height;
	EDGE *subrow_edges = NULL;
	EDGE *tail = *ptail;

	do
	{
		EDGE *next = tail->next;

		if(y == tail->ytop)
		{
			tail->next = subrow_edges;
			subrow_edges = tail;

			if(tail->height_left < min_height)
			{
				min_height = tail->height_left;
			}

			*ptail = next;
		}
		else
		{
			ptail = &tail->next;
		}

		tail = next;
	} while(tail);

	if(subrow_edges)
	{
		SortEdges(subrow_edges, UINT_MAX, &subrow_edges);
		active->head = MergeSortedEdges(active->head, subrow_edges);
		active->min_height = min_height;
	}
}

/* Advance the edges on the active list by one subsample row by
 * updating their x positions.  Drop edges from the list that end. */
INLINE static void ActiveListSubstepEdges(ACTIVE_LIST* active)
{
	EDGE **cursor = &active->head;
	GRID_SCALED_X prev_x = INT_MIN;
	EDGE *unsorted = NULL;
	EDGE *edge = *cursor;

	do
	{
		UNROLL3({
			EDGE *next;

		if(NULL == edge)
		{
			break;
		}

		next = edge->next;
		if(--edge->height_left)
		{
			edge->x.quo += edge->dxdy.quo;
			edge->x.rem += edge->dxdy.rem;
			if(edge->x.rem >= 0)
			{
				++edge->x.quo;
				edge->x.rem -= edge->dy;
			}

			if(edge->x.quo < prev_x)
			{
				*cursor = next;
				edge->next = unsorted;
				unsorted = edge;
			}
			else
			{
				prev_x = edge->x.quo;
				cursor = &edge->next;
			}
		}
		else
		{
			*cursor = next;
		}
		edge = next;
	})
	} while (1);

	if(unsorted)
	{
		SortEdges(unsorted, UINT_MAX, &unsorted);
		active->head = MergeSortedEdges(active->head, unsorted);
	}
}

INLINE static void ApplyNonZeroFillRuleForSubrow(
	ACTIVE_LIST* active,
	CELL_LIST* coverages
)
{
	EDGE *edge = active->head;
	int winding = 0;
	int xstart;
	int xend;

	CellListRewind(coverages);

	while(NULL != edge)
	{
		xstart = edge->x.quo;
		winding = edge->direction;
		while(1)
		{
			edge = edge->next;
			if(NULL == edge)
			{
				/* ASSERT_NOT_REACHED; */
				return;
			}

			winding += edge->direction;
			if(0 == winding)
			{
				if(edge->next == NULL || edge->next->x.quo != edge->x.quo)
				{
					break;
				}
			}
		}

		xend = edge->x.quo;
		CellListAddSubspan(coverages, xstart, xend);

		edge = edge->next;
	}
}

static void ApplyEvenOddFillRuleForSubrow(
	ACTIVE_LIST* active,
	CELL_LIST* coverages)
{
	EDGE *edge = active->head;
	int xstart;
	int xend;

	CellListRewind(coverages);

	while(NULL != edge)
	{
		xstart = edge->x.quo;

		while(1)
		{
			edge = edge->next;
			if (NULL == edge)
			{
				/* ASSERT_NOT_REACHED; */
				return;
			}

			if(edge->next == NULL || edge->next->x.quo != edge->x.quo)
				break;

			edge = edge->next;
		}

		xend = edge->x.quo;
		CellListAddSubspan(coverages, xstart, xend);

		edge = edge->next;
	}
}

static void ApplyNonZeroFillRuleAndStepEdges(
	ACTIVE_LIST* active,
	CELL_LIST* coverages
)
{
	EDGE **cursor = &active->head;
	EDGE *left_edge;

	left_edge = *cursor;
	while(NULL != left_edge)
	{
		EDGE *right_edge;
		int winding = left_edge->direction;

		left_edge->height_left -= GRID_Y;
		if(left_edge->height_left)
		{
			cursor = &left_edge->next;
		}
		else
		{
			*cursor = left_edge->next;
		}

		while(1)
		{
			right_edge = *cursor;
			if(NULL == right_edge)
			{
				CellListRenderEdge(coverages, left_edge, +1);
				return;
			}

			right_edge->height_left -= GRID_Y;
			if(right_edge->height_left)
			{
				cursor = &right_edge->next;
			}
			else
			{
				*cursor = right_edge->next;
			}

			winding += right_edge->direction;
			if(0 == winding)
			{
				if(right_edge->next == NULL ||
					right_edge->next->x.quo != right_edge->x.quo)
				{
					break;
				}
			}

			if(! right_edge->vertical)
			{
				right_edge->x.quo += right_edge->dxdy_full.quo;
				right_edge->x.rem += right_edge->dxdy_full.rem;
				if(right_edge->x.rem >= 0)
				{
					++right_edge->x.quo;
					right_edge->x.rem -= right_edge->dy;
				}
			}
		}

		CellListRenderEdge(coverages, left_edge, +1);
		CellListRenderEdge(coverages, right_edge, -1);

		left_edge = *cursor;
	}
}

static void ApplyEvenOddFillRuleAndStepEdges(
	ACTIVE_LIST* active,
	CELL_LIST* coverages
)
{
	EDGE **cursor = &active->head;
	EDGE *left_edge;

	left_edge = *cursor;
	while(NULL != left_edge)
	{
		EDGE *right_edge;

		left_edge->height_left -= GRID_Y;
		if(left_edge->height_left)
		{
			cursor = &left_edge->next;
		}
		else
		{
			*cursor = left_edge->next;
		}

		while(1)
		{
			right_edge = *cursor;
			if(NULL == right_edge)
			{
				CellListRenderEdge(coverages, left_edge, +1);
				return;
			}

			right_edge->height_left -= GRID_Y;
			if(right_edge->height_left)
			{
				cursor = &right_edge->next;
			}
			else
			{
				*cursor = right_edge->next;
			}

			if(right_edge->next == NULL ||
				right_edge->next->x.quo != right_edge->x.quo)
			{
				break;
			}

			if(! right_edge->vertical)
			{
				right_edge->x.quo += right_edge->dxdy_full.quo;
				right_edge->x.rem += right_edge->dxdy_full.rem;
				if(right_edge->x.rem >= 0)
				{
					++right_edge->x.quo;
					right_edge->x.rem -= right_edge->dy;
				}
			}
		}

		CellListRenderEdge(coverages, left_edge,  +1);
		CellListRenderEdge(coverages, right_edge, -1);

		left_edge = *cursor;
	}
}

static void InitializeGlitterScanConverter(
	GRAPHICS_CLIP_TOR_GLITTER_SCAN_CONVERTER* converter,
	jmp_buf* jmp
)
{
	InitializePolygon(converter->polygon, jmp);
	InitializeActiveList(converter->active);
	InitializeCellList(converter->coverages, jmp);
	converter->ymin=0;
	converter->ymax=0;
}

static void GlitterScanConverterFinish(GRAPHICS_CLIP_TOR_GLITTER_SCAN_CONVERTER* converter)
{
	PolygonFinish(converter->polygon);
	CellListFinish(converter->coverages);
	converter->ymin=0;
	converter->ymax=0;
}

static GRID_SCALED IntegerToGridScaled(int i, int scale)
{
	/* Clamp to max/min representable scaled number. */
	if(i >= 0)
	{
		if (i >= INT_MAX/scale)
		{
			i = INT_MAX/scale;
		}
	}
	else
	{
		if(i <= INT_MIN/scale)
		{
			i = INT_MIN/scale;
		}
	}
	return i*scale;
}

#define IntegerToGridScaledX(x) IntegerToGridScaled((x), GRID_X)
#define IntegerToGridScaledY(x) IntegerToGridScaled((x), GRID_Y)

static eGRAPHICS_STATUS GlitterScanConverterReset(
	GRAPHICS_CLIP_TOR_GLITTER_SCAN_CONVERTER* converter,
	int ymin,
	int ymax
)
{
	eGRAPHICS_STATUS status;

	converter->ymin = 0;
	converter->ymax = 0;

	ymin = IntegerToGridScaledY(ymin);
	ymax = IntegerToGridScaledY(ymax);

	ActiveListReset(converter->active);
	CellListReset(converter->coverages);
	status = PolygonReset(converter->polygon, ymin, ymax);
	if(status)
	{
		return status;
	}

	converter->ymin = ymin;
	converter->ymax = ymax;
	return GRAPHICS_STATUS_SUCCESS;
}

/* INPUT_TO_GRID_X/Y (in_coord, out_grid_scaled, grid_scale)
 *   These macros convert an input coordinate in the client's
 *   device space to the rasterisation grid.
 */
/* Gah.. this bit of ugly defines INPUT_TO_GRID_X/Y so as to use
 * shifts if possible, and something saneish if not.
 */
#if !defined(INPUT_TO_GRID_Y) && defined(GRID_Y_BITS) && GRID_Y_BITS <= GLITTER_INPUT_BITS
#  define INPUT_TO_GRID_Y(in, out) (out) = (in) >> (GLITTER_INPUT_BITS - GRID_Y_BITS)
#else
#  define INPUT_TO_GRID_Y(in, out) INPUT_TO_GRID_general(in, out, GRID_Y)
#endif

#if !defined(INPUT_TO_GRID_X) && defined(GRID_X_BITS) && GRID_X_BITS <= GLITTER_INPUT_BITS
#  define INPUT_TO_GRID_X(in, out) (out) = (in) >> (GLITTER_INPUT_BITS - GRID_X_BITS)
#else
#  define INPUT_TO_GRID_X(in, out) INPUT_TO_GRID_general(in, out, GRID_X)
#endif

#define INPUT_TO_GRID_general(in, out, grid_scale) do {		\
	long long tmp__ = (long long)(grid_scale) * (in);	\
	tmp__ >>= GLITTER_INPUT_BITS;				\
	(out) = tmp__;						\
} while (0)

static void GlitterScanConverterAddEdge(
	GRAPHICS_CLIP_TOR_GLITTER_SCAN_CONVERTER* converter,
	const GRAPHICS_EDGE* edge,
	int clip
)
{
	GRAPHICS_EDGE e;

	INPUT_TO_GRID_Y (edge->top, e.top);
	INPUT_TO_GRID_Y (edge->bottom, e.bottom);
	if(e.top >= e.bottom)
	{
		return;
	}

	/* XXX: possible overflows if GRID_X/Y > 2**GLITTER_INPUT_BITS */
	INPUT_TO_GRID_Y(edge->line.point1.y, e.line.point1.y);
	INPUT_TO_GRID_Y(edge->line.point2.y, e.line.point2.y);
	if(e.line.point1.y == e.line.point2.y)
	{
		return;
	}

	INPUT_TO_GRID_X (edge->line.point1.x, e.line.point1.x);
	INPUT_TO_GRID_X (edge->line.point2.x, e.line.point2.x);

	e.direction = edge->direction;

	PolygonAddEdge(converter->polygon, &e, clip);
}

static int ActiveListIsVertical(ACTIVE_LIST* active)
{
	EDGE *e;

	for(e = active->head; e != NULL; e = e->next)
	{
		if(! e->vertical)
		{
			return FALSE;
		}
	}

	return TRUE;
}

static void StepEdges(ACTIVE_LIST* active, int count)
{
	EDGE **cursor = &active->head;
	EDGE *edge;

	for(edge = *cursor; edge != NULL; edge = *cursor)
	{
		edge->height_left -= GRID_Y * count;
		if(edge->height_left)
		{
			cursor = &edge->next;
		}
		else
		{
			*cursor = edge->next;
		}
	}
}

static eGRAPHICS_STATUS BlitCoverages(
	CELL_LIST* cells,
	GRAPHICS_SPAN_RENDERER* renderer,
	POOL* span_pool,
	int y,
	int height
)
{
	CELL *cell = cells->head.next;
	int prev_x = -1;
	int cover = 0, last_cover = 0;
	int clip = 0;
	GRAPHICS_HALF_OPEN_SPAN *spans;
	unsigned num_spans;

	ASSERT(cell != &cells->tail);

	/* Count number of cells remaining. */
	{
		CELL *next = cell;
		num_spans = 2;
		while(next->next)
		{
			next = next->next;
			++num_spans;
		}
		num_spans = 2*num_spans;
	}

	/* Allocate enough spans for the row. */
	PoolReset(span_pool);
	spans = PoolAlloc(span_pool, sizeof(spans[0])*num_spans);
	num_spans = 0;

	/* Form the spans from the coverages and areas. */
	for(; cell->next; cell = cell->next)
	{
		int x = cell->x;
		int area;

		if(x > prev_x && cover != last_cover)
		{
			spans[num_spans].x = prev_x;
			spans[num_spans].coverage = GRID_AREA_TO_ALPHA (cover);
			spans[num_spans].inverse = 0;
			last_cover = cover;
			++num_spans;
		}

		cover += cell->covered_height*GRID_X*2;
		clip += cell->covered_height*GRID_X*2;
		area = cover - cell->uncovered_area;

		if(area != last_cover)
		{
			spans[num_spans].x = x;
			spans[num_spans].coverage = GRID_AREA_TO_ALPHA (area);
			spans[num_spans].inverse = 0;
			last_cover = area;
			++num_spans;
		}

		prev_x = x+1;
	}

	/* Dump them into the renderer. */
	return renderer->render_rows(renderer, y, height, spans, num_spans);
}

static void GlitterScanConverterRender(
	GRAPHICS_CLIP_TOR_GLITTER_SCAN_CONVERTER* converter,
	int nonzero_fill,
	GRAPHICS_SPAN_RENDERER* span_renderer,
	POOL* span_pool
)
{
	int i, j;
	int ymax_i = converter->ymax / GRID_Y;
	int ymin_i = converter->ymin / GRID_Y;
	int h = ymax_i - ymin_i;
	POLYGON *polygon = converter->polygon;
	CELL_LIST *coverages = converter->coverages;
	ACTIVE_LIST *active = converter->active;

	/* Render each pixel row. */
	for(i = 0; i < h; i = j)
	{
		int do_full_step = 0;

		j = i + 1;

		/* Determine if we can ignore this row or use the full pixel
		 * stepper. */
		if(GRID_Y == EDGE_Y_BUCKET_HEIGHT && ! polygon->y_buckets[i])
		{
			if(! active->head)
			{
				for (; j < h && ! polygon->y_buckets[j]; j++) {}
				continue;
			}

			do_full_step = ActiveListCanStepFullRow (active);
		}

		if(do_full_step)
		{
			/* Step by a full pixel row's worth. */
			if(nonzero_fill)
			{
				ApplyNonZeroFillRuleAndStepEdges (active, coverages);
			}
			else
			{
				ApplyEvenOddFillRuleAndStepEdges (active, coverages);
			}

			if(ActiveListIsVertical (active))
			{
				while(j < h &&
						polygon->y_buckets[j] == NULL &&
						active->min_height >= 2*GRID_Y
				)
				{
					active->min_height -= GRID_Y;
					j++;
				}
				if(j != i + 1)
				{
					StepEdges(active, j - (i + 1));
				}
			}
		}
		else
		{
			GRID_SCALED_Y suby;

			/* Subsample this row. */
			for(suby = 0; suby < GRID_Y; suby++)
			{
				GRID_SCALED_X y = (i+ymin_i)*GRID_Y + suby;

				if(polygon->y_buckets[i])
				{
					ActiveListMergeEdgesFromPolygon (active,
							  &polygon->y_buckets[i], y,
							  polygon
					);
				}

				if(nonzero_fill)
				{
					ApplyNonZeroFillRuleForSubrow (active, coverages);
				}
				else
				{
					ApplyEvenOddFillRuleForSubrow (active, coverages);
				}

				ActiveListSubstepEdges(active);
			}
		}

		BlitCoverages(coverages, span_renderer, span_pool, i+ymin_i, j -i);
		CellListReset(coverages);

		if(! active->head)
		{
			active->min_height = INT_MAX;
		}
		else
		{
			active->min_height -= GRID_Y;
		}
	}
}

static void GraphicsClipTorScanConverterDestroy(void* converter)
{
	GRAPHICS_CLIP_TOR_SCAN_CONVERTER *self = converter;
	if(self == NULL)
	{
		return;
	}
	GlitterScanConverterFinish (self->converter);
	PoolFinish(self->span_pool.base);
	MEM_FREE_FUNC(self);
}

static eGRAPHICS_STATUS GraphicsClipTorScanConverterGenerate(
	void* converter,
	GRAPHICS_SPAN_RENDERER* renderer
)
{
	GRAPHICS_CLIP_TOR_SCAN_CONVERTER *self = converter;
	eGRAPHICS_STATUS status;

	if((status = setjmp (self->jmp)))
	{
		return status;
	}

	GlitterScanConverterRender(self->converter,
					self->fill_rule == GRAPHICS_FILL_RULE_WINDING,
					renderer,
					self->span_pool.base
	);
	return GRAPHICS_STATUS_SUCCESS;
}

eGRAPHICS_STATUS InitializeGraphicsClipTorScanConverter(
	GRAPHICS_CLIP_TOR_SCAN_CONVERTER* converter,
	GRAPHICS_CLIP* clip,
	GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE fill_rule,
	eGRAPHICS_ANTIALIAS antialias
)
{
	GRAPHICS_POLYGON clipper;
	eGRAPHICS_STATUS status;
	int i;

	converter->base.destroy = GraphicsClipTorScanConverterDestroy;
	converter->base.generate = GraphicsClipTorScanConverterGenerate;

	InitializePool(converter->span_pool.base, &converter->jmp,
		   250 * sizeof(converter->span_pool.embedded[0]),
		   sizeof(converter->span_pool.embedded));

	InitializeGlitterScanConverter(converter->converter, &converter->jmp);
	status = GlitterScanConverterReset(converter->converter,
					   clip->extents.y,
					   clip->extents.y + clip->extents.height
	);
	if(UNLIKELY(status))
	{
		goto bail;
	}

	converter->fill_rule = fill_rule;
	converter->antialias = antialias;

	for(i = 0; i < polygon->num_edges; i++)
	{
		GlitterScanConverterAddEdge(converter->converter,
					&polygon->edges[i], FALSE);
	}

	status = GraphicsClipGetPolygon(clip, &clipper,
					  &converter->clip_fill_rule, &converter->clip_antialias);
	if(UNLIKELY(status))
	{
		goto bail;
	}

	for(i = 0; i < clipper.num_edges; i++)
	{
		GlitterScanConverterAddEdge(converter->converter,
						&clipper.edges[i], TRUE);
	}
	GraphicsPolygonFinish(&clipper);

	return status;

 bail:
	converter->base.destroy(&converter->base);
	return status;
}

GRAPHICS_SCAN_CONVERTER* GraphicscClipTorScanConverterCreate(
	GRAPHICS_CLIP* clip,
	GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE fill_rule,
	eGRAPHICS_ANTIALIAS antialias
)
{
	GRAPHICS_CLIP_TOR_SCAN_CONVERTER *self;
	eGRAPHICS_STATUS status;

	self = MEM_CALLOC_FUNC(1, sizeof(*self));
	if(UNLIKELY(self == NULL))
	{
		status = GRAPHICS_STATUS_NO_MEMORY;
		goto bail_nomem;
	}

	status = InitializeGraphicsClipTorScanConverter(self, clip,
		polygon, fill_rule, antialias);
	if(UNLIKELY(status))
	{
		return NULL;
	}

	return &self->base;
bail_nomem:
	return NULL;
}

#ifdef __cplusplus
}
#endif
