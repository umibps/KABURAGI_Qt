/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* glitter-paths - polygon scan converter
 *
 * Copyright (c) 2008  M Joonas Pihlaja
 * Copyright (c) 2007  David Turner
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
/* This is the Glitter paths scan converter incorporated into cairo.
 * The source is from commit 734c53237a867a773640bd5b64816249fa1730f8
 * of
 *
 *   https://gitweb.freedesktop.org/?p=users/joonas/glitter-paths
 */
/* Glitter-paths is a stand alone polygon rasteriser derived from
 * David Turner's reimplementation of Tor Anderssons's 15x17
 * supersampling rasteriser from the Apparition graphics library.  The
 * main new feature here is cheaply choosing per-scan line between
 * doing fully analytical coverage computation for an entire row at a
 * time vs. using a supersampling approach.
 *
 * David Turner's code can be found at
 *
 *   http://david.freetype.org/rasterizer-shootout/raster-comparison-20070813.tar.bz2
 *
 * In particular this file incorporates large parts of ftgrays_tor10.h
 * from raster-comparison-20070813.tar.bz2
 */
/* Overview
 *
 * A scan converter's basic purpose to take polygon edges and convert
 * them into an RLE compressed A8 mask.  This one works in two phases:
 * gathering edges and generating spans.
 *
 * 1) As the user feeds the scan converter edges they are vertically
 * clipped and bucketted into a _polygon_ data structure.  The edges
 * are also snapped from the user's coordinates to the subpixel grid
 * coordinates used during scan conversion.
 *
 *	 user
 *	  |
 *	  | edges
 *	  V
 *	polygon buckets
 *
 * 2) Generating spans works by performing a vertical sweep of pixel
 * rows from top to bottom and maintaining an _active_list_ of edges
 * that intersect the row.  From the active list the fill rule
 * determines which edges are the left and right edges of the start of
 * each span, and their contribution is then accumulated into a pixel
 * coverage list (_cell_list_) as coverage deltas.  Once the coverage
 * deltas of all edges are known we can form spans of constant pixel
 * coverage by summing the deltas during a traversal of the cell list.
 * At the end of a pixel row the cell list is sent to a coverage
 * blitter for rendering to some target surface.
 *
 * The pixel coverages are computed by either supersampling the row
 * and box filtering a mono rasterisation, or by computing the exact
 * coverages of edges in the active list.  The supersampling method is
 * used whenever some edge starts or stops within the row or there are
 * edge intersections in the row.
 *
 *   polygon bucket for	   \
 *   current pixel row		|
 *	  |					 |
 *	  | activate new edges  |  Repeat GRID_Y times if we
 *	  V					 \  are supersampling this row,
 *   active list			  /  or just once if we're computing
 *	  |					 |  analytical coverage.
 *	  | coverage deltas	 |
 *	  V					 |
 *   pixel coverage list	 /
 *	  |
 *	  V
 *   coverage blitter
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <setjmp.h>

#include "graphics_types.h"
#include "graphics_compositor.h"
#include "graphics_surface.h"
#include "graphics.h"
#include "graphics_flags.h"
#include "graphics_matrix.h"
#include "graphics_pen.h"
#include "graphics_memory_pool.h"
#include "graphics_private.h"
#include "graphics_scan_converter_private.h"
#include "graphics_inline.h"

/*-------------------------------------------------------------------------
 * cairo specific config
 */
#define I static

/* Prefer cairo's status type. */
#define GLITTER_HAVE_STATUS_T 1
#define GLITTER_STATUS_SUCCESS GRAPHICS_STATUS_SUCCESS
#define GLITTER_STATUS_NO_MEMORY GRAPHICS_STATUS_NO_MEMORY
typedef eGRAPHICS_STATUS eGLITTER_STATUS;

/* The input coordinate scale and the rasterisation grid scales. */
#define GLITTER_INPUT_BITS GRAPHICS_FIXED_FRAC_BITS
//#define GRID_X_BITS CAIRO_FIXED_FRAC_BITS
//#define GRID_Y 15
#define GRID_X_BITS 2
#define GRID_Y_BITS 2

/* Set glitter up to use a cairo span renderer to do the coverage
 * blitting. */
struct GRAPHICS_TOR22_SCAN_ACTIVE_POOL;
struct GRAPHICS_TOR22_SCAN_CELL_LIST;

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
typedef int glitter_input_scaled_t;

#if !GLITTER_HAVE_STATUS_T
typedef enum {
	GLITTER_STATUS_SUCCESS = 0,
	GLITTER_STATUS_NO_MEMORY
} eGLITTER_STATUS;
#endif

#ifndef I
# define I /*static*/
#endif

/* Opaque type for scan converting. */
typedef struct _GLITTER_SCAN_CONVERTER GLITTER_SCAN_CONVERTER;

/* Reset a scan converter to accept polygon edges and set the clip box
 * in pixels.  Allocates O(ymax-ymin) bytes of memory.	The clip box
 * is set to integer pixel coordinates xmin <= x < xmax, ymin <= y <
 * ymax. */
I eGLITTER_STATUS
GlitterScanConverterReset(
	GLITTER_SCAN_CONVERTER* converter,
	int xmin, int ymin,
	int xmax, int ymax
);

/* Render the polygon in the scan converter to the given A8 format
 * image raster.  Only the pixels accessible as pixels[y*stride+x] for
 * x,y inside the clip box are written to, where xmin <= x < xmax,
 * ymin <= y < ymax.  The image is assumed to be clear on input.
 *
 * If nonzero_fill is true then the interior of the polygon is
 * computed with the non-zero fill rule.  Otherwise the even-odd fill
 * rule is used.
 *
 * The scan converter must be reset or destroyed after this call. */

/*-------------------------------------------------------------------------
 * glitter-paths.c: Implementation internal types
 */
#include <stdlib.h>
#include <string.h>
#include <limits.h>

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
	_GRID_TO_INT_FRAC_shift(x, i, f, GRID_X_BITS)
#else
#  define GRID_X_TO_INT_FRAC(x, i, f) \
	_GRID_TO_INT_FRAC_general(x, i, f, GRID_X)
#endif

#define _GRID_TO_INT_FRAC_general(t, i, f, m) do {	\
	(i) = (t) / (m);					\
	(f) = (t) % (m);					\
	if ((f) < 0) {					\
	--(i);						\
	(f) += (m);					\
	}							\
} while (0)

#define _GRID_TO_INT_FRAC_shift(t, i, f, b) do {	\
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
#elif GRID_XY == 32
#  define  GRID_AREA_TO_ALPHA(c)  (((c) << 3) | -(((c) & 0x20) >> 5))
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
INLINE static GRAPHICS_TOR22_SCAN_ACTIVE_QUOREM FlooredDivrem(int a, int b)
{
	GRAPHICS_TOR22_SCAN_ACTIVE_QUOREM qr;
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
static GRAPHICS_TOR22_SCAN_ACTIVE_QUOREM FlooredMulDivrem(int x, int a, int b)
{
	GRAPHICS_TOR22_SCAN_ACTIVE_QUOREM qr;
	long long xa = (long long)x*a;
	qr.quo = (int32)(xa/b);
	qr.rem = xa%b;
	if((xa>=0) != (b>=0) && qr.rem)
	{
		qr.quo -= 1;
		qr.rem += b;
	}
	return qr;
}

static GRAPHICS_TOR22_SCAN_ACTIVE_POOL_CHUNK* InitializePoolChunk(
	GRAPHICS_TOR22_SCAN_ACTIVE_POOL_CHUNK* p,
	GRAPHICS_TOR22_SCAN_ACTIVE_POOL_CHUNK* prev_chunk,
	size_t capacity
)
{
	p->prev_chunk = prev_chunk;
	p->size = 0;
	p->capacity = capacity;
	return p;
}

static GRAPHICS_TOR22_SCAN_ACTIVE_POOL_CHUNK* PoolChunkCreate(GRAPHICS_TOR22_SCAN_ACTIVE_POOL* pool, size_t size)
{
	GRAPHICS_TOR22_SCAN_ACTIVE_POOL_CHUNK *p;

	p = (GRAPHICS_TOR22_SCAN_ACTIVE_POOL_CHUNK*)MEM_ALLOC_FUNC(size + sizeof(GRAPHICS_TOR22_SCAN_ACTIVE_POOL_CHUNK));
	if(UNLIKELY(NULL == p))
	{
		longjmp(*pool->jmp, GRAPHICS_STATUS_NO_MEMORY);
	}

	return InitializePoolChunk(p, pool->current, size);
}

static void InitializePool(
	GRAPHICS_TOR22_SCAN_ACTIVE_POOL* pool,
	jmp_buf *jmp,
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

static void PoolFinish(GRAPHICS_TOR22_SCAN_ACTIVE_POOL* pool)
{
	GRAPHICS_TOR22_SCAN_ACTIVE_POOL_CHUNK *p = pool->current;
	do
	{
		while(NULL != p)
		{
			GRAPHICS_TOR22_SCAN_ACTIVE_POOL_CHUNK *prev = p->prev_chunk;
			if (p != pool->sentinel)
			MEM_FREE_FUNC(p);
			p = prev;
		}
		p = pool->first_free;
		pool->first_free = NULL;
	} while(NULL != p);
}

/* Satisfy an allocation by first allocating a new large enough chunk
 * and adding it to the head of the pool's chunk list. This function
 * is called as a fallback if PoolAlloc() couldn't do a quick
 * allocation from the current chunk in the pool. */
static void* PoolAllocFromNewChunk(
	GRAPHICS_TOR22_SCAN_ACTIVE_POOL* pool,
	size_t size
)
{
	GRAPHICS_TOR22_SCAN_ACTIVE_POOL_CHUNK *chunk;
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
		chunk = PoolChunkCreate (pool, capacity);
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
INLINE static void* PoolAlloc(GRAPHICS_TOR22_SCAN_ACTIVE_POOL* pool, size_t size)
{
	GRAPHICS_TOR22_SCAN_ACTIVE_POOL_CHUNK *chunk = pool->current;

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

/* Relinquish all PoolAlloced memory back to the pool. */
static void PoolReset(GRAPHICS_TOR22_SCAN_ACTIVE_POOL* pool)
{
	/* Transfer all used chunks to the chunk free list. */
	GRAPHICS_TOR22_SCAN_ACTIVE_POOL_CHUNK *chunk = pool->current;
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
 * we're good to CellListFind() the cell any x coordinate. */
INLINE static void CellListRewind (GRAPHICS_TOR22_SCAN_CELL_LIST* cells)
{
	cells->cursor = &cells->head;
}

INLINE static void CellListMaybeRewind(GRAPHICS_TOR22_SCAN_CELL_LIST* cells, int x)
{
	if(x < cells->cursor->x)
	{
		cells->cursor = cells->rewind;
		if(x < cells->cursor->x)
		{
			cells->cursor = &cells->head;
		}
	}
}

INLINE static void CellListSetRewind(GRAPHICS_TOR22_SCAN_CELL_LIST* cells)
{
	cells->rewind = cells->cursor;
}

static void InitializeCellList(GRAPHICS_TOR22_SCAN_CELL_LIST* cells, jmp_buf* jmp)
{
	InitializePool(cells->cell_pool.base, jmp,
			256*sizeof(GRAPHICS_TOR22_SCAN_CELL),
				sizeof(cells->cell_pool.embedded));
	cells->tail.next = NULL;
	cells->tail.x = INT_MAX;
	cells->head.x = INT_MIN;
	cells->head.next = &cells->tail;
	CellListRewind(cells);
}

static void CellListFinish(GRAPHICS_TOR22_SCAN_CELL_LIST* cells)
{
	PoolFinish (cells->cell_pool.base);
}

/* Empty the cell list.  This is called at the start of every pixel
 * row. */
INLINE static void CellListReset(GRAPHICS_TOR22_SCAN_CELL_LIST* cells)
{
	CellListRewind(cells);
	cells->head.next = &cells->tail;
	PoolReset(cells->cell_pool.base);
}

INLINE static GRAPHICS_TOR22_SCAN_CELL* CellListAlloc(
	GRAPHICS_TOR22_SCAN_CELL_LIST* cells,
	GRAPHICS_TOR22_SCAN_CELL* tail,
	int x
)
{
	GRAPHICS_TOR22_SCAN_CELL *cell;

	cell = PoolAlloc(cells->cell_pool.base, sizeof(GRAPHICS_TOR22_SCAN_CELL));
	cell->next = tail->next;
	tail->next = cell;
	cell->x = x;
	*(uint32 *)&cell->uncovered_area = 0;

	return cell;
}

/* Find a cell at the given x-coordinate.  Returns %NULL if a new cell
 * needed to be allocated but couldn't be.  Cells must be found with
 * non-Decrementreasing x-coordinate until the cell list is rewound using
 * CellListRewind(). Ownership of the returned cell is retained by
 * the cell list. */
INLINE static GRAPHICS_TOR22_SCAN_CELL* CellListFind(GRAPHICS_TOR22_SCAN_CELL_LIST* cells, int x)
{
	GRAPHICS_TOR22_SCAN_CELL *tail = cells->cursor;

	if(tail->x == x)
	{
		return tail;
	}

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
		tail = CellListAlloc (cells, tail, x);
	}
	return cells->cursor = tail;

}

/* Find two cells at x1 and x2.	 This is exactly equivalent
 * to
 *
 *   pair.cell1 = CellListFind(cells, x1);
 *   pair.cell2 = CellListFind(cells, x2);
 *
 * except with less function call overhead. */
INLINE static GRAPHICS_TOR22_SCAN_CELL_PAIR CellListFindPair(GRAPHICS_TOR22_SCAN_CELL_LIST* cells, int x1, int x2)
{
	GRAPHICS_TOR22_SCAN_CELL_PAIR pair;

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
		pair.cell1 = CellListAlloc (cells, pair.cell1, x1);
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
		pair.cell2 = CellListAlloc (cells, pair.cell2, x2);
	}

	cells->cursor = pair.cell2;
	return pair;
}

/* Add a subpixel span covering [x1, x2) to the coverage cells. */
INLINE static void CellListAddSubspan(
	GRAPHICS_TOR22_SCAN_CELL_LIST* cells,
	GRID_SCALED_X x1,
	GRID_SCALED_X x2
)
{
	int ix1, fx1;
	int ix2, fx2;

	if(x1 == x2)
	{
		return;
	}

	GRID_X_TO_INT_FRAC(x1, ix1, fx1);
	GRID_X_TO_INT_FRAC(x2, ix2, fx2);

	if(ix1 != ix2)
	{
		GRAPHICS_TOR22_SCAN_CELL_PAIR p;
		p = CellListFindPair(cells, ix1, ix2);
		p.cell1->uncovered_area += 2*fx1;
		++p.cell1->covered_height;
		p.cell2->uncovered_area -= 2*fx2;
		--p.cell2->covered_height;
	}
	else
	{
		GRAPHICS_TOR22_SCAN_CELL *cell = CellListFind(cells, ix1);
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
 * non-Decrementreasing x-coordinate.)  */
static void CellListRenderEdge(
	GRAPHICS_TOR22_SCAN_CELL_LIST* cells,
	GRAPHICS_TOR22_SCAN_ACTIVE_EDGE* edge,
	int sign
)
{
	GRID_SCALED_X fx;
	GRAPHICS_TOR22_SCAN_CELL *cell;
	int ix;

	GRID_X_TO_INT_FRAC(edge->x.quo, ix, fx);

	/* We always know that ix1 is >= the cell list cursor in this
	 * case due to the no-intersections precondition.  */
	cell = CellListFind(cells, ix);
	cell->covered_height += sign*GRID_Y;
	cell->uncovered_area += sign*2*fx*GRID_Y;
}

static void InitializePolygon(GRAPHICS_TOR22_SCAN_ACTIVE_POLYGON* polygon, jmp_buf* jmp)
{
	polygon->ymin = polygon->ymax = 0;
	polygon->y_buckets = polygon->y_buckets_embedded;
	InitializePool(polygon->edge_pool.base, jmp,
					8192 - sizeof(GRAPHICS_TOR22_SCAN_ACTIVE_POOL_CHUNK),
						sizeof(polygon->edge_pool.embedded));
}

static void PolygonFinish(GRAPHICS_TOR22_SCAN_ACTIVE_POLYGON* polygon)
{
	if(polygon->y_buckets != polygon->y_buckets_embedded)
	{
		MEM_FREE_FUNC(polygon->y_buckets);
	}

	PoolFinish (polygon->edge_pool.base);
}

/* Empties the polygon of all edges. The polygon is then prepared to
 * receive new edges and clip them to the vertical range
 * [ymin,ymax). */
static eGLITTER_STATUS PolygonReset(
	GRAPHICS_TOR22_SCAN_ACTIVE_POLYGON* polygon,
	GRID_SCALED_Y ymin,
	GRID_SCALED_Y ymax
)
{
	unsigned h = ymax - ymin;
	unsigned num_buckets = GRAPHICS_TOR22_SCAN_ACTIVE_EDGE_Y_BUCKET_INDEX(ymax + GRID_Y-1, ymin);

	PoolReset(polygon->edge_pool.base);

	if(UNLIKELY(h > 0x7FFFFFFFU - GRID_Y))
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
		polygon->y_buckets = MEM_CALLOC_FUNC(sizeof(GRAPHICS_TOR22_SCAN_ACTIVE_EDGE*), num_buckets);
		if(UNLIKELY(NULL == polygon->y_buckets))
		{
			goto bail_no_mem;
		}
	}
	(void)memset(polygon->y_buckets, 0, num_buckets * sizeof(GRAPHICS_TOR22_SCAN_ACTIVE_EDGE*));

	polygon->ymin = ymin;
	polygon->ymax = ymax;
	return GLITTER_STATUS_SUCCESS;

bail_no_mem:
	polygon->ymin = 0;
	polygon->ymax = 0;
	return GLITTER_STATUS_NO_MEMORY;
}

static void PolygonInsertEdgeIntoItsY_Bucket(
	GRAPHICS_TOR22_SCAN_ACTIVE_POLYGON* polygon,
	GRAPHICS_TOR22_SCAN_ACTIVE_EDGE* e
)
{
	unsigned ix = GRAPHICS_TOR22_SCAN_ACTIVE_EDGE_Y_BUCKET_INDEX(e->ytop, polygon->ymin);
	GRAPHICS_TOR22_SCAN_ACTIVE_EDGE **ptail = &polygon->y_buckets[ix];
	e->next = *ptail;
	*ptail = e;
}

INLINE static void PolygonAddEdge(
	GRAPHICS_TOR22_SCAN_ACTIVE_POLYGON* polygon,
	const GRAPHICS_EDGE* edge
)
{
	GRAPHICS_TOR22_SCAN_ACTIVE_EDGE *e;
	GRID_SCALED_X dx;
	GRID_SCALED_Y dy;
	GRID_SCALED_Y ytop, ybot;
	GRID_SCALED_Y ymin = polygon->ymin;
	GRID_SCALED_Y ymax = polygon->ymax;

	if(UNLIKELY(edge->top >= ymax || edge->bottom <= ymin))
	{
		return;
	}

	e = PoolAlloc(polygon->edge_pool.base, sizeof(GRAPHICS_TOR22_SCAN_ACTIVE_EDGE));

	dx = edge->line.point2.x - edge->line.point1.x;
	dy = edge->line.point2.y - edge->line.point1.y;
	e->dy = dy;
	e->direction = edge->direction;

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
	}
	else
	{
		e->vertical = FALSE;
		e->dxdy = FlooredDivrem (dx, dy);
		if(ytop == edge->line.point1.y)
		{
			e->x.quo = edge->line.point1.x;
			e->x.rem = 0;
		}
		else
		{
			e->x = FlooredMulDivrem (ytop - edge->line.point1.y, dx, dy);
			e->x.quo += edge->line.point1.x;
		}
	}

	PolygonInsertEdgeIntoItsY_Bucket (polygon, e);

	e->x.rem -= dy;		/* Bias the remainder for faster
							 * edge advancement. */
}

static void ActiveListReset(GRAPHICS_TOR22_SCAN_ACTIVE_LIST* active)
{
	active->head.vertical = 1;
	active->head.height_left = INT_MAX;
	active->head.x.quo = INT_MIN;
	active->head.prev = NULL;
	active->head.next = &active->tail;
	active->tail.prev = &active->head;
	active->tail.next = NULL;
	active->tail.x.quo = INT_MAX;
	active->tail.height_left = INT_MAX;
	active->tail.vertical = 1;
	active->min_height = 0;
	active->is_vertical = 1;
}

static void InitializeActiveList(GRAPHICS_TOR22_SCAN_ACTIVE_LIST* active)
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
static GRAPHICS_TOR22_SCAN_ACTIVE_EDGE* MergeSortedEdges(GRAPHICS_TOR22_SCAN_ACTIVE_EDGE* head_a, GRAPHICS_TOR22_SCAN_ACTIVE_EDGE* head_b)
{
	GRAPHICS_TOR22_SCAN_ACTIVE_EDGE *head, **next, *prev;
	int32 x;

	prev = head_a->prev;
	next = &head;
	if(head_a->x.quo <= head_b->x.quo)
	{
		head = head_a;
	}
	else
	{
		head = head_b;
		head_b->prev = prev;
		goto start_with_b;
	}

	do
	{
		x = head_b->x.quo;
		while(head_a != NULL && head_a->x.quo <= x)
		{
			prev = head_a;
			next = &head_a->next;
			head_a = head_a->next;
		}

		head_b->prev = prev;
		*next = head_b;
		if(head_a == NULL)
		{
			return head;
		}
start_with_b:
		x = head_a->x.quo;
		while(head_b != NULL && head_b->x.quo <= x)
		{
			prev = head_b;
			next = &head_b->next;
			head_b = head_b->next;
		}

		head_a->prev = prev;
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
 * Special case single element list, unroll/INLINE the sorting of the first two elements.
 * Some tail recursion is used since we iterate on the bottom-up solution of the problem
 * (we start with a small sorted list and keep merging other lists of the same size to it).
 */
static GRAPHICS_TOR22_SCAN_ACTIVE_EDGE* SortEdges(
	GRAPHICS_TOR22_SCAN_ACTIVE_EDGE* list,
	unsigned int level,
	GRAPHICS_TOR22_SCAN_ACTIVE_EDGE** head_out
)
{
	GRAPHICS_TOR22_SCAN_ACTIVE_EDGE *head_other, *remaining;
	unsigned int i;

	head_other = list->next;

	if(head_other == NULL)
	{
		*head_out = list;
		return NULL;
	}

	remaining = head_other->next;
	if(list->x.quo <= head_other->x.quo)
	{
		*head_out = list;
		head_other->next = NULL;
	}
	else
	{
		*head_out = head_other;
		head_other->prev = list->prev;
		head_other->next = list;
		list->prev = head_other;
		list->next = NULL;
	}

	for(i = 0; i < level && remaining; i++)
	{
		remaining = SortEdges (remaining, i, &head_other);
		*head_out = MergeSortedEdges (*head_out, head_other);
	}

	return remaining;
}

static GRAPHICS_TOR22_SCAN_ACTIVE_EDGE* MergeUnsortedEdges(GRAPHICS_TOR22_SCAN_ACTIVE_EDGE* head, GRAPHICS_TOR22_SCAN_ACTIVE_EDGE* unsorted)
{
	SortEdges(unsorted, UINT_MAX, &unsorted);
	return MergeSortedEdges (head, unsorted);
}

/* Test if the edges on the active list can be safely advanced by a
 * full row without intersections or any edges ending. */
INLINE static int CanDoFullRow(GRAPHICS_TOR22_SCAN_ACTIVE_LIST* active)
{
	const GRAPHICS_TOR22_SCAN_ACTIVE_EDGE *e;

	/* Recomputes the minimum height of all edges on the active
	 * list if we have been dropping edges. */
	if(active->min_height <= 0)
	{
		int min_height = INT_MAX;
		int is_vertical = 1;

		e = active->head.next;
		while(NULL != e)
		{
			if(e->height_left < min_height)
			{
				min_height = e->height_left;
			}
			is_vertical &= e->vertical;
			e = e->next;
		}

		active->is_vertical = is_vertical;
		active->min_height = min_height;
	}

	if(active->min_height < GRID_Y)
	{
		return 0;
	}

	return active->is_vertical;
}

/* Merges edges on the given subpixel row from the polygon to the
 * active_list. */
INLINE static void ActiveListMergeEdgesFromBucket(
	GRAPHICS_TOR22_SCAN_ACTIVE_LIST* active,
	GRAPHICS_TOR22_SCAN_ACTIVE_EDGE* edges
)
{
	active->head.next = MergeUnsortedEdges (active->head.next, edges);
}

INLINE static void PolygonFillBuckets(
	GRAPHICS_TOR22_SCAN_ACTIVE_LIST* active,
	GRAPHICS_TOR22_SCAN_ACTIVE_EDGE* edge,
	int y,
	GRAPHICS_TOR22_SCAN_ACTIVE_EDGE** buckets
)
{
	GRID_SCALED_Y min_height = active->min_height;
	int is_vertical = active->is_vertical;

	while(edge)
	{
		GRAPHICS_TOR22_SCAN_ACTIVE_EDGE *next = edge->next;
		int suby = edge->ytop - y;
		if(buckets[suby])
		{
			buckets[suby]->prev = edge;
		}
		edge->next = buckets[suby];
		edge->prev = NULL;
		buckets[suby] = edge;
		if(edge->height_left < min_height)
		{
			min_height = edge->height_left;
		}
		is_vertical &= edge->vertical;
		edge = next;
	}

	active->is_vertical = is_vertical;
	active->min_height = min_height;
}

INLINE static void SubRow(
	GRAPHICS_TOR22_SCAN_ACTIVE_LIST* active,
	GRAPHICS_TOR22_SCAN_CELL_LIST* coverages,
	unsigned int mask
)
{
	GRAPHICS_TOR22_SCAN_ACTIVE_EDGE *edge = active->head.next;
	int xstart = INT_MIN, prev_x = INT_MIN;
	int winding = 0;

	CellListRewind (coverages);

	while(&active->tail != edge)
	{
		GRAPHICS_TOR22_SCAN_ACTIVE_EDGE *next = edge->next;
		int xend = edge->x.quo;

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
				GRAPHICS_TOR22_SCAN_ACTIVE_EDGE *pos = edge->prev;
				pos->next = next;
				next->prev = pos;
				do
				{
					pos = pos->prev;
				} while(edge->x.quo < pos->x.quo);
				pos->next->prev = edge;
				edge->next = pos->next;
				edge->prev = pos;
				pos->next = edge;
			}
			else
			{
				prev_x = edge->x.quo;
			}
		}
		else
		{
			edge->prev->next = next;
			next->prev = edge->prev;
		}

		winding += edge->direction;
		if((winding & mask) == 0)
		{
			if(next->x.quo != xend)
			{
				CellListAddSubspan (coverages, xstart, xend);
				xstart = INT_MIN;
			}
		}
		else if(xstart == INT_MIN)
		{
			xstart = xend;
		}

		edge = next;
	}
}

INLINE static void Decrement(GRAPHICS_TOR22_SCAN_ACTIVE_EDGE* e, int h)
{
	e->height_left -= h;
	if(e->height_left == 0)
	{
		e->prev->next = e->next;
		e->next->prev = e->prev;
	}
}

static void FullRow(
	GRAPHICS_TOR22_SCAN_ACTIVE_LIST* active,
	GRAPHICS_TOR22_SCAN_CELL_LIST* coverages,
	unsigned int mask
)
{
	GRAPHICS_TOR22_SCAN_ACTIVE_EDGE *left = active->head.next;

	while(&active->tail != left)
	{
		GRAPHICS_TOR22_SCAN_ACTIVE_EDGE *right;
		int winding;

		Decrement(left, GRID_Y);

		winding = left->direction;
		right = left->next;
		do
		{
			Decrement(right, GRID_Y);

			winding += right->direction;
			if((winding & mask) == 0 && right->next->x.quo != right->x.quo)
			{
				break;
			}
			right = right->next;
		} while(1);

		CellListSetRewind(coverages);
		CellListRenderEdge(coverages, left, +1);
		CellListRenderEdge(coverages, right, -1);

		left = right->next;
	}
}

static void InitializeGlitterScanConverter(GLITTER_SCAN_CONVERTER* converter, jmp_buf* jmp)
{
	InitializePolygon(converter->polygon, jmp);
	InitializeActiveList(converter->active);
	InitializeCellList(converter->coverages, jmp);
	converter->xmin=0;
	converter->ymin=0;
	converter->xmax=0;
	converter->ymax=0;
}

static void GlitterScanConverterFinish(GLITTER_SCAN_CONVERTER* self)
{
	if(self->spans != self->spans_embedded)
	{
		MEM_FREE_FUNC(self->spans);
	}

	PolygonFinish(self->polygon);
	CellListFinish(self->coverages);

	self->xmin=0;
	self->ymin=0;
	self->xmax=0;
	self->ymax=0;
}

static GRID_SCALED IntegerToGridScaled(int i, int scale)
{
	/* Clamp to max/min representable scaled number. */
	if(i >= 0)
	{
		if(i >= INT_MAX/scale)
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
#define IntegerToGridScaledY(y) IntegerToGridScaled((y), GRID_Y)

I eGLITTER_STATUS GlitterScanConverterReset(
	GLITTER_SCAN_CONVERTER* converter,
	int xmin, int ymin,
	int xmax, int ymax
)
{
	eGLITTER_STATUS status;

	converter->xmin = 0; converter->xmax = 0;
	converter->ymin = 0; converter->ymax = 0;

	if(xmax - xmin > sizeof(converter->spans_embedded) / sizeof(converter->spans_embedded[0]))
	{
		converter->spans = MEM_CALLOC_FUNC(sizeof(GRAPHICS_HALF_OPEN_SPAN), xmax - xmin);
		if(UNLIKELY(converter->spans == NULL))
		{
			return GRAPHICS_STATUS_NO_MEMORY;
		}
	}
	else
	{
		converter->spans = converter->spans_embedded;
	}

	xmin = IntegerToGridScaledX(xmin);
	ymin = IntegerToGridScaledY(ymin);
	xmax = IntegerToGridScaledX(xmax);
	ymax = IntegerToGridScaledY(ymax);

	ActiveListReset(converter->active);
	CellListReset(converter->coverages);
	status = PolygonReset(converter->polygon, ymin, ymax);
	if(status)
	{
		return status;
	}

	converter->xmin = xmin;
	converter->xmax = xmax;
	converter->ymin = ymin;
	converter->ymax = ymax;
	return GLITTER_STATUS_SUCCESS;
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

/* Add a new polygon edge from pixel (x1,y1) to (x2,y2) to the scan
 * converter.  The coordinates represent pixel positions scaled by
 * 2**GLITTER_PIXEL_BITS.  If this function fails then the scan
 * converter should be reset or destroyed.  Dir must be +1 or -1,
 * with the latter reversing the orientation of the edge. */
I void GlitterScanConverterAddEdge(
	GLITTER_SCAN_CONVERTER* converter,
	const GRAPHICS_EDGE* edge
)
{
	GRAPHICS_EDGE e;

	INPUT_TO_GRID_Y(edge->top, e.top);
	INPUT_TO_GRID_Y(edge->bottom, e.bottom);
	if(e.top >= e.bottom)
	{
		return;
	}

	/* XXX: possible overflows if GRID_X/Y > 2**GLITTER_INPUT_BITS */
	INPUT_TO_GRID_Y(edge->line.point1.y, e.line.point1.y);
	INPUT_TO_GRID_Y(edge->line.point2.y, e.line.point2.y);
	if(e.line.point1.y == e.line.point2.y)
	{
		e.line.point2.y++; /* Fudge to prevent div-by-zero */
	}

	INPUT_TO_GRID_X(edge->line.point1.x, e.line.point1.x);
	INPUT_TO_GRID_X(edge->line.point2.x, e.line.point2.x);

	e.direction = edge->direction;

	PolygonAddEdge(converter->polygon, &e);
}

static void StepEdges(GRAPHICS_TOR22_SCAN_ACTIVE_LIST* active, int count)
{
	GRAPHICS_TOR22_SCAN_ACTIVE_EDGE *edge;

	count *= GRID_Y;
	for(edge = active->head.next; edge != &active->tail; edge = edge->next)
	{
		edge->height_left -= count;
		if(! edge->height_left)
		{
			edge->prev->next = edge->next;
			edge->next->prev = edge->prev;
		}
	}
}

static eGLITTER_STATUS BlitA8(
	GRAPHICS_TOR22_SCAN_CELL_LIST* cells,
	GRAPHICS_SPAN_RENDERER* renderer,
	GRAPHICS_HALF_OPEN_SPAN* spans,
	int y, int height,
	int xmin, int xmax)
{
	GRAPHICS_TOR22_SCAN_CELL *cell = cells->head.next;
	int prev_x = xmin, last_x = -1;
	int16 cover = 0, last_cover = 0;
	unsigned num_spans;

	if(cell == &cells->tail)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	/* Skip cells to the left of the clip region. */
	while(cell->x < xmin)
	{
		cover += cell->covered_height;
		cell = cell->next;
	}
	cover *= GRID_X*2;

	/* Form the spans from the coverages and areas. */
	num_spans = 0;
	for(; cell->x < xmax; cell = cell->next)
	{
		int x = cell->x;
		int16 area;

		if(x > prev_x && cover != last_cover)
		{
			spans[num_spans].x = prev_x;
			spans[num_spans].coverage = GRID_AREA_TO_ALPHA(cover);
			last_cover = cover;
			last_x = prev_x;
			++num_spans;
		}

		cover += cell->covered_height*GRID_X*2;
		area = cover - cell->uncovered_area;

		if(area != last_cover)
		{
			spans[num_spans].x = x;
			spans[num_spans].coverage = GRID_AREA_TO_ALPHA(area);
			last_cover = area;
			last_x = x;
			++num_spans;
		}

		prev_x = x+1;
	}

	if(prev_x <= xmax && cover != last_cover)
	{
		spans[num_spans].x = prev_x;
		spans[num_spans].coverage = GRID_AREA_TO_ALPHA(cover);
		last_cover = cover;
		last_x = prev_x;
		++num_spans;
	}

	if(last_x < xmax && last_cover)
	{
		spans[num_spans].x = xmax;
		spans[num_spans].coverage = 0;
		++num_spans;
	}

	/* Dump them into the renderer. */
	return renderer->render_rows(renderer, y, height, spans, num_spans);
}

#define GRID_AREA_TO_A1(A)  ((GRID_AREA_TO_ALPHA (A) > 127) ? 255 : 0)
static eGLITTER_STATUS BlitA1(
	GRAPHICS_TOR22_SCAN_CELL_LIST* cells,
	GRAPHICS_SPAN_RENDERER* renderer,
	GRAPHICS_HALF_OPEN_SPAN* spans,
	int y, int height,
	int xmin, int xmax
)
{
	GRAPHICS_TOR22_SCAN_CELL *cell = cells->head.next;
	int prev_x = xmin, last_x = -1;
	int16 cover = 0;
	uint8 coverage, last_cover = 0;
	unsigned num_spans;

	if(cell == &cells->tail)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	/* Skip cells to the left of the clip region. */
	while(cell->x < xmin)
	{
		cover += cell->covered_height;
		cell = cell->next;
	}
	cover *= GRID_X*2;

	/* Form the spans from the coverages and areas. */
	num_spans = 0;
	for(; cell->x < xmax; cell = cell->next)
	{
		int x = cell->x;
		int16 area;

		coverage = GRID_AREA_TO_A1 (cover);
		if(x > prev_x && coverage != last_cover)
		{
			last_x = spans[num_spans].x = prev_x;
			last_cover = spans[num_spans].coverage = coverage;
			++num_spans;
		}

		cover += cell->covered_height*GRID_X*2;
		area = cover - cell->uncovered_area;

		coverage = GRID_AREA_TO_A1 (area);
		if(coverage != last_cover)
		{
			last_x = spans[num_spans].x = x;
			last_cover = spans[num_spans].coverage = coverage;
			++num_spans;
		}

		prev_x = x+1;
	}

	coverage = GRID_AREA_TO_A1(cover);
	if(prev_x <= xmax && coverage != last_cover)
	{
		last_x = spans[num_spans].x = prev_x;
		last_cover = spans[num_spans].coverage = coverage;
		++num_spans;
	}

	if(last_x < xmax && last_cover)
	{
		spans[num_spans].x = xmax;
		spans[num_spans].coverage = 0;
		++num_spans;
	}
	if(num_spans == 1)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	/* Dump them into the renderer. */
	return renderer->render_rows(renderer, y, height, spans, num_spans);
}


I void GlitterScanConverterRender(
	GLITTER_SCAN_CONVERTER* converter,
	unsigned int winding_mask,
	int antialias,
	GRAPHICS_SPAN_RENDERER* renderer
)
{
	int i, j;
	int ymax_i = converter->ymax / GRID_Y;
	int ymin_i = converter->ymin / GRID_Y;
	int xmin_i, xmax_i;
	int h = ymax_i - ymin_i;
	GRAPHICS_TOR22_SCAN_ACTIVE_POLYGON *polygon = converter->polygon;
	GRAPHICS_TOR22_SCAN_CELL_LIST *coverages = converter->coverages;
	GRAPHICS_TOR22_SCAN_ACTIVE_LIST *active = converter->active;
	GRAPHICS_TOR22_SCAN_ACTIVE_EDGE *buckets[GRID_Y] = { 0 };

	xmin_i = converter->xmin / GRID_X;
	xmax_i = converter->xmax / GRID_X;
	if(xmin_i >= xmax_i)
	{
		return;
	}

	/* Render each pixel row. */
	for(i = 0; i < h; i = j)
	{
		int do_FullRow = 0;

		j = i + 1;

		/* Determine if we can ignore this row or use the full pixel
		 * stepper. */
		if(! polygon->y_buckets[i])
		{
			if(active->head.next == &active->tail)
			{
				active->min_height = INT_MAX;
				active->is_vertical = 1;
				for(; j < h && ! polygon->y_buckets[j]; j++) {}
				continue;
			}

			do_FullRow = CanDoFullRow(active);
		}

		if(do_FullRow)
		{
			/* Step by a full pixel row's worth. */
			FullRow(active, coverages, winding_mask);

			if(active->is_vertical)
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
					StepEdges (active, j - (i + 1));
				}
			}
		}
		else
		{
			int sub;

			PolygonFillBuckets(active,
								polygon->y_buckets[i],
								(i+ymin_i)*GRID_Y,
								buckets);

			/* Subsample this row. */
			for(sub = 0; sub < GRID_Y; sub++)
			{
				if(buckets[sub])
				{
					ActiveListMergeEdgesFromBucket(active, buckets[sub]);
					buckets[sub] = NULL;
				}

				SubRow (active, coverages, winding_mask);
			}
		}

		if(antialias)
		{
			BlitA8 (coverages, renderer, converter->spans,
						i+ymin_i, j-i, xmin_i, xmax_i);
		}
		else
		{
			BlitA1(coverages, renderer, converter->spans,
					i+ymin_i, j-i, xmin_i, xmax_i);
		}
		CellListReset (coverages);

		active->min_height -= GRID_Y;
	}
}

static void GraphicsTor22ScanConverterDestroy(void* converter)
{
	GRAPHICS_TOR22_SCAN_CONVERTER *self = (GRAPHICS_TOR22_SCAN_CONVERTER*)converter;
	if(self == NULL)
	{
		return;
	}
	GlitterScanConverterFinish (self->converter);
	MEM_FREE_FUNC(self);
}

eGRAPHICS_STATUS GraphicsTor22ScanConverterAddPolygon(
	void* converter,
	const GRAPHICS_POLYGON* polygon
)
{
	GRAPHICS_TOR22_SCAN_CONVERTER *self = (GRAPHICS_TOR22_SCAN_CONVERTER*)converter;
	int i;

#if 0
	FILE *file = fopen ("polygon.txt", "w");
	_cairo_debug_print_polygon (file, polygon);
	fclose (file);
#endif

	for(i = 0; i < polygon->num_edges; i++)
	{
		GlitterScanConverterAddEdge(self->converter, &polygon->edges[i]);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsTor22ScanConverterGenerate(
	void* converter,
	GRAPHICS_SPAN_RENDERER* renderer
)
{
	GRAPHICS_TOR22_SCAN_CONVERTER *self = (GRAPHICS_TOR22_SCAN_CONVERTER*)converter;
	eGRAPHICS_STATUS status;

	if((status = setjmp (self->jmp)))
	{
		return status;
	}

	GlitterScanConverterRender(self->converter,
								self->fill_rule == GRAPHICS_FILL_RULE_WINDING ? ~0 : 1,
								self->antialias != GRAPHICS_ANTIALIAS_NONE,
								renderer);
	return GRAPHICS_STATUS_SUCCESS;
}

eGRAPHICS_STATUS InitializeGraphicsTor22ScanConverter(
	GRAPHICS_TOR22_SCAN_CONVERTER* converter,
	int xmin,
	int ymin,
	int xmax,
	int ymax,
	eGRAPHICS_FILL_RULE fill_rule,
	eGRAPHICS_ANTIALIAS antialias
)
{
	eGRAPHICS_STATUS status;

	converter->base.destroy = GraphicsTor22ScanConverterDestroy;
	converter->base.generate = GraphicsTor22ScanConverterGenerate;

	InitializeGlitterScanConverter(converter->converter, &converter->jmp);
	status = GlitterScanConverterReset (converter->converter,
					   xmin, ymin, xmax, ymax);
	if(UNLIKELY(status))
	{
		goto bail;
	}

	converter->fill_rule = fill_rule;
	converter->antialias = antialias;

	return GRAPHICS_STATUS_SUCCESS;

 bail:
	converter->base.destroy(&converter->base);

	return GRAPHICS_STATUS_NULL_POINTER;
}

GRAPHICS_SCAN_CONVERTER* GraphicsTor22ScanConverterCreate(
	int xmin,
	int ymin,
	int xmax,
	int ymax,
	eGRAPHICS_FILL_RULE	fill_rule,
	eGRAPHICS_ANTIALIAS	antialias
)
{
	GRAPHICS_TOR22_SCAN_CONVERTER *self;
	eGRAPHICS_STATUS status;

	self = (GRAPHICS_TOR22_SCAN_CONVERTER*)MEM_ALLOC_FUNC(sizeof(GRAPHICS_TOR22_SCAN_CONVERTER));
	if(UNLIKELY(self == NULL))
	{
		status = GRAPHICS_STATUS_NO_MEMORY;
		goto bail_nomem;
	}

	status = InitializeGraphicsTor22ScanConverter(self, xmin, ymin,
		xmax, ymax, fill_rule, antialias);

	return &self->base;

 bail_nomem:
	return &self->base;
}
