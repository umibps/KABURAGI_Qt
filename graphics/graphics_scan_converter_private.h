#ifndef _INCLUDED_GRAPHICS_SCAN_CONVERTER_PRIVATE_H_
#define _INCLUDED_GRAPHICS_SCAN_CONVERTER_PRIVATE_H_

#include <setjmp.h>
#include "graphics_compositor.h"

typedef struct _MONO_SCAN_QUOREM
{
	int32 quo;
	int32 rem;
} MONO_SCAN_QUOREM;

typedef struct _MONO_SCAN_EDGE
{
	struct _MONO_SCAN_EDGE *next, *prev;

	int32 height_left;
	int32 direction;
	int32 vertical;

	int32 dy;
	MONO_SCAN_QUOREM x;
	MONO_SCAN_QUOREM dxdy;
} MONO_SCAN_EDGE;

/* A collection of sorted and vertically clipped edges of the polygon.
* Edges are moved from the polygon to an active list while scan
* converting. */
typedef struct _MONO_SCAN_POLYGON
{
	/* The vertical clip extents. */
	int32 ymin, ymax;

	int num_edges;
	MONO_SCAN_EDGE *edges;

	/* Array of edges all starting in the same bucket.	An edge is put
	* into bucket MONO_SCAN_EDGE_BUCKET_INDEX(edge->ytop, polygon->ymin) when
	* it is added to the polygon. */
	MONO_SCAN_EDGE **y_buckets;

	MONO_SCAN_EDGE *y_buckets_embedded[64];
	MONO_SCAN_EDGE edges_embedded[32];
} MONO_SCAN_POLYGON;

typedef struct _MONO_SCAN_CONVERTER
{
	MONO_SCAN_POLYGON polygon[1];

	/* Leftmost edge on the current scan line. */
	MONO_SCAN_EDGE head, tail;
	int is_vertical;

	GRAPHICS_HALF_OPEN_SPAN *spans;
	GRAPHICS_HALF_OPEN_SPAN spans_embedded[64];

	int num_spans;

	/* Clip box. */
	int32 xmin, xmax;
	int32 ymin, ymax;
} MONO_SCAN_CONVERTER;

typedef struct _GRAPHICS_MONO_SCAN_CONVERTER
{
	GRAPHICS_SCAN_CONVERTER base;

	MONO_SCAN_CONVERTER converter[1];
	eGRAPHICS_FILL_RULE fill_rule;
} GRAPHICS_MONO_SCAN_CONVERTER;


typedef struct _GRAPHICS_TOR_SCAN_CONVERTER_QUOREM
{
	int32 quo;
	int64 rem;
} GRAPHICS_TOR_SCAN_CONVERTER_QUOREM;

/* Header for a chunk of memory in a memory pool. */
typedef struct _GRAPHICS_TOR_SCAN_CONVERTER_POOL_CHUNK
{
	/* # bytes used in this chunk. */
	size_t size;

	/* # bytes total in this chunk */
	size_t capacity;

	/* Pointer to the previous chunk or %NULL if this is the sentinel
	* chunk in the pool header. */
	struct _GRAPHICS_TOR_SCAN_CONVERTER_POOL_CHUNK *prev_chunk;

	/* Actual data starts here. Well aligned even for 64 bit types. */
	int64 data;
} GRAPHICS_TOR_SCAN_CONVERTER_POOL_CHUNK;

/* The int64_t data member of _pool_chunk just exists to enforce alignment,
* it shouldn't be included in the allocated size for the struct. */
#define GRAPHICS_TOR_SCAN_CONVERTER_SIZEOF_POOL_CHUNK (sizeof(GRAPHICS_TOR_SCAN_CONVERTER_POOL_CHUNK) - sizeof(int64))

/* A memory pool.  This is supposed to be embedded on the stack or
* within some other structure.	 It may optionally be followed by an
* embedded array from which requests are fulfilled until
* malloc needs to be called to allocate a first real chunk. */
typedef struct _GRAPHICS_TOR_SCAN_CONVERTER_POOL
{
	/* Chunk we're allocating from. */
	GRAPHICS_TOR_SCAN_CONVERTER_POOL_CHUNK *current;

	jmp_buf *jmp;

	/* Free list of previously allocated chunks.  All have >= default
	* capacity. */
	GRAPHICS_TOR_SCAN_CONVERTER_POOL_CHUNK *first_free;

	/* The default capacity of a chunk. */
	size_t default_capacity;

	/* Header for the sentinel chunk.  Directly following the pool
	* struct should be some space for embedded elements from which
	* the sentinel chunk allocates from. This is expressed as a char
	* array so that the 'int64_t data' member of _pool_chunk isn't
	* included. This way embedding struct pool in other structs works
	* without wasting space. */
	char sentinel[GRAPHICS_TOR_SCAN_CONVERTER_SIZEOF_POOL_CHUNK];
} GRAPHICS_TOR_SCAN_CONVERTER_POOL;

/* A polygon edge. */
typedef struct _GRAPHICS_TOR_SCAN_CONVERTER_EDGE
{
	/* Next in y-bucket or active list. */
	struct _GRAPHICS_TOR_SCAN_CONVERTER_EDGE *next, *prev;

	/* The clipped y of the top of the edge. */
	int ytop;

	/* Number of subsample rows remaining to scan convert of this
	* edge. */
	int height_left;

	/* Original sign of the edge: +1 for downwards, -1 for upwards
	* edges.  */
	int direction;
	int cell;

	/* Current x coordinate while the edge is on the active
	* list. Initialised to the x coordinate of the top of the
	* edge. The quotient is in int units and the
	* remainder is mod dy in int units.*/
	GRAPHICS_TOR_SCAN_CONVERTER_QUOREM x;

	/* Advance of the current x when moving down a subsample line. */
	GRAPHICS_TOR_SCAN_CONVERTER_QUOREM dxdy;

	/* Advance of the current x when moving down a full pixel
	* row. Only initialised when the height of the edge is large
	* enough that there's a chance the edge could be stepped by a
	* full row's worth of subsample rows at a time. */
	GRAPHICS_TOR_SCAN_CONVERTER_QUOREM dxdy_full;

	/* y2-y1 after orienting the edge downwards.  */
	int64 dy;
} GRAPHICS_TOR_SCAN_CONVERTER_EDGE;

/* A collection of sorted and vertically clipped edges of the polygon.
* Edges are moved from the polygon to an active list while scan
* converting. */
typedef struct _GRAPHICS_TOR_SCAN_CONVERTER_POLYGON
{
	/* The vertical clip extents. */
	int ymin, ymax;

	/* Array of edges all starting in the same bucket.	An edge is put
	* into bucket EDGE_BUCKET_INDEX(edge->ytop, polygon->ymin) when
	* it is added to the polygon. */
	GRAPHICS_TOR_SCAN_CONVERTER_EDGE **y_buckets;
	GRAPHICS_TOR_SCAN_CONVERTER_EDGE *y_buckets_embedded[64];

	struct
	{
		GRAPHICS_TOR_SCAN_CONVERTER_POOL base[1];
		GRAPHICS_TOR_SCAN_CONVERTER_EDGE embedded[32];
	} edge_pool;
} GRAPHICS_TOR_SCAN_CONVERTER_POLYGON;

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
typedef struct _GRAPHICS_TOR_SCAN_CONVERTER_CELL
{
	struct _GRAPHICS_TOR_SCAN_CONVERTER_CELL *next;
	int			 x;
	int16		 uncovered_area;
	int16		 covered_height;
} GRAPHICS_TOR_SCAN_CONVERTER_CELL;

/* A cell list represents the scan line sparsely as cells ordered by
* ascending x.  It is geared towards scanning the cells in order
* using an internal cursor. */
typedef struct _GRAPHICS_TOR_SCAN_CONVERTER_CELL_LIST
{
	/* Sentinel nodes */
	struct _GRAPHICS_TOR_SCAN_CONVERTER_CELL head, tail;

	/* Cursor state for iterating through the cell list. */
	GRAPHICS_TOR_SCAN_CONVERTER_CELL *cursor, *rewind;

	/* Cells in the cell list are owned by the cell list and are
	* allocated from this pool.  */
	struct {
		GRAPHICS_TOR_SCAN_CONVERTER_POOL base[1];
		GRAPHICS_TOR_SCAN_CONVERTER_CELL embedded[32];
	} cell_pool;
} GRAPHICS_TOR_SCAN_CONVERTER_CELL_LIST;

typedef struct _GRAPHICS_TOR_SCAN_CONVERTER_CELL_PAIR
{
	GRAPHICS_TOR_SCAN_CONVERTER_CELL *cell1;
	GRAPHICS_TOR_SCAN_CONVERTER_CELL *cell2;
} GRAPHICS_TOR_SCAN_CONVERTER_CELL_PAIR;

/* The active list contains edges in the current scan line ordered by
* the x-coordinate of the intercept of the edge and the scan line. */
typedef struct _GRAPHICS_TOR_SCAN_CONVERTER_ACTIVE_LIST
{
	/* Leftmost edge on the current scan line. */
	GRAPHICS_TOR_SCAN_CONVERTER_EDGE head, tail;

	/* A lower bound on the height of the active edges is used to
	* estimate how soon some active edge ends.	 We can't advance the
	* scan conversion by a full pixel row if an edge ends somewhere
	* within it. */
	int min_height;
	int is_vertical;
} GRAPHICS_TOR_SCAN_CONVERTER_ACTIVE_LIST;

typedef struct _GRAPHICS_TOR_GLITTER_SCAN_CONVERTER
{
	GRAPHICS_TOR_SCAN_CONVERTER_POLYGON	polygon[1];
	GRAPHICS_TOR_SCAN_CONVERTER_ACTIVE_LIST	active[1];
	GRAPHICS_TOR_SCAN_CONVERTER_CELL_LIST	coverages[1];

	GRAPHICS_HALF_OPEN_SPAN *spans;
	GRAPHICS_HALF_OPEN_SPAN spans_embedded[64];

	/* Clip box. */
	int xmin, xmax;
	int ymin, ymax;
} GRAPHICS_TOR_GLITTER_SCAN_CONVERTER;

typedef struct _GRAPHICS_TOR_SCAN_CONVERTER
{
	GRAPHICS_SCAN_CONVERTER base;

	GRAPHICS_TOR_GLITTER_SCAN_CONVERTER converter[1];
	eGRAPHICS_FILL_RULE fill_rule;
	eGRAPHICS_ANTIALIAS antialias;

	jmp_buf jmp;
} GRAPHICS_TOR_SCAN_CONVERTER;

/* ===================TOR22_SCAN_CONVERTER================================ */

/* All polygon coordinates are snapped onto a subsample grid. "Grid
* scaled" numbers are fixed precision reals with multiplier GRID_X or
* GRID_Y. */
typedef int GRID_SCALED;
typedef int GRID_SCALED_X;
typedef int GRID_SCALED_Y;

typedef struct _GRAPHICS_TOR22_SCAN_ACTIVE_QUOREM
{
	int32 quo;
	int32 rem;
} GRAPHICS_TOR22_SCAN_ACTIVE_QUOREM;

/* Header for a chunk of memory in a memory pool. */
typedef struct _GRAPHICS_TOR22_SCAN_ACTIVE_POOL_CHUNK
{
	/* # bytes used in this chunk. */
	size_t size;

	/* # bytes total in this chunk */
	size_t capacity;

	/* Pointer to the previous chunk or %NULL if this is the sentinel
	* chunk in the pool header. */
	struct _GRAPHICS_TOR22_SCAN_ACTIVE_POOL_CHUNK *prev_chunk;

	/* Actual data starts here.	 Well aligned for pointers. */
} GRAPHICS_TOR22_SCAN_ACTIVE_POOL_CHUNK;

/* A memory pool.  This is supposed to be embedded on the stack or
* within some other structure.	 It may optionally be followed by an
* embedded array from which requests are fulfilled until
* malloc needs to be called to allocate a first real chunk. */
typedef struct _GRAPHICS_TOR22_SCAN_ACTIVE_POOL
{
	/* Chunk we're allocating from. */
	GRAPHICS_TOR22_SCAN_ACTIVE_POOL_CHUNK *current;

	jmp_buf *jmp;

	/* Free list of previously allocated chunks.  All have >= default
	* capacity. */
	GRAPHICS_TOR22_SCAN_ACTIVE_POOL_CHUNK *first_free;

	/* The default capacity of a chunk. */
	size_t default_capacity;

	/* Header for the sentinel chunk.  Directly following the pool
	* struct should be some space for embedded elements from which
	* the sentinel chunk allocates from. */
	GRAPHICS_TOR22_SCAN_ACTIVE_POOL_CHUNK sentinel[1];
} GRAPHICS_TOR22_SCAN_ACTIVE_POOL;

/* A polygon edge. */
typedef struct _GRAPHICS_TOR22_SCAN_ACTIVE_EDGE
{
	/* Next in y-bucket or active list. */
	struct _GRAPHICS_TOR22_SCAN_ACTIVE_EDGE *next, *prev;

	/* Number of subsample rows remaining to scan convert of this
	* edge. */
	GRID_SCALED_Y height_left;

	/* Original sign of the edge: +1 for downwards, -1 for upwards
	* edges.  */
	int direction;
	int vertical;

	/* Current x coordinate while the edge is on the active
	* list. Initialised to the x coordinate of the top of the
	* edge. The quotient is in GRID_SCALED_X units and the
	* remainder is mod dy in GRID_SCALED_Y units.*/
	GRAPHICS_TOR22_SCAN_ACTIVE_QUOREM x;

	/* Advance of the current x when moving down a subsample line. */
	GRAPHICS_TOR22_SCAN_ACTIVE_QUOREM dxdy;

	/* The clipped y of the top of the edge. */
	GRID_SCALED_Y ytop;

	/* y2-y1 after orienting the edge downwards.  */
	GRID_SCALED_Y dy;
} GRAPHICS_TOR22_SCAN_ACTIVE_EDGE;

#define GRAPHICS_TOR22_SCAN_ACTIVE_EDGE_Y_BUCKET_INDEX(y, ymin) (((y) - (ymin))/GRID_Y)

/* A collection of sorted and vertically clipped edges of the polygon.
* Edges are moved from the polygon to an active list while scan
* converting. */
typedef struct _GRAPHICS_TOR22_SCAN_ACTIVE_POLYGON
{
	/* The vertical clip extents. */
	GRID_SCALED_Y ymin, ymax;

	/* Array of edges all starting in the same bucket.	An edge is put
	* into bucket GRAPHICS_TOR22_SCAN_ACTIVE_EDGE_BUCKET_INDEX(edge->ytop, polygon->ymin) when
	* it is added to the polygon. */
	GRAPHICS_TOR22_SCAN_ACTIVE_EDGE **y_buckets;
	GRAPHICS_TOR22_SCAN_ACTIVE_EDGE *y_buckets_embedded[64];

	struct
	{
		GRAPHICS_TOR22_SCAN_ACTIVE_POOL base[1];
		GRAPHICS_TOR22_SCAN_ACTIVE_EDGE embedded[32];
	} edge_pool;
} GRAPHICS_TOR22_SCAN_ACTIVE_POLYGON;

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
typedef struct _GRAPHICS_TOR22_SCAN_CELL
{
	struct _GRAPHICS_TOR22_SCAN_CELL	*next;
	int				x;
	int16			uncovered_area;
	int16			covered_height;
} GRAPHICS_TOR22_SCAN_CELL;

/* A cell list represents the scan line sparsely as cells ordered by
* ascending x.  It is geared towards scanning the cells in order
* using an internal cursor. */
typedef struct _GRAPHICS_TOR22_SCAN_CELL_LIST
{
	/* Sentinel nodes */
	GRAPHICS_TOR22_SCAN_CELL head, tail;

	/* Cursor state for iterating through the cell list. */
	GRAPHICS_TOR22_SCAN_CELL *cursor, *rewind;

	/* Cells in the cell list are owned by the cell list and are
	* allocated from this pool.  */
	struct
	{
		GRAPHICS_TOR22_SCAN_ACTIVE_POOL base[1];
		GRAPHICS_TOR22_SCAN_CELL embedded[32];
	} cell_pool;
} GRAPHICS_TOR22_SCAN_CELL_LIST;

typedef struct _GRAPHICS_TOR22_SCAN_CELL_PAIR
{
	GRAPHICS_TOR22_SCAN_CELL *cell1;
	GRAPHICS_TOR22_SCAN_CELL *cell2;
} GRAPHICS_TOR22_SCAN_CELL_PAIR;

/* The active list contains edges in the current scan line ordered by
* the x-coordinate of the intercept of the edge and the scan line. */
typedef struct _GRAPHICS_TOR22_SCAN_ACTIVE_LIST
{
	/* Leftmost edge on the current scan line. */
	GRAPHICS_TOR22_SCAN_ACTIVE_EDGE head, tail;

	/* A lower bound on the height of the active edges is used to
	* estimate how soon some active edge ends.	 We can't advance the
	* scan conversion by a full pixel row if an edge ends somewhere
	* within it. */
	GRID_SCALED_Y min_height;
	int is_vertical;
} GRAPHICS_TOR22_SCAN_ACTIVE_LIST;

typedef struct _GLITTER_SCAN_CONVERTER
{
	GRAPHICS_TOR22_SCAN_ACTIVE_POLYGON		polygon[1];
	GRAPHICS_TOR22_SCAN_ACTIVE_LIST	active[1];
	GRAPHICS_TOR22_SCAN_CELL_LIST	coverages[1];

	GRAPHICS_HALF_OPEN_SPAN *spans;
	GRAPHICS_HALF_OPEN_SPAN spans_embedded[64];

	/* Clip box. */
	GRID_SCALED_X xmin, xmax;
	GRID_SCALED_Y ymin, ymax;
} GLITTER_SCAN_CONVERTER;

typedef struct _GRAPHICS_TOR22_SCAN_CONVERTER
{
	GRAPHICS_SCAN_CONVERTER base;

	GLITTER_SCAN_CONVERTER converter[1];
	eGRAPHICS_FILL_RULE fill_rule;
	eGRAPHICS_ANTIALIAS antialias;

	jmp_buf jmp;
} GRAPHICS_TOR22_SCAN_CONVERTER;

typedef union _GRAPHICS_SCAN_CONVERTERS
{
	GRAPHICS_SCAN_CONVERTER converter;
	GRAPHICS_MONO_SCAN_CONVERTER mono_scan_converter;
	GRAPHICS_TOR_SCAN_CONVERTER tor_scan_converter;
	GRAPHICS_TOR22_SCAN_CONVERTER tor22_scan_converter;
} GRAPHICS_SCAN_CONVERTERS;

extern eGRAPHICS_STATUS InitializeGraphicsMonoScanConverter(
	GRAPHICS_MONO_SCAN_CONVERTER* converter,
	int			xmin,
	int			ymin,
	int			xmax,
	int			ymax,
	eGRAPHICS_FILL_RULE fill_rule
);

extern eGRAPHICS_STATUS GraphicsMonoScanConvereterAddPolygon(
	void* converter,
	const GRAPHICS_POLYGON* polygon
);

extern eGRAPHICS_STATUS InitializeGraphicsTorScanConverter(
	GRAPHICS_TOR_SCAN_CONVERTER* converter,
	int xmin,
	int ymin,
	int xmax,
	int ymax,
	eGRAPHICS_FILL_RULE fill_rule,
	eGRAPHICS_ANTIALIAS	antialias
);

extern eGRAPHICS_STATUS GraphicsTorScanConverterAddPolygon(
	void* converter,
	const GRAPHICS_POLYGON* polygon
);

extern eGRAPHICS_STATUS InitializeGraphicsTor22ScanConverter(
	GRAPHICS_TOR22_SCAN_CONVERTER* converter,
	int xmin,
	int ymin,
	int xmax,
	int ymax,
	eGRAPHICS_FILL_RULE fill_rule,
	eGRAPHICS_ANTIALIAS antialias
);

extern eGRAPHICS_STATUS GraphicsTor22ScanConverterAddPolygon(
	void* converter,
	const GRAPHICS_POLYGON* polygon
);

extern eGRAPHICS_STATUS GraphicsTorScanConverterAddPolygon(
	void* converter,
	const GRAPHICS_POLYGON* polygon
);

#endif /* #ifndef _INCLUDED_GRAPHICS_SCAN_CONVERTER_PRIVATE_H_ */
