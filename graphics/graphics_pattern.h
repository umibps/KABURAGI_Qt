#ifndef _INCLUDED_GRAPHICS_PATTERN_H_
#define _INCLUDED_GRAPHICS_PATTERN_H_

#include "graphics_types.h"
#include "graphics_list.h"
#include "graphics_status.h"

enum
{
	GRAPHICS_PATTERN_NOTIFY_MATRIX = 0x01,
	GRAPHICS_PATTERN_NOTIFY_FILTER = 0x02,
	GRAPHICS_PATTERN_NOTIFY_EXTEND = 0x04,
	GRAPHICS_PATTERN_NOTIFY_OPAITY = 0x08
};

typedef struct _GRAPHICS_PATTERN_OBSERVER
{
	void (*notify)(struct _GRAPHICS_PATTERN_OBSERVER* observer,
				   struct _GRAPHICS_PATTERN* pattern,
				   unsigned int flags
			   );
	GRAPHICS_LIST *link;
} GRAPHICS_PATTERN_OBSERVER;

typedef struct _GRAPHICS_PATTERN
{
	int reference_count;
	eGRAPHICS_STATUS status;
	// POINTER_ARRAY user_data;
	GRAPHICS_LIST observers;

	eGRAPHICS_PATTERN_TYPE type;

	eGRAPHICS_FILTER filter;
	eGRAPHICS_EXTENED extend;
	int has_component_alpha : 1;
	int own_memory : 1;

	GRAPHICS_MATRIX matrix;
	GRAPHICS_FLOAT opacity;

	void *graphics;
} GRAPHICS_PATTERN;

typedef struct _GRAPHICS_SOLID_PATTERN
{
	GRAPHICS_PATTERN base;
	GRAPHICS_COLOR color;
} GRAPHICS_SOLID_PATTERN;

typedef struct _GRAPHICS_SURFACE_PATTERN
{
	GRAPHICS_PATTERN base;
	struct _GRAPHICS_SURFACE *surface;
} GRAPHICS_SURFACE_PATTERN;

typedef struct _GRAPHICS_GRADIENT_STOP
{
	GRAPHICS_FLOAT offset;
	GRAPHICS_COLOR_STOP color;
} GRAPHICS_GRADIENT_STOP;

typedef struct _GRAPHICS_GRADIENT_PATTERN
{
	GRAPHICS_PATTERN base;

	unsigned int num_stops;
	unsigned int stop_size;
	GRAPHICS_GRADIENT_STOP *stops;
	GRAPHICS_GRADIENT_STOP stops_embedded[2];
} GRAPHICS_GRADIENT_PATTERN;

typedef struct _GRAPHICS_LINEAR_PATTERN
{
	GRAPHICS_GRADIENT_PATTERN base;

	GRAPHICS_POINT_FLOAT point1;
	GRAPHICS_POINT_FLOAT point2;
} GRAPHICS_LINEAR_PATTERN;

typedef struct _GRAPHICS_RADIAL_PATTERN
{
	GRAPHICS_GRADIENT_PATTERN base;

	GRAPHICS_CIRCLE_FLOAT circle1;
	GRAPHICS_CIRCLE_FLOAT circle2;
} GRAPHICS_RADIAL_PATTERN;

typedef union _GRAPHICS_GRADIENT_PATTERN_UNION
{
	GRAPHICS_GRADIENT_PATTERN base;
	GRAPHICS_LINEAR_PATTERN linear;
	GRAPHICS_RADIAL_PATTERN radial;
} GRAPHICS_GRADIENT_PATTERN_UNION;

typedef struct _GRAPHICS_MESH_PATCH
{
	GRAPHICS_POINT_FLOAT points[4][4];
	GRAPHICS_COLOR colors[4];
} GRAPHICS_MESH_PATCH;

typedef struct _GRAPHICS_MESH_PATTERN
{
	GRAPHICS_PATTERN base;

	GRAPHICS_ARRAY patches;
	GRAPHICS_MESH_PATCH *current_patch;
	int current_side;
	int has_control_point[4];
	int has_color[4];
} GRAPHICS_MESH_PATTERN;

typedef struct _GRAPHICS_RASTER_SOURCE_PATTERN
{
	GRAPHICS_PATTERN base;

	eGRAPHICS_CONTENT content;
	GRAPHICS_RECTANGLE_INT extents;

	GRAPHICS_RASTER_SOURCE_ACQUIRE_FUNC acquire;
	GRAPHICS_RASTER_SOURCE_RELEASE_FUNC release;
	GRAPHICS_RASTER_SOURCE_SNAPSHOT_FUNC snapshot;
	GRAPHICS_RASTER_SOURCE_COPY_FUNC copy;
	GRAPHICS_RASTER_SOURCE_FINISH_FUNC finish;

	void *user_data;
} GRAPHICS_RASTER_SOURCE_PATTERN;

typedef union _GRAPHICS_PATTERN_UNION
{
	GRAPHICS_PATTERN base;

	GRAPHICS_SOLID_PATTERN solid;
	GRAPHICS_SURFACE_PATTERN surface;
	GRAPHICS_GRADIENT_PATTERN_UNION gradient;
	GRAPHICS_MESH_PATTERN mesh;
	GRAPHICS_RASTER_SOURCE_PATTERN raster_source;
} GRAPHICS_PATTERN_UNION;

#ifdef __cplusplus
extern "C" {
#endif

extern void InitializeGraphicsPattern(GRAPHICS_PATTERN* pattern, eGRAPHICS_PATTERN_TYPE type, void* graphics);
extern void InitializeGraphicsPatternSolid(GRAPHICS_SOLID_PATTERN* pattern, const GRAPHICS_COLOR* color, void* graphics);
extern void InitializeGraphicsPatternStaticCopy(GRAPHICS_PATTERN* pattern, const GRAPHICS_PATTERN* other);
extern void InitializeGraphicsPatternLinear(
	GRAPHICS_LINEAR_PATTERN* pattern,
	FLOAT_T x0, FLOAT_T y0,
	FLOAT_T x1, FLOAT_T y1,
	void* graphics
);
extern GRAPHICS_PATTERN* CreateGraphicsPatternLinear(
	FLOAT_T x0, FLOAT_T y0,
	FLOAT_T x1, FLOAT_T y1,
	void* graphics
);
extern void InitializeGraphicsPatternRadial(GRAPHICS_RADIAL_PATTERN* pattern,
	FLOAT_T cx0, FLOAT_T cy0, FLOAT_T radius0,
	FLOAT_T cx1, FLOAT_T cy1, FLOAT_T radius1,
	void* graphics
);
extern GRAPHICS_PATTERN* CreateGraphicsPatternRadial(
	FLOAT_T cx0, FLOAT_T cy0, FLOAT_T radius0,
	FLOAT_T cx1, FLOAT_T cy1, FLOAT_T radius1,
	void* graphics
);
extern INLINE int ReleaseGraphcisPattern(GRAPHICS_PATTERN* pattern);
extern void DestroyGraphicsPattern(GRAPHICS_PATTERN* pattern);
extern void GraphicsPatternFinish(GRAPHICS_PATTERN* pattern);
extern int GraphicsPatternIsClear(const GRAPHICS_PATTERN* pattern);
extern int GraphicsPatternIsOpaque(const GRAPHICS_PATTERN* abstract_pattern, const GRAPHICS_RECTANGLE_INT* sample);
extern GRAPHICS_PATTERN* GraphicsPatternReference(GRAPHICS_PATTERN* pattern);
extern void InitializeGraphicsPatternStaticCopy(GRAPHICS_PATTERN* pattern, const GRAPHICS_PATTERN* other);
extern eGRAPHICS_FILTER GraphicsPatternAnalyzeFilter(const GRAPHICS_PATTERN* pattern);
extern void GraphicsPatternGetExtents(const GRAPHICS_PATTERN* pattern, GRAPHICS_RECTANGLE_INT* extents, int is_vector);
extern void GraphicsPatternSampledArea(
	const GRAPHICS_PATTERN* pattern,
	const GRAPHICS_RECTANGLE_INT* extents,
	GRAPHICS_RECTANGLE_INT* sample
);
extern GRAPHICS_PATTERN* CreateGraphicsPatternInError(eGRAPHICS_STATUS status, void* graphics);
extern int InitializeGraphicsPatternForSurface(GRAPHICS_SURFACE_PATTERN* pattern, struct _GRAPHICS_SURFACE* surface);
extern GRAPHICS_PATTERN* CreateGraphicsPatternForSurface(struct _GRAPHICS_SURFACE* surface);
extern void GraphicsPatternSetMatrix(GRAPHICS_PATTERN* pattern, const GRAPHICS_MATRIX* matrix);
extern void GraphicsPatternSetFilter(GRAPHICS_PATTERN* pattern,eGRAPHICS_FILTER filter);
extern GRAPHICS_PATTERN* CreateGraphicsPatternSolid(const GRAPHICS_COLOR* color, void* graphics);
extern GRAPHICS_PATTERN* CreateGraphicsPatternRGBA(FLOAT_T red, FLOAT_T green, FLOAT_T blue, FLOAT_T alpha, void* graphics);
extern int GraphicsGradientPatternIsSolid(
	const GRAPHICS_GRADIENT_PATTERN* gradient,
	const GRAPHICS_RECTANGLE_INT* extents,
	GRAPHICS_COLOR* color
);
extern void GraphicsPatternPretransform(GRAPHICS_PATTERN* pattern, const GRAPHICS_MATRIX* matrix);
extern void GraphicsPatternTransform(GRAPHICS_PATTERN* pattern, const GRAPHICS_MATRIX* matrix_inverse);
extern void GraphicsRasterSourcePatternRelease(
	const GRAPHICS_PATTERN* abstract_pattern,
	struct _GRAPHICS_SURFACE* surface
);
extern struct _GRAPHICS_SURFACE* GraphicsRasterSourcePatternAcquire(
	const GRAPHICS_PATTERN* abstract_pattern,
	struct _GRAPHICS_SURFACE* target,
	const GRAPHICS_RECTANGLE_INT* extents
);
extern void GraphicsGradientPatternFitToRange(
	const GRAPHICS_GRADIENT_PATTERN* gradient,
	FLOAT_T max_value,
	GRAPHICS_MATRIX* out_matrix,
	GRAPHICS_CIRCLE_FLOAT out_circle[2]
);
extern void GraphicsMeshPatternRasterize(
	const GRAPHICS_MESH_PATTERN* mesh,
	void* data,
	int width,
	int height,
	int stride,
	FLOAT_T x_offset,
	FLOAT_T y_offset
);
extern void GraphicsPatternSetExtend(GRAPHICS_PATTERN* pattern, eGRAPHICS_EXTENED extend);

extern void GraphicsPatternAddColorStopRGB(
	GRAPHICS_PATTERN* pattern,
	FLOAT_T offset,
	FLOAT_T red,
	FLOAT_T green,
	FLOAT_T blue
);
extern void GraphicsPatternAddColorStopRGBA(
	GRAPHICS_PATTERN* pattern,
	FLOAT_T offset,
	FLOAT_T red,
	FLOAT_T green,
	FLOAT_T blue,
	FLOAT_T alpha
);

#ifdef __cplusplus
}
#endif

#endif // #ifndef _INCLUDED_GRAPHICS_PATTERN_H_
