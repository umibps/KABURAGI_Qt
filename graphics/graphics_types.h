#ifndef _INCLUDED_GRAPHICS_TYPES_H_
#define _INCLUDED_GRAPHICS_TYPES_H_

#include <limits.h>
#include <math.h>
#include <float.h>
#include "types.h"

typedef double GRAPHICS_FLOAT;
typedef int32 GRAPHICS_FLOAT_FIXED;
typedef uint32 GRAPHICS_FLOAT_FIXED_UNSIGNED;
typedef struct _GRAPHICS_COMPOSITE_RECTANGLES GRAPHICS_COMPOSITE_RECTANGLES;
#define FABS(value) fabs((value))
#define FLOAT_EPSILON DBL_EPSILON
#define SQRT(value) sqrt((value))
#define CEIL(value) ceil((value))
#define ACOS(value) acos((value))

typedef struct _GRAPHICS_POINT
{
	GRAPHICS_FLOAT_FIXED x, y;
} GRAPHICS_POINT;

typedef struct _GRAPHICS_POINT_FLOAT
{
	GRAPHICS_FLOAT x, y;
} GRAPHICS_POINT_FLOAT;

typedef struct _GRAPHICS_MATRIX
{
	double xx, yx;
	double xy, yy;
	double x0, y0;
} GRAPHICS_MATRIX;

typedef struct _GRAPHICS_SLOPE
{
	GRAPHICS_FLOAT_FIXED dx, dy;
} GRAPHICS_SLOPE, GRAPHICS_DISTANCE;

#include "../pixel_manipulate/pixel_manipulate.h"
#include "graphics_status.h"
#include "graphics_list.h"
#include "graphics_pen.h"
#include "memory.h"


#define GRAPHICS_RECTANGLE_MAX (INT_MAX >> 8)
#define GRAPHICS_RECTANGLE_MIN (INT_MIN >> 8)

#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN || \
	defined(__BIG_ENDIAN__) || \
	defined(__ARMEB__) || \
	defined(__THUMBEB__) || \
	defined(__AARCH64EB__) || \
	defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__) || defined(_M_PPC)
#	   define GRAPHICS_BIG_ENDIAN 1
#else
#	   define GRAPHICS_LITTLE_ENDIAN 1
#endif

typedef struct _GRAPHICS_SURFACE*
		(*GRAPHICS_RASTER_SOURCE_ACQUIRE_FUNC)(struct _GRAPHICS_PATTERN* pattern,
											   void* callback_data,
											   struct _GRAPHICS_SURFACE* target,
											   struct _GRAPHICS_RECTANGLE_INT* extents
											  );

typedef void
		(*GRAPHICS_RASTER_SOURCE_RELEASE_FUNC)(struct _GRAPHICS_PATTERN* pattern,
											   void* callback_data,
											   struct _GRAPHICS_SURFACE* surface
											  );

typedef eGRAPHICS_STATUS
	   (*GRAPHICS_RASTER_SOURCE_SNAPSHOT_FUNC)(struct _GRAPHICS_PATTERN* pattern,
											   void* callback_data
											  );

typedef eGRAPHICS_STATUS
	   (*GRAPHICS_RASTER_SOURCE_COPY_FUNC)(struct _GRAPHICS_PATTERN* pattern,
												  void* callback_data,
												  const struct _GRAPHICS_PATTERN* other
												 );

typedef void
	   (*GRAPHICS_RASTER_SOURCE_FINISH_FUNC)(struct _GRAPHICS_PATTERN* pattern,
											 void* callback_data
											);

typedef enum _eGRAPHICS_FORMAT
{
	GRAPHICS_FORMAT_INVALID = -1,
	GRAPHICS_FORMAT_ARGB32 = PIXEL_MANIPULATE_FORMAT_A8R8G8B8,
	GRAPHICS_FORMAT_RGB24 = PIXEL_MANIPULATE_FORMAT_X8R8G8B8,
	GRAPHICS_FORMAT_A8 = PIXEL_MANIPULATE_FORMAT_A8,
	GRAPHICS_FORMAT_A1 = 3
} eGRAPHICS_FORMAT;

typedef enum _eGRAPHICS_OPERATOR
{
	GRAPHICS_OPERATOR_CLEAR = 0,
	GRAPHICS_OPERATOR_SOURCE,
	GRAPHICS_OPERATOR_OVER,
	GRAPHICS_OPERATOR_IN,
	GRAPHICS_OPERATOR_OUT,
	GRAPHICS_OPERATOR_ATOP,
	GRAPHICS_OPERATOR_DEST,
	GRAPHICS_OPERATOR_DEST_OVER,
	GRAPHICS_OPERATOR_DEST_IN,
	GRAPHICS_OPERATOR_DEST_OUT,
	GRAPHICS_OPERATOR_DEST_ATOP,
	GRAPHICS_OPERATOR_XOR,
	GRAPHICS_OPERATOR_COLOR_BLEND_START = GRAPHICS_OPERATOR_XOR,
	GRAPHICS_OPERATOR_ADD,
	GRAPHICS_OPERATOR_SATURATE,
	GRAPHICS_OPERATOR_MULTIPLY,
	GRAPHICS_OPERATOR_SCREEN,
	GRAPHICS_OPERATOR_OVERLAY,
	GRAPHICS_OPERATOR_DARKEN,
	GRAPHICS_OPERATOR_LIGHTEN,
	GRAPHICS_OPERATOR_COLOR_DODGE,
	GRAPHICS_OPERATOR_COLOR_BURN,
	GRAPHICS_OPERATOR_HARD_LIGHT,
	GRAPHICS_OPERATOR_SOFT_LIGHT,
	GRAPHICS_OPERATOR_DIFFERENCE,
	GRAPHICS_OPERATOR_EXCLUSION,
	GRAPHICS_OPERATOR_HSL_HUE,
	GRAPHICS_OPERATOR_HSL_SATURATION,
	GRAPHICS_OPERATOR_HSL_COLOR,
	GRAPHICS_OPERATOR_HSL_LUMINOSITY
} eGRAPHICS_OPERATOR;

typedef enum _eGRAPHICS_ANTIALIAS
{
	GRAPHICS_ANTIALIAS_DEFAULT,
	GRAPHICS_ANTIALIAS_NONE,
	GRAPHICS_ANTIALIAS_GRAY,
	GRAPHICS_ANTIALIAS_SUBPIXEL,

	GRAPHICS_ANTIALIAS_FAST,
	GRAPHICS_ANTIALIAS_GOOD,
	GRAPHICS_ANTIALIAS_BEST
} eGRAPHICS_ANTIALIAS;

typedef enum _eGRAPHICS_FILL_RULE
{
	GRAPHICS_FILL_RULE_WINDING,
	GRAPHICS_FILL_RULE_EVEN_ODD
} eGRAPHICS_FILL_RULE;

typedef enum _eGRAPHICS_LINE_CAP
{
	GRAPHICS_LINE_CAP_BUTT,
	GRAPHICS_LINE_CAP_ROUND,
	GRAPHICS_LINE_CAP_SQUARE,
	GRAPHICS_LINE_CAP_DEFAULT = GRAPHICS_LINE_CAP_BUTT
} eGRAPHICS_LINE_CAP;

typedef enum _eGRAPHICS_LINE_JOIN
{
	GRAPHICS_LINE_JOIN_MITER,
	GRAPHICS_LINE_JOIN_ROUND,
	GRAPHICS_LINE_JOIN_BEVEL
} eGRAPHICS_LINE_JOIN;

typedef enum _eGRAPHICS_INTEGER_STATUS
{
	GRAPHICS_INTEGER_STATUS_SUCCESS = 0,
	GRAPHICS_INTEGER_STATUS_NO_MEMORY,
	GRAPHICS_INTEGER_STATUS_INVALID_ID_RESTORE,
	GRAPHICS_INTEGER_STATUS_INVALID_POP_GROUP,
	GRAPHICS_INTEGER_STATUS_NO_CURRENT_POINT,
	GRAPHICS_INTEGER_STATUS_INVALID_ID_MATRIX,
	GRAPHICS_INTEGER_STATUS_INVALID_ID_STATUS,
	GRAPHICS_INTEGER_STATUS_NULL_POINTER,
	GRAPHICS_INTEGER_STATUS_INVALID_STRING,
	GRAPHICS_INTEGER_STATUS_INVALID_PATH_DATA,
	GRAPHICS_INTEGER_STATUS_READ_ERROR,
	GRAPHICS_INTEGER_STATUS_WRITE_ERROR,
	GRAPHICS_INTEGER_STATUS_SURFACE_FINISHED,
	GRAPHICS_INTEGER_STATUS_SURFACE_TYPE_MISMATCH,
	GRAPHICS_INTEGER_STATUS_PATTERN_TYPE_MISMATCH,
	GRAPHICS_INTEGER_STATUS_INVALID_CONTENT,
	GRAPHICS_INTEGER_STATUS_INVALID_FORMAT,
	GRAPHICS_INTEGER_STATUS_INVALID_VISUAL,
	GRAPHICS_INTEGER_STATUS_INVALID_DASH,
	GRAPHICS_INTEGER_STATUS_INVALID_INDEX,

	GRAPHICS_INTEGER_STATUS_LAST_STATUS,

	GRAPHICS_INTEGER_STATUS_UNSUPPORTED = 100,
	GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO
} eGRAPHICS_INTEGER_STATUS;

typedef enum _eGRAPHICS_CONTENT
{
	GRAPHICS_CONTENT_COLOR = 0x01000,
	GRAPHICS_CONTENT_ALPHA = 0x02000,
	GRAPHICS_CONTENT_COLOR_ALPHA = 0x03000,
	GRAPHICS_CONTENT_INVALID = 0xFFFFFFFF
} eGRAPHICS_CONTENT;

typedef enum _eGRAPHICS_PATTERN_TYPE
{
	GRAPHICS_PATTERN_TYPE_SOLID,
	GRAPHICS_PATTERN_TYPE_SURFACE,
	GRAPHICS_PATTERN_TYPE_LINEAR,
	GRAPHICS_PATTERN_TYPE_RADIAL,
	GRAPHICS_PATTERN_TYPE_MESH,
	GRAPHICS_PATTERN_TYPE_RASTER_SOURCE
} eGRAPHICS_PATTERN_TYPE;

typedef enum _eGRAPHICS_OPERATOR_BOUNDED
{
	GRAPHICS_OPERATOR_BOUNDED_BY_MASK = 1 << 1,
	GRAPHICS_OPERATOR_BOUNDED_BY_SOURCE = 1 << 2
} eGRAPHICS_OPERATOR_BOUNDED;

typedef struct _GRAPHICS_CLIP GRAPHICS_CLIP;

typedef struct _GRAPHICS_STROKE_STYLE
{
	GRAPHICS_FLOAT line_width;
	eGRAPHICS_LINE_CAP line_cap;
	eGRAPHICS_LINE_JOIN line_join;
	GRAPHICS_FLOAT miter_limit;
	GRAPHICS_FLOAT *dash;
	unsigned num_dashes;
	GRAPHICS_FLOAT dash_offset;
} GRAPHICS_STROKE_STYLE;

typedef enum _eGRAPHICS_FILTER
{
	GRAPHICS_FILTER_FAST,
	GRAPHICS_FILTER_GOOD,
	GRAPHICS_FILTER_BEST,
	GRAPHICS_FILTER_NEAREST,
	GRAPHICS_FILTER_BILINEAR,
	GRAPHICS_FILTER_GAUSSIAN,
	GRAPHICS_FILTER_DEFAULT = GRAPHICS_FILTER_GOOD
} eGRAPHICS_FILTER;

typedef enum _eGRAPHICS_REGION_OVERLAP
{
	GRAPHICS_REGION_OVERLAP_IN,
	GRAPHICS_REGION_OVERLAP_OUT,
	GRAPHICS_REGION_OVERLAP_PART
} eGRAPHICS_REGION_OVERLAP;

typedef enum _eGRAPHICS_EXTENED
{
	GRAPHICS_EXTEND_NONE,
	GRAPHICS_EXTEND_REPEAT,
	GRAPHICS_EXTEND_REFLECT,
	GRAPHICS_EXTEND_PAD,
	GRAPHICS_EXTEND_SURFACE_DEFAULT = GRAPHICS_EXTEND_NONE,
	GRAPHICS_EXTEND_GRADIENT_DEFAULT = GRAPHICS_EXTEND_PAD
} eGRAPHICS_EXTENED;

typedef struct _GRAPHICS_SPLINE_KNOTS
{
	GRAPHICS_POINT a, b, c, d;
} GRAPHICS_SPLINE_KNOTS;

typedef eGRAPHICS_STATUS (*GRAPHICS_SPLINE_ADD_POINT_FUNCTION)(
	void* closure, const GRAPHICS_POINT* point, const GRAPHICS_SLOPE* tangent);

typedef struct _GRAPHICS_SPLINE
{
	GRAPHICS_SPLINE_ADD_POINT_FUNCTION add_point_function;
	void *closure;

	GRAPHICS_SPLINE_KNOTS knots;

	GRAPHICS_SLOPE initial_slope;
	GRAPHICS_SLOPE final_slope;

	int has_point;
	GRAPHICS_POINT last_point;
} GRAPHICS_SPLINE;

typedef struct _GRAPHICS_CIRCLE_FLOAT
{
	GRAPHICS_POINT_FLOAT center;
	GRAPHICS_FLOAT radius;
} GRAPHICS_CIRCLE_FLOAT;

typedef struct _GRAPHICS_ARRAY
{
	unsigned int size;
	unsigned int num_elements;
	unsigned int element_size;
	char *elements;
} GRAPHICS_ARRAY;

typedef struct _GRAPHICS_RECTANGLE_INT
{
	int x, y;
	int width, height;
	int unbouned : 1;
	int empty : 1;
} GRAPHICS_RECTANGLE_INT;

typedef struct _GRAPHICS_LINE
{
	GRAPHICS_POINT point1;
	GRAPHICS_POINT point2;
} GRAPHICS_LINE, GRAPHICS_BOX;

typedef struct _GRAPHCIS_COLOR
{
	GRAPHICS_FLOAT red;
	GRAPHICS_FLOAT green;
	GRAPHICS_FLOAT blue;
	GRAPHICS_FLOAT alpha;

	unsigned short red_short;
	unsigned short green_short;
	unsigned short blue_short;
	unsigned short alpha_short;
} GRAPHICS_COLOR;

typedef struct _GRAPHCIS_COLOR_STOP
{
	GRAPHICS_FLOAT red;
	GRAPHICS_FLOAT green;
	GRAPHICS_FLOAT blue;
	GRAPHICS_FLOAT alpha;

	unsigned short red_short;
	unsigned short green_short;
	unsigned short blue_short;
	unsigned short alpha_short;
} GRAPHICS_COLOR_STOP;

typedef struct _GRAPHICS_REGION
{
	int reference_count;
	eGRAPHICS_STATUS status;

	PIXEL_MANIPULATE_REGION32 region;
} GRAPHICS_REGION;

typedef struct _GRAPHICS_BOXES_CHUNK
{
	struct _GRAPHICS_BOXES_CHUNK* next;
	GRAPHICS_BOX* base;
	int count;
	int size;
} GRAPHICS_BOXES_CHUNK;

typedef struct _GRAPHICS_BOXES
{
	eGRAPHICS_STATUS status;
	GRAPHICS_BOX limit;
	const GRAPHICS_BOX *limits;
	int num_limits;

	int num_boxes;

	unsigned int is_pixel_aligned;

	GRAPHICS_BOXES_CHUNK chunks, *tail;

	GRAPHICS_BOX boxes_embedded[32];
} GRAPHICS_BOXES;

typedef struct _GRAPHICS_TRAPEZOID
{
	int32 top, bottom;
	GRAPHICS_LINE left, right;
} GRAPHICS_TRAPEZOID;

typedef struct _GRAPHICS_TRAPS
{
	eGRAPHICS_STATUS status;

	GRAPHICS_BOX bounds;
	const GRAPHICS_BOX *limits;
	int num_limits;

	unsigned int maybe_region : 1;
	unsigned int has_intersections : 1;
	unsigned int is_rectilinear : 1;
	unsigned int is_rectangular : 1;

	int num_traps;
	int traps_size;
	GRAPHICS_TRAPEZOID *traps;
	GRAPHICS_TRAPEZOID traps_embedded[16];
} GRAPHICS_TRAPS;

typedef struct _GRAPHICS_EDGE
{
	GRAPHICS_LINE line;
	int top, bottom;
	int direction;
} GRAPHICS_EDGE;

typedef struct _GRAPHICS_POLYGON
{
	eGRAPHICS_STATUS status;

	GRAPHICS_BOX extents;
	GRAPHICS_BOX limit;
	const GRAPHICS_BOX *limits;
	int num_limits;

	int num_edges;
	int edges_size;
	GRAPHICS_EDGE *edges;
	GRAPHICS_EDGE edges_embedded[32];
} GRAPHICS_POLYGON;

typedef struct _GRAPHICS_CONTOUR_CHAIN
{
	GRAPHICS_POINT *points;
	int num_points, size_points;
	struct _GRAPHICS_CONTOUR_CHAIN *next;
} GRAPHICS_CONTOUR_CHAIN;

typedef struct _GRAPHICS_CONTOUR
{
	GRAPHICS_LIST next;
	int direction;
	GRAPHICS_CONTOUR_CHAIN chain, *tail;

	GRAPHICS_POINT embedded_points[64];
} GRAPHICS_CONTOUR;

typedef struct _GRAPHICS_STROKE_FACE
{
	GRAPHICS_POINT counter_clock_wise;
	GRAPHICS_POINT point;
	GRAPHICS_POINT clock_wise;
	GRAPHICS_SLOPE device_vector;
	GRAPHICS_POINT_FLOAT device_slope;
	GRAPHICS_POINT_FLOAT user_vector;
	FLOAT_T length;
} GRAPHICS_STROKE_FACE;

typedef struct _GRAPHICS_TRISTRIP
{
	eGRAPHICS_STATUS status;
	const GRAPHICS_BOX *limits;
	int num_limits;

	int num_points;
	int size_points;
	GRAPHICS_POINT *points;
	GRAPHICS_POINT points_embedded[64];
} GRAPHICS_TRISTRIP;

typedef enum _eGRAPHICS_PATH_DATA_TYPE
{
	GRAPHICS_PATH_MOVE_TO,
	GRAPHICS_PATH_LINE_TO,
	GRAPHICS_PATH_CURVE_TO,
	GRAPHICS_PATH_CLOSE_PATH
} eGRAPHICS_PATH_DATA_TYPE;

typedef struct _GRAPHICS_GLYPH
{
	uint32 index;
	GRAPHICS_FLOAT x;
	GRAPHICS_FLOAT y;
} GRAPHICS_GLYPH;

typedef union _GRAPHICS_PATH_DATA
{
	struct
	{
		eGRAPHICS_PATH_DATA_TYPE type;
		int length;
	} header;
	struct
	{
		FLOAT_T x, y;
	} point;
} GRAPHICS_PATH_DATA;

typedef struct _GRAPHICS_PATH
{
	eGRAPHICS_STATUS status;
	GRAPHICS_PATH_DATA *data;
	int num_data;
	int own_memory;
} GRAPHICS_PATH;

typedef struct _GRAPHICS_RECTANGLE
{
	FLOAT_T x, y, width, height;
} GRAPHICS_RECTANGLE;

typedef struct _GRAPHICS_PEN_VERTEX
{
	GRAPHICS_POINT point;

	GRAPHICS_SLOPE slope_ccw;
	GRAPHICS_SLOPE slope_cw;
} GRAPHICS_PEN_VERTEX;

typedef struct _GRAPHICS_PEN
{
	FLOAT_T radius;
	FLOAT_T tolerance;

	int num_vertices;
	GRAPHICS_PEN_VERTEX *vertices;
	GRAPHICS_PEN_VERTEX vertices_embedded[32];
} GRAPHICS_PEN;

typedef struct _GRAPHICS_RECTANGLE_LIST
{
	eGRAPHICS_STATUS status;
	GRAPHICS_RECTANGLE *rectangles;
	int num_rectangles;
} GRAPHICS_RECTANGLE_LIST;

typedef struct _GRAPHICS_OBSERVER
{
	GRAPHICS_LIST link;
	void (*callback)(struct _GRAPHICS_OBSERVER* self, void* arg);
} GRAPHICS_OBSERVER;

typedef struct _GRAPHICS_STROKER_DASH
{
	int dashed;
	unsigned int dash_index;
	int dash_on;
	int dash_starts_on;
	FLOAT_T dash_remain;

	FLOAT_T dash_offset;
	const FLOAT_T *dashes;
	unsigned int num_dashes;
} GRAPHICS_STROKER_DASH;

typedef struct _GRAPHICS_STROKE_CONTOUR
{
	GRAPHICS_CONTOUR contour;
} GRAPHICS_STROKE_CONTOUR;

typedef struct _GRAPHICS_STROKER
{
	GRAPHICS_STROKE_STYLE style;

	GRAPHICS_STROKE_CONTOUR clock_wise, counter_clock_wise;
	uint64 contour_tolerance;
	GRAPHICS_POLYGON *polygon;

	const GRAPHICS_MATRIX *matrix;
	const GRAPHICS_MATRIX *matrix_inverse;
	FLOAT_T tolerance;
	FLOAT_T spline_cusp_tolerance;
	FLOAT_T half_line_width;
	FLOAT_T matrix_determinant;
	int matrix_determin_positive;

	void *closure;
	eGRAPHICS_STATUS (*add_external_edge)(
		void* closure, const GRAPHICS_POINT* point, const GRAPHICS_POINT* point2);
	eGRAPHICS_STATUS (*add_triangle)(
		void* closure, const GRAPHICS_POINT triangle[3]);
	eGRAPHICS_STATUS (*add_triangle_fan)(
		void* closure, const GRAPHICS_POINT* mid_point, const GRAPHICS_POINT* points, int num_points);
	eGRAPHICS_STATUS (*add_convex_quad)(
		void* closure, const GRAPHICS_POINT quad[4]);

	GRAPHICS_PEN pen;

	GRAPHICS_POINT current_point;
	GRAPHICS_POINT first_point;

	int has_initial_sub_path;

	int has_current_face;
	GRAPHICS_STROKE_FACE current_face;

	int has_first_face;
	GRAPHICS_STROKE_FACE first_face;

	GRAPHICS_STROKER_DASH dash;

	int has_bounds;
	GRAPHICS_BOX bounds;
} GRAPHICS_STROKER;

typedef eGRAPHICS_STATUS (*GRAPHICS_SPLINE_ADD_POINT_FUNCTION)(
	void* closure, const GRAPHICS_POINT* point, const GRAPHICS_SLOPE* tangent
);

#define GRAPHICS_MATRIX_COMPUTE_DETERMINANT(MAT) ((MAT)->xx * (MAT)->yy - (MAT)->yx * (MAT)->xy)

#define INITIALIZE_GRAPHICS_SLOPE(SLOPE, A, B) ((SLOPE)->dx = (B)->x - (A)->x, (SLOPE)->dy = (B)->y - (A)->y)

#define GRAPHICS_STRIDE_ALIGNMENT (sizeof(uint32))
#define GRAPHICS_STRIDE_FOR_WIDTH_BPP(w, bpp) \
	((((bpp) * (w) + 7)/8 + GRAPHICS_STRIDE_ALIGNMENT - 1) & - GRAPHICS_STRIDE_ALIGNMENT)

static INLINE int GraphicsSlopeCompareSign(FLOAT_T dx1, FLOAT_T dy1, FLOAT_T dx2, FLOAT_T dy2)
{
	FLOAT_T c = (dx1 * dy2 - dx2 * dy1);

	if(c > 0)
	{
		return 1;
	}
	if(c < 0)
	{
		return -1;
	}
	return 0;
}

static INLINE eGRAPHICS_INTEGER_STATUS GraphicsContourAddPointRealloc(GRAPHICS_CONTOUR* contour, const GRAPHICS_POINT* point)
{
	GRAPHICS_CONTOUR_CHAIN *tail = contour->tail;
	GRAPHICS_CONTOUR_CHAIN *next;

	ASSERT(tail->next == NULL);

	next = (GRAPHICS_CONTOUR_CHAIN*)MEM_ALLOC_FUNC(
				tail->size_points*2 * sizeof(GRAPHICS_POINT) + sizeof(*tail));
	next->size_points = tail->size_points*2;
	next->num_points = 1;
	next->points = (GRAPHICS_POINT*)(next+1);
	next->next = NULL;
	tail->next = next;
	contour->tail = next;

	next->points[0] = *point;
	return GRAPHICS_INTEGER_STATUS_SUCCESS;
}

static INLINE eGRAPHICS_INTEGER_STATUS GraphicsContourAddPoint(GRAPHICS_CONTOUR* contour, const GRAPHICS_POINT* point)
{
	GRAPHICS_CONTOUR_CHAIN *tail = contour->tail;

	if(tail->num_points == tail->size_points)
	{
		return GraphicsContourAddPointRealloc(contour, point);
	}

	tail->points[tail->num_points++] = *point;
	return GRAPHICS_INTEGER_STATUS_SUCCESS;
}

static INLINE GRAPHICS_POINT* GraphicsContourFirstPoint(GRAPHICS_CONTOUR* contour)
{
	return &contour->chain.points[0];
}

static INLINE int GraphicsPolygonIsEmpty(const GRAPHICS_POLYGON* polygon)
{
	return polygon->num_edges == 0 || polygon->extents.point2.x <= polygon->extents.point1.x;
}

#ifdef __cplusplus
extern "C" {
#endif

extern eGRAPHICS_FORMAT GetGraphicsFormatFromContent(eGRAPHICS_CONTENT conent);
extern eGRAPHICS_CONTENT GetGraphicsContentFromFormat(eGRAPHICS_FORMAT format);
extern const void* GraphicsArrayIndexConst(const GRAPHICS_ARRAY* array, unsigned int index);
extern void GraphicsStrokeStyleInitialize(GRAPHICS_STROKE_STYLE* style);
extern eGRAPHICS_STATUS GraphicsStrokeStyleInitializeCopy(GRAPHICS_STROKE_STYLE* style, const GRAPHICS_STROKE_STYLE* other);
extern int GraphicsRectangleIntersect(GRAPHICS_RECTANGLE_INT* dst, GRAPHICS_RECTANGLE_INT* src);
extern void GraphicsBoxRoundToRectangle(const GRAPHICS_BOX* box, GRAPHICS_RECTANGLE_INT* rectangle);
extern int GraphicsBoxIntersectsLineSegment(const GRAPHICS_BOX* box, GRAPHICS_LINE* line);
extern void GraphicsBoxFromRectangle(GRAPHICS_BOX* box, const GRAPHICS_RECTANGLE_INT* rectangle);
extern void GraphicsBoxesLimit(GRAPHICS_BOXES* boxes, const GRAPHICS_BOX* limits, int num_limits);
extern void InitializeGraphicsBoxesForArray(GRAPHICS_BOXES* boxes, GRAPHICS_BOX* array, int num_boxes);
extern void InitializeGraphicsBoxesFromRectangle(
	GRAPHICS_BOXES* boxes,
	int x,
	int y,
	int width,
	int height
);
extern void InitializeGraphicsBoxesWithClip(GRAPHICS_BOXES* boxes, GRAPHICS_CLIP* clip);

extern int GraphicsLinesCompareAtY(
	const GRAPHICS_LINE* a,
	const GRAPHICS_LINE* b,
	int y
);

extern eGRAPHICS_STATUS GraphicsMatrixInvert(GRAPHICS_MATRIX* matrix);

extern void GraphicsObserversNotify(GRAPHICS_LIST* observers, void* arg);

extern int InitializeGraphicsSpline(
	GRAPHICS_SPLINE* spline,
	GRAPHICS_SPLINE_ADD_POINT_FUNCTION add_point_function,
	void* closure,
	const GRAPHICS_POINT* a, const GRAPHICS_POINT* b,
	const GRAPHICS_POINT* c, const GRAPHICS_POINT* d
);
extern eGRAPHICS_STATUS GraphicsSplineDecompose(GRAPHICS_SPLINE* spline, FLOAT_T tolerance);
extern void InitializeGraphicsColorRGB(
	GRAPHICS_COLOR* color,
	FLOAT_T red,
	FLOAT_T green,
	FLOAT_T blue
);
extern void InitializeGraphicsColorRGBA(
	GRAPHICS_COLOR* color,
	FLOAT_T red,
	FLOAT_T green,
	FLOAT_T blue,
	FLOAT_T alpha
);
extern INLINE int GraphicsColorEqual(const GRAPHICS_COLOR* color1, const GRAPHICS_COLOR* color2);
extern INLINE int GraphicsColorStopEqual(const GRAPHICS_COLOR_STOP *a, const GRAPHICS_COLOR_STOP* b);
extern INLINE void GraphicsColorMultiplyAlpha(GRAPHICS_COLOR* color, FLOAT_T alpha);
extern void InitializeGraphicsTraps(GRAPHICS_TRAPS* traps);
extern void InitializeGraphicsTrapsWithClip(GRAPHICS_TRAPS* traps, const GRAPHICS_CLIP* clip);
extern eGRAPHICS_STATUS GraphicsTrapsInitializeBoxes(GRAPHICS_TRAPS* traps, const GRAPHICS_BOXES* boxes);
extern void GraphicsTrapsClear(GRAPHICS_TRAPS* traps);
extern void GraphicsTrapsLimit(GRAPHICS_TRAPS* traps, const GRAPHICS_BOX* limits, int num_limits);
extern void GraphicsTrapsTessellateConvexQuad(
	GRAPHICS_TRAPS* traps,
	const GRAPHICS_POINT q[4]
);
extern void GraphicsTrapsTessellateTriangleWithEdges(
	GRAPHICS_TRAPS* traps,
	const GRAPHICS_POINT t[3],
	const GRAPHICS_POINT edges[4]
);

extern void InitializeGraphicsPolygon(
	GRAPHICS_POLYGON* polygon,
	const GRAPHICS_BOX* limits,
	int num_limits
);
extern eGRAPHICS_STATUS GraphicsPolygonInitializeBoxes(GRAPHICS_POLYGON* polygon, const GRAPHICS_BOXES*boxes);
extern eGRAPHICS_STATUS InitializeGraphicsPolygonBoxArray(
	GRAPHICS_POLYGON* polygon,
	GRAPHICS_BOX* boxes,
	int num_boxes
);
extern void InitializeGraphicsPolygonWithClip(GRAPHICS_POLYGON* polygon, const GRAPHICS_CLIP* clip);
extern void GraphicsPolygonFinish(GRAPHICS_POLYGON* polygon);
extern void GraphicsPolygonTranslate(GRAPHICS_POLYGON* polygon, int dx, int dy);
extern eGRAPHICS_STATUS GraphicsPolygonAddLine(
	GRAPHICS_POLYGON* polygon,
	const GRAPHICS_LINE* line,
	int top,
	int bottom,
	int direction
);
extern void GraphicsPolygonAddEdge(
	GRAPHICS_POLYGON* polygon,
	const GRAPHICS_POINT* point1,
	const GRAPHICS_POINT* point2,
	int direction
);
extern eGRAPHICS_STATUS GraphicsPolygonAddExternalEdge(
	GRAPHICS_POLYGON* polygon,
	const GRAPHICS_POINT* point1,
	const GRAPHICS_POINT* point2
);
extern eGRAPHICS_STATUS GraphicsPolygonAddContour(GRAPHICS_POLYGON* polygon, const GRAPHICS_CONTOUR* contour);

extern eGRAPHICS_STATUS GraphicsPolygonIntersect(
	GRAPHICS_POLYGON* a,
	int winding_a,
	GRAPHICS_POLYGON* b,
	int winding_b
);
extern eGRAPHICS_STATUS GraphicsPolygonIntersectWithBoxes(
	GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE* winding,
	GRAPHICS_BOX* boxes,
	int num_boxes
);
extern eGRAPHICS_INTEGER_STATUS GraphicsRasterisePolygonToTraps(
	GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE fill_rule,
	eGRAPHICS_ANTIALIAS antialias,
	GRAPHICS_TRAPS* traps
);

extern int GraphicsSlopeCompare(const GRAPHICS_SLOPE* a, const GRAPHICS_SLOPE* b);

extern void InitializeGraphicsContour(GRAPHICS_CONTOUR* contour, int direction);
extern void GraphicsContourFinish(GRAPHICS_CONTOUR* contour);
extern void GraphicsContourReset(GRAPHICS_CONTOUR* contour);

extern void GraphicsTrapsFinish(GRAPHICS_TRAPS* traps);
extern void GraphicsTrapsAddTrap(
	GRAPHICS_TRAPS* traps,
	GRAPHICS_FLOAT_FIXED top,
	GRAPHICS_FLOAT_FIXED bottom,
	const GRAPHICS_LINE* left,
	const GRAPHICS_LINE* right
);
extern int GraphicsTrapsToBoxes(
	GRAPHICS_TRAPS* traps,
	eGRAPHICS_ANTIALIAS antialias,
	GRAPHICS_BOXES* boxes
);
extern int GraphicsTrapsContain(const GRAPHICS_TRAPS* traps, FLOAT_T x, FLOAT_T y);
extern void GraphicsTrapsExtents(const GRAPHICS_TRAPS* traps, GRAPHICS_BOX* extents);

extern int GraphicsPolygonGrow(GRAPHICS_POLYGON* polygon);

extern int GraphicsFormatStrideForWidth(eGRAPHICS_FORMAT format, int width);

extern eGRAPHICS_INTEGER_STATUS GraphicsClipGetPolygon(
	const GRAPHICS_CLIP* clip,
	GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE* fill_rule,
	eGRAPHICS_ANTIALIAS* antialias
);
extern struct _GRAPHICS_SURFACE* GraphicsClipGetImage(
	const GRAPHICS_CLIP* clip,
	struct _GRAPHICS_SURFACE* target,
	const GRAPHICS_RECTANGLE_INT* extents
);

extern void InitializeGraphicsTristrip(GRAPHICS_TRISTRIP* strip);
extern void InitializeGraphicsTristripWithClip(GRAPHICS_TRISTRIP* strip, const GRAPHICS_CLIP* clip);
extern void GraphicsTristripFinish(GRAPHICS_TRISTRIP* strip);
extern void GraphicsTristripLimit(
	GRAPHICS_TRISTRIP* strip,
	const GRAPHICS_BOX* limits,
	int num_limits
);
extern void GraphicsTristripAddPoint(GRAPHICS_TRISTRIP* strip, const GRAPHICS_POINT* p);
extern void GraphicsTristripMoveTo(GRAPHICS_TRISTRIP* strip, const GRAPHICS_POINT* p);
extern void GraphicsTristripTranslate(GRAPHICS_TRISTRIP* strip, int x, int y);
extern void GraphicsTristripExtents(const GRAPHICS_TRISTRIP* strip, GRAPHICS_BOX* extents);

#ifdef __cplusplus
}
#endif

#endif  // #ifndef _INCLUDED_GRAPHICS_TYPES_H_
