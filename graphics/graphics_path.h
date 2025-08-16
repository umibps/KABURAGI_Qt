#ifndef _INCLUDED_GRAPHICS_PATH_H_
#define _INCLUDED_GRAPHICS_PATH_H_

#include "graphics_types.h"
#include "graphics_definitions.h"
#include "graphics_list.h"

typedef char GRAPHICS_PATH_OPERATOR;

typedef struct _GRAPHICS_PATH_BUFFER
{
	GRAPHICS_LIST link;
	unsigned int num_ops;
	unsigned int size_ops;
	unsigned int num_points;
	unsigned int size_points;

	GRAPHICS_PATH_OPERATOR *op;
	GRAPHICS_POINT *points;
} GRAPHICS_PATH_BUFFER;

typedef struct _GRAPHICS_PATH_BUFFER_FIXED
{
	GRAPHICS_PATH_BUFFER base;

	GRAPHICS_PATH_OPERATOR op[GRAPHICS_PATH_BUFFER_SIZE];
	GRAPHICS_POINT points[2 * GRAPHICS_PATH_BUFFER_SIZE];
} GRAPHICS_PATH_BUFFER_FIXED;

typedef struct _GRAPHICS_PATH_FIXED
{
	GRAPHICS_POINT last_move_point;
	GRAPHICS_POINT current_point;
	unsigned int has_current_point : 1;
	unsigned int need_move_to : 1;
	unsigned int has_extents : 1;
	unsigned int has_curve_to : 1;
	unsigned int stroke_is_rectilinear : 1;
	unsigned int fill_is_rectilinear : 1;
	unsigned int fill_maybe_region : 1;
	unsigned int fill_is_empty : 1;

	GRAPHICS_BOX extents;
	GRAPHICS_PATH_BUFFER_FIXED buffer;
} GRAPHICS_PATH_FIXED;

typedef struct _GRAPHICS_PATH_FIXED_ITERATER
{
	const GRAPHICS_PATH_BUFFER *first;
	const GRAPHICS_PATH_BUFFER *buffer;
	unsigned int num_operator;
	unsigned int num_point;
} GRAPHICS_PATH_FIXED_ITERATER;

#ifdef __cplusplus
extern "C" {
#endif

extern void InitializeGraphicsPathFixed(GRAPHICS_PATH_FIXED* path);
extern void InitializeGraphicsPathFixedIterator(GRAPHICS_PATH_FIXED_ITERATER* iter, const GRAPHICS_PATH_FIXED* path);
extern eGRAPHICS_STATUS InitializeGraphicsPathFixedCopy(GRAPHICS_PATH_FIXED* path, const GRAPHICS_PATH_FIXED* other);

extern void GraphicsPathFixedFinish(GRAPHICS_PATH_FIXED* path);
extern eGRAPHICS_STATUS GraphicsPathFixedInterpret(
	GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_STATUS (*move_to)(void* closure, const GRAPHICS_POINT* point),
	eGRAPHICS_STATUS (*line_to)(void* closure, const GRAPHICS_POINT* point),
	eGRAPHICS_STATUS (*curve_to)(void* closure, const GRAPHICS_POINT* point0, const GRAPHICS_POINT* point1, const GRAPHICS_POINT* point2),
	eGRAPHICS_STATUS (*close_path)(void* closure),
	void* closure
);
extern eGRAPHICS_STATUS GraphicsPathFixedInterpretFlat(
	GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_STATUS (*move_to)(void* closure, const GRAPHICS_POINT* point),
	eGRAPHICS_STATUS (*line_to)(void* closure, const GRAPHICS_POINT* point),
	eGRAPHICS_STATUS (*close_path)(void* closure),
	void* closure,
	FLOAT_T tolerance
);

extern void GraphicsStrokeStyleMaxDistanceFromPath(
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_MATRIX* ctm,
	FLOAT_T* dx, FLOAT_T* dy
);

extern void GraphicsStrokeStyleMaxLineDistanceFromPath(
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_MATRIX* matrix,
	FLOAT_T* dx,
	FLOAT_T* dy
);

extern void GraphicsStrokeStyleMaxJoinDistanceFromPath(
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_MATRIX* matrix,
	FLOAT_T* dx,
	FLOAT_T* dy
);

extern int GraphicsStrokeStyleDashCanApproximate(
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_MATRIX* matrix,
	FLOAT_T tolerance
);

extern void GraphicsStrokeStyleDashApproximate(
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_MATRIX* matrix,
	FLOAT_T tolerance,
	FLOAT_T* dash_offset,
	FLOAT_T* dashes,
	unsigned int* num_dashes
);

extern void GraphicsPathFixedApproximateStrokeExtents(
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_MATRIX* ctm,
	int is_vector,
	GRAPHICS_RECTANGLE_INT* extents
);

extern void GraphicsPathFixedApproximateFillExtents(const GRAPHICS_PATH_FIXED* path, GRAPHICS_RECTANGLE_INT* extents);

extern void GraphicsPathFixedApproximateClipExtents(const GRAPHICS_PATH_FIXED* path, GRAPHICS_RECTANGLE_INT* extents);

extern void GraphicsPathFixedTranslate(GRAPHICS_PATH_FIXED* path, int32 offset_fixed_x, int32 offset_fixed_y);

extern void GraphicsPathFixedNewSubPath(GRAPHICS_PATH_FIXED* path);

extern eGRAPHICS_STATUS GraphicsPathFixedMoveTo(GRAPHICS_PATH_FIXED* path, GRAPHICS_FLOAT_FIXED x, GRAPHICS_FLOAT_FIXED y);
extern eGRAPHICS_STATUS GraphicsPathFixedRelativeMoveTo(GRAPHICS_PATH_FIXED* path, GRAPHICS_FLOAT_FIXED dx, GRAPHICS_FLOAT_FIXED dy);
extern eGRAPHICS_STATUS GraphicsPathFixedLineTo(GRAPHICS_PATH_FIXED* path, GRAPHICS_FLOAT_FIXED x, GRAPHICS_FLOAT_FIXED y);
extern eGRAPHICS_STATUS GraphicsPathFixedRelativeLineTo(GRAPHICS_PATH_FIXED* path, GRAPHICS_FLOAT_FIXED dx, GRAPHICS_FLOAT_FIXED dy);

extern eGRAPHICS_STATUS GraphicsSplineBound(
	GRAPHICS_SPLINE_ADD_POINT_FUNCTION add_point_function,
	void* closure,
	const GRAPHICS_POINT* point0,
	const GRAPHICS_POINT* point1,
	const GRAPHICS_POINT* point2,
	const GRAPHICS_POINT* point3
);

extern eGRAPHICS_STATUS GraphicsPathFixedCurveTo(
	GRAPHICS_PATH_FIXED* path,
	GRAPHICS_FLOAT_FIXED x0,
	GRAPHICS_FLOAT_FIXED y0,
	GRAPHICS_FLOAT_FIXED x1,
	GRAPHICS_FLOAT_FIXED y1,
	GRAPHICS_FLOAT_FIXED x2,
	GRAPHICS_FLOAT_FIXED y2
);

extern eGRAPHICS_STATUS GraphicsPathFixedRelativeCurveTo(
	GRAPHICS_PATH_FIXED* path,
	GRAPHICS_FLOAT_FIXED dx0, GRAPHICS_FLOAT_FIXED dy0,
	GRAPHICS_FLOAT_FIXED dx1, GRAPHICS_FLOAT_FIXED dy1,
	GRAPHICS_FLOAT_FIXED dx2, GRAPHICS_FLOAT_FIXED dy2
);

extern int GraphicsPathFixedExtents(const GRAPHICS_PATH_FIXED* path, GRAPHICS_BOX* box);

extern eGRAPHICS_STATUS GraphicsPathFixedClosePath(GRAPHICS_PATH_FIXED* path);

extern int GraphicsPathFixedGetCurrentPoint(
	GRAPHICS_PATH_FIXED* path,
	GRAPHICS_FLOAT_FIXED* x,
	GRAPHICS_FLOAT_FIXED* y
);

extern eGRAPHICS_INTEGER_STATUS GraphicsPathFixedStrokePolygonToTraps(
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* stroke_style,
	const GRAPHICS_MATRIX* matrix,
	const GRAPHICS_MATRIX* matrix_inverse,
	FLOAT_T tolerance,
	GRAPHICS_TRAPS* traps
);
extern eGRAPHICS_INTEGER_STATUS GraphicsPathFixedStrokeToTraps(
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_MATRIX* matrix,
	const GRAPHICS_MATRIX* matrix_inverse,
	FLOAT_T tolerance,
	GRAPHICS_TRAPS* traps
);

extern int GraphicsPathFixedIsBox(const GRAPHICS_PATH_FIXED* path, GRAPHICS_BOX* box);

extern int GraphicsPathFixedIsStrokeBox(
	const GRAPHICS_PATH_FIXED* path,
	GRAPHICS_BOX* box
);

extern int GraphicsPathFixedInFill(
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rule,
	FLOAT_T tolerance,
	FLOAT_T x,
	FLOAT_T y
);

extern eGRAPHICS_STATUS GraphicsPathFixedStrokeRectilinearToBoxes(
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* stroke_style,
	const GRAPHICS_MATRIX* matrix,
	eGRAPHICS_ANTIALIAS antialias,
	GRAPHICS_BOXES* boxes
);

extern eGRAPHICS_STATUS GraphicsPathFixedStrokeToPolygon(
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_MATRIX* matrix,
	const GRAPHICS_MATRIX* matrix_inverse,
	FLOAT_T tolerance,
	GRAPHICS_POLYGON* polygon
);

extern int GraphicsPathFixedIteratorIsFillBox(GRAPHICS_PATH_FIXED_ITERATER* _iter, GRAPHICS_BOX* box);

extern int GraphicsPathFixedIteratorAtEnd(const GRAPHICS_PATH_FIXED_ITERATER* iter);

extern int GraphicsSplineIntersects(
	const GRAPHICS_POINT* a,
	const GRAPHICS_POINT* b,
	const GRAPHICS_POINT* c,
	const GRAPHICS_POINT* d,
	const GRAPHICS_BOX* box
);

extern eGRAPHICS_STATUS GraphicsPathFixedFillToTraps(
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rule,
	FLOAT_T tolerance,
	GRAPHICS_TRAPS* traps
);

extern eGRAPHICS_STATUS GraphicsPathFixedFillRectilinearToBoxes(
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rule,
	eGRAPHICS_ANTIALIAS antialias,
	GRAPHICS_BOXES* boxes
);

extern eGRAPHICS_STATUS GraphicsPathFixedFillToPolygon(
	const GRAPHICS_PATH_FIXED* path,
	FLOAT_T tolerance,
	GRAPHICS_POLYGON* polygon
);

extern eGRAPHICS_INTEGER_STATUS GraphicsPathFixedStrokeToTristrip(
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_MATRIX* matrix,
	const GRAPHICS_MATRIX* matrix_inverse,
	FLOAT_T tolerance,
	GRAPHICS_TRISTRIP* strip
);

#ifdef __cplusplus
}
#endif

#endif // #ifndef _INCLUDED_GRAPHICS_PATH_H_
