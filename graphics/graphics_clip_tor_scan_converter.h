#ifndef _INCLUDED_GRAPHICS_CLIP_TOR_SCAN_CONVERTER_H_
#define _INCLUDED_GRAPHICS_CLIP_TOR_SCAN_CONVERTER_H_

#include <setjmp.h>
#include "graphics_types.h"

typedef int GLITTER_INPUT_SCALED;

/* Opaque type for scan converting. */
typedef struct _GLITTER_SCAN_CONVERTER GLITTER_SCAN_CONVERTER;

/* All polygon coordinates are snapped onto a subsample grid. "Grid
 * scaled" numbers are fixed precision reals with multiplier GRID_X or
 * GRID_Y. */
typedef int GRID_SCALED;
typedef int GRID_SCALED_X;
typedef int GRID_SCALED_Y;

typedef struct _QUOREM
{
	int32 quo;
	int32 rem;
} QUOREM;

/* Header for a chunk of memory in a memory pool. */
typedef struct _POOL_CHUNK
{
	/* # bytes used in this chunk. */
	size_t size;

	/* # bytes total in this chunk */
	size_t capacity;

	/* Pointer to the previous chunk or %NULL if this is the sentinel
	 * chunk in the pool header. */
	struct _POOL_CHUNK *prev_chunk;

	/* Actual data starts here.	 Well aligned for pointers. */
} POOL_CHUNK;

/* A memory pool.  This is supposed to be embedded on the stack or
 * within some other structure.	 It may optionally be followed by an
 * embedded array from which requests are fulfilled until
 * malloc needs to be called to allocate a first real chunk. */
typedef struct _POOL
{
	/* Chunk we're allocating from. */
	POOL_CHUNK *current;

	jmp_buf *jmp;

	/* Free list of previously allocated chunks.  All have >= default
	 * capacity. */
	POOL_CHUNK *first_free;

	/* The default capacity of a chunk. */
	size_t default_capacity;

	/* Header for the sentinel chunk.  Directly following the pool
	 * struct should be some space for embedded elements from which
	 * the sentinel chunk allocates from. */
	POOL_CHUNK sentinel[1];
} POOL;

/* A polygon edge. */
typedef struct _EDGE
{
	/* Next in y-bucket or active list. */
	struct _EDGE *next;

	/* Current x coordinate while the edge is on the active
	 * list. Initialised to the x coordinate of the top of the
	 * edge. The quotient is in grid_scaled_x_t units and the
	 * remainder is mod dy in grid_scaled_y_t units.*/
	QUOREM x;

	/* Advance of the current x when moving down a subsample line. */
	QUOREM dxdy;

	/* Advance of the current x when moving down a full pixel
	 * row. Only initialised when the height of the edge is large
	 * enough that there's a chance the edge could be stepped by a
	 * full row's worth of subsample rows at a time. */
	QUOREM dxdy_full;

	/* The clipped y of the top of the edge. */
	GRID_SCALED_Y ytop;

	/* y2-y1 after orienting the edge downwards.  */
	GRID_SCALED_Y dy;

	/* Number of subsample rows remaining to scan convert of this
	 * edge. */
	GRID_SCALED_Y height_left;

	/* Original sign of the edge: +1 for downwards, -1 for upwards
	 * edges.  */
	int direction;
	int vertical;
	int clip;
} EDGE;

typedef int GRID_AREA;

/* Number of subsample rows per y-bucket. Must be GRID_Y. */
#define EDGE_Y_BUCKET_HEIGHT GRID_Y

#define EDGE_Y_BUCKET_INDEX(y, ymin) (((y) - (ymin))/EDGE_Y_BUCKET_HEIGHT)

/* A collection of sorted and vertically clipped edges of the polygon.
 * Edges are moved from the polygon to an active list while scan
 * converting. */
typedef struct _POLYGON
{
	/* The vertical clip extents. */
	GRID_SCALED_Y ymin, ymax;

	/* Array of edges all starting in the same bucket.	An edge is put
	 * into bucket EDGE_BUCKET_INDEX(edge->ytop, polygon->ymin) when
	 * it is added to the polygon. */
	EDGE **y_buckets;
	EDGE *y_buckets_embedded[64];

	struct
	{
		POOL base[1];
		EDGE embedded[32];
	} edge_pool;
} POLYGON;

/* A cell records the effect on pixel coverage of polygon edges
 * passing through a pixel.  It contains two accumulators of pixel
 * coverage.
 *
 * Consider the effects of a polygon edge on the coverage of a pixel
 * it intersects and that of the following one.  The coverage of the
 * following pixel is the height of the edge multiplied by the width
 * of the pixel, and the coverage of the pixel itself is the area of
 * the trapezoid formed by the edge and the right side of the pixel.
 *
 * +-----------------------+-----------------------+
 * |					   |					   |
 * |					   |					   |
 * |_______________________|_______________________|
 * |   \...................|.......................|\
 * |	\..................|.......................| |
 * |	 \.................|.......................| |
 * |	  \....covered.....|.......................| |
 * |	   \....area.......|.......................| } covered height
 * |		\..............|.......................| |
 * |uncovered\.............|.......................| |
 * |  area	\............|.......................| |
 * |___________\...........|.......................|/
 * |					   |					   |
 * |					   |					   |
 * |					   |					   |
 * +-----------------------+-----------------------+
 *
 * Since the coverage of the following pixel will always be a multiple
 * of the width of the pixel, we can store the height of the covered
 * area instead.  The coverage of the pixel itself is the total
 * coverage minus the area of the uncovered area to the left of the
 * edge.  As it's faster to compute the uncovered area we only store
 * that and subtract it from the total coverage later when forming
 * spans to blit.
 *
 * The heights and areas are signed, with left edges of the polygon
 * having positive sign and right edges having negative sign.  When
 * two edges intersect they swap their left/rightness so their
 * contribution above and below the intersection point must be
 * computed separately. */
typedef struct _CELL
{
	struct _CELL* next;
	int x;
	GRID_AREA uncovered_area;
	GRID_SCALED_Y covered_height;
	GRID_SCALED_Y clipped_height;
} CELL;

/* A cell list represents the scan line sparsely as cells ordered by
 * ascending x.  It is geared towards scanning the cells in order
 * using an internal cursor. */
typedef struct _CELL_LIST
{
	/* Sentinel nodes */
	CELL head, tail;

	/* Cursor state for iterating through the cell list. */
	CELL *cursor;

	/* Cells in the cell list are owned by the cell list and are
	 * allocated from this pool.  */
	struct
	{
		POOL base[1];
		CELL embedded[32];
	} cell_pool;
} CELL_LIST;

typedef struct _CELL_PAIR
{
	CELL *cell1;
	CELL *cell2;
} CELL_PAIR;

/* The active list contains edges in the current scan line ordered by
 * the x-coordinate of the intercept of the edge and the scan line. */
typedef struct _ACTIVE_LIST
{
	/* Leftmost edge on the current scan line. */
	EDGE *head;

	/* A lower bound on the height of the active edges is used to
	 * estimate how soon some active edge ends.	 We can't advance the
	 * scan conversion by a full pixel row if an edge ends somewhere
	 * within it. */
	GRID_SCALED_Y min_height;
} ACTIVE_LIST;

typedef struct _GRAPHICS_CLIP_TOR_GLITTER_SCAN_CONVERTER
{
	POLYGON	polygon[1];
	ACTIVE_LIST	active[1];
	CELL_LIST	coverages[1];

	/* Clip box. */
	GRID_SCALED_Y ymin, ymax;
} GRAPHICS_CLIP_TOR_GLITTER_SCAN_CONVERTER;

struct _GRAPHICS_CLIP_TOR_SCAN_CONVERTER
{
	GRAPHICS_SCAN_CONVERTER base;

	GRAPHICS_CLIP_TOR_GLITTER_SCAN_CONVERTER converter[1];
	eGRAPHICS_FILL_RULE fill_rule;
	eGRAPHICS_ANTIALIAS antialias;

	eGRAPHICS_FILL_RULE clip_fill_rule;
	eGRAPHICS_ANTIALIAS clip_antialias;

	jmp_buf jmp;

	struct
	{
		POOL base[1];
		GRAPHICS_HALF_OPEN_SPAN embedded[32];
	} span_pool;
};

typedef struct _GRAPHICS_CLIP_TOR_SCAN_CONVERTER GRAPHICS_CLIP_TOR_SCAN_CONVERTER;

#ifdef __cplusplus
extern "C" {
#endif

extern eGRAPHICS_STATUS InitializeGraphicsClipTorScanConverter(
	GRAPHICS_CLIP_TOR_SCAN_CONVERTER* converter,
	GRAPHICS_CLIP* clip,
	GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE fill_rule,
	eGRAPHICS_ANTIALIAS antialias
);

extern GRAPHICS_SCAN_CONVERTER* GraphicscClipTorScanConverterCreate(
	GRAPHICS_CLIP* clip,
	GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE fill_rule,
	eGRAPHICS_ANTIALIAS antialias
);

#ifdef __cplusplus
}
#endif

#endif	/* #ifndef _INCLUDED_GRAPHICS_CLIP_TOR_SCAN_CONVERTER_H_ */
