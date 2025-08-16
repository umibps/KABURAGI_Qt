#include <math.h>
#include <string.h>
#include "graphics.h"
#include "graphics_function.h"
#include "graphics_region.h"
#include "graphics_private.h"
#include "graphics_matrix.h"
#include "graphics_pen.h"
#include "graphics_inline.h"
#include "types.h"
#include "../memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_FULL_CIRCLES 65536
#define STACK_BUFFER_SIZE (512 * sizeof(int))

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

typedef struct _PATH_FLATTENER
{
	FLOAT_T tolerance;
	GRAPHICS_POINT current_point;
	eGRAPHICS_STATUS (*move_to)(void* closure, const GRAPHICS_POINT* point);
	eGRAPHICS_STATUS (*line_to)(void* closure, const GRAPHICS_POINT* point);
	eGRAPHICS_STATUS (*close_path)(void* closure);
	void *closure;
} PATH_FLATTENER;

typedef struct _GRAPHICS_IN_FILL
{
	FLOAT_T tolerance;
	int on_edge;
	int winding;

	GRAPHICS_FLOAT_FIXED x, y;

	int has_current_point;
	GRAPHICS_POINT current_point;
	GRAPHICS_POINT first_point;
} GRAPHICS_IN_FILL;

static int EdgeCompareForY_againstX(
	const GRAPHICS_POINT* point1,
	const GRAPHICS_POINT* point2,
	GRAPHICS_FLOAT_FIXED y,
	GRAPHICS_FLOAT_FIXED x
)
{
	GRAPHICS_FLOAT_FIXED adx, ady;
	GRAPHICS_FLOAT_FIXED dx, dy;
	int64 L, R;

	adx = point2->x  - point1->x;
	dx = x - point1->x;

	if(adx == 0)
	{
		return -dx;
	}
	if((adx ^ dx) < 0)
	{
		return adx;
	}

	dy = y - point1->y;
	ady = point2->y - point1->y;

	L = GRAPHICS_INT32x32_64_MULTI(dy, adx);
	R = GRAPHICS_INT32x32_64_MULTI(dx, ady);

	return (L == R) ? 0 : (L < R) ? -1 : 1;
}

static void InitializeGraphicsInFill(
	GRAPHICS_IN_FILL* in_fill,
	FLOAT_T tolerance,
	FLOAT_T x,
	FLOAT_T y
)
{
	in_fill->on_edge = FALSE;
	in_fill->winding = 0;
	in_fill->tolerance = tolerance;

	in_fill->x = GraphicsFixedFromFloat(x);
	in_fill->y = GraphicsFixedFromFloat(y);

	in_fill->has_current_point = FALSE;
	in_fill->current_point.x = 0;
	in_fill->current_point.y = 0;
}

static INLINE void TranslatePoint(GRAPHICS_POINT* point, const GRAPHICS_POINT* offset)
{
	point->x += offset->x;
	point->y += offset->y;
}

static void GraphicsInFillAddEdge(
	GRAPHICS_IN_FILL* in_fill,
	const GRAPHICS_POINT* point1,
	const GRAPHICS_POINT* point2
)
{
	int direction;

	if(in_fill->on_edge)
	{
		return;
	}

	direction = 1;
	if(point2->y < point1->y)
	{
		const GRAPHICS_POINT *temp;

		temp = point1;
		point1 = point2;
		point2 = temp;

		direction = -1;
	}

	if((point1->x == in_fill->x && point1->y == in_fill->y)
		|| (point2->x == in_fill->x && point2->y == in_fill->y)
		|| (! (point2->y < in_fill->y || point1->y > in_fill->y
			|| (point1->x > in_fill->x && point2->x > in_fill->x)
			|| (point1->x < in_fill->x && point2->x < in_fill->x))
		&& EdgeCompareForY_againstX(point1, point2, in_fill->y, in_fill->x) == 0)
	)
	{
		in_fill->on_edge = TRUE;
		return;
	}

	if(point2->y <= in_fill->y || point1->y > in_fill->y)
	{
		return;
	}

	if(point1->x >= in_fill->x && point2->x >= in_fill->x)
	{
		return;
	}

	if((point1->x <= in_fill->x && point2->x <= in_fill->x)
		|| EdgeCompareForY_againstX(point1, point2, in_fill->y, in_fill->x) < 0
	)
	{
		in_fill->winding += direction;
	}
}

static eGRAPHICS_STATUS GraphicsInFillMoveTo(void* closure, const GRAPHICS_POINT* point)
{
	GRAPHICS_IN_FILL *in_fill = (GRAPHICS_IN_FILL*)closure;

	if(in_fill->has_current_point)
	{
		GraphicsInFillAddEdge(in_fill, &in_fill->current_point, &in_fill->first_point);
	}

	in_fill->first_point = *point;
	in_fill->current_point = *point;
	in_fill->has_current_point = TRUE;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsInFillLineTo(void* closure, const GRAPHICS_POINT* point)
{
	GRAPHICS_IN_FILL *in_fill = (GRAPHICS_IN_FILL*)closure;

	if(in_fill->has_current_point)
	{
		GraphicsInFillAddEdge(in_fill, &in_fill->current_point, point);
	}

	in_fill->current_point = *point;
	in_fill->has_current_point = TRUE;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsInFillCurveTo(
	void* closure,
	const GRAPHICS_POINT* b,
	const GRAPHICS_POINT* c,
	const GRAPHICS_POINT* d
)
{
	GRAPHICS_IN_FILL *in_fill = (GRAPHICS_IN_FILL*)closure;
	GRAPHICS_SPLINE spline;
	GRAPHICS_FLOAT_FIXED top, bottom, left;

	bottom = top = in_fill->current_point.y;
	if(b->y < top) top = b->y;
	if(b->y > bottom) bottom = b->y;
	if(c->y < top) top = c->y;
	if(c->y > bottom) bottom = c->y;
	if(d->y < top) top = d->y;
	if(d->y > bottom) bottom = d->y;
	if(bottom < in_fill->y || top > in_fill->y)
	{
		in_fill->current_point = *d;
		return GRAPHICS_STATUS_SUCCESS;
	}

	left = in_fill->current_point.x;
	if(b->x < left) left = b->x;
	if(c->x < left) left = c->x;
	if(d->x < left) left = d->x;
	if(left > in_fill->x)
	{
		in_fill->current_point = *d;
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(FALSE == InitializeGraphicsSpline(&spline,
		(GRAPHICS_SPLINE_ADD_POINT_FUNCTION)GraphicsInFillLineTo, in_fill,
			&in_fill->current_point, b, c, d)
	)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	return GraphicsSplineDecompose(&spline, in_fill->tolerance);
}

static eGRAPHICS_STATUS GraphicsInFillClosePath(void* closure)
{
	GRAPHICS_IN_FILL *in_fill = (GRAPHICS_IN_FILL*)closure;

	if(in_fill->has_current_point)
	{
		GraphicsInFillAddEdge(in_fill, &in_fill->current_point, &in_fill->first_point);

		in_fill->has_current_point = FALSE;
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static void ComputeFace(
	const GRAPHICS_POINT* point,
	const GRAPHICS_SLOPE* device_slope,
	FLOAT_T slope_dx,
	FLOAT_T slope_dy,
	GRAPHICS_STROKER* stroker,
	GRAPHICS_STROKE_FACE* face
);

GRAPHICS_PATH_BUFFER* CreateGraphicsPathBuffer(int size_ops, int size_points)
{
	GRAPHICS_PATH_BUFFER *buffer;

	size_ops += sizeof(FLOAT_T) - ((sizeof(GRAPHICS_PATH_BUFFER) + size_ops) % sizeof(FLOAT_T));
	buffer = (GRAPHICS_PATH_BUFFER*)MEM_ALLOC_FUNC(size_points * (sizeof(GRAPHICS_POINT) + (size_ops + sizeof(GRAPHICS_PATH_BUFFER))));
	if(buffer != NULL)
	{
		buffer->num_ops = 0;
		buffer->num_points = 0;
		buffer->size_ops = size_ops;
		buffer->size_points = size_points;

		buffer->op = (GRAPHICS_PATH_OPERATOR*)(buffer + 1);
		buffer->points = (GRAPHICS_POINT*)(buffer->op + size_ops);
	}

	return buffer;
}

static INLINE void GraphicsPathBufferDestroy(GRAPHICS_PATH_BUFFER* buffer)
{
	MEM_FREE_FUNC(buffer);
}

static INLINE int GraphicsSlopoeEqual(const GRAPHICS_SLOPE* a, const GRAPHICS_SLOPE* b)
{
	return INTEGER64_EQUAL(INTEGER32x32_64_MULTI(a->dy, b->dx),
		INTEGER32x32_64_MULTI(b->dy, a->dx));
}

static INLINE int GraphicsSlopeBackwards(const GRAPHICS_SLOPE* a, const GRAPHICS_SLOPE* b)
{
	return INTEGER64_NEGATIVE(INTEGER64_ADD(INTEGER32x32_64_MULTI(a->dx, b->dx),
		INTEGER32x32_64_MULTI(a->dy, b->dy)));
}

static eGRAPHICS_STATUS CountMoveTo(void* counter, const GRAPHICS_POINT* point)
{
	int *count = (int*)counter;
	*count = *count + 2;
	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS CountLineTo(void* counter, const GRAPHICS_POINT* point)
{
	int *count = (int*)counter;
	*count = *count + 2;
	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS CountCurveTo(
	void* counter,
	const GRAPHICS_POINT* point1,
	const GRAPHICS_POINT* point2,
	const GRAPHICS_POINT* point3
)
{
	int *count = (int*)counter;
	*count = *count + 4;
	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS CountClosePath(void* counter)
{
	int *count = (int*)counter;
	*count = *count + 1;
	return GRAPHICS_STATUS_SUCCESS;
}

static int GraphicsPathCount(GRAPHICS_PATH* path, GRAPHICS_PATH_FIXED* path_Fixed, FLOAT_T tolerance, int flatten)
{
	eGRAPHICS_STATUS status;
	int count = 0;

	if(flatten)
	{
		status = GraphicsPathFixedInterpretFlat(path_Fixed, CountMoveTo,
			CountLineTo, CountClosePath, &count, tolerance);
	}
	else
	{
		status = GraphicsPathFixedInterpret(path_Fixed, CountMoveTo,
			CountLineTo, CountCurveTo, CountClosePath, &count);
	}

	if(UNLIKELY(status))
	{
		return -1;
	}

	return count;
}

typedef struct _GRAPHICS_PATH_POPULATE
{
	GRAPHICS_PATH_DATA *data;
	GRAPHICS_CONTEXT *context;
} GRAPHICS_PATH_POPULATE;

static eGRAPHICS_STATUS PathPopulateMoveTo(void* closure, const GRAPHICS_POINT* point)
{
	GRAPHICS_PATH_POPULATE *path_populate = (GRAPHICS_PATH_POPULATE*)closure;
	GRAPHICS_PATH_DATA *data = path_populate->data;
	FLOAT_T x, y;

	x = GRAPHICS_FIXED_TO_FLOAT(point->x);
	y = GRAPHICS_FIXED_TO_FLOAT(point->y);

	path_populate->context->backend->backend_to_user(path_populate->context, &x, &y);

	data->header.type = GRAPHICS_PATH_MOVE_TO;
	data->header.length = 2;

	data[1].point.x = x;
	data[1].point.y = y;

	//path_populate->data += data->header.length;
	path_populate->data += 2;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS PathPopulateLineTo(void* closure, const GRAPHICS_POINT* point)
{
	GRAPHICS_PATH_POPULATE *path_populate = (GRAPHICS_PATH_POPULATE*)closure;
	GRAPHICS_PATH_DATA *data = path_populate->data;
	FLOAT_T x, y;

	x = GRAPHICS_FIXED_TO_FLOAT(point->x);
	y = GRAPHICS_FIXED_TO_FLOAT(point->y);

	path_populate->context->backend->backend_to_user(path_populate->context, &x, &y);

	data->header.type = GRAPHICS_PATH_LINE_TO;
	data->header.length = 2;

	data[1].point.x = x;
	data[1].point.y = y;

	//path_populate->data += data->header.length;
	path_populate->data += 2;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS PathPopulateCurveTo(
	void* closure,
	const GRAPHICS_POINT* point1,
	const GRAPHICS_POINT* point2,
	const GRAPHICS_POINT* point3
)
{
	GRAPHICS_PATH_POPULATE *path_populate = (GRAPHICS_PATH_POPULATE*)closure;
	GRAPHICS_PATH_DATA *data = path_populate->data;
	FLOAT_T x1, y1, x2, y2, x3, y3;

	x1 = GRAPHICS_FIXED_TO_FLOAT(point1->x);
	y1 = GRAPHICS_FIXED_TO_FLOAT(point1->y);
	path_populate->context->backend->backend_to_user(path_populate->context, &x1, &y1);

	x2 = GRAPHICS_FIXED_TO_FLOAT(point2->x);
	y2 = GRAPHICS_FIXED_TO_FLOAT(point2->y);
	path_populate->context->backend->backend_to_user(path_populate->context, &x2, &y2);

	x3 = GRAPHICS_FIXED_TO_FLOAT(point3->x);
	y3 = GRAPHICS_FIXED_TO_FLOAT(point3->y);
	path_populate->context->backend->backend_to_user(path_populate->context, &x3, &y3);

	data->header.type = GRAPHICS_PATH_CURVE_TO;
	data->header.length = 4;

	data[1].point.x = x1;
	data[1].point.y = y1;

	data[2].point.x = x2;
	data[2].point.y = y2;

	data[3].point.x = x3;
	data[3].point.y = y3;

	//path_populate->data += data->header.length;
	path_populate->data += 4;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS PathPopulateClosePath(void* closure)
{
	GRAPHICS_PATH_POPULATE *path_populate = (GRAPHICS_PATH_POPULATE*)closure;
	GRAPHICS_PATH_DATA *data = path_populate->data;

	data->header.type = GRAPHICS_PATH_CLOSE_PATH;
	data->header.length = 1;

	//path_populate->data += data->header.length;
	path_populate->data += 1;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsPathPopulate(
	GRAPHICS_PATH* path,
	GRAPHICS_PATH_FIXED* path_fixed,
	GRAPHICS_CONTEXT* context,
	int flattern
)
{
	eGRAPHICS_STATUS status;
	GRAPHICS_PATH_POPULATE path_populate;

	path_populate.data = path->data;
	path_populate.context = context;

	if(flattern)
	{
		status = GraphicsPathFixedInterpretFlat(path_fixed, PathPopulateMoveTo,
			PathPopulateLineTo, PathPopulateClosePath, &path_populate, GraphicsGetTolerance(context));
	}
	else
	{
		status = GraphicsPathFixedInterpret(path_fixed, PathPopulateMoveTo,
			PathPopulateLineTo, PathPopulateCurveTo, PathPopulateClosePath, &path_populate);
	}

	if(UNLIKELY(status))
	{
		return status;
	}

	ASSERT(path_populate.data - path->data == path->num_data);

	return GRAPHICS_STATUS_SUCCESS;
}

static GRAPHICS_PATH* CreateGraphicsPathInernal(GRAPHICS_PATH_FIXED* path_fixed, GRAPHICS_CONTEXT* context, int flatten)
{
	GRAPHICS_PATH *path;

	path = (GRAPHICS_PATH*)MEM_ALLOC_FUNC(sizeof(*path));
	if(UNLIKELY(path == NULL))
	{
		return NULL;
	}
	path->own_memory = TRUE;

	path->num_data = GraphicsPathCount(path, path_fixed, GraphicsGetTolerance(context), flatten);

	if(path->num_data < 0)
	{
		MEM_FREE_FUNC(path);
		return NULL;
	}

	if(path->num_data != 0)
	{
		path->data = (GRAPHICS_PATH_DATA*)MEM_ALLOC_FUNC(sizeof(*path->data)*path->num_data);
		if(UNLIKELY(path->data == NULL))
		{
			return NULL;
		}
		path->status = GraphicsPathPopulate(path, path_fixed, context, flatten);
	}
	else
	{
		path->data = NULL;
		path->status = GRAPHICS_STATUS_SUCCESS;
	}

	return path;
}

struct _GRAPHICS_PATH* CreateGraphicsPath(struct _GRAPHICS_PATH_FIXED* path, struct _GRAPHICS_CONTEXT* context)
{
	return CreateGraphicsPathInernal(path, context, FALSE);
}

struct _GRAPHICS_PATH* CreateGraphicsPathFlat(struct _GRAPHICS_PATH_FIXED* path, struct _GRAPHICS_CONTEXT* context)
{
	return CreateGraphicsPathInernal(path, context, TRUE);
}

void GraphicsPathFixedFinish(GRAPHICS_PATH_FIXED* path)
{
	GRAPHICS_PATH_BUFFER *buffer, *head;

	buffer = GRAPHICS_PATH_BUFFER_NEXT(GRAPHICS_PATH_HEAD(path));
	head = (GRAPHICS_PATH_BUFFER*)buffer->link.prev;

	while(buffer != head)
	{
		GRAPHICS_PATH_BUFFER *ptr = buffer;
		buffer = GRAPHICS_PATH_BUFFER_NEXT(buffer);
		GraphicsPathBufferDestroy(ptr);
	}
}

eGRAPHICS_STATUS GraphicsPathFixedInterpret(
	GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_STATUS (*move_to)(void* closure, const GRAPHICS_POINT* point),
	eGRAPHICS_STATUS (*line_to)(void* closure, const GRAPHICS_POINT* point),
	eGRAPHICS_STATUS (*curve_to)(void* closure, const GRAPHICS_POINT* point0, const GRAPHICS_POINT* point1, const GRAPHICS_POINT* point2),
	eGRAPHICS_STATUS (*close_path)(void* closure),
	void* closure
)
{
	const GRAPHICS_PATH_BUFFER *buffer;
	eGRAPHICS_STATUS status;

	buffer = &path->buffer.base;
	do
	{
		GRAPHICS_POINT *points = buffer->points;
		unsigned int i;
		for(i=0; i<buffer->num_ops; i++)
		{
			switch(buffer->op[i])
			{
			case GRAPHICS_PATH_MOVE_TO:
				status = (*move_to)(closure, &points[0]);
				points += 1;
				break;
			case GRAPHICS_PATH_LINE_TO:
				status = (*line_to)(closure, &points[0]);
				points += 1;
				break;
			case GRAPHICS_PATH_CURVE_TO:
				status = (*curve_to)(closure, &points[0], &points[1], &points[2]);
				points += 3;
				break;
			case GRAPHICS_PATH_CLOSE_PATH:
				status = (*close_path)(closure);
				break;
			}

			if(UNLIKELY(status))
			{
				return status;
			}
		}
	} while((buffer = buffer->link.next) != &path->buffer.base);

	if(path->need_move_to != FALSE && path->has_current_point != FALSE)
	{
		return (*move_to)(closure, &path->current_point);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static void StrokerAddCap(
	GRAPHICS_STROKER* stroker,
	const GRAPHICS_STROKE_FACE* f,
	GRAPHICS_STROKE_CONTOUR* c
);

static INLINE int WithinTolerance(
	const GRAPHICS_POINT* point1,
	const GRAPHICS_POINT* point2,
	uint64 tolerance
)
{
	return FALSE;
}

static INLINE void StrokerContourAddPoint(
	GRAPHICS_STROKER* stroker,
	GRAPHICS_STROKE_CONTOUR* c,
	const GRAPHICS_POINT* point
)
{
	if(FALSE == WithinTolerance(point, GraphicsContourLastPoint(&c->contour),
			stroker->contour_tolerance))
	{
		GraphicsContourAddPoint(&c->contour, point);
	}
}

static INLINE void StrokerTranslatePoint(GRAPHICS_POINT* point, const GRAPHICS_POINT* offset)
{
	point->x += offset->x;
	point->y += offset->y;
}

static void StrokerAddLeadingCap(
	GRAPHICS_STROKER* stroker,
	const GRAPHICS_STROKE_FACE* face,
	GRAPHICS_STROKE_CONTOUR* c
)
{
	GRAPHICS_STROKE_FACE reversed;
	GRAPHICS_POINT t;
	
	reversed = *face;
	
	reversed.user_vector.x = - reversed.user_vector.x;
	reversed.user_vector.y = - reversed.user_vector.y;
	reversed.device_vector.dx = - reversed.device_vector.dx;
	reversed.device_vector.dy = - reversed.device_vector.dy;
	
	t = reversed.clock_wise;
	reversed.clock_wise = reversed.counter_clock_wise;
	reversed.counter_clock_wise = t;
	
	StrokerAddCap(stroker, &reversed, c);
}

static INLINE void StrokerAddTrailingCap(
	GRAPHICS_STROKER* stroker,
	const GRAPHICS_STROKE_FACE* face,
	GRAPHICS_STROKE_CONTOUR* c
)
{
	StrokerAddCap(stroker, face, c);
}

static INLINE FLOAT_T StrokerNomalizeSlope(FLOAT_T* dx, FLOAT_T* dy)
{
	FLOAT_T dx0 = *dx, dy0 = *dy;
	FLOAT_T magnitude;
	
	if(dx0 == 0.0)
	{
		*dx = 0.0;
		if(dy0 > 0.0)
		{
			magnitude = dy0;
			*dy = 1.0;
		}
		else
		{
			magnitude = -dy0;
			*dy = - 1.0;
		}
	}
	else if(dy0 == 0.0)
	{
		*dy = 0.0;
		if(dx0 > 0.0)
		{
			magnitude = dx0;
			*dx = 1.0;
		}
		else
		{
			magnitude = - dx0;
			*dx = - 1.0;
		}
	}
	else
	{
		magnitude = HYPOT(dx0, dy0);
		*dx = dx0 / magnitude;
		*dy = dy0 / magnitude;
	}
	
	return magnitude;
}

static void StrokerComputeFace(
	const GRAPHICS_POINT* point,
	const GRAPHICS_SLOPE* devide_slope,
	GRAPHICS_STROKER* stroker,
	GRAPHICS_STROKE_FACE* face
)
{
	FLOAT_T face_dx, face_dy;
	GRAPHICS_POINT offset_counter_clock_wise, offset_clock_wise;
	FLOAT_T slope_dx, slope_dy;
	
	slope_dx = GRAPHICS_FIXED_TO_FLOAT(devide_slope->dx);
	slope_dy = GRAPHICS_FIXED_TO_FLOAT(devide_slope->dy);
	face->length = StrokerNomalizeSlope(&slope_dx, &slope_dy);
	face->device_slope.x = slope_dx;
	face->device_slope.y = slope_dy;
	
	if(FALSE == GRAPHICS_MATRIX_IS_IDENTITY(stroker->matrix_inverse))
	{
		GraphicsMatrixTransformDistance(stroker->matrix_inverse, &slope_dx, &slope_dy);
		StrokerNomalizeSlope(&slope_dx, &slope_dy);
		
		if(stroker->matrix_determin_positive)
		{
			face_dx = - slope_dy * stroker->half_line_width;
			face_dy = slope_dx * stroker->half_line_width;
		}
		else
		{
			face_dx = slope_dy * stroker->half_line_width;
			face_dy = - slope_dx * stroker->half_line_width;
		}
		
		GraphicsMatrixTransformDistance(stroker->matrix, &face_dx, &face_dy);
	}
	else
	{
		face_dx = - slope_dy * stroker->half_line_width;
		face_dy = slope_dx * stroker->half_line_width;
	}
	
	offset_counter_clock_wise.x = GraphicsFixedFromFloat(face_dx);
	offset_counter_clock_wise.y = GraphicsFixedFromFloat(face_dy);
	offset_clock_wise.x = - offset_counter_clock_wise.x;
	offset_clock_wise.y = - offset_counter_clock_wise.y;
	
	face->counter_clock_wise = *point;
	StrokerTranslatePoint(&face->counter_clock_wise, &offset_counter_clock_wise);
	
	face->point = *point;
	
	face->clock_wise = *point;
	StrokerTranslatePoint(&face->clock_wise, &offset_clock_wise);
	
	face->user_vector.x = slope_dx;
	face->user_vector.y = slope_dy;
	
	face->device_vector = *devide_slope;
}

static void StrokerAddFan(
	GRAPHICS_STROKER* stroker,
	const GRAPHICS_SLOPE* in_vector,
	const GRAPHICS_SLOPE* out_vector,
	const GRAPHICS_POINT* mid_point,
	int clockwise,
	GRAPHICS_STROKE_CONTOUR* c
)
{
	GRAPHICS_PEN *pen = &stroker->pen;
	int start, stop;
	
	if(stroker->has_bounds &&
		FALSE == GraphicsBoxContainsPoint(&stroker->bounds, mid_point))
	{
		return;
	}
	
	if(clockwise)
	{
		GraphicsPenFindActiveClockwiseVertices(pen, in_vector, out_vector, &start, &stop);
		while(start != stop)
		{
			GRAPHICS_POINT p = *mid_point;
			StrokerTranslatePoint(&p, &pen->vertices[start].point);
			StrokerContourAddPoint(stroker, c, &p);
			
			if(++start == pen->num_vertices)
			{
				start = 0;
			}
		}
	}
	else
	{
		GraphicsPenFindActiveCounterClockwiseVertices(pen,
							in_vector, out_vector, &start, &stop);
		while(start != stop)
		{
			GRAPHICS_POINT p = *mid_point;
			StrokerTranslatePoint(&p, &pen->vertices[start].point);
			StrokerContourAddPoint(stroker, c, &p);
			
			if(start-- == 0)
			{
				start += pen->num_vertices;
			}
		}
	}
}

static void StrokerAddCap(
	GRAPHICS_STROKER* stroker,
	const GRAPHICS_STROKE_FACE* f,
	GRAPHICS_STROKE_CONTOUR* c
)
{
	switch(stroker->style.line_cap)
	{
	case GRAPHICS_LINE_CAP_ROUND:
		{
			GRAPHICS_SLOPE slope;

			slope.dx = -f->device_vector.dx;
			slope.dy = -f->device_vector.dy;

			StrokerAddFan(stroker, &f->device_vector, &slope, &f->point, FALSE, c);
		}
		break;
	case GRAPHICS_LINE_CAP_SQUARE:
		{
			GRAPHICS_SLOPE f_vector;
			GRAPHICS_POINT p;
			FLOAT_T dx, dy;
			
			dx = f->user_vector.x;
			dy = f->user_vector.y;
			dx *= stroker->half_line_width;
			dy *= stroker->half_line_width;
			GraphicsMatrixTransformDistance(stroker->matrix, &dx, &dy);
			f_vector.dx = GraphicsFixedFromFloat(dx);
			f_vector.dy = GraphicsFixedFromFloat(dy);
			
			p.x = f->counter_clock_wise.x + f_vector.dx;
			p.y = f->counter_clock_wise.y + f_vector.dy;
			StrokerContourAddPoint(stroker, c, &p);
			
			p.x = f->clock_wise.x + f_vector.dx;
			p.y = f->clock_wise.y + f_vector.dy;
			StrokerContourAddPoint(stroker, c, &p);
		}
	case GRAPHICS_LINE_CAP_BUTT:
	default:
		break;
	}
	StrokerContourAddPoint(stroker, c, &f->clock_wise);
}

static void StrokerAddCaps(GRAPHICS_STROKER* stroker)
{
	if(stroker->has_initial_sub_path
		&& ! stroker->has_first_face
		&& ! stroker->has_current_face
		&& stroker->style.line_cap == GRAPHICS_LINE_CAP_ROUND
	)
	{
		GRAPHICS_SLOPE slope = {GRAPHICS_FIXED_ONE, 0};
		GRAPHICS_STROKE_FACE face;
		
		StrokerComputeFace(&stroker->first_point, &slope, stroker, &face);
		
		StrokerAddLeadingCap(stroker, &face, &stroker->counter_clock_wise);
		StrokerAddTrailingCap(stroker, &face, &stroker->counter_clock_wise);
		
		GraphicsContourAddPoint(&stroker->counter_clock_wise.contour,
						GraphicsContourFirstPoint(&stroker->counter_clock_wise.contour));
						
		GraphicsPolygonAddContour(stroker->polygon, &stroker->counter_clock_wise.contour);
		GraphicsContourReset(&stroker->counter_clock_wise.contour);
	}
	else
	{
		if(stroker->has_current_face)
		{
			StrokerAddTrailingCap(stroker, &stroker->current_face, &stroker->counter_clock_wise);
		}
		
		GraphicsPolygonAddContour(stroker->polygon, &stroker->counter_clock_wise.contour);
		GraphicsContourReset(&stroker->counter_clock_wise.contour);
		
		if(stroker->has_first_face)
		{
			GraphicsContourAddPoint(&stroker->counter_clock_wise.contour, &stroker->first_face.clock_wise);
			StrokerAddLeadingCap(stroker, &stroker->first_face, &stroker->counter_clock_wise);
			
			GraphicsPolygonAddContour(stroker->polygon, &stroker->counter_clock_wise.contour);
			GraphicsContourReset(&stroker->counter_clock_wise.contour);
		}
		
		GraphicsPolygonAddContour(stroker->polygon, &stroker->clock_wise.contour);
		GraphicsContourReset(&stroker->clock_wise.contour);
	}
}

static INLINE int SlopeCompareSign(FLOAT_T dx1, FLOAT_T dy1, FLOAT_T dx2, FLOAT_T dy2)
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

static INLINE int JoinIsClockwise(const GRAPHICS_STROKE_FACE* in, const GRAPHICS_STROKE_FACE* out)
{
	return GraphicsSlopeCompare(&in->device_vector, &out->device_vector);
}

static void StrokerInnerJoin(
	GRAPHICS_STROKER* stroker,
	const GRAPHICS_STROKE_FACE* in,
	const GRAPHICS_STROKE_FACE* out,
	int clockwise
)
{
	const GRAPHICS_POINT *out_point;
	GRAPHICS_STROKE_CONTOUR *inner;
	
	if(clockwise)
	{
		inner = &stroker->counter_clock_wise;
		out_point = &out->counter_clock_wise;
	}
	else
	{
		inner = &stroker->clock_wise;
		out_point = &out->clock_wise;
	}
	StrokerContourAddPoint(stroker, inner, &in->point);
	StrokerContourAddPoint(stroker, inner, out_point);
}

static void StrokerInnerClose(
	GRAPHICS_STROKER* stroker,
	const GRAPHICS_STROKE_FACE* in,
	GRAPHICS_STROKE_FACE* out
)
{
	const GRAPHICS_POINT *in_point;
	GRAPHICS_STROKE_CONTOUR *inner;
	
	if(JoinIsClockwise(in, out))
	{
		inner = &stroker->counter_clock_wise;
		in_point = &out->counter_clock_wise;
	}
	else
	{
		inner = &stroker->clock_wise;
		in_point = &out->clock_wise;
	}
	
	StrokerContourAddPoint(stroker, inner, &in->point);
	StrokerContourAddPoint(stroker, inner, in_point);
	*GraphicsContourFirstPoint(&inner->contour) = *GraphicsContourLastPoint(&inner->contour);
}

static void StrokerOuterJoin(
	GRAPHICS_STROKER* stroker,
	const GRAPHICS_STROKE_FACE* in,
	const GRAPHICS_STROKE_FACE* out,
	int clockwise
)
{
	const GRAPHICS_POINT *in_point, *out_point;
	GRAPHICS_STROKE_CONTOUR *outer;
	
	if(in->clock_wise.x == out->clock_wise.x && in->clock_wise.y == out->clock_wise.y
		&& in->counter_clock_wise.x == out->counter_clock_wise.x && in->counter_clock_wise.y == out->counter_clock_wise.y)
	{
		return;	
	}
	
	if(clockwise)
	{
		in_point = &in->clock_wise;
		out_point = &out->clock_wise;
		outer = &stroker->clock_wise;
	}
	else
	{
		in_point = &in->counter_clock_wise;
		out_point = &out->counter_clock_wise;
		outer = &stroker->counter_clock_wise;
	}
	
	switch(stroker->style.line_join)
	{
	case GRAPHICS_LINE_JOIN_ROUND:
		StrokerAddFan(stroker, &in->device_vector, &out->device_vector, &in->point, clockwise, outer);
		break;
	case GRAPHICS_LINE_JOIN_MITER:
	default:
		{
			FLOAT_T in_dot_out = in->device_slope.x * out->device_slope.x
									+ in->device_slope.y * out->device_slope.y;
			FLOAT_T ml = stroker->style.miter_limit;
		
			if(2 <= ml * ml * (1 + in_dot_out))
			{
				FLOAT_T x1, y1, x2, y2;
				FLOAT_T mx, my;
				FLOAT_T dx1, dx2, dy1, dy2;
				FLOAT_T ix, iy;
				FLOAT_T fdx1, fdy1, fdx2, fdy2;
				FLOAT_T mdx, mdy;
			
				x1 = GRAPHICS_FIXED_TO_FLOAT(in_point->x);
				y1 = GRAPHICS_FIXED_TO_FLOAT(in_point->y);
				dx1 = in->device_slope.x;
				dy1 = in->device_slope.y;
			
				x2 = GRAPHICS_FIXED_TO_FLOAT(out_point->x);
				y2 = GRAPHICS_FIXED_TO_FLOAT(out_point->y);
				dx2 = out->device_slope.x;
				dy2 = out->device_slope.y;
			
				my = (((x2 - x1) * dy1 * dy2 - y2 * dx2 * dy1 + y1 * dx1 * dy2)
					/ (dx1 * dy2 - dx2 * dy1));
				if(FABS(dy1) >= FABS(dy2))
				{
					mx = (my - y1) * dx1 / dy1 + x1;
				}
				else
				{
					mx = (my - y2) * dx2 / dy2 + x2;
				}
			
				ix = GRAPHICS_FIXED_TO_FLOAT(in->point.x);
				iy = GRAPHICS_FIXED_TO_FLOAT(in->point.y);

				fdx1 = x1 - ix, fdy1 = y1 - iy;
			
				fdx2 = x2 - ix, fdy2 = y2 - iy;
			
				mdx = mx - ix,  mdy = my - iy;
			
				if(SlopeCompareSign(fdx1, fdy1, mdx, mdy)
					!= SlopeCompareSign(fdx2, fdy2, mdx, mdy))
				{
					GRAPHICS_POINT p;
				
					p.x = GraphicsFixedFromFloat(mx);
					p.y = GraphicsFixedFromFloat(my);
				
					*GraphicsContourLastPoint(&outer->contour) = p;
					return;
				}
			}
		}
		break;
	case GRAPHICS_LINE_JOIN_BEVEL:
		break;
	}
	StrokerContourAddPoint(stroker, outer, out_point);
}

static void StrokerOuterClose(
	GRAPHICS_STROKER* stroker,
	const GRAPHICS_STROKE_FACE* in,
	const GRAPHICS_STROKE_FACE* out
)
{
	const GRAPHICS_POINT *in_point, *out_point;
	GRAPHICS_STROKE_CONTOUR *outer;
	int clockwise;
	
	if(in->clock_wise.x == out->clock_wise.x && in->clock_wise.y == out->clock_wise.y
		&& in->counter_clock_wise.x == out->counter_clock_wise.x && in->counter_clock_wise.y == out->counter_clock_wise.x)
	{
		return;
	}
	
	clockwise = JoinIsClockwise(in, out);
	if(clockwise)
	{
		in_point = &in->clock_wise;
		out_point = &out->clock_wise;
		outer = &stroker->clock_wise;
	}
	else
	{
		in_point = &in->counter_clock_wise;
		out_point = &out->counter_clock_wise;
		outer = &stroker->counter_clock_wise;
	}
	
	if(WithinTolerance(in_point, out_point, stroker->contour_tolerance))
	{
		*GraphicsContourFirstPoint(&outer->contour) = *GraphicsContourLastPoint(&outer->contour);
		return;
	}
	
	switch(stroker->style.line_join)
	{
	case GRAPHICS_LINE_JOIN_ROUND:
		if((in->device_slope.x * out->device_slope.x + in->device_slope.y * out->device_slope.y)
			< stroker->spline_cusp_tolerance)
		{
			StrokerAddFan(stroker, &in->device_vector, &out->device_vector, &in->point, clockwise, outer);
			break;	
		}
	case GRAPHICS_LINE_JOIN_MITER:
	default:
		{
			FLOAT_T in_dot_out = in->device_slope.x * out->device_slope.x
									+ in->device_slope.y * out->device_slope.y;
			FLOAT_T ml = stroker->style.miter_limit;
			
			if(2 <= ml * ml * (1 + in_dot_out))
			{
				FLOAT_T x1, y1, x2, y2;
				FLOAT_T mx, my;
				FLOAT_T dx1, dx2, dy1, dy2;
				FLOAT_T ix, iy;
				FLOAT_T fdx1, fdy1, fdx2, fdy2;
				FLOAT_T mdx, mdy;
				
				x1 = GRAPHICS_FIXED_TO_FLOAT(in_point->x);
				y1 = GRAPHICS_FIXED_TO_FLOAT(in_point->y);
				dx1 = in->device_slope.x;
				dy1 = in->device_slope.y;
				
				x2 = GRAPHICS_FIXED_TO_FLOAT(out_point->x);
				y2 = GRAPHICS_FIXED_TO_FLOAT(out_point->y);
				dx2 = out->device_slope.x;
				dy2 = out->device_slope.y;
				
				my = (((x2 - x1) * dy1 * dy2 - y2 * dx2 * dy1 + y1 * dx1 * dy2)
						/ (dx1 * dy2 - dx2 * dy1));
				if(FABS(dy1) >= FABS(dy2))
				{
					mx = (my - y1) * dx1 / dy1 + x1;
				}
				else
				{
					mx = (my - y2) * dx2 / dy2 + x2;
				}
				
				ix = GRAPHICS_FIXED_TO_FLOAT(in->point.x);
				iy = GRAPHICS_FIXED_TO_FLOAT(in->point.y);
				
				fdx1 = x1 - ix, fdy1 = y1 - iy;
				
				fdx2 = x2 - ix, fdy2 = y2 - iy;
				
				mdx = mx - ix,  mdy = my - iy;
				
				if(SlopeCompareSign(fdx1, fdy1, mdx, mdy) != SlopeCompareSign(fdx2, fdy2, mdx, mdy))
				{
					GRAPHICS_POINT p;
					
					p.x = GraphicsFixedFromFloat(mx);
					p.y = GraphicsFixedFromFloat(my);
					
					*GraphicsContourLastPoint(&outer->contour) = p;
					*GraphicsContourFirstPoint(&outer->contour) = p;
					return;
				}
			}
		}
		break;
	case GRAPHICS_LINE_JOIN_BEVEL:
		break;
	}
	StrokerContourAddPoint(stroker, outer, out_point);
}

static eGRAPHICS_STATUS StrokerClosePath(void* closure);

static eGRAPHICS_STATUS StrokerMoveTo(void* closure, const GRAPHICS_POINT* point)
{
	GRAPHICS_STROKER *stroker = (GRAPHICS_STROKER*)closure;
	
	StrokerAddCaps(stroker);
	
	stroker->has_first_face = FALSE;
	stroker->has_current_face = FALSE;
	stroker->has_initial_sub_path = FALSE;
	
	stroker->first_point = *point;

	stroker->current_face.point = *point;
	
	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS StrokerLineTo(void* closure, const GRAPHICS_POINT* point)
{
	GRAPHICS_STROKER *stroker = (GRAPHICS_STROKER*)closure;
	GRAPHICS_STROKE_FACE start;
	GRAPHICS_POINT *point1 = &stroker->current_face.point;
	GRAPHICS_SLOPE devide_slope;
	
	stroker->has_initial_sub_path = TRUE;
	
	if(point1->x == point1->x && point1->y == point->y)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}
	
	INITIALIZE_GRAPHICS_SLOPE(&devide_slope, point1, point);
	StrokerComputeFace(point1, &devide_slope, stroker, &start);
	
	if(stroker->has_current_face)
	{
		int clock_wise = GraphicsSlopeCompare(&stroker->current_face.device_vector, &start.device_vector);
		if(clock_wise)
		{
			clock_wise = clock_wise < 0;
			if(FALSE == WithinTolerance(&stroker->current_face.counter_clock_wise,
						&start.counter_clock_wise, stroker->contour_tolerance)
				|| FALSE == WithinTolerance(&stroker->current_face.clock_wise, &start.clock_wise, stroker->contour_tolerance)
			)
			{
				StrokerOuterJoin(stroker, &stroker->current_face, &start, clock_wise);
				StrokerInnerJoin(stroker, &stroker->current_face, &start, clock_wise);
			}
		}
	}
	else
	{
		if(FALSE == stroker->has_first_face)
		{
			stroker->first_face = start;
			stroker->has_first_face = TRUE;
		}
		stroker->has_current_face = TRUE;
		
		StrokerContourAddPoint(stroker, &stroker->clock_wise, &start.clock_wise);
		StrokerContourAddPoint(stroker, &stroker->counter_clock_wise, &start.counter_clock_wise);
	}
	
	stroker->current_face = start;
	stroker->current_face.point = *point;
	stroker->current_face.counter_clock_wise.x += devide_slope.dx;
	stroker->current_face.counter_clock_wise.y += devide_slope.dy;
	stroker->current_face.clock_wise.x += devide_slope.dx;
	stroker->current_face.clock_wise.y += devide_slope.dy;
	
	StrokerContourAddPoint(stroker, &stroker->clock_wise, &stroker->current_face.clock_wise);
	StrokerContourAddPoint(stroker, &stroker->counter_clock_wise, &stroker->current_face.counter_clock_wise);
	
	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS StrokerSplineTo(
	void* closure,
	const GRAPHICS_POINT* point,
	const GRAPHICS_SLOPE* tangent
)
{
	GRAPHICS_STROKER *stroker = (GRAPHICS_STROKER*)closure;
	GRAPHICS_STROKE_FACE face;
	
	if((tangent->dx | tangent->dy) == 0)
	{
		GRAPHICS_STROKE_CONTOUR *outer;
		GRAPHICS_POINT t;
		int clockwise;
		
		face = stroker->current_face;
		
		face.user_vector.x = - face.user_vector.x;
		face.user_vector.y = - face.user_vector.y;
		face.device_vector.dx = - face.device_vector.dx;
		face.device_vector.dy = - face.device_vector.dy;
		
		t = face.clock_wise;
		face.clock_wise = face.counter_clock_wise;
		face.counter_clock_wise = t;
		
		clockwise = JoinIsClockwise(&stroker->current_face, &face);
		outer = closure ? &stroker->clock_wise : &stroker->counter_clock_wise;
		
		StrokerAddFan(stroker, &stroker->current_face.device_vector, &face.device_slope,
						&stroker->current_face.point, clockwise, outer);
	}
	else
	{
		StrokerComputeFace(point, tangent, stroker, &face);
		
		if((face.device_slope.x * stroker->current_face.device_slope.x
			+ face.device_slope.y * stroker->current_face.device_slope.y) < stroker->spline_cusp_tolerance)
		{
			GRAPHICS_STROKE_CONTOUR *outer;
			int clockwise = JoinIsClockwise(&stroker->current_face, &face);
			
			stroker->current_face.clock_wise.x += face.point.x - stroker->current_face.point.x;
			stroker->current_face.clock_wise.y += face.point.y - stroker->current_face.point.y;
			StrokerContourAddPoint(stroker, &stroker->counter_clock_wise, &stroker->current_face.counter_clock_wise);
			
			outer = clockwise ? &stroker->clock_wise : &stroker->counter_clock_wise;
			StrokerAddFan(stroker, &stroker->current_face.device_vector, &face.device_vector,
							&stroker->current_face.point, clockwise, outer);
		}
		
		StrokerContourAddPoint(stroker, &stroker->clock_wise, &face.clock_wise);
		StrokerContourAddPoint(stroker, &stroker->counter_clock_wise, &face.counter_clock_wise);
	}
	
	stroker->current_face = face;
	
	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS StrokerCurveTo(
	void* closure,
	const GRAPHICS_POINT* b,
	const GRAPHICS_POINT* c,
	const GRAPHICS_POINT* d
)
{
	GRAPHICS_STROKER *stroker = (GRAPHICS_STROKER*)closure;
	GRAPHICS_SPLINE spline;
	GRAPHICS_STROKE_FACE face;
	
	if(stroker->has_bounds && FALSE == GraphicsSplineIntersects(&stroker->current_face.point, b, c, d, &stroker->bounds))
	{
		return StrokerLineTo(closure, d);
	}
	
	if(FALSE == InitializeGraphicsSpline(&spline, StrokerSplineTo, stroker, &stroker->current_face.point, b, c, d))
	{
		return StrokerLineTo(closure, d);
	}
	
	StrokerComputeFace(&stroker->current_face.point, &spline.initial_slope, stroker, &face);
	
	if(stroker->has_current_face)
	{
		int clockwise = JoinIsClockwise(&stroker->current_face, &face);
		
		StrokerOuterJoin(stroker, &stroker->current_face, &face, clockwise);
		StrokerInnerJoin(stroker, &stroker->current_face, &face, clockwise);
	}
	else
	{
		if(FALSE == stroker->has_first_face)
		{
			stroker->first_face = face;
			stroker->has_first_face = TRUE;
		}
		stroker->has_current_face = TRUE;
		
		StrokerContourAddPoint(stroker, &stroker->clock_wise, &face.clock_wise);
		StrokerContourAddPoint(stroker, &stroker->counter_clock_wise, &face.counter_clock_wise);
	}
	stroker->current_face = face;
	
	return GraphicsSplineDecompose(&spline, stroker->tolerance);
}

static eGRAPHICS_STATUS StrokerClosePath(void* closure)
{
	GRAPHICS_STROKER *stroker = (GRAPHICS_STROKER*)closure;
	eGRAPHICS_STATUS status;
	
	status = StrokerLineTo(stroker, &stroker->first_point);
	if(UNLIKELY(status))
	{
		return status;
	}
	
	if(stroker->has_first_face && stroker->has_current_face)
	{
		StrokerOuterClose(stroker, &stroker->current_face, &stroker->first_face);
		StrokerInnerClose(stroker, &stroker->current_face, &stroker->first_face);
		
		GraphicsPolygonAddContour(stroker->polygon, &stroker->clock_wise.contour);
		GraphicsPolygonAddContour(stroker->polygon, &stroker->counter_clock_wise.contour);
		
		GraphicsContourReset(&stroker->clock_wise.contour);
		GraphicsContourReset(&stroker->counter_clock_wise.contour);
	}
	else
	{
		StrokerAddCaps(stroker);
	}
	
	stroker->has_initial_sub_path = FALSE;
	stroker->has_first_face = FALSE;
	stroker->has_current_face = FALSE;
	
	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS MoveTo(void* closure, const GRAPHICS_POINT* point)
{
	PATH_FLATTENER *flattener = (PATH_FLATTENER*)closure;

	flattener->current_point = *point;

	return flattener->move_to(flattener->closure, point);
}

static eGRAPHICS_STATUS LineTo(void* closure, const GRAPHICS_POINT* point)
{
	PATH_FLATTENER *flattener = (PATH_FLATTENER*)closure;

	flattener->current_point = *point;

	return flattener->line_to(flattener->closure, point);
}

static eGRAPHICS_STATUS CurveTo(void* closure, const GRAPHICS_POINT* point1, const GRAPHICS_POINT* point2, const GRAPHICS_POINT* point3)
{
	PATH_FLATTENER *flattener = (PATH_FLATTENER*)closure;
	GRAPHICS_SPLINE spline;
	GRAPHICS_POINT *point0 = &flattener->current_point;

	if(FALSE == InitializeGraphicsSpline(&spline, (GRAPHICS_SPLINE_ADD_POINT_FUNCTION)flattener->line_to,
		flattener->closure, point0, point1, point2, point3))
	{
		return LineTo(closure, point3);
	}

	flattener->current_point = *point3;

	return GraphicsSplineDecompose(&spline, flattener->tolerance);
}

static eGRAPHICS_STATUS ClosePath(void* closure)
{
	PATH_FLATTENER *flattner = (PATH_FLATTENER*)closure;

	return flattner->close_path(flattner->closure);
}

static void AddFan(
	GRAPHICS_STROKER* stroker,
	const GRAPHICS_SLOPE* in_vector,
	const GRAPHICS_SLOPE* out_vector,
	const GRAPHICS_POINT* mid_point,
	int clockwise,
	GRAPHICS_CONTOUR* contour
)
{
	GRAPHICS_PEN *pen = &stroker->pen;
	int start, stop;

	if(stroker->has_bounds && FALSE == GraphicsBoxContainsPoint(&stroker->bounds, mid_point))
	{
		return;
	}

	ASSERT(stroker->pen.num_vertices);

	if(clockwise)
	{
		GraphicsPenFindActiveCounterClockwiseVertices(pen, in_vector, out_vector, &start, &stop);
		while(start != stop)
		{
			GRAPHICS_POINT p = *mid_point;
			TranslatePoint(&p, &pen->vertices[start].point);
			GraphicsContourAddPoint(contour, &p);

			if(++start == pen->num_vertices)
			{
				start = 0;
			}
		}
	}
	else
	{
		GraphicsPenFindActiveCounterClockwiseVertices(pen, in_vector, out_vector, &start, &stop);
		while(start != stop)
		{
			GRAPHICS_POINT p = *mid_point;
			TranslatePoint(&p, &pen->vertices[start].point);
			GraphicsContourAddPoint(contour, &p);

			if(start-- == 0)
			{
				start += pen->num_vertices;
			}
		}
	}
}

static void AddCap(
	GRAPHICS_STROKER* stroker,
	const GRAPHICS_STROKE_FACE* f,
	GRAPHICS_CONTOUR* c
)
{
	switch (stroker->style.line_cap)
	{
	case GRAPHICS_LINE_CAP_ROUND:
		{
			GRAPHICS_SLOPE slope;

			slope.dx = -f->device_vector.dx;
			slope.dy = -f->device_vector.dy;

			AddFan(stroker, &f->device_vector, &slope, &f->point, FALSE, c);
			break;
		}

		case GRAPHICS_LINE_CAP_SQUARE:
		{
			GRAPHICS_SLOPE fvector;
			GRAPHICS_POINT p;
			double dx, dy;

			dx = f->user_vector.x;
			dy = f->user_vector.y;
			dx *= stroker->half_line_width;
			dy *= stroker->half_line_width;
			GraphicsMatrixTransformDistance(stroker->matrix, &dx, &dy);
			fvector.dx = GraphicsFixedFromFloat(dx);
			fvector.dy = GraphicsFixedFromFloat(dy);

			p.x = f->counter_clock_wise.x + fvector.dx;
			p.y = f->counter_clock_wise.y + fvector.dy;
			ContourAddPoint(stroker, c, &p);

			p.x = f->clock_wise.x + fvector.dx;
			p.y = f->clock_wise.y + fvector.dy;
			ContourAddPoint(stroker, c, &p);
		}

		case GRAPHICS_LINE_CAP_BUTT:
		default:
			break;
	}
	ContourAddPoint(stroker, c, &f->clock_wise);
}

static void AddLeadingCap(GRAPHICS_STROKER* stroker, const GRAPHICS_STROKE_FACE* face, GRAPHICS_CONTOUR* contour)
{
	GRAPHICS_STROKE_FACE reversed;
	GRAPHICS_POINT t;

	reversed = *face;

	reversed.user_vector.x = - reversed.user_vector.x;
	reversed.user_vector.y = - reversed.user_vector.y;
	reversed.device_vector.dx = - reversed.device_vector.dx;
	reversed.device_vector.dy = - reversed.device_vector.dy;

	t = reversed.clock_wise;
	reversed.clock_wise = reversed.counter_clock_wise;
	reversed.counter_clock_wise = t;

	AddCap(stroker, &reversed, contour);
}

static void AddTrailingCap(GRAPHICS_STROKER* stroker, const GRAPHICS_STROKE_FACE* face, GRAPHICS_CONTOUR* contour)
{
	AddCap(stroker, face, contour);
}

static INLINE FLOAT_T NormalizeSlope(FLOAT_T* dx, FLOAT_T* dy)
{
	FLOAT_T dx0 = *dx,	dy0 = *dy;
	FLOAT_T magnitude;

	if(dx0 == 0.0)
	{
		*dx = 0.0;
		if(*dy > 0.0)
		{
			magnitude = dy0;
			*dy = 1.0;
		}
		else
		{
			magnitude = -dy0;
			*dy = -1.0;
		}
	}
	else if(dy0 == 0.0)
	{
		*dy = 0.0;
		if(dx0 > 0.0)
		{
			magnitude = dx0;
			*dx = 1.0;
		}
		else
		{
			magnitude = -dx0;
			*dx = -1.0;
		}
	}
	else
	{
		magnitude = hypot(dx0, dy0);
		*dx = dx0 / magnitude;
		*dy = dy0 / magnitude;
	}

	return magnitude;
}

static void StrokePolygonCapsComputeFace(
	const GRAPHICS_POINT* point,
	const GRAPHICS_SLOPE* devide_slope,
	GRAPHICS_STROKER* stroker,
	GRAPHICS_STROKE_FACE* face
)
{
	FLOAT_T face_dx, face_dy;
	GRAPHICS_POINT offset_counter_clock_wise, offset_clock_wise;
	FLOAT_T slope_dx, slope_dy;

	slope_dx = GRAPHICS_FIXED_TO_FLOAT(devide_slope->dx);
	slope_dy = GRAPHICS_FIXED_TO_FLOAT(devide_slope->dy);
	face->length = NormalizeSlope(&slope_dx, &slope_dy);
	face->device_slope.x = slope_dx;
	face->device_slope.y = slope_dy;

	if(FALSE == GRAPHICS_MATRIX_IS_IDENTITY(stroker->matrix_inverse))
	{
		GraphicsMatrixTransformDistance(stroker->matrix_inverse, &slope_dx, &slope_dy);
		NormalizeSlope(&slope_dx, &slope_dy);

		if(stroker->matrix_determin_positive)
		{
			face_dx = - slope_dy * stroker->half_line_width;
			face_dy = slope_dx * stroker->half_line_width;
		}
		else
		{
			face_dx = slope_dy * stroker->half_line_width;
			face_dy = - slope_dx * stroker->half_line_width;
		}

		GraphicsMatrixTransformDistance(stroker->matrix, &face_dx, &face_dy);
	}
	else
	{
		face_dx = - slope_dy * stroker->half_line_width;
		face_dy = slope_dx * stroker->half_line_width;
	}

	offset_counter_clock_wise.x = GraphicsFixedFromFloat(face_dx);
	offset_counter_clock_wise.y = GraphicsFixedFromFloat(face_dy);
	offset_clock_wise.x = - offset_counter_clock_wise.x;
	offset_clock_wise.y = - offset_counter_clock_wise.y;

	face->counter_clock_wise = *point;
	TranslatePoint(&face->counter_clock_wise, &offset_counter_clock_wise);

	face->point = *point;

	face->clock_wise = *point;
	TranslatePoint(&face->clock_wise, &offset_clock_wise);

	face->user_vector.x = slope_dx;
	face->user_vector.y = slope_dy;

	face->device_vector = *devide_slope;
}

static void AddCaps(GRAPHICS_STROKER* stroker)
{
	if(stroker->has_initial_sub_path && FALSE == stroker->has_first_face
		&& FALSE == stroker->has_current_face && stroker->style.line_cap == GRAPHICS_LINE_CAP_ROUND)
	{
		GRAPHICS_SLOPE slope = {GRAPHICS_FIXED_ONE_FLOAT, 0};
		GRAPHICS_STROKE_FACE face;

		StrokePolygonCapsComputeFace(&stroker->first_point, &slope, stroker, &face);

		AddLeadingCap(stroker, &face, &stroker->counter_clock_wise);
		AddTrailingCap(stroker, &face, &stroker->counter_clock_wise);

		GraphicsContourAddPoint(&stroker->counter_clock_wise.contour,
								GraphicsContourFirstPoint(&stroker->counter_clock_wise.contour));

		GraphicsPolygonAddContour(stroker->polygon, &stroker->counter_clock_wise.contour);
		GraphicsContourReset(&stroker->counter_clock_wise.contour);
	}
	else
	{
		if(stroker->has_current_face)
		{
			AddTrailingCap(stroker, &stroker->current_face, &stroker->counter_clock_wise);
		}

		GraphicsPolygonAddContour(stroker->polygon, &stroker->counter_clock_wise.contour);
		GraphicsContourReset(&stroker->counter_clock_wise.contour);

		if (stroker->has_first_face)
		{
			GraphicsContourAddPoint(&stroker->counter_clock_wise.contour, &stroker->first_face.clock_wise);
			AddLeadingCap(stroker, &stroker->first_face, &stroker->counter_clock_wise);

			GraphicsPolygonAddContour(stroker->polygon, &stroker->counter_clock_wise.contour);
			GraphicsContourReset(&stroker->counter_clock_wise.contour);
		}
	}
}

eGRAPHICS_STATUS GraphicsPathFixedInterpretFlat(
	GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_STATUS (*move_to)(void* closure, const GRAPHICS_POINT* point),
	eGRAPHICS_STATUS (*line_to)(void* closure, const GRAPHICS_POINT* point),
	eGRAPHICS_STATUS (*close_path)(void* closure),
	void* closure,
	FLOAT_T tolerance
)
{
	PATH_FLATTENER flattener;

	if(path->has_curve_to == NULL)
	{
		GraphicsPathFixedInterpret(path, move_to, line_to, NULL, close_path, closure);
	}

	flattener.tolerance = tolerance;
	flattener.move_to = move_to;
	flattener.line_to = line_to;
	flattener.close_path = close_path;
	flattener.closure = closure;
	return GraphicsPathFixedInterpret(path, MoveTo, LineTo, CurveTo, ClosePath, &flattener);
}

void GraphicsPathFixedApproximateStrokeExtents(
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_MATRIX* ctm,
	int is_vector,
	GRAPHICS_RECTANGLE_INT* extents
)
{
	if(path->has_extents != FALSE)
	{
		GRAPHICS_BOX box_extents;
		FLOAT_T dx, dy;

		GraphicsStrokeStyleMaxDistanceFromPath(style, path, ctm, &dx, &dy);
		if(is_vector != FALSE)
		{
			FLOAT_T min = GRAPHICS_FIXED_TO_FLOAT(GRAPHICS_FIXED_EPXILON*2);
			if(dx < min)
			{
				dx = min;
			}
			if(dy < min)
			{
				dy = min;
			}
		}

		box_extents = path->extents;
		box_extents.point1.x -= GraphicsFixedFromFloat(dx);
		box_extents.point1.y -= GraphicsFixedFromFloat(dy);
		box_extents.point2.x += GraphicsFixedFromFloat(dx);
		box_extents.point2.y += GraphicsFixedFromFloat(dy);

		GraphicsBoxRoundToRectangle(&box_extents, extents);
	}
	else
	{
		extents->x = extents->y = 0;
		extents->width = extents->height = 0;
	}
}

void GraphicsPathFixedApproximateClipExtents(const GRAPHICS_PATH_FIXED* path, GRAPHICS_RECTANGLE_INT* extents)
{
	GraphicsPathFixedApproximateFillExtents(path, extents);
}

void GraphicsPathFixedFillExtents(
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rurle,
	FLOAT_T tolerane,
	GRAPHICS_RECTANGLE_INT* extents
)
{
	if(path->extents.point1.x < path->extents.point2.x
		&& path->extents.point1.y < path->extents.point2.y)
	{
		GraphicsBoxRoundToRectangle(&path->extents, extents);
	}
	else
	{
		extents->x = extents->y = 0;
		extents->width = extents->height = 0;
	}
}

void GraphicsPathFixedApproximateFillExtents(const GRAPHICS_PATH_FIXED* path, GRAPHICS_RECTANGLE_INT* extents)
{
	GraphicsPathFixedFillExtents(path, GRAPHICS_FILL_RULE_WINDING, 0, extents);
}

void GraphicsPathFixedTranslate(GRAPHICS_PATH_FIXED* path, int32 offset_fixed_x, int32 offset_fixed_y)
{
	GRAPHICS_PATH_BUFFER *buffer;
	unsigned int i;

	if(offset_fixed_x == 0 && offset_fixed_y == 0)
	{
		return;
	}

	path->last_move_point.x += offset_fixed_x;
	path->last_move_point.y += offset_fixed_y;
	path->current_point.x += offset_fixed_x;
	path->current_point.y += offset_fixed_y;

	path->fill_maybe_region = TRUE;

	buffer = GRAPHICS_PATH_HEAD(path);
	do
	{
		for(i=0; i<buffer->num_points; i++)
		{
			buffer->points[i].x += offset_fixed_x;
			buffer->points[i].y += offset_fixed_y;

			if(path->fill_maybe_region != FALSE)
			{
				path->fill_maybe_region = GRAPHICS_FIXED_IS_INTEGER(buffer->points[i].x)
						&& GRAPHICS_FIXED_IS_INTEGER(buffer->points[i].y);
			}
		}
	} while((buffer = (GRAPHICS_PATH_BUFFER*)(buffer->link.next)) != GRAPHICS_PATH_HEAD(path));

	path->fill_maybe_region &= path->fill_is_rectilinear;

	path->extents.point1.x += offset_fixed_x;
	path->extents.point1.y += offset_fixed_y;
	path->extents.point2.x += offset_fixed_x;
	path->extents.point2.y += offset_fixed_y;
}

static int GraphicsPathFixedIteratorNextOperator(GRAPHICS_PATH_FIXED_ITERATER* iter)
{
	if(++iter->num_point >= iter->buffer->num_ops)
	{
		iter->buffer = GRAPHICS_PATH_BUFFER_NEXT(iter->buffer);
		if(iter->buffer == iter->first)
		{
			iter->buffer = NULL;
			return FALSE;
		}
		iter->num_operator = 0;
		iter->num_point = 0;
	}

	return TRUE;
}

int GraphicsPathFixedIteratorIsFillBox(GRAPHICS_PATH_FIXED_ITERATER* _iter, GRAPHICS_BOX* box)
{
	GRAPHICS_POINT points[5];
	GRAPHICS_PATH_FIXED_ITERATER iter;

	if(_iter->buffer == NULL)
	{
		return FALSE;
	}

	iter = *_iter;

	if(iter.num_operator == iter.buffer->num_ops && FALSE == GraphicsPathFixedIteratorNextOperator(&iter))
	{
		return FALSE;
	}

	if(iter.buffer->op[iter.num_operator] != GRAPHICS_PATH_MOVE_TO)
	{
		return FALSE;
	}
	points[0] = iter.buffer->points[iter.num_point++];
	if(FALSE == GraphicsPathFixedIteratorNextOperator(&iter))
	{
		return FALSE;
	}

	if(iter.buffer->op[iter.num_operator] != GRAPHICS_PATH_LINE_TO)
	{
		return FALSE;
	}
	if(FALSE == GraphicsPathFixedIteratorNextOperator(&iter))
	{
		return FALSE;
	}

	switch(iter.buffer->op[iter.num_operator])
	{
	case GRAPHICS_PATH_CLOSE_PATH:
		GraphicsPathFixedIteratorNextOperator(&iter);
	case GRAPHICS_PATH_MOVE_TO:
		box->point1 = box->point2 = points[0];
		*_iter = iter;
		return TRUE;
	default:
		return FALSE;
	case GRAPHICS_PATH_LINE_TO:
		break;
	}

	points[2] = iter.buffer->points[iter.num_point++];
	if(FALSE == GraphicsPathFixedIteratorNextOperator(&iter))
	{
		return FALSE;
	}

	if(iter.buffer->op[iter.num_operator] != GRAPHICS_PATH_LINE_TO)
	{
		return FALSE;
	}
	points[3] = iter.buffer->points[iter.num_point++];

	if(FALSE == GraphicsPathFixedIteratorNextOperator(&iter))
	{

	}
	else if(iter.buffer->op[iter.num_operator] == GRAPHICS_PATH_LINE_TO)
	{
		points[4] = iter.buffer->points[iter.num_point];
		if(points[4].x != points[0].x || points[4].y != points[0].y)
		{
			return FALSE;
		}
		GraphicsPathFixedIteratorNextOperator(&iter);
	}
	else if(iter.buffer->op[iter.num_operator] == GRAPHICS_PATH_CLOSE_PATH)
	{
		GraphicsPathFixedIteratorNextOperator(&iter);
	}
	else if(iter.buffer->op[iter.num_operator] == GRAPHICS_PATH_MOVE_TO)
	{

	}
	else
	{
		return FALSE;
	}

	if(points[0].y == points[1].y
		&& points[1].x == points[2].x
		&& points[2].y == points[3].y
		&& points[3].x == points[0].x
	)
	{
		box->point1 = points[0];
		box->point2 = points[2];
		*_iter = iter;
		return TRUE;
	}

	if(points[0].x == points[1].x
		&& points[1].y == points[2].y
		&& points[2].x == points[3].x
		&& points[3].y == points[0].y
	)
	{
		box->point1 = points[1];
		box->point2 = points[3];
		*_iter = iter;
		return TRUE;
	}

	return FALSE;
}

typedef struct _GRAPHICS_FILLER
{
	GRAPHICS_POLYGON *polygon;
	FLOAT_T tolerance;

	GRAPHICS_BOX limit;
	int has_limits;

	GRAPHICS_POINT current_point;
	GRAPHICS_POINT last_move_to;
} GRAPHICS_FILLER;

typedef struct _GRAPHICS_FILLER_RECTILINEAR_ALIGNED
{
	GRAPHICS_POLYGON *polygon;

	GRAPHICS_POINT current_point;
	GRAPHICS_POINT last_move_to;
} GRAPHICS_FILLER_RECTILINEAR_ALIGNED;

static eGRAPHICS_STATUS GraphicsFillerRectilinearAlignedLineTo(void* closure, const GRAPHICS_POINT* point)
{
	GRAPHICS_FILLER_RECTILINEAR_ALIGNED *filler = (GRAPHICS_FILLER_RECTILINEAR_ALIGNED*)closure;
	eGRAPHICS_STATUS status;
	GRAPHICS_POINT p;

	p.x = GRAPHICS_FIXED_ROUND_DOWN(point->x);
	p.y = GRAPHICS_FIXED_ROUND_DOWN(point->y);

	status = GraphicsPolygonAddExternalEdge(filler->polygon, &filler->current_point, &p);

	filler->current_point = p;

	return status;
}

static eGRAPHICS_STATUS GraphicsFillerRectilinearAlignedClose(void* closure)
{
	GRAPHICS_FILLER_RECTILINEAR_ALIGNED *filler = (GRAPHICS_FILLER_RECTILINEAR_ALIGNED*)closure;
	return GraphicsFillerRectilinearAlignedLineTo(closure, &filler->last_move_to);
}

static eGRAPHICS_STATUS GraphicsFillerRectilinearAlignedMoveTo(void* closure, const GRAPHICS_POINT* point)
{
	GRAPHICS_FILLER_RECTILINEAR_ALIGNED *filler = (GRAPHICS_FILLER_RECTILINEAR_ALIGNED*)closure;
	eGRAPHICS_STATUS status;
	GRAPHICS_POINT p;

	status = GraphicsFillerRectilinearAlignedClose(closure);
	if(UNLIKELY(status))
	{
		return status;
	}

	p.x = GRAPHICS_FIXED_ROUND_DOWN(point->x);
	p.y = GRAPHICS_FIXED_ROUND_DOWN(point->y);

	filler->current_point = p;
	filler->last_move_to = p;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsFillerLineTo(void* closure, const GRAPHICS_POINT* point)
{
	GRAPHICS_FILLER *filler = (GRAPHICS_FILLER*)closure;
	eGRAPHICS_STATUS status;

	status = GraphicsPolygonAddExternalEdge(filler->polygon, &filler->current_point, point);
	filler->current_point = *point;

	return status;
}

static eGRAPHICS_STATUS GraphicsFillerClose(void* closure)
{
	GRAPHICS_FILLER *filler = (GRAPHICS_FILLER*)closure;
	return GraphicsFillerLineTo(closure, &filler->last_move_to);
}

static eGRAPHICS_STATUS GraphicsFillerMoveTo(void* closure, const GRAPHICS_POINT* point)
{
	GRAPHICS_FILLER *filler = (GRAPHICS_FILLER*)closure;
	eGRAPHICS_STATUS status;

	status = GraphicsFillerClose(closure);
	if(UNLIKELY(status))
	{
		return status;
	}

	filler->current_point = *point;
	filler->last_move_to = *point;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsFillerCurveTo(
	void* closure,
	const GRAPHICS_POINT* point1,
	const GRAPHICS_POINT* point2,
	const GRAPHICS_POINT* point3
)
{
	GRAPHICS_FILLER *filler = (GRAPHICS_FILLER*)closure;
	GRAPHICS_SPLINE spline;

	if(filler->has_limits)
	{
		if(FALSE == GraphicsSplineIntersects(&filler->current_point, point1,
						point2, point3, &filler->limit))
		{
			return GraphicsFillerLineTo(filler, point3);
		}
	}

	if(FALSE == InitializeGraphicsSpline(&spline, (GRAPHICS_SPLINE_ADD_POINT_FUNCTION)GraphicsFillerLineTo,
										 filler, &filler->current_point, point1, point2, point3))
	{
		return GraphicsFillerLineTo(closure, point3);
	}

	return GraphicsSplineDecompose(&spline, filler->tolerance);
}

eGRAPHICS_STATUS GraphicsPathFixedFillToPolygon(
	const GRAPHICS_PATH_FIXED* path,
	FLOAT_T tolerance,
	GRAPHICS_POLYGON* polygon
)
{
	GRAPHICS_FILLER filler;
	eGRAPHICS_STATUS status;

	filler.polygon = polygon;
	filler.tolerance = tolerance;

	filler.has_limits = FALSE;
	if(polygon->num_limits != 0)
	{
		filler.has_limits = TRUE;
		filler.limit = polygon->limit;
	}

	filler.current_point.x = 0;
	filler.current_point.y = 0;
	filler.last_move_to = filler.current_point;

	status = GraphicsPathFixedInterpret(path, GraphicsFillerMoveTo,
				GraphicsFillerLineTo, GraphicsFillerCurveTo, GraphicsFillerClose, &filler);
	if(UNLIKELY(status))
	{
		return status;
	}

	return GraphicsFillerClose(&filler);
}

eGRAPHICS_STATUS GraphicsPathFixedFillRectilinearToPolygon(
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_ANTIALIAS antialias,
	GRAPHICS_POLYGON* polygon
)
{
	GRAPHICS_FILLER_RECTILINEAR_ALIGNED filler;
	eGRAPHICS_STATUS status;

	if(antialias != GRAPHICS_ANTIALIAS_NONE)
	{
		return GraphicsPathFixedFillToPolygon(path, 0.0, polygon);
	}

	filler.polygon = polygon;

	filler.current_point.x = 0;
	filler.current_point.y = 0;
	filler.last_move_to = filler.current_point;

	status = GraphicsPathFixedInterpretFlat(path, GraphicsFillerRectilinearAlignedMoveTo,
				GraphicsFillerRectilinearAlignedLineTo, GraphicsFillerRectilinearAlignedClose, &filler, 0.0);
	if(UNLIKELY(status))
	{
		return status;
	}

	return GraphicsFillerRectilinearAlignedClose(&filler);
}

static eGRAPHICS_STATUS GraphicsPathFixedFillRectilinearTessellateToBoxes(
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rule,
	eGRAPHICS_ANTIALIAS antialias,
	GRAPHICS_BOXES* boxes
)
{
	GRAPHICS_POLYGON polygon;
	eGRAPHICS_STATUS status;

	InitializeGraphicsPolygon(&polygon, boxes->limits, boxes->num_limits);
	boxes->num_limits = 0;

	status = GraphicsPathFixedFillRectilinearToPolygon(path, antialias, &polygon);
	if(LIKELY(status == GRAPHICS_STATUS_SUCCESS))
	{
		GraphicsBentleyOttmannTessellateRectilinearPolygonTolBoxes(&polygon, fill_rule, boxes);
	}

	GraphicsPolygonFinish(&polygon);

	return status;
}

void InitializeGraphicsPathFixed(GRAPHICS_PATH_FIXED* path)
{
	InitializeGraphicsList(&path->buffer.base.link);

	path->buffer.base.num_ops = 0;
	path->buffer.base.num_points = 0;
	path->buffer.base.size_ops = sizeof(path->buffer.op) / sizeof(path->buffer.op[0]);
	path->buffer.base.size_points = sizeof(path->buffer.points) / sizeof(path->buffer.points[0]);
	path->buffer.base.op = path->buffer.op;
	path->buffer.base.points = path->buffer.points;

	path->current_point.x = 0;
	path->current_point.y = 0;
	path->last_move_point = path->current_point;

	path->has_current_point = FALSE;
	path->need_move_to = TRUE;
	path->has_extents = FALSE;
	path->has_curve_to = FALSE;
	path->stroke_is_rectilinear = TRUE;
	path->fill_is_rectilinear = TRUE;
	path->fill_maybe_region = TRUE;
	path->fill_is_empty = TRUE;

	path->extents.point1.x = path->extents.point1.y = 0;
	path->extents.point2.x = path->extents.point2.y = 0;
}

static void GraphicsPathFixedAddBuffer(GRAPHICS_PATH_FIXED* path, GRAPHICS_PATH_BUFFER* buffer)
{
	GraphicsListAddTail(&buffer->link, &(GRAPHICS_PATH_HEAD(path))->link);
}

eGRAPHICS_STATUS InitializeGraphicsPathFixedCopy(GRAPHICS_PATH_FIXED* path, const GRAPHICS_PATH_FIXED* other)
{
	GRAPHICS_PATH_BUFFER *buffer, *other_buffer;
	unsigned int num_points, num_ops;

	InitializeGraphicsList(&path->buffer.base.link);

	path->buffer.base.op = path->buffer.op;
	path->buffer.base.points = path->buffer.points;
	path->buffer.base.size_ops = sizeof(path->buffer.op) / sizeof(path->buffer.op[0]);
	path->buffer.base.size_points = sizeof(path->buffer.points) / sizeof(path->buffer.points[0]);

	path->current_point = other->current_point;
	path->need_move_to = other->need_move_to;
	path->has_extents = other->has_extents;
	path->has_curve_to = other->has_curve_to;
	path->stroke_is_rectilinear = other->stroke_is_rectilinear;
	path->fill_is_rectilinear = other->fill_is_rectilinear;
	path->fill_maybe_region = other->fill_maybe_region;
	path->fill_is_empty = other->fill_is_empty;

	path->extents = other->extents;

	path->buffer.base.num_ops = other->buffer.base.num_ops;
	path->buffer.base.num_points = other->buffer.base.num_points;
	(void)memcpy(path->buffer.op, other->buffer.base.op,
				 other->buffer.base.num_ops * sizeof(other->buffer.op[0]));
	(void)memcpy(path->buffer.points, other->buffer.points,
				 other->buffer.base.num_points * sizeof(other->buffer.points[0]));

	num_points = num_ops = 0;
	for(other_buffer = GRAPHICS_PATH_BUFFER_NEXT(GRAPHICS_PATH_HEAD(other));
		other_buffer != GRAPHICS_PATH_HEAD(other); other_buffer = GRAPHICS_PATH_BUFFER_NEXT(other_buffer))
	{
		num_ops += other_buffer->num_ops;
		num_points += other_buffer->num_points;
	}

	if(num_ops)
	{
		buffer = CreateGraphicsPathBuffer(num_ops, num_points);
		if(buffer == NULL)
		{
			GraphicsPathFixedFinish(path);
			return GRAPHICS_STATUS_NO_MEMORY;
		}

		for(other_buffer = GRAPHICS_PATH_BUFFER_NEXT(GRAPHICS_PATH_HEAD(other));
			  other_buffer != GRAPHICS_PATH_HEAD(other); other_buffer = GRAPHICS_PATH_BUFFER_NEXT(other_buffer))
		{
			(void)memcpy(buffer->op + buffer->num_ops, other_buffer->op,
						 other_buffer->num_ops * sizeof(buffer->op[0]));
			buffer->num_ops += other_buffer->num_ops;

			(void)memcpy(buffer->points + buffer->num_points, other_buffer->points,
						 other_buffer->num_points * sizeof(buffer->points[0]));
			buffer->num_points += other_buffer->num_points;
		}

		GraphicsPathFixedAddBuffer(path, buffer);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

void InitializeGraphicsPathFixedIterator(GRAPHICS_PATH_FIXED_ITERATER* iter, const GRAPHICS_PATH_FIXED* path)
{
	iter->first = iter->buffer = GRAPHICS_PATH_HEAD(path);
	iter->num_operator = 0;
	iter->num_point = 0;
}

static void GraphicsPathBufferAddOperate(GRAPHICS_PATH_BUFFER* buffer, GRAPHICS_PATH_OPERATOR op)
{
	buffer->op[buffer->num_ops++] = op;
}

static void GraphicsPathBufferAddPoints(
	GRAPHICS_PATH_BUFFER* buffer,
	const GRAPHICS_POINT* points,
	int num_points
)
{
	if(num_points == 0)
	{
		return;
	}

	(void)memcpy(buffer->points + buffer->num_points, points, sizeof(points[0]) * num_points);
	buffer->num_points += num_points;
}

static eGRAPHICS_STATUS GraphicsPathFixedAdd(
	GRAPHICS_PATH_FIXED* path,
	char op,
	const GRAPHICS_POINT* points,
	int num_points
)
{
	GRAPHICS_PATH_BUFFER *buffer = (GRAPHICS_PATH_BUFFER*)path->buffer.base.link.prev;

	if(buffer->num_ops + 1 > buffer->size_ops ||
		buffer->num_points + num_points > buffer->size_points)
	{
		buffer = CreateGraphicsPathBuffer(buffer->num_ops * 2, buffer->num_points * 2);
		if(UNLIKELY(buffer == NULL))
		{
			return GRAPHICS_STATUS_NO_MEMORY;
		}
#ifdef _DEBUG
		InitializeGraphicsList(&buffer->link);
#endif

		GraphicsPathFixedAddBuffer(path, buffer);
	}

	GraphicsPathBufferAddOperate(buffer, op);
	GraphicsPathBufferAddPoints(buffer, points, num_points);

	return GRAPHICS_STATUS_SUCCESS;
}

void GraphicsPathFixedNewSubPath(GRAPHICS_PATH_FIXED* path)
{
	if(FALSE == path->need_move_to)
	{
		if(path->fill_is_rectilinear)
		{
			path->fill_is_rectilinear = path->current_point.x == path->last_move_point.x
					|| path->current_point.y == path->last_move_point.y;
			path->fill_maybe_region &= path->fill_is_rectilinear;
		}
		path->need_move_to = TRUE;
	}

	path->has_current_point = FALSE;
}

eGRAPHICS_STATUS GraphicsPathFixedMoveTo(GRAPHICS_PATH_FIXED* path, GRAPHICS_FLOAT_FIXED x, GRAPHICS_FLOAT_FIXED y)
{
	GraphicsPathFixedNewSubPath(path);

	path->has_current_point = TRUE;
	path->current_point.x = x;
	path->current_point.y = y;
	path->last_move_point = path->current_point;

	return GRAPHICS_STATUS_SUCCESS;
}

eGRAPHICS_STATUS GraphicsPathFixedRelativeMoveTo(GRAPHICS_PATH_FIXED* path, GRAPHICS_FLOAT_FIXED dx, GRAPHICS_FLOAT_FIXED dy)
{
	if(UNLIKELY(FALSE == path->has_current_point))
	{
		return GRAPHICS_STATUS_NO_CURRENT_POINT;
	}

	return GraphicsPathFixedMoveTo(path, path->current_point.x + dx, path->current_point.y + dy);
}

static eGRAPHICS_STATUS GraphicsPathFixedMoveToApply(GRAPHICS_PATH_FIXED* path)
{
	if(LIKELY(FALSE == path->need_move_to))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}
	
	path->need_move_to = FALSE;
	
	if(path->has_extents)
	{
		GraphicsBoxAddPoint(&path->extents, &path->current_point);
	}
	else
	{
		GraphicsBoxSet(&path->extents, &path->current_point, &path->current_point);
		path->has_extents = TRUE;
	}
	
	path->last_move_point = path->current_point;
	
	return GraphicsPathFixedAdd(path, GRAPHICS_PATH_MOVE_TO, &path->current_point, 1);
}

static GRAPHICS_PATH_OPERATOR GraphicsPathFixedLastOperator(GRAPHICS_PATH_FIXED* path)
{
	GRAPHICS_PATH_BUFFER *buffer;

	buffer = (GRAPHICS_PATH_BUFFER*)path->buffer.base.link.prev;

	return buffer->op[buffer->num_ops - 1];
}

static INLINE const GRAPHICS_POINT* GraphicsPathFixedPenultimatePoint(GRAPHICS_PATH_FIXED* path)
{
	GRAPHICS_PATH_BUFFER *buffer;
	GRAPHICS_PATH_BUFFER *previous;

	buffer = (GRAPHICS_PATH_BUFFER*)path->buffer.base.link.prev;
	if(LIKELY(buffer->num_points >= 2))
	{
		return &buffer->points[buffer->num_points - 2];
	}

	previous = (GRAPHICS_PATH_BUFFER*)(buffer->link.prev);

	return &previous->points[previous->num_ops - (2 - buffer->num_points)];
}

static void GraphicsPathFixedDropLineTo(GRAPHICS_PATH_FIXED* path)
{
	GRAPHICS_PATH_BUFFER *buffer;

	buffer = (GRAPHICS_PATH_BUFFER*)path->buffer.base.link.prev;
	buffer->num_points--;
	buffer->num_ops--;
}

eGRAPHICS_STATUS GraphicsPathFixedLineTo(GRAPHICS_PATH_FIXED* path, GRAPHICS_FLOAT_FIXED x, GRAPHICS_FLOAT_FIXED y)
{
	eGRAPHICS_STATUS status;
	GRAPHICS_POINT point;
	
	if(FALSE == path->has_current_point)
	{
		return GraphicsPathFixedMoveTo(path, x, y);
	}
	
	point.x = x;
	point.y = y;
	
	if(path->has_current_point == FALSE)
	{
		return GraphicsPathFixedMoveTo(path, point.x, point.y);
	}

	status = GraphicsPathFixedMoveToApply(path);
	if(UNLIKELY(status))
	{
		return status;
	}

	if(GraphicsPathFixedLastOperator(path) != GRAPHICS_PATH_MOVE_TO)
	{
		if(x == path->current_point.x && y == path->current_point.y)
		{
			return GRAPHICS_STATUS_SUCCESS;
		}
	}

	if(GraphicsPathFixedLastOperator(path) == GRAPHICS_PATH_LINE_TO)
	{
		const GRAPHICS_POINT *p;

		p = GraphicsPathFixedPenultimatePoint(path);
		if(p->x == path->current_point.x && p->y == path->current_point.y)
		{
			GraphicsPathFixedDropLineTo(path);
		}
		else
		{
			GRAPHICS_SLOPE previous, self;

			INITIALIZE_GRAPHICS_SLOPE(&previous, p, &path->current_point);
			INITIALIZE_GRAPHICS_SLOPE(&self, &path->current_point, &point);
			if(GraphicsSlopoeEqual(&previous, &self) && FALSE == GraphicsSlopeBackwards(&previous, &self))
			{
				GraphicsPathFixedDropLineTo(path);
			}
		}
	}

	if(path->stroke_is_rectilinear)
	{
		path->stroke_is_rectilinear = path->current_point.x == x
			|| path->current_point.y == y;
		path->fill_is_rectilinear &= path->stroke_is_rectilinear;
		path->fill_maybe_region &= path->fill_is_rectilinear;
		if(path->fill_maybe_region)
		{
			path->fill_maybe_region = GRAPHICS_FIXED_IS_INTEGER(x)
					&& GRAPHICS_FIXED_IS_INTEGER(y);
		}
		if(path->fill_is_empty)
		{
			path->fill_is_empty = path->current_point.x == x
					&& path->current_point.y == y;
		}
	}

	path->current_point = point;

	GraphicsBoxAddPoint(&path->extents, &point);

	return GraphicsPathFixedAdd(path, GRAPHICS_PATH_LINE_TO, &point, 1);
}

eGRAPHICS_STATUS GraphicsPathFixedRelativeLineTo(GRAPHICS_PATH_FIXED* path, GRAPHICS_FLOAT_FIXED dx, GRAPHICS_FLOAT_FIXED dy)
{
	if(UNLIKELY(FALSE == path->has_current_point))
	{
		return GRAPHICS_STATUS_NO_CURRENT_POINT;
	}

	return GraphicsPathFixedLineTo(path, path->current_point.x + dx, path->current_point.y + dy);
}

eGRAPHICS_STATUS GraphicsSplineBound(
	GRAPHICS_SPLINE_ADD_POINT_FUNCTION add_point_function,
	void* closure,
	const GRAPHICS_POINT* point0,
	const GRAPHICS_POINT* point1,
	const GRAPHICS_POINT* point2,
	const GRAPHICS_POINT* point3
)
{
	FLOAT_T x0, x1, x2, x3;
	FLOAT_T y0, y1, y2, y3;
	FLOAT_T a, b, c;
	FLOAT_T t[4];
	int t_num = 0, i;
	eGRAPHICS_STATUS status;

	x0 = GRAPHICS_FIXED_TO_FLOAT(point0->x);
	y0 = GRAPHICS_FIXED_TO_FLOAT(point0->y);
	x1 = GRAPHICS_FIXED_TO_FLOAT(point1->x);
	y1 = GRAPHICS_FIXED_TO_FLOAT(point1->y);
	x2 = GRAPHICS_FIXED_TO_FLOAT(point2->x);
	y2 = GRAPHICS_FIXED_TO_FLOAT(point2->y);
	x3 = GRAPHICS_FIXED_TO_FLOAT(point3->x);
	y3 = GRAPHICS_FIXED_TO_FLOAT(point3->y);

#define ADD(t0) \
	{ \
		FLOAT_T _t0 = (t0); \
		if (0 < _t0 && _t0 < 1) \
			t[t_num++] = _t0; \
	}

#define FIND_EXTREMES(a,b,c) \
	{ \
		if (a == 0) { \
			if (b != 0) \
				ADD (-c / (2*b)); \
		} else { \
			FLOAT_T b2 = b * b; \
			FLOAT_T delta = b2 - a * c; \
			if (delta > 0) { \
			int feasible; \
			FLOAT_T _2ab = 2 * a * b; \
			\
			if (_2ab >= 0) \
				feasible = delta > b2 && delta < a*a + b2 + _2ab; \
			else if (-b / a >= 1) \
				feasible = delta < b2 && delta > a*a + b2 + _2ab; \
			else \
				feasible = delta < b2 || delta < a*a + b2 + _2ab; \
				\
			if (UNLIKELY(feasible)) { \
				FLOAT_T sqrt_delta = sqrt (delta); \
				ADD ((-b - sqrt_delta) / a); \
				ADD ((-b + sqrt_delta) / a); \
			} \
			} else if (delta == 0) { \
			ADD (-b / a); \
			} \
		} \
		}

	/* Find X extremes */
	a = -x0 + 3*x1 - 3*x2 + x3;
	b =  x0 - 2*x1 + x2;
	c = -x0 + x1;
	FIND_EXTREMES (a, b, c);

	/* Find Y extremes */
	a = -y0 + 3*y1 - 3*y2 + y3;
	b =  y0 - 2*y1 + y2;
	c = -y0 + y1;
	FIND_EXTREMES (a, b, c);

	status = add_point_function (closure, point0, NULL);
	if (UNLIKELY(status))
	{
		return status;
	}

	for (i = 0; i < t_num; i++)
	{
		GRAPHICS_POINT p;
		FLOAT_T x, y;
		FLOAT_T t_1_0, t_0_1;
		FLOAT_T t_2_0, t_0_2;
		FLOAT_T t_3_0, t_2_1_3, t_1_2_3, t_0_3;

		t_1_0 = t[i];		  /*	  t  */
		t_0_1 = 1 - t_1_0;	 /* (1 - t) */

		t_2_0 = t_1_0 * t_1_0; /*	  t  *	  t  */
		t_0_2 = t_0_1 * t_0_1; /* (1 - t) * (1 - t) */

		t_3_0   = t_2_0 * t_1_0;	 /*	  t  *	  t  *	  t	  */
		t_2_1_3 = t_2_0 * t_0_1 * 3; /*	  t  *	  t  * (1 - t) * 3 */
		t_1_2_3 = t_1_0 * t_0_2 * 3; /*	  t  * (1 - t) * (1 - t) * 3 */
		t_0_3   = t_0_1 * t_0_2;	 /* (1 - t) * (1 - t) * (1 - t)	 */

		x = x0 * t_0_3
			+ x1 * t_1_2_3
			+ x2 * t_2_1_3
			+ x3 * t_3_0;
		y = y0 * t_0_3
			  + y1 * t_1_2_3
			  + y2 * t_2_1_3
			  + y3 * t_3_0;

		p.x = GraphicsFixedFromFloat(x);
		p.y = GraphicsFixedFromFloat(y);
		status = add_point_function(closure, &p, NULL);
		if (UNLIKELY(status))
		{
			return status;
		}
	}

	return add_point_function(closure, point3, NULL);
}

int GraphicsSplineIntersects(
	const GRAPHICS_POINT* a,
	const GRAPHICS_POINT* b,
	const GRAPHICS_POINT* c,
	const GRAPHICS_POINT* d,
	const GRAPHICS_BOX* box
)
{
	GRAPHICS_BOX bounds;

	if(GraphicsBoxContainsPoint(box, a)
		|| GraphicsBoxContainsPoint(box, b)
		|| GraphicsBoxContainsPoint(box, c)
		|| GraphicsBoxContainsPoint(box, d)
	)
	{
		return TRUE;
	}

	bounds.point2 = bounds.point1 = *a;
	GraphicsBoxAddPoint(&bounds, b);
	GraphicsBoxAddPoint(&bounds, c);
	GraphicsBoxAddPoint(&bounds, d);

	if(bounds.point2.x <= box->point1.x || bounds.point1.x >= box->point2.x
			|| bounds.point2.y <= box->point1.y || bounds.point1.y >= box->point2.y)
	{
		return FALSE;
	}

	return TRUE;
}

/*
eGRAPHICS_STATUS GraphicsPathFixedRelativeLineTo(GRAPHICS_PATH_FIXED* path, GRAPHICS_FLOAT_FIXED dx, GRAPHICS_FLOAT_FIXED dy)
{
	if(UNLIKELY(!path->has_current_point))
	{
		return GRAPHICS_STATUS_NO_CURRENT_POINT;
	}

	return GraphicsPathFixedLineTo(path,
		path->current_point.x + dx, path->current_point.y + dy);
}
*/

eGRAPHICS_STATUS GraphicsPathFixedCurveTo(
	GRAPHICS_PATH_FIXED* path,
	GRAPHICS_FLOAT_FIXED x0,
	GRAPHICS_FLOAT_FIXED y0,
	GRAPHICS_FLOAT_FIXED x1,
	GRAPHICS_FLOAT_FIXED y1,
	GRAPHICS_FLOAT_FIXED x2,
	GRAPHICS_FLOAT_FIXED y2
)
{
	eGRAPHICS_STATUS status;
	GRAPHICS_POINT point[3];

	if(path->current_point.x == x2 && path->current_point.y == y2)
	{
		if(x1 == x2 && x0 == x2 && y1 == y2 && y0 == y2)
		{
			return GraphicsPathFixedLineTo(path, x2, y2);
		}
	}

	if(FALSE == path->has_current_point)
	{
		status = GraphicsPathFixedMoveTo(path, x0, y0);
		ASSERT(status == GRAPHICS_STATUS_SUCCESS);
	}

	status = GraphicsPathFixedMoveToApply(path);
	if(UNLIKELY(status))
	{
		return status;
	}

	if(GraphicsPathFixedLastOperator(path) == GRAPHICS_PATH_LINE_TO)
	{
		const GRAPHICS_POINT *p;

		p = GraphicsPathFixedPenultimatePoint(path);
		if(p->x == path->current_point.x && p->y == path->current_point.y)
		{
			GraphicsPathFixedDropLineTo(path);
		}
	}

	point[0].x = x0;	point[0].y = y0;
	point[1].x = x1;	point[1].y = y1;
	point[2].x = x2;	point[2].y = y2;

	GraphicsBoxAddCurveTo(&path->extents, &path->current_point,
		&point[0], &point[1], &point[2]);

	path->current_point = point[2];
	path->has_curve_to = TRUE;
	path->stroke_is_rectilinear = FALSE;
	path->fill_is_rectilinear = FALSE;
	path->fill_maybe_region = FALSE;
	path->fill_is_empty = FALSE;

	return GraphicsPathFixedAdd(path, GRAPHICS_PATH_CURVE_TO, point, 3);
}

eGRAPHICS_STATUS GraphicsPathFixedRelativeCurveTo(
	GRAPHICS_PATH_FIXED* path,
	GRAPHICS_FLOAT_FIXED dx0, GRAPHICS_FLOAT_FIXED dy0,
	GRAPHICS_FLOAT_FIXED dx1, GRAPHICS_FLOAT_FIXED dy1,
	GRAPHICS_FLOAT_FIXED dx2, GRAPHICS_FLOAT_FIXED dy2
)
{
	if(UNLIKELY(FALSE == path->has_current_point))
	{
		return GRAPHICS_STATUS_NO_CURRENT_POINT;
	}

	return GraphicsPathFixedCurveTo(path,
		path->current_point.x + dx0, path->current_point.y + dy0,
		path->current_point.x + dx1, path->current_point.y + dy1,
		path->current_point.x + dx2, path->current_point.y + dy2
	);
}

eGRAPHICS_STATUS GraphicsPathFixedClosePath(GRAPHICS_PATH_FIXED* path)
{
	eGRAPHICS_STATUS status;

	if(FALSE == path->has_current_point)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	status = GraphicsPathFixedLineTo(path, path->last_move_point.x, path->last_move_point.y);
	if(UNLIKELY(status))
	{
		return status;
	}

	if(GraphicsPathFixedLastOperator(path) == GRAPHICS_PATH_LINE_TO)
	{
		GraphicsPathFixedDropLineTo(path);
	}

	path->need_move_to = TRUE;

	return GraphicsPathFixedAdd(path, GRAPHICS_PATH_CLOSE_PATH, NULL, 0);
}

int GraphicsPathFixedExtents(const GRAPHICS_PATH_FIXED* path, GRAPHICS_BOX* box)
{
	*box = path->extents;
	return path->has_extents;
}

int GraphicsPathFixedGetCurrentPoint(
	GRAPHICS_PATH_FIXED* path,
	GRAPHICS_FLOAT_FIXED* x,
	GRAPHICS_FLOAT_FIXED* y
)
{
	if(FALSE == path->has_current_point)
	{
		return FALSE;
	}

	*x = path->current_point.x;
	*y = path->current_point.y;

	return TRUE;
}

static FLOAT_T ArcErrorNormalized(FLOAT_T angle)
{
	return 2.0/27.0 * pow (sin (angle / 4), 6) / pow (cos (angle / 4), 2);
}

static FLOAT_T ArcMaximumAngleForToleranceNormalized(FLOAT_T tolerance)
{
	FLOAT_T angle, error;
	int i;

	struct
	{
		double angle;
		double error;
	} table[] =
	{
		{ M_PI / 1.0,   0.0185185185185185036127 },
		{ M_PI / 2.0,   0.000272567143730179811158 },
		{ M_PI / 3.0,   2.38647043651461047433e-05 },
		{ M_PI / 4.0,   4.2455377443222443279e-06 },
		{ M_PI / 5.0,   1.11281001494389081528e-06 },
		{ M_PI / 6.0,   3.72662000942734705475e-07 },
		{ M_PI / 7.0,   1.47783685574284411325e-07 },
		{ M_PI / 8.0,   6.63240432022601149057e-08 },
		{ M_PI / 9.0,   3.2715520137536980553e-08 },
		{ M_PI / 10.0,  1.73863223499021216974e-08 },
		{ M_PI / 11.0,  9.81410988043554039085e-09 }
	};
	const int table_size = sizeof(table) / sizeof(table[0]);

	for(i = 0; i < table_size; i++)
	{
		if(table[i].error < tolerance)
		{
			return table[i].angle;
		}
	}

	++i;
	do
	{
		angle = M_PI / i++;
		error = ArcErrorNormalized(angle);
	} while(error > tolerance);

	return angle;
}

static int ArcSegmentsNeeded(FLOAT_T angle, FLOAT_T radius, GRAPHICS_MATRIX* matrix, FLOAT_T tolerance)
{
	FLOAT_T major_axis, max_angle;

	major_axis = GraphicsMatrixTransformedCircleMajorAxis(matrix, radius);
	max_angle = ArcMaximumAngleForToleranceNormalized(tolerance / major_axis);

	return ceil(fabs(angle) / max_angle);
}

static void GraphicsArcSegment(
	GRAPHICS_CONTEXT* context,
	FLOAT_T xc,
	FLOAT_T yc,
	FLOAT_T radius,
	FLOAT_T angle_a,
	FLOAT_T angle_b
)
{
	FLOAT_T r_sin_a, r_cos_a;
	FLOAT_T r_sin_b, r_cos_b;
	FLOAT_T h;

	r_sin_a = radius * sin(angle_a);
	r_cos_a = radius * cos(angle_a);
	r_sin_b = radius * sin(angle_b);
	r_cos_b = radius * cos(angle_b);

	h = 4.0/3.0 * tan((angle_b - angle_a) / 4.0);

	GraphicsCurveTo(context,
		xc + r_cos_a - h * r_sin_a,
		yc + r_sin_a + h * r_cos_a,
		xc + r_cos_b + h * r_sin_b,
		yc + r_sin_b - h * r_cos_b,
		xc + r_cos_b,
		yc + r_sin_b
	);
}

static void GraphicsArcInDirection(
	GRAPHICS_CONTEXT* context,
	FLOAT_T xc,
	FLOAT_T yc,
	FLOAT_T radius,
	FLOAT_T angle_min,
	FLOAT_T angle_max,
	eGRAPHICS_DIRECTION direction
)
{
	if(context->status != GRAPHICS_INTEGER_STATUS_SUCCESS)
	{
		return;
	}

	if(angle_max - angle_min > 2 * M_PI * MAX_FULL_CIRCLES)
	{
		angle_max = fmod(angle_max - angle_min, 2 * M_PI);
		angle_min = fmod(angle_min, 2 * M_PI);
		angle_max += angle_min + 2 * M_PI * MAX_FULL_CIRCLES;
	}

	if(angle_max - angle_min > M_PI)
	{
		FLOAT_T angle_mid = angle_min + (angle_max - angle_min) / 2.0;
		if(direction == GRAPHICS_DIRECTION_FORWARD)
		{
			GraphicsArcInDirection(context, xc, yc, radius,
				angle_min, angle_mid, direction);

			GraphicsArcInDirection(context, xc, yc, radius,
				angle_mid, angle_max, direction);
		}
		else
		{
			GraphicsArcInDirection(context, xc, yc, radius,
				angle_mid, angle_max, direction);

			GraphicsArcInDirection(context, xc, yc, radius,
				angle_min, angle_mid, direction);
		}
	}
	else if(angle_max != angle_min)
	{
		GRAPHICS_MATRIX matrix;
		int i, segments;
		FLOAT_T step;

		GraphicsGetMatrix(context, &matrix);
		segments = ArcSegmentsNeeded(angle_max - angle_min,
			radius, &matrix, GraphicsGetTolerance(context));
		step = (angle_max - angle_min) / segments;
		segments -= 1;

		if(direction == GRAPHICS_DIRECTION_REVERSE)
		{
			FLOAT_T t;

			t = angle_min;
			angle_min = angle_max;
			angle_max = t;

			step = - step;
		}

		GraphicsLineTo(context, xc + radius * cos(angle_min),
			yc + radius * sin(angle_min));

		for(i=0; i<segments; i++, angle_min += step)
		{
			GraphicsArcSegment(context, xc, yc, radius, angle_min, angle_min + step);
		}
		GraphicsArcSegment(context, xc, yc, radius, angle_min, angle_max);
	}
	else
	{
		GraphicsLineTo(context, xc + radius * cos(angle_min),
			yc + radius * sin(angle_min));
	}
}

void GraphicsArcPath(
	GRAPHICS_CONTEXT* context,
	FLOAT_T xc,
	FLOAT_T yc,
	FLOAT_T radius,
	FLOAT_T angle1,
	FLOAT_T angle2
)
{
	GraphicsArcInDirection(context, xc, yc, radius, angle1, angle2, GRAPHICS_DIRECTION_FORWARD);
}

void GraphicsArcPathNegative(
	GRAPHICS_CONTEXT* context,
	FLOAT_T xc,
	FLOAT_T yc,
	FLOAT_T radius,
	FLOAT_T angle1,
	FLOAT_T angle2
)
{
	GraphicsArcInDirection(context, xc, yc, radius, angle1, angle2, GRAPHICS_DIRECTION_REVERSE);
}

eGRAPHICS_STATUS GraphicsPathAppendToContext(const struct _GRAPHICS_PATH* path, struct _GRAPHICS_CONTEXT* context)
{
	const GRAPHICS_PATH_DATA *point, *end;

	end = &path->data[path->num_data];
	for(point = &path->data[0]; point < end; point += point->header.length)
	{
		switch(point->header.type)
		{
		case GRAPHICS_PATH_MOVE_TO:
			if(UNLIKELY(point->header.length < 2))
			{
				return GRAPHICS_STATUS_INVALID_PATH_DATA;
			}

			GraphicsMoveTo(context, point[1].point.x, point[1].point.y);
			break;
		case GRAPHICS_PATH_LINE_TO:
			if(UNLIKELY(point->header.length < 2))
			{
				return GRAPHICS_STATUS_INVALID_PATH_DATA;
			}

			GraphicsLineTo(context, point[1].point.x, point[1].point.y);
			break;
		case GRAPHICS_PATH_CURVE_TO:
			if(UNLIKELY(point->header.length < 4))
			{
				return GRAPHICS_STATUS_INVALID_PATH_DATA;
			}

			GraphicsCurveTo(context, point[1].point.x, point[1].point.y,
				point[2].point.x, point[2].point.y, point[3].point.x, point[3].point.y);
			break;
		case GRAPHICS_PATH_CLOSE_PATH:
			if(UNLIKELY(point->header.length < 1))
			{
				return GRAPHICS_STATUS_INVALID_PATH_DATA;
			}
			GraphicsClosePath(context);
			break;
		default:
			return GRAPHICS_STATUS_INVALID_PATH_DATA;
		}
	}

	if(UNLIKELY(context->status))
	{
		return context->status;
	}

	return GRAPHICS_STATUS_SUCCESS;
}

void GraphicsStrokerDashStart(GRAPHICS_STROKER_DASH* dash)
{
	FLOAT_T offset;
	int on = TRUE;
	unsigned int i = 0;

	if(dash->dashed == FALSE)
	{
		return;
	}

	offset = dash->dash_offset;

	while(offset > 0.0 && offset > dash->dashes[i])
	{
		offset -= dash->dashes[i];
		on = !on;
		if(++i == dash->num_dashes)
		{
			i = 0;
		}
	}

	dash->dash_index = i;
	dash->dash_on = dash->dash_starts_on = on;
	dash->dash_remain = dash->dashes[i] - offset;
}

void InitializeGraphicsStrokerDash(GRAPHICS_STROKER_DASH* dash, const GRAPHICS_STROKE_STYLE* style)
{
	dash->dashed = style->dash != NULL;
	if(dash->dashed == FALSE)
	{
		return;
	}

	dash->dashes = style->dash;
	dash->num_dashes = style->num_dashes;
	dash->dash_offset = style->dash_offset;

	GraphicsStrokerDashStart(dash);
}

static void GraphicsStrokerLimit(
	GRAPHICS_STROKER* stroker,
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_BOX* boxes,
	int num_boxes
)
{
	FLOAT_T dx, dy;
	GRAPHICS_FLOAT_FIXED fdx, fdy;

	stroker->has_bounds = TRUE;
	GraphicsBoxesGetExtents(boxes, num_boxes, &stroker->bounds);

	GraphicsStrokeStyleMaxDistanceFromPath(&stroker->style, path, stroker->matrix, &dx, &dy);

	fdx = GraphicsFixedFromFloat(dx);
	fdy = GraphicsFixedFromFloat(dy);

	stroker->bounds.point1.x -= fdx;
	stroker->bounds.point2.x += fdx;

	stroker->bounds.point1.y -= fdy;
	stroker->bounds.point2.y += fdy;
}

static eGRAPHICS_STATUS InitializeGraphicsStroker(
	GRAPHICS_STROKER* stroker,
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* stroke_style,
	const GRAPHICS_MATRIX* matrix,
	const GRAPHICS_MATRIX* matrix_inverse,
	FLOAT_T tolerance,
	const GRAPHICS_BOX* limits,
	int num_limits
)
{
	eGRAPHICS_STATUS status;

	stroker->style = *stroke_style;
	stroker->matrix = matrix;
	stroker->matrix_inverse = matrix_inverse;
	stroker->tolerance = tolerance;
	stroker->half_line_width = stroke_style->line_width / 2.0;

	stroker->spline_cusp_tolerance = 1 - tolerance / stroker->half_line_width;
	stroker->spline_cusp_tolerance *= stroker->spline_cusp_tolerance;
	stroker->spline_cusp_tolerance *= 2;
	stroker->spline_cusp_tolerance -= 1;

	stroker->matrix_determinant = GRAPHICS_MATRIX_COMPUTE_DETERMINANT(stroker->matrix);
	stroker->matrix_determin_positive = stroker->matrix_determinant >= 0.0;

	status = InitializeGraphicsPen(&stroker->pen, stroker->half_line_width, tolerance, matrix);
	if(UNLIKELY(status))
	{
		return status;
	}

	stroker->has_current_face = FALSE;
	stroker->has_first_face = FALSE;
	stroker->has_initial_sub_path = FALSE;

	InitializeGraphicsStrokerDash(&stroker->dash, stroke_style);

	stroker->add_external_edge = NULL;

	stroker->has_bounds = FALSE;
	if(num_limits != 0)
	{
		GraphicsStrokerLimit(stroker, path, limits, num_limits);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

int GraphicsPolygonGrow(GRAPHICS_POLYGON* polygon)
{
	GRAPHICS_EDGE *new_edges;
	int old_size = polygon->edges_size;
	int new_size = 4 * old_size;

	if(polygon->edges == polygon->edges_embedded)
	{
		new_edges = (GRAPHICS_EDGE*)MEM_ALLOC_FUNC(sizeof(*new_edges) * new_size);
		if(new_edges != NULL)
		{
			(void)memcpy(new_edges, polygon->edges, old_size * sizeof(*new_edges));
		}
	}
	else
	{
		new_edges = MEM_REALLOC_FUNC(polygon->edges, sizeof(*new_edges) * new_size);
	}

	if(UNLIKELY(new_edges == NULL))
	{
		polygon->status = GRAPHICS_STATUS_NO_MEMORY;
		return FALSE;
	}

	polygon->edges = new_edges;
	polygon->edges_size = new_size;

	return TRUE;
}

static INLINE int ComputeNormalizedDeviceSlope(FLOAT_T* dx, FLOAT_T* dy, const GRAPHICS_MATRIX* matrix_inverse, FLOAT_T* magnitude_out)
{
	FLOAT_T dx0 = *dx, dy0 = *dy;
	FLOAT_T magnitude;

	GraphicsMatrixTransformDistance(matrix_inverse, &dx0, &dy0);

	if(dx0 == 0.0 && dy0 == 0.0)
	{
		*magnitude_out = 0.0;
		return FALSE;
	}

	if(dx0 == 0.0)
	{
		*dx = 0.0;
		if(dy0 > 0.0)
		{
			magnitude = dy0;
			*dy = 1.0;
		}
		else
		{
			magnitude = -dy0;
			*dy = -1.0;
		}
	}
	else if(dy0 == 0.0)
	{
		*dy = 0.0;
		if(dx0 > 0.0)
		{
			magnitude = dx0;
			*dx = 1.0;
		}
		else
		{
			magnitude = -dy0;
			*dy = -1.0;
		}
	}
	else
	{
		magnitude = hypot(dx0, dy0);
		*dx = dx0 / magnitude;
		*dy = dy0 / magnitude;
	}

	*magnitude_out = magnitude;

	return TRUE;
}

/*
static INLINE void ContourAddPoint(GRAPHICS_STROKER* stroker, GRAPHICS_CONTOUR* contour, const GRAPHICS_POINT* point)
{
	if(FALSE == WithinTolerance(point, &contour->tail->points[contour->tail->num_points-1])
	{
		GraphicsContourAddPoint(&contour->contour, point);
	}
}

*/

static INLINE int PathIsQuad(const GRAPHICS_PATH_FIXED* path)
{
	const GRAPHICS_PATH_BUFFER *buffer = GRAPHICS_PATH_HEAD(path);

	if(buffer->num_ops < 4 || buffer->num_ops > 6)
	{
		return FALSE;
	}

	if(buffer->op[0] != GRAPHICS_PATH_MOVE_TO
		|| buffer->op[1] != GRAPHICS_PATH_LINE_TO
		|| buffer->op[2] != GRAPHICS_PATH_LINE_TO
		|| buffer->op[3] != GRAPHICS_PATH_LINE_TO
	)
	{
		return FALSE;
	}

	if(buffer->num_ops > 4)
	{
		if(buffer->op[4] == GRAPHICS_PATH_LINE_TO)
		{
			if(buffer->points[4].x != buffer->points[0].x
				|| buffer->points[4].y != buffer->points[0].y)
			{
				return FALSE;
			}
		}
		else if(buffer->op[4] != GRAPHICS_PATH_CLOSE_PATH)
		{
			return FALSE;
		}

		if(buffer->num_ops == 6)
		{
			if(buffer->op[5] != GRAPHICS_PATH_MOVE_TO
				&& buffer->op[5] != GRAPHICS_PATH_CLOSE_PATH)
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

static INLINE int PointsFromRectangle(const GRAPHICS_POINT* points)
{
	if(points[0].y == points[1].y
		&& points[1].x == points[2].x
		&& points[2].y == points[3].y
		&& points[3].x == points[0].x
	)
	{
		return TRUE;
	}
	if(points[0].x == points[1].x
		&& points[1].y == points[2].y
		&& points[2].x == points[3].x
		&& points[3].y == points[0].y
	)
	{
		return TRUE;
	}
	return FALSE;
}

static INLINE void CannonicalBox(
	GRAPHICS_BOX* box,
	const GRAPHICS_POINT* point1,
	const GRAPHICS_POINT* point2
)
{
	if(point1->x <= point2->x)
	{
		box->point1.x = point1->x;
		box->point2.x = point2->x;
	}
	else
	{
		box->point1.x = point2->x;
		box->point2.x = point1->x;
	}

	if(point1->y <= point2->y)
	{
		box->point1.y = point1->y;
		box->point2.y = point2->y;
	}
	else
	{
		box->point1.y = point2->y;
		box->point2.y = point1->y;
	}
}

static void ComputeFace(
	const GRAPHICS_POINT* point,
	const GRAPHICS_SLOPE* device_slope,
	FLOAT_T slope_dx,
	FLOAT_T slope_dy,
	GRAPHICS_STROKER* stroker,
	GRAPHICS_STROKE_FACE* face
)
{
	FLOAT_T face_dx, face_dy;
	GRAPHICS_POINT offset_ccw, offset_cw;

	if(stroker->matrix_determin_positive)
	{
		face_dx = - slope_dy * stroker->half_line_width;
		face_dy = slope_dx * stroker->half_line_width;
	}
	else
	{
		face_dx = slope_dy * stroker->half_line_width;
		face_dy = - slope_dx * stroker->half_line_width;
	}

	GraphicsMatrixTransformDistance(stroker->matrix, &face_dx, &face_dy);

	offset_ccw.x = GraphicsFixedFromFloat(face_dx);
	offset_ccw.y = GraphicsFixedFromFloat(face_dy);
	offset_cw.x = -offset_ccw.x;
	offset_cw.y = -offset_ccw.y;

	face->counter_clock_wise = *point;
	TranslatePoint(&face->counter_clock_wise, &offset_ccw);

	face->point = *point;

	face->clock_wise = *point;
	TranslatePoint(&face->clock_wise, &offset_cw);

	face->user_vector.x = slope_dx;
	face->user_vector.y = slope_dy;

	face->device_vector = *device_slope;
}

static eGRAPHICS_STATUS TessellateFan(
	GRAPHICS_STROKER* stroker,
	const GRAPHICS_SLOPE* in_vector,
	const GRAPHICS_SLOPE* out_vector,
	const GRAPHICS_POINT* mid_point,
	const GRAPHICS_POINT* in_point,
	const GRAPHICS_POINT* out_point,
	int clockwise
)
{
	GRAPHICS_POINT stack_points[64], *points = stack_points;
	GRAPHICS_PEN *pen = &stroker->pen;
	int start, stop, num_points = 0;
	eGRAPHICS_STATUS status;

	if(stroker->has_bounds && FALSE == GraphicsBoxContainsPoint(&stroker->bounds, mid_point))
	{
		goto BEVEL;
	}

	ASSERT(stroker->pen.num_vertices != 0);

	if(clockwise)
	{
		GraphicsPenFindActiveCounterClockwiseVertices(pen, in_vector, out_vector, &start, &stop);
		if(stroker->add_external_edge != NULL)
		{
			GRAPHICS_POINT last;
			last = *in_point;
			while(start != stop)
			{
				GRAPHICS_POINT p = *mid_point;
				TranslatePoint(&p, &pen->vertices[start].point);

				status = stroker->add_external_edge(stroker->closure, &last, &p);

				if(UNLIKELY(status))
				{
					return status;
				}
				last = p;

				if(start-- == 0)
				{
					start += pen->num_vertices;
				}
			}
			status = stroker->add_external_edge(stroker->closure, &last, out_point);
		}
		else
		{
			if(start == stop)
			{
				goto BEVEL;
			}

			num_points = stop - start;
			if(num_points < 0)
			{
				num_points += pen->num_vertices;
				num_points += 2;
				if(num_points > (int)(sizeof(stack_points) / sizeof(stack_points[0])))
				{
					points = (GRAPHICS_POINT*)MEM_ALLOC_FUNC(sizeof(*points) * num_points);
					if(UNLIKELY(points == NULL))
					{
						return GRAPHICS_STATUS_NO_MEMORY;
					}
				}

				points[0] = *in_point;
				num_points = 1;
				while(start != stop)
				{
					points[num_points] = *mid_point;
					TranslatePoint(&points[num_points], &pen->vertices[start].point);
					num_points++;

					if(start-- == 0)
					{
						start += pen->num_vertices;
					}
				}

				points[num_points++] = *out_point;
			}
		}
	}
	else
	{
		GraphicsPenFindActiveCounterClockwiseVertices(pen, in_vector, out_vector, &start, &stop);
		if(stroker->add_external_edge != NULL)
		{
			GRAPHICS_POINT last;
			last = *in_point;
			while(start != stop)
			{
				GRAPHICS_POINT p = *mid_point;
				TranslatePoint(&p, &pen->vertices[start].point);

				status = stroker->add_external_edge(stroker->closure, &p, &last);
				if(UNLIKELY(status))
				{
					return status;
				}
				last = p;

				if(++start == pen->num_vertices)
				{
					start = 0;
				}
			}
			status = stroker->add_external_edge(stroker->closure, out_point, &last);
		}
		else
		{
			if(start == stop)
			{
				goto BEVEL;
			}

			num_points = stop - start;
			if(num_points < 0)
			{
				num_points += pen->num_vertices;
			}
			num_points += 2;
			if(num_points > (int)(sizeof(stack_points) / sizeof(stack_points[0])))
			{
				points = (GRAPHICS_POINT*)MEM_ALLOC_FUNC(sizeof(*points) * num_points);
				if(UNLIKELY(points == NULL))
				{
					return GRAPHICS_STATUS_NO_MEMORY;
				}
			}

			points[0] = *in_point;
			num_points = 1;
			while(start != stop)
			{
				points[num_points] = *mid_point;
				TranslatePoint(&points[num_points], &pen->vertices[start].point);
				num_points++;

				if(++start == pen->num_vertices)
				{
					start = 0;
				}
			}
			points[num_points++] = *out_point;
		}
	}

	if(num_points != 0)
	{
		status = stroker->add_triangle_fan(stroker->closure, mid_point, points, num_points);
	}

	if(points != stack_points)
	{
		MEM_FREE_FUNC(points);
	}

	return status;

BEVEL:
	if(stroker->add_external_edge != NULL)
	{
		if(clockwise)
		{
			return stroker->add_external_edge(stroker->closure, in_point, out_point);
		}
		else
		{
			return stroker->add_external_edge(stroker->closure, out_point, in_point);
		}
	}


	stack_points[0] = *mid_point;
	stack_points[1] = *in_point;
	stack_points[2] = *out_point;
	return stroker->add_triangle(stroker->closure, stack_points);
}

static eGRAPHICS_STATUS GraphicsStrokerAddCap(GRAPHICS_STROKER* stroker, const GRAPHICS_STROKE_FACE* face)
{
	switch(stroker->style.line_cap)
	{
	case GRAPHICS_LINE_CAP_ROUND:
		{
			GRAPHICS_SLOPE slope;

			slope.dx = - face->device_vector.dx;
			slope.dy = - face->device_vector.dy;

			return TessellateFan(stroker, &face->device_vector, &slope, &face->point,
									&face->clock_wise, &face->counter_clock_wise, FALSE);
		}
	case GRAPHICS_LINE_CAP_SQUARE:
		{
			FLOAT_T dx, dy;
			GRAPHICS_SLOPE face_vector;
			GRAPHICS_POINT quad[4];

			dx = face->user_vector.x;
			dy = face->user_vector.y;
			dx *= stroker->half_line_width;
			dy *= stroker->half_line_width;
			GraphicsMatrixTransformDistance(stroker->matrix, &dx, &dy);
			face_vector.dx = GraphicsFixedFromFloat(dx);
			face_vector.dy = GraphicsFixedFromFloat(dy);

			quad[0] = face->counter_clock_wise;
			quad[1].x = face->counter_clock_wise.x + face_vector.dx;
			quad[1].y = face->counter_clock_wise.y + face_vector.dy;
			quad[2].x = face->clock_wise.x + face_vector.dx;
			quad[2].y = face->clock_wise.y + face_vector.dy;
			quad[3] = face->counter_clock_wise;

			if(stroker->add_external_edge != NULL)
			{
				eGRAPHICS_STATUS status;

				status = stroker->add_external_edge(stroker->closure, &quad[0], &quad[1]);
				if(UNLIKELY(status))
				{
					return status;
				}

				status = stroker->add_external_edge(stroker->closure, &quad[1], &quad[2]);
				if(UNLIKELY(status))
				{
					return status;
				}

				status = stroker->add_external_edge(stroker->closure, &quad[2], &quad[3]);
				if(UNLIKELY(status))
				{
					return status;
				}

				return GRAPHICS_STATUS_SUCCESS;
			}
			else
			{
				return stroker->add_convex_quad(stroker->closure, quad);
			}
		}
	case GRAPHICS_LINE_CAP_BUTT:
	default:
		{
			if(stroker->add_external_edge != NULL)
			{
				return stroker->add_external_edge(stroker->closure, &face->counter_clock_wise, &face->clock_wise);
			}
		}
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsStrokerAddLeadingCap(GRAPHICS_STROKER* stroker, const GRAPHICS_STROKE_FACE* face)
{
	GRAPHICS_STROKE_FACE reversed;
	GRAPHICS_POINT t;

	reversed = *face;

	reversed.user_vector.x = - reversed.user_vector.x;
	reversed.user_vector.y = - reversed.user_vector.y;
	reversed.device_vector.dx = - reversed.device_vector.dx;
	reversed.device_vector.dy = - reversed.device_vector.dy;
	t = reversed.clock_wise;
	reversed.clock_wise = reversed.counter_clock_wise;
	reversed.counter_clock_wise = t;

	return GraphicsStrokerAddCap(stroker, &reversed);
}

static eGRAPHICS_STATUS GraphicsStrokerAddTrailingCap(GRAPHICS_STROKER* stroker, const GRAPHICS_STROKE_FACE* face)
{
	return GraphicsStrokerAddCap(stroker, face);
}

static eGRAPHICS_STATUS GraphicsStrokerAddCaps(GRAPHICS_STROKER* stroker)
{
	eGRAPHICS_STATUS status;

	if(stroker->has_initial_sub_path && FALSE == stroker->has_first_face
		&& FALSE == stroker->has_current_face && stroker->style.line_cap == GRAPHICS_LINE_CAP_ROUND)
	{
		FLOAT_T dx = 1.0, dy = 0.0;
		FLOAT_T temp;
		GRAPHICS_SLOPE slope = {GRAPHICS_FIXED_ONE_FLOAT, 0};
		GRAPHICS_STROKE_FACE face;

		ComputeNormalizedDeviceSlope(&dx, &dy, stroker->matrix_inverse, &temp);

		ComputeFace(&stroker->first_point, &slope, dx, dy, stroker, &face);

		status = GraphicsStrokerAddLeadingCap(stroker, &face);
		if(UNLIKELY(status))
		{
			return status;
		}

		status = GraphicsStrokerAddTrailingCap(stroker, &face);
		if(UNLIKELY(status))
		{
			return status;
		}
	}

	if(stroker->has_first_face)
	{
		status = GraphicsStrokerAddLeadingCap(stroker, &stroker->first_face);
		if(UNLIKELY(status))
		{
			return status;
		}
	}

	if(stroker->has_current_face)
	{
		status = GraphicsStrokerAddTrailingCap(stroker, &stroker->current_face);
		if(UNLIKELY(status))
		{
			return status;
		}
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsStrokerAddSubEdge(
	GRAPHICS_STROKER* stroker,
	const GRAPHICS_POINT* point1,
	const GRAPHICS_POINT* point2,
	GRAPHICS_SLOPE* device_slope,
	FLOAT_T slope_dx,
	FLOAT_T slope_dy,
	GRAPHICS_STROKE_FACE* start,
	GRAPHICS_STROKE_FACE* end
)
{
	ComputeFace(point1, device_slope, slope_dx, slope_dy, stroker, start);
	*end = *start;

	if(point1->x == point2->x && point1->y == point2->y)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	end->point = *point2;
	end->counter_clock_wise.x += point2->x - point1->x;
	end->counter_clock_wise.y += point2->y - point1->y;
	end->clock_wise.x += point2->x - point1->x;
	end->clock_wise.x += point2->x - point1->y;

	if(stroker->add_external_edge != NULL)
	{
		eGRAPHICS_STATUS status;

		status = stroker->add_external_edge(stroker->closure, &start->counter_clock_wise, &start->clock_wise);

		return status;
	}

	{
		GRAPHICS_POINT quad[4];

		quad[0] = start->clock_wise;
		quad[1] = end->clock_wise;
		quad[2] = end->counter_clock_wise;
		quad[3] = start->counter_clock_wise;

		return stroker->add_convex_quad(stroker->closure, quad);
	}
}

static INLINE int GraphicsStrokerJoinIsClockwise(const GRAPHICS_STROKE_FACE* in, const GRAPHICS_STROKE_FACE* out)
{
	GRAPHICS_SLOPE in_slope, out_slope;

	INITIALIZE_GRAPHICS_SLOPE(&in_slope, &in->point, &in->clock_wise);
	INITIALIZE_GRAPHICS_SLOPE(&out_slope, &out->point, &out->clock_wise);

	return GraphicsSlopeCompare(&in_slope, &out_slope);
}

static eGRAPHICS_STATUS GraphicsStrokerJoin(
	GRAPHICS_STROKER* stroker,
	const GRAPHICS_STROKE_FACE* in,
	const GRAPHICS_STROKE_FACE* out
)
{
	int clockwise = GraphicsStrokerJoinIsClockwise(out, in);
	const GRAPHICS_POINT *in_point, *out_point;
	GRAPHICS_POINT points[4];
	eGRAPHICS_STATUS status;

	if(in->clock_wise.x == out->clock_wise.x && in->clock_wise.y == out->clock_wise.y
		&& in->counter_clock_wise.x == out->counter_clock_wise.x && in->counter_clock_wise.y == out->counter_clock_wise.y)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(clockwise)
	{
		if(stroker->add_external_edge != NULL)
		{
			status = stroker->add_external_edge(stroker->closure, &out->clock_wise, &in->point);

			if(UNLIKELY(status))
			{
				return status;
			}

			status = stroker->add_external_edge(stroker->closure, &in->point, &in->clock_wise);

			if(UNLIKELY(status))
			{
				return status;
			}
		}

		in_point = &in->counter_clock_wise;
		out_point = &out->counter_clock_wise;
	}
	else
	{
		if(stroker->add_external_edge != NULL)
		{
			status = stroker->add_external_edge(stroker->closure, &in->counter_clock_wise, &in->point);

			if(UNLIKELY(status))
			{
				return status;
			}

			status = stroker->add_external_edge(stroker->closure, &in->point, &out->counter_clock_wise);

			if(UNLIKELY(status))
			{
				return status;
			}
		}

		in_point = &in->clock_wise;
		out_point = &out->clock_wise;
	}

	switch(stroker->style.line_join)
	{
	case GRAPHICS_LINE_JOIN_ROUND:
		return TessellateFan(stroker, &in->device_vector, &out->device_vector,
							 &in->point, in_point, out_point, clockwise);

	case GRAPHICS_LINE_JOIN_MITER:
	default:
		{
			FLOAT_T in_dot_out = - in->user_vector.x * out->user_vector.x
				+ - in->user_vector.y * out->user_vector.y;
			FLOAT_T ml = stroker->style.miter_limit;

			if(2 <= ml * ml * (1 - in_dot_out))
			{
				FLOAT_T x1, y1, x2, y2;
				FLOAT_T mx, my;
				FLOAT_T dx1, dx2, dy1, dy2;
				FLOAT_T ix, iy;
				FLOAT_T fdx1, fdy1, fdx2, fdy2;
				FLOAT_T mdx, mdy;

				x1 = GRAPHICS_FIXED_TO_FLOAT(in_point->x);
				y1 = GRAPHICS_FIXED_TO_FLOAT(in_point->y);
				dx1 = in->user_vector.x;
				dy1 = in->user_vector.y;
				GraphicsMatrixTransformDistance(stroker->matrix, &dx2, &dy2);

				x2 = GRAPHICS_FIXED_TO_FLOAT(out_point->x);
				y2 = GRAPHICS_FIXED_TO_FLOAT(out_point->y);
				dx2 = out->user_vector.x;
				dy2 = out->user_vector.y;
				GraphicsMatrixTransformDistance(stroker->matrix, &dx2, &dy2);

				my = (((x2 - x1) * dy1 * dy2 - y2 * dx2 * dy1 + y1 * dx1 * dy2) / (dx1 * dy2 - dx2 * dy1));
				if(fabs(dy1) >= fabs(dy2))
				{
					mx = (my - y1) * dx1 / dy1 + x1;
				}
				else
				{
					mx = (my - y2) * dx2 / dy2 + x2;
				}

				ix = GRAPHICS_FIXED_TO_FLOAT(in->point.x);
				iy = GRAPHICS_FIXED_TO_FLOAT(in_point->y);

				fdx1 = x1 - ix, fdy1 = y1 - iy;
				fdx2 = x2 - ix, fdy2 = y2 - iy;
				mdx = mx - ix,  mdy = my - iy;

				if(GraphicsSlopeCompareSign(fdx1, fdy1, mdx, mdy)
					!= GraphicsSlopeCompareSign(fdx2, fdy2, mdx, mdy))
				{
					if(stroker->add_external_edge != NULL)
					{
						points[0].x = GraphicsFixedFromFloat(mx);
						points[0].y = GraphicsFixedFromFloat(my);

						if(clockwise)
						{
							status = stroker->add_external_edge(stroker->closure, in_point, &points[0]);
							if(UNLIKELY(status))
							{
								return status;
							}
							status = stroker->add_external_edge(stroker->closure, &points[0], out_point);
							if(UNLIKELY(status))
							{
								return status;
							}
						}
						else
						{
							status = stroker->add_external_edge(stroker->closure, out_point, &points[0]);
							if(UNLIKELY(status))
							{
								return status;
							}
							status = stroker->add_external_edge(stroker->closure, &points[0], in_point);
							if(UNLIKELY(status))
							{
								return status;
							}
						}
						return GRAPHICS_STATUS_SUCCESS;
					}
					else
					{
						points[0] = in->point;
						points[1] = *in_point;
						points[2].x = GraphicsFixedFromFloat(mx);
						points[2].y = GraphicsFixedFromFloat(my);
						points[3] = *out_point;

						return stroker->add_convex_quad(stroker->closure, points);
					}
				}
			}
		}
	case GRAPHICS_LINE_JOIN_BEVEL:
		if(stroker->add_external_edge != NULL)
		{
			if(clockwise)
			{
				return stroker->add_external_edge(stroker->closure, in_point, out_point);
			}
			else
			{
				return stroker->add_external_edge(stroker->closure, out_point, in_point);
			}
		}
		else
		{
			points[0] = in->point;
			points[1] = *in_point;
			points[2] = *out_point;

			return stroker->add_triangle(stroker->closure, points);
		}
	}
}

static eGRAPHICS_STATUS GraphicsStrokerMoveTo(void* closure, const GRAPHICS_POINT* point)
{
	GRAPHICS_STROKER *stroker = (GRAPHICS_STROKER*)closure;
	eGRAPHICS_STATUS status;

	GraphicsStrokerDashStart(&stroker->dash);

	status = GraphicsStrokerAddCaps(stroker);
	if(UNLIKELY(status))
	{
		return status;
	}

	stroker->first_point = *point;
	stroker->current_point = *point;

	stroker->has_first_face = FALSE;
	stroker->has_current_face = FALSE;
	stroker->has_initial_sub_path = FALSE;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsStrokerLineTo(void* closure, const GRAPHICS_POINT* point)
{
	GRAPHICS_STROKER *stroker = (GRAPHICS_STROKER*)closure;
	GRAPHICS_STROKE_FACE start, end;
	GRAPHICS_POINT *point1 = &stroker->current_point;
	GRAPHICS_SLOPE device_slope;
	FLOAT_T slope_dx, slope_dy, temp;
	eGRAPHICS_STATUS status;

	stroker->has_initial_sub_path = TRUE;

	if(point1->x == point->x && point1->y == point->y)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	INITIALIZE_GRAPHICS_SLOPE(&device_slope, point1, point);
	slope_dx = GRAPHICS_FIXED_TO_FLOAT(point->x - point1->x);
	slope_dy = GRAPHICS_FIXED_TO_FLOAT(point->y - point1->y);
	ComputeNormalizedDeviceSlope(&slope_dx, &slope_dy, stroker->matrix_inverse, &temp);

	status = GraphicsStrokerAddSubEdge(stroker, point1, point, &device_slope,
		slope_dx, slope_dy, &start, &end);

	if(UNLIKELY(status))
	{
		return status;
	}

	if(stroker->has_first_face)
	{
		status = GraphicsStrokerJoin(stroker, &stroker->current_face, &start);
		if(UNLIKELY(status))
		{
			return status;
		}
	}
	else if(FALSE == stroker->has_first_face)
	{
		stroker->first_face = start;
		stroker->has_first_face = TRUE;
	}
	stroker->current_face = end;
	stroker->has_current_face = TRUE;

	stroker->current_point = *point;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsStrokerSplineTo(void* closure, const GRAPHICS_POINT* point, const GRAPHICS_SLOPE* tangent)
{
	GRAPHICS_STROKER *stroker = (GRAPHICS_STROKER*)closure;
	GRAPHICS_STROKE_FACE new_face;
	FLOAT_T slope_dx, slope_dy;
	GRAPHICS_POINT points[3];
	GRAPHICS_POINT intersect_point;
	FLOAT_T temp;

	stroker->has_initial_sub_path = TRUE;

	if(stroker->current_point.x == point->x && stroker->current_point.y == point->y)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	slope_dx = GRAPHICS_FIXED_TO_FLOAT(tangent->dx);
	slope_dy = GRAPHICS_FIXED_TO_FLOAT(tangent->dy);

	if(FALSE == ComputeNormalizedDeviceSlope(&slope_dx, &slope_dy, stroker->matrix, &temp))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	ComputeFace(point, tangent, slope_dx, slope_dy, stroker, &new_face);

	ASSERT(stroker->has_current_face);

	if((new_face.device_slope.x * stroker->current_face.device_slope.x
		+ new_face.device_slope.y * stroker->current_face.device_slope.y) < stroker->spline_cusp_tolerance)
	{
		const GRAPHICS_POINT *in_point, *out_point;
		int clockwise = GraphicsStrokerJoinIsClockwise(&new_face, &stroker->current_face);
		if(clockwise)
		{
			in_point = &stroker->current_face.clock_wise;
			out_point = &new_face.clock_wise;
		}
		else
		{
			in_point = &stroker->current_face.counter_clock_wise;
			out_point = &new_face.counter_clock_wise;
		}
		TessellateFan(stroker, &stroker->current_face.device_vector, &new_face.device_vector,
			&stroker->current_face.point, in_point, out_point, clockwise);
	}

	if(SlowSegmentIntersection(&stroker->current_face.clock_wise, &stroker->current_face.counter_clock_wise,
		&new_face.clock_wise, &new_face.counter_clock_wise, &intersect_point))
	{
		points[0] = stroker->current_face.counter_clock_wise;
		points[1] = new_face.counter_clock_wise;
		points[2] = intersect_point;
		stroker->add_triangle(stroker->closure, points);

		points[0] = stroker->current_face.clock_wise;
		points[1] = new_face.clock_wise;
		stroker->add_triangle(stroker->closure, points);
	}
	else
	{
		points[0] = stroker->current_face.counter_clock_wise;
		points[1] = stroker->current_face.clock_wise;
		points[2] = new_face.clock_wise;
		stroker->add_triangle(stroker->closure, points);

		points[0] = stroker->current_face.counter_clock_wise;
		points[1] = new_face.clock_wise;
		points[2] = new_face.counter_clock_wise;
		stroker->add_triangle(stroker->closure, points);
	}

	stroker->current_face = new_face;
	stroker->has_current_face = TRUE;
	stroker->current_point = *point;

	return GRAPHICS_STATUS_SUCCESS;
}

void GraphicsStrokerDashStep(GRAPHICS_STROKER_DASH* dash, FLOAT_T step)
{
	dash->dash_remain -= step;
	if(dash->dash_remain < GRAPHICS_FIXED_ERROR_FLOAT)
	{
		if(++dash->dash_index == dash->num_dashes)
		{
			dash->dash_index = 0;
		}

		dash->dash_on = !dash->dash_on;
		dash->dash_remain += dash->dashes[dash->dash_index];
	}
}

static eGRAPHICS_STATUS GraphicsStrokerLineToDahsed(void* closure, const GRAPHICS_POINT* point2)
{
	GRAPHICS_STROKER *stroker = (GRAPHICS_STROKER*)closure;
	FLOAT_T magnitude, remain, step_length = 0;
	FLOAT_T slope_dx, slope_dy;
	FLOAT_T dx2, dy2;
	GRAPHICS_STROKE_FACE sub_start, sub_end;
	GRAPHICS_POINT *point1 = &stroker->current_point;
	GRAPHICS_SLOPE device_slope;
	GRAPHICS_LINE segment;
	int fully_in_bounds;
	eGRAPHICS_STATUS status;

	stroker->has_initial_sub_path = stroker->dash.dash_starts_on;

	if(point1->x == point2->x && point1->y == point2->y)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	fully_in_bounds = TRUE;
	if(stroker->has_bounds && (FALSE == GraphicsBoxContainsPoint(&stroker->bounds, point1)
		|| FALSE == GraphicsBoxContainsPoint(&stroker->bounds, point2)))
	{
		fully_in_bounds = FALSE;
	}

	INITIALIZE_GRAPHICS_SLOPE(&device_slope, point1, point2);

	slope_dx = GRAPHICS_FIXED_TO_FLOAT(point2->x - point1->x);
	slope_dy = GRAPHICS_FIXED_TO_FLOAT(point2->y - point1->y);

	if(FALSE == ComputeNormalizedDeviceSlope(&slope_dx, &slope_dy, stroker->matrix_inverse, &magnitude))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	remain = magnitude;
	segment.point1 = *point1;
	while(remain)
	{
		step_length = MINIMUM(stroker->dash.dash_remain, remain);
		remain -= step_length;
		dx2 = slope_dx * (magnitude - remain);
		dy2 = slope_dy * (magnitude - remain);
		GraphicsMatrixTransformDistance(stroker->matrix, &dx2, &dy2);
		segment.point2.x = GraphicsFixedFromFloat(dx2) + point1->x;
		segment.point2.y = GraphicsFixedFromFloat(dy2) + point1->y;

		if(stroker->dash.dash_on && (fully_in_bounds || (FALSE == stroker->has_first_face
			&& stroker->dash.dash_starts_on) || GraphicsBoxIntersectsLineSegment(&stroker->bounds, &segment)))
		{
			status = GraphicsStrokerAddSubEdge(stroker, &segment.point1, &segment.point2,
						&device_slope, slope_dx, slope_dy, &sub_start, &sub_end);
			if(UNLIKELY(status))
			{
				return status;
			}

			if(stroker->has_current_face)
			{
				status = GraphicsStrokerJoin(stroker, &stroker->current_face, &sub_start);
				if(UNLIKELY(status))
				{
					return status;
				}

				stroker->has_current_face = FALSE;
			}
			else if(FALSE == stroker->has_first_face && stroker->dash.dash_starts_on)
			{
				stroker->first_face = sub_start;
				stroker->has_first_face = TRUE;
			}
			else
			{
				status = GraphicsStrokerAddLeadingCap(stroker, &sub_start);
				if(UNLIKELY(status))
				{
					return status;
				}
			}

			if(remain)
			{
				status = GraphicsStrokerAddTrailingCap(stroker, &sub_end);
				if(UNLIKELY(status))
				{
					return status;
				}
			}
			else
			{
				stroker->current_face = sub_end;
				stroker->has_first_face = TRUE;
			}
		}
		else
		{
			if(stroker->has_current_face)
			{
				status = GraphicsStrokerAddTrailingCap(stroker, &stroker->current_face);
				if(UNLIKELY(status))
				{
					return status;
				}
				stroker->has_current_face = FALSE;
			}
		}

		GraphicsStrokerDashStep(&stroker->dash, step_length);
		segment.point1 = segment.point2;
	}

	if(stroker->dash.dash_on && FALSE == stroker->has_current_face)
	{
		ComputeFace(point2, &device_slope, slope_dx, slope_dy,
			stroker, &stroker->current_face);
		status = GraphicsStrokerAddLeadingCap(stroker, &stroker->current_face);
		if(UNLIKELY(status))
		{
			return status;
		}

		stroker->has_current_face = TRUE;
	}

	stroker->current_point = *point2;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsStrokerCurveTo(
	void* closure,
		const GRAPHICS_POINT* b,
		const GRAPHICS_POINT* c,
		const GRAPHICS_POINT* d
)
{
	GRAPHICS_STROKER *stroker = (GRAPHICS_STROKER*)closure;
	GRAPHICS_SPLINE spline;
	eGRAPHICS_LINE_JOIN line_join_save;
	GRAPHICS_STROKE_FACE face;
	FLOAT_T slope_dx, slope_dy, temp;
	GRAPHICS_SPLINE_ADD_POINT_FUNCTION line_to;
	GRAPHICS_SPLINE_ADD_POINT_FUNCTION spline_to;
	eGRAPHICS_STATUS status = GRAPHICS_STATUS_SUCCESS;

	line_to = stroker->dash.dashed ? (GRAPHICS_SPLINE_ADD_POINT_FUNCTION)GraphicsStrokerLineToDahsed
								   : (GRAPHICS_SPLINE_ADD_POINT_FUNCTION)GraphicsStrokerLineTo;

	spline_to = stroker->dash.dashed ? (GRAPHICS_SPLINE_ADD_POINT_FUNCTION)GraphicsStrokerLineToDahsed
								   : (GRAPHICS_SPLINE_ADD_POINT_FUNCTION)GraphicsStrokerSplineTo;

	if(FALSE == InitializeGraphicsSpline(&spline, spline_to, stroker, &stroker->current_point, b, c, d))
	{
		GRAPHICS_SLOPE fallcack_slope;
		INITIALIZE_GRAPHICS_SLOPE(&fallcack_slope, &stroker->current_point, d);
		return line_to(closure, d, &fallcack_slope);
	}

	if(stroker->dash.dashed || stroker->dash.dash_on)
	{
		slope_dx = GRAPHICS_FIXED_TO_FLOAT(spline.initial_slope.dx);
		slope_dy = GRAPHICS_FIXED_TO_FLOAT(spline.initial_slope.dy);
		if(ComputeNormalizedDeviceSlope(&slope_dx, &slope_dy, stroker->matrix_inverse, &temp))
		{
			ComputeFace(&stroker->current_point, &spline.initial_slope, slope_dx, slope_dy,
						stroker, &face);
		}
		if(stroker->has_current_face)
		{
			status = GraphicsStrokerJoin(stroker, &stroker->current_face, &face);
			if(UNLIKELY(status))
			{
				return status;
			}
		}
		else if(FALSE == stroker->has_first_face)
		{
			stroker->first_face = face;
			stroker->has_first_face = TRUE;
		}

		line_join_save = stroker->style.line_join;
		stroker->style.line_join = GRAPHICS_LINE_JOIN_ROUND;

		status = GraphicsSplineDecompose(&spline, stroker->tolerance);
		if(UNLIKELY(status))
		{
			return status;
		}

		if(FALSE == stroker->dash.dashed || stroker->dash.dash_on)
		{
			slope_dx = GRAPHICS_FIXED_TO_FLOAT(spline.final_slope.dx);
			slope_dy = GRAPHICS_FIXED_TO_FLOAT(spline.final_slope.dy);
			if(ComputeNormalizedDeviceSlope(&slope_dx, &slope_dy, stroker->matrix_inverse, &temp))
			{
				ComputeFace(&stroker->current_point, &spline.final_slope, slope_dx, slope_dy, stroker, &face);
			}
		}

		status = GraphicsStrokerJoin(stroker, &stroker->current_face, &face);
		if(UNLIKELY(status))
		{
			return status;
		}
		stroker->current_face = face;
	}
	stroker->style.line_join = line_join_save;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsStrokerClosePath(void* closure)
{
	GRAPHICS_STROKER *stroker = (GRAPHICS_STROKER*)closure;
	eGRAPHICS_STATUS status;

	if(stroker->dash.dashed)
	{
		status = GraphicsStrokerLineToDahsed(stroker, &stroker->first_point);
	}
	else
	{
		status = GraphicsStrokerLineTo(stroker, &stroker->first_point);
	}
	if(UNLIKELY(status))
	{
		return status;
	}
	else
	{
		status = GraphicsStrokerAddCaps(stroker);
		if(UNLIKELY(status))
		{
			return status;
		}
	}

	stroker->has_initial_sub_path = FALSE;
	stroker->has_first_face = FALSE;
	stroker->has_current_face = FALSE;

	return GRAPHICS_STATUS_SUCCESS;
}

static void GraphicsStrokerFinish(GRAPHICS_STROKER* stroker)
{
	GraphicsPenFinish(&stroker->pen);
}

eGRAPHICS_STATUS GraphicsPathFixedStrokeDashedToPolygon(
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* stroke_style,
	const GRAPHICS_MATRIX* matrix,
	const GRAPHICS_MATRIX* matrix_inverse,
	FLOAT_T tolerance,
	GRAPHICS_POLYGON* polygon
)
{
	GRAPHICS_STROKER stroker;
	eGRAPHICS_STATUS status;

	status = InitializeGraphicsStroker(&stroker, path, stroke_style,
		matrix, matrix_inverse, tolerance, polygon->limits, polygon->num_limits);
	if(UNLIKELY(status))
	{
		return status;
	}

	stroker.add_external_edge = GraphicsPolygonAddExternalEdge;
	stroker.closure = (void*)polygon;

	status = GraphicsPathFixedInterpret(path, GraphicsStrokerMoveTo,
		stroker.dash.dashed ? GraphicsStrokerLineToDahsed : GraphicsStrokerLineTo, GraphicsStrokerCurveTo,
			GraphicsStrokerClosePath, &stroker
	);

	if(UNLIKELY(status))
	{
		goto BAIL;
	}

	status = GraphicsStrokerAddCaps(&stroker);

BAIL:
	GraphicsStrokerFinish(&stroker);

	return status;
}

eGRAPHICS_STATUS GraphicsPathFixedStrokeToPolygon(
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_MATRIX* matrix,
	const GRAPHICS_MATRIX* matrix_inverse,
	FLOAT_T tolerance,
	GRAPHICS_POLYGON* polygon
)
{
	GRAPHICS_STROKER stroker;
	eGRAPHICS_STATUS status;

	if(style->num_dashes != 0)
	{
		return GraphicsPathFixedStrokeDashedToPolygon(
			path, style, matrix, matrix_inverse, tolerance, polygon);
	}

	stroker.has_bounds = polygon->num_limits;
	if(stroker.has_bounds)
	{
		FLOAT_T dx, dy;
		GRAPHICS_FLOAT_FIXED fdx, fdy;
		int i;

		stroker.bounds = polygon->limits[0];
		for(i=1; i<polygon->num_limits; i++)
		{
			GraphicsBoxAddBox(&stroker.bounds, &polygon->limits[i]);
		}

		GraphicsStrokeStyleMaxDistanceFromPath(style, path, matrix, &dx, &dy);
		fdx = GraphicsFixedFromFloat(dx);
		fdy = GraphicsFixedFromFloat(dy);

		stroker.bounds.point1.x -= fdx;
		stroker.bounds.point2.x += fdx;
		stroker.bounds.point1.y -= fdy;
		stroker.bounds.point2.y += fdy;
	}

	stroker.style = *style;
	stroker.matrix = matrix;
	stroker.matrix_inverse = matrix_inverse;
	stroker.tolerance = tolerance;
	stroker.half_line_width = style->line_width / 2;

	stroker.spline_cusp_tolerance = 1 - tolerance / stroker.half_line_width;
	stroker.spline_cusp_tolerance *= stroker.spline_cusp_tolerance;
	stroker.spline_cusp_tolerance *= 2;
	stroker.spline_cusp_tolerance -= 1;
	stroker.matrix_determin_positive = GRAPHICS_MATRIX_COMPUTE_DETERMINANT(matrix) >= 0.0;

	stroker.pen.num_vertices = 0;
	if(path->has_curve_to || style->line_join == GRAPHICS_LINE_JOIN_ROUND || style->line_cap == GRAPHICS_LINE_CAP_ROUND)
	{
		status = InitializeGraphicsPen(&stroker.pen, stroker.half_line_width, tolerance, matrix);
		if(UNLIKELY(status))
		{
			return status;
		}
		if(stroker.pen.num_vertices <= 1)
		{
			return GRAPHICS_STATUS_SUCCESS;
		}
	}

	stroker.has_current_face = FALSE;
	stroker.has_first_face = FALSE;
	stroker.has_initial_sub_path = FALSE;

	InitializeGraphicsContour(&stroker.clock_wise.contour, 1);
	InitializeGraphicsContour(&stroker.counter_clock_wise.contour, -1);
	tolerance *= GRAPHICS_FIXED_ONE_FLOAT;
	tolerance *= tolerance;
	stroker.contour_tolerance = tolerance;
	stroker.polygon = polygon;

	status = GraphicsPathFixedInterpret(path, StrokerMoveTo, StrokerLineTo, StrokerCurveTo,
		StrokerClosePath, &stroker);

	if(LIKELY(status == GRAPHICS_STATUS_SUCCESS))
	{
		StrokerAddCaps(&stroker);
	}

	GraphicsContourFinish(&stroker.clock_wise.contour);
	GraphicsContourFinish(&stroker.counter_clock_wise.contour);
	if(stroker.pen.num_vertices != 0)
	{
		GraphicsPenFinish(&stroker.pen);
	}

	return status;
}

typedef struct _SEGMENT
{
	GRAPHICS_POINT point1, point2;
	unsigned flags;
#define HORIZONTAL 0x01
#define FORWARDS 0x02
#define JOIN 0x04
} SEGMENT;

typedef struct _GRAPHICS_RECTLINEAR_STROKER
{
	const GRAPHICS_STROKE_STYLE *stroke_style;
	const GRAPHICS_MATRIX *matrix;
	eGRAPHICS_ANTIALIAS antialias;

	GRAPHICS_FLOAT_FIXED half_line_x, half_line_y;
	GRAPHICS_BOXES *boxes;
	GRAPHICS_POINT current_point;
	GRAPHICS_POINT first_point;
	int open_sub_path;

	GRAPHICS_STROKER_DASH dash;

	int has_bounds;
	GRAPHICS_BOX bounds;

	int num_segments;
	int segments_size;
	SEGMENT *segments;
	SEGMENT segments_embedded[8];
} GRAPHICS_RECTLINEAR_STROKER;

static int InitializeGraphicsRectilinearStroker(
	GRAPHICS_RECTLINEAR_STROKER* stroker,
	const GRAPHICS_STROKE_STYLE* stroke_style,
	const GRAPHICS_MATRIX* matrix,
	eGRAPHICS_ANTIALIAS antialias,
	GRAPHICS_BOXES* boxes
)
{
	if(stroke_style->line_join != GRAPHICS_LINE_JOIN_MITER)
	{
		return FALSE;
	}

	if(stroke_style->miter_limit < M_SQRT2)
	{
		return FALSE;
	}

	if(FALSE == (stroke_style->line_cap == GRAPHICS_LINE_CAP_BUTT
				 || stroke_style->line_cap == GRAPHICS_LINE_CAP_SQUARE))
	{
		return FALSE;
	}

	if(FALSE == GraphicsMatrixIsScale(matrix))
	{
		return FALSE;
	}

	stroker->stroke_style = stroke_style;
	stroker->matrix = matrix;
	stroker->antialias = antialias;

	stroker->half_line_x = GraphicsFixedFromFloat(FABS(matrix->xx) * stroke_style->line_width / 2.0);
	stroker->half_line_y = GraphicsFixedFromFloat(FABS(matrix->yy) * stroke_style->line_width / 2.0);

	stroker->open_sub_path = FALSE;
	stroker->segments = stroker->segments_embedded;
	stroker->segments_size = sizeof(stroker->segments_embedded) / sizeof(stroker->segments_embedded[0]);
	stroker->num_segments = 0;

	InitializeGraphicsStrokerDash(&stroker->dash, stroke_style);

	stroker->has_bounds = FALSE;

	stroker->boxes = boxes;

	return TRUE;
}

static void GraphicsRectilinearStrokerFinish(GRAPHICS_RECTLINEAR_STROKER* stroker)
{
	if(stroker->segments != stroker->segments_embedded)
	{
		MEM_FREE_FUNC(stroker->segments);
	}
}

static eGRAPHICS_STATUS GraphicsRectilinearStrokerAddSegment(
	GRAPHICS_RECTLINEAR_STROKER* stroker,
	const GRAPHICS_POINT* point1,
	const GRAPHICS_POINT* point2,
	unsigned flags
)
{
	if(stroker->num_segments == stroker->segments_size)
	{
		int new_size = stroker->segments_size * 2;
		SEGMENT *new_segments;

		if(stroker->segments == stroker->segments_embedded)
		{
			new_segments = (SEGMENT*)MEM_ALLOC_FUNC(new_size * sizeof(SEGMENT));
			if(new_segments == NULL)
			{
				return GRAPHICS_STATUS_NO_MEMORY;
			}

			(void)memcpy(new_segments, stroker->segments, stroker->num_segments * sizeof(SEGMENT));
		}
		else
		{
			new_segments = (SEGMENT*)MEM_REALLOC_FUNC(stroker->segments, new_size * sizeof(SEGMENT));
			if(new_segments == NULL)
			{
				return GRAPHICS_STATUS_NO_MEMORY;
			}
		}

		stroker->segments_size = new_size;
		stroker->segments = new_segments;
	}

	stroker->segments[stroker->num_segments].point1 = *point1;
	stroker->segments[stroker->num_segments].point2 = *point2;
	stroker->segments[stroker->num_segments].flags = flags;
	stroker->num_segments++;

	return GRAPHICS_STATUS_SUCCESS;
}

static void GraphicsRectilinearStrokerLimit(
	GRAPHICS_RECTLINEAR_STROKER* stroker,
	const GRAPHICS_BOX* boxes,
	int num_boxes
)
{
	stroker->has_bounds = TRUE;
	GraphicsBoxesGetExtents(boxes, num_boxes, &stroker->bounds);

	stroker->bounds.point1.x -= stroker->half_line_x;
	stroker->bounds.point2.x += stroker->half_line_x;

	stroker->bounds.point1.y -= stroker->half_line_y;
	stroker->bounds.point2.y += stroker->half_line_y;
}

static eGRAPHICS_STATUS GraphicsRectilinearStrokerEmitSegments(GRAPHICS_RECTLINEAR_STROKER* stroker)
{
	eGRAPHICS_LINE_CAP line_cap = stroker->stroke_style->line_cap;
	GRAPHICS_FLOAT_FIXED half_line_x = stroker->half_line_x;
	GRAPHICS_FLOAT_FIXED half_line_y = stroker->half_line_y;
	eGRAPHICS_STATUS status;
	int i, j;

	for(i=0; i<stroker->num_segments; i++)
	{
		int lengthen_initial, lengthen_final;
		GRAPHICS_POINT *a, *b;
		GRAPHICS_BOX box;

		a = &stroker->segments[i].point1;
		b = &stroker->segments[i].point2;

		j = i == 0 ? stroker->num_segments - 1 : i - 1;
		lengthen_initial = (stroker->segments[i].flags ^ stroker->segments[j].flags) & HORIZONTAL;
		j = i == stroker->num_segments - 1 ? 0 : i + 1;
		lengthen_final = (stroker->segments[i].flags ^ stroker->segments[j].flags) & HORIZONTAL;
		if(stroker->open_sub_path)
		{
			if(i == 0)
			{
				lengthen_initial = line_cap != GRAPHICS_LINE_CAP_BUTT;
			}
			if(i == stroker->num_segments - 1)
			{
				lengthen_final = line_cap != GRAPHICS_LINE_CAP_BUTT;
			}
		}

		if(lengthen_initial | lengthen_final)
		{
			if(a->y == b->y)
			{
				if(a->x < b->x)
				{
					if(lengthen_initial)
					{
						a->x -= half_line_x;
					}
					if(lengthen_final)
					{
						b->x += half_line_x;
					}
				}
				else
				{
					if(lengthen_initial)
					{
						a->x += half_line_x;
					}
					if(lengthen_final)
					{
						b->x -= half_line_x;
					}
				}
			}
			else
			{
				if(a->y < b->y)
				{
					if(lengthen_initial)
					{
						a->y -= half_line_y;
					}
					if(lengthen_final)
					{
						b->y += half_line_y;
					}
				}
				else
				{
					if(lengthen_initial)
					{
						a->y += half_line_y;
					}
					if(lengthen_final)
					{
						b->y -= half_line_y;
					}
				}
			}
		}

		if(a->y == b->y)
		{
			a->y -= half_line_y;
			b->y += half_line_y;
		}
		else
		{
			a->x -= half_line_x;
			b->x += half_line_x;
		}

		if(a->x < b->x)
		{
			box.point1.x = a->x;
			box.point2.x = b->x;
		}
		else
		{
			box.point1.x = b->x;
			box.point2.x = a->x;
		}
		if(a->y < b->y)
		{
			box.point1.y = a->y;
			box.point2.y = b->y;
		}
		else
		{
			box.point1.y = b->y;
			box.point2.y = a->y;
		}

		status = GraphicsBoxesAdd(stroker->boxes, stroker->antialias, &box);
		if(UNLIKELY(status))
		{
			return status;
		}
	}

	stroker->num_segments = 0;
	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsRectilinearStrokerEmitSegmentsDashed(GRAPHICS_RECTLINEAR_STROKER* stroker)
{
	eGRAPHICS_STATUS status;
	eGRAPHICS_LINE_CAP line_cap = stroker->stroke_style->line_cap;
	GRAPHICS_FLOAT_FIXED half_line_x = stroker->half_line_x;
	GRAPHICS_FLOAT_FIXED half_line_y = stroker->half_line_y;
	int i;

	for(i=0; i<stroker->num_segments; i++)
	{
		GRAPHICS_POINT *a, *b;
		int is_horizontal;
		GRAPHICS_BOX box;

		a = &stroker->segments[i].point1;
		b = &stroker->segments[i].point2;

		is_horizontal = stroker->segments[i].flags & HORIZONTAL;

		if(line_cap == GRAPHICS_LINE_CAP_BUTT
			&& stroker->segments[i].flags & JOIN
			&& (i != stroker->num_segments - 1 || (FALSE == stroker->open_sub_path && stroker->dash.dash_starts_on))
		)
		{
			GRAPHICS_SLOPE out_slope;
			int j = (i + 1) % stroker->num_segments;
			int forwards = !!(stroker->segments[i].flags & FORWARDS);

			INITIALIZE_GRAPHICS_SLOPE(&out_slope, &stroker->segments[j].point1,
				&stroker->segments[j].point2);
			box.point2 = box.point1 = stroker->segments[i].point2;

			if(is_horizontal)
			{
				if(forwards)
				{
					box.point2.x += half_line_x;
				}
				else
				{
					box.point1.x -= half_line_x;
				}
			}
			else
			{
				if(forwards)
				{
					box.point2.y += half_line_y;
				}
				else
				{
					box.point1.y -= half_line_y;
				}
			}

			status = GraphicsBoxesAdd(stroker->boxes, stroker->antialias, &box);
			if(UNLIKELY(status))
			{
				return status;
			}
		}

		if(is_horizontal)
		{
			if(line_cap == GRAPHICS_LINE_CAP_SQUARE)
			{
				if(a->x <= b->x)
				{
					a->x -= half_line_x;
					b->x += half_line_x;
				}
				else
				{
					a->x += half_line_x;
					b->x -= half_line_x;
				}
			}

			a->y += half_line_y;
			b->y -= half_line_y;
		}
		else
		{
			if(line_cap == GRAPHICS_LINE_CAP_SQUARE)
			{
				if(a->y <= b->y)
				{
					a->y -= half_line_y;
					b->y += half_line_y;
				}
				else
				{
					a->y += half_line_y;
					b->y -= half_line_y;
				}
			}

			a->x += half_line_x;
			b->x -= half_line_x;
		}

		if(a->x == b->x && a->y == b->y)
		{
			continue;
		}

		if(a->x < b->x)
		{
			box.point1.x = a->x;
			box.point2.x = b->x;
		}
		else
		{
			box.point1.x = b->x;
			box.point2.x = a->x;
		}
		if(a->y < b->y)
		{
			box.point1.y = a->y;
			box.point2.y = b->y;
		}
		else
		{
			box.point1.y = b->y;
			box.point2.y = a->y;
		}

		status = GraphicsBoxesAdd(stroker->boxes, stroker->antialias, &box);
		if(UNLIKELY(status))
		{
			return status;
		}
	}

	stroker->num_segments = 0;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsRectilinearStrokerMoveTo(void* closure, const GRAPHICS_POINT* point)
{
	GRAPHICS_RECTLINEAR_STROKER *stroker = (GRAPHICS_RECTLINEAR_STROKER*)closure;
	eGRAPHICS_STATUS status;

	if(stroker->dash.dashed)
	{
		status = GraphicsRectilinearStrokerEmitSegmentsDashed(stroker);
	}
	else
	{
		status = GraphicsRectilinearStrokerEmitSegments(stroker);
	}
	if(UNLIKELY(status))
	{
		return status;
	}

	GraphicsStrokerDashStart(&stroker->dash);

	stroker->current_point = *point;
	stroker->first_point = *point;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsRectilinearStrokerLineTo(void* closure, const GRAPHICS_POINT* b)
{
	GRAPHICS_RECTLINEAR_STROKER *stroker = (GRAPHICS_RECTLINEAR_STROKER*)closure;
	GRAPHICS_POINT *a = &stroker->current_point;
	eGRAPHICS_STATUS status;

	ASSERT(a->x == b->x || a->y == b->y);

	if(a->x == b->x && a->y == b->y)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	status = GraphicsRectilinearStrokerAddSegment(stroker,
				a, b, (a->y == b->y) | JOIN);

	stroker->current_point = *b;
	stroker->open_sub_path = TRUE;

	return status;
}

static eGRAPHICS_STATUS GraphicsRectilinearStrokerLineToDashed(void* closure, const GRAPHICS_POINT* point)
{
	GRAPHICS_RECTLINEAR_STROKER *stroker = (GRAPHICS_RECTLINEAR_STROKER*)closure;
	const GRAPHICS_POINT *a = &stroker->current_point;
	const GRAPHICS_POINT *b = point;
	int fully_in_bounds;
	FLOAT_T sf, sign, remain;
	GRAPHICS_FLOAT_FIXED magnitude;
	eGRAPHICS_STATUS status;
	GRAPHICS_LINE segment;
	int dash_on = FALSE;
	unsigned is_horizontal;

	if(a->x == b->x && a->y == b->y)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	ASSERT(a->x == b->x || a->y == b->y);

	fully_in_bounds = TRUE;
	if(stroker->has_bounds &&
			(FALSE == GraphicsBoxContainsPoint(&stroker->bounds, a)
			|| FALSE == GraphicsBoxContainsPoint(&stroker->bounds, b))
	)
	{
		fully_in_bounds = FALSE;
	}

	is_horizontal = a->y == b->y;
	if(is_horizontal)
	{
		magnitude = b->x - a->x;
		sf = FABS(stroker->matrix->xx);
	}
	else
	{
		magnitude = b->y - a->y;
		sf = FABS(stroker->matrix->yy);
	}
	if(magnitude < 0)
	{
		remain = GRAPHICS_FIXED_TO_FLOAT(-magnitude);
		sign = 1.0;
	}
	else
	{
		remain = GRAPHICS_FIXED_TO_FLOAT(magnitude);
		sign = -1.0;
	}

	segment.point2 = segment.point1 = *a;
	while(remain > 0.0)
	{
		FLOAT_T step_length;

		step_length = MINIMUM(sf * stroker->dash.dash_remain, remain);
		remain -= step_length;

		magnitude = GraphicsFixedFromFloat(sign * remain);
		if(is_horizontal & 0x01)
		{
			segment.point2.x = b->x + magnitude;
		}
		else
		{
			segment.point2.y = b->y + magnitude;
		}

		if(stroker->dash.dash_on &&
			(fully_in_bounds || GraphicsBoxIntersectsLineSegment(&stroker->bounds, &segment))
		)
		{
			status = GraphicsRectilinearStrokerAddSegment(stroker,
						&segment.point1, &segment.point2, is_horizontal | (remain <= 0.0) << 2);
			if(UNLIKELY(status))
			{
				return status;
			}

			dash_on = TRUE;
		}
		else
		{
			dash_on = FALSE;
		}

		GraphicsStrokerDashStep(&stroker->dash, step_length / sf);
		segment.point1 = segment.point2;
	}

	if(stroker->dash.dash_on && FALSE == dash_on &&
		(fully_in_bounds || GraphicsBoxIntersectsLineSegment(&stroker->bounds, &segment))
	)
	{
		status = GraphicsRectilinearStrokerAddSegment(stroker,
					&segment.point1, &segment.point1, is_horizontal | JOIN);
		if(UNLIKELY(status))
		{
			return status;
		}
	}

	stroker->current_point = *point;
	stroker->open_sub_path = TRUE;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsRectilinearStrokerClosePath(void* closure)
{
	GRAPHICS_RECTLINEAR_STROKER *stroker = (GRAPHICS_RECTLINEAR_STROKER*)closure;
	eGRAPHICS_STATUS status;

	if(FALSE == stroker->open_sub_path)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(stroker->dash.dashed)
	{
		status = GraphicsRectilinearStrokerLineToDashed(stroker, &stroker->first_point);
	}
	else
	{
		status = GraphicsRectilinearStrokerLineTo(stroker, &stroker->first_point);
	}
	if(UNLIKELY(status))
	{
		return status;
	}

	stroker->open_sub_path = FALSE;

	if(stroker->dash.dashed)
	{
		status = GraphicsRectilinearStrokerEmitSegmentsDashed(stroker);
	}
	else
	{
		status = GraphicsRectilinearStrokerEmitSegments(stroker);
	}
	if(UNLIKELY(status))
	{
		return status;
	}

	return GRAPHICS_STATUS_SUCCESS;
}

eGRAPHICS_STATUS GraphicsPathFixedStrokeRectilinearToBoxes(
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* stroke_style,
	const GRAPHICS_MATRIX* matrix,
	eGRAPHICS_ANTIALIAS antialias,
	GRAPHICS_BOXES* boxes
)
{
	GRAPHICS_RECTLINEAR_STROKER rectilinear_stroker;
	eGRAPHICS_STATUS status;
	GRAPHICS_BOX box;

	if(FALSE == InitializeGraphicsRectilinearStroker(&rectilinear_stroker,
				stroke_style, matrix, antialias, boxes))
	{
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	}

	if(FALSE == rectilinear_stroker.dash.dashed
		&& GraphicsPathFixedIsStrokeBox(path, &box)
		&& box.point2.x - box.point1.x > 2 * rectilinear_stroker.half_line_x
		&& box.point2.y - box.point1.y > 2 * rectilinear_stroker.half_line_y
	)
	{
		GRAPHICS_BOX b;

		b.point1.x = box.point1.x - rectilinear_stroker.half_line_x;
		b.point2.x = box.point2.x + rectilinear_stroker.half_line_x;
		b.point1.y = box.point1.y - rectilinear_stroker.half_line_y;
		b.point2.y = box.point1.y + rectilinear_stroker.half_line_y;
		status = GraphicsBoxesAdd(boxes, antialias, &b);

		b.point1.x = box.point1.x - rectilinear_stroker.half_line_x;
		b.point2.x = box.point1.x + rectilinear_stroker.half_line_x;
		b.point1.y = box.point1.y + rectilinear_stroker.half_line_y;
		b.point2.y = box.point2.y - rectilinear_stroker.half_line_y;
		status = GraphicsBoxesAdd(boxes, antialias, &b);

		b.point1.x = box.point2.x - rectilinear_stroker.half_line_x;
		b.point2.x = box.point2.x + rectilinear_stroker.half_line_x;
		b.point1.y = box.point1.y + rectilinear_stroker.half_line_y;
		b.point2.y = box.point2.y - rectilinear_stroker.half_line_y;
		status = GraphicsBoxesAdd(boxes, antialias, &b);

		b.point1.x = box.point1.x - rectilinear_stroker.half_line_x;
		b.point2.x = box.point2.x + rectilinear_stroker.half_line_x;
		b.point1.y = box.point2.y - rectilinear_stroker.half_line_y;
		b.point2.y = box.point2.y + rectilinear_stroker.half_line_y;
		status = GraphicsBoxesAdd(boxes, antialias, &b);

		goto DONE;
	}

	if(boxes->num_limits != 0)
	{
		GraphicsRectilinearStrokerLimit(&rectilinear_stroker,
			boxes->limits, boxes->num_limits);
	}

	status = GraphicsPathFixedInterpret(path, GraphicsRectilinearStrokerMoveTo,
				rectilinear_stroker.dash.dashed ? GraphicsRectilinearStrokerLineToDashed : GraphicsRectilinearStrokerLineTo,
				NULL, GraphicsRectilinearStrokerClosePath, &rectilinear_stroker
			 );
	if(UNLIKELY(status))
	{
		goto BAIL;
	}

	if(rectilinear_stroker.dash.dashed)
	{
		status = GraphicsRectilinearStrokerEmitSegmentsDashed(&rectilinear_stroker);
	}
	else
	{
		status = GraphicsRectilinearStrokerEmitSegments(&rectilinear_stroker);
	}
	if(UNLIKELY(status))
	{
		goto BAIL;
	}

DONE:
	GraphicsRectilinearStrokerFinish(&rectilinear_stroker);
	return GRAPHICS_INTEGER_STATUS_SUCCESS;

BAIL:
	GraphicsRectilinearStrokerFinish(&rectilinear_stroker);
	GraphicsBoxesClear(boxes);
	return status;
}

eGRAPHICS_INTEGER_STATUS GraphicsPathFixedStrokePolygonToTraps(
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* stroke_style,
	const GRAPHICS_MATRIX* matrix,
	const GRAPHICS_MATRIX* matrix_inverse,
	FLOAT_T tolerance,
	GRAPHICS_TRAPS* traps
)
{
	eGRAPHICS_INTEGER_STATUS status;
	GRAPHICS_POLYGON polygon;

	InitializeGraphicsPolygon(&polygon, traps->limits, traps->num_limits);
	status = GraphicsPathFixedStrokeToPolygon(path, stroke_style, matrix, matrix_inverse,
				tolerance, &polygon);
	if(UNLIKELY(status))
	{
		goto BAIL;
	}

	if(UNLIKELY(polygon.status))
	{
		goto BAIL;
	}

	status = GraphicsBentleyOttmannTessellatePolygon(traps, &polygon, GRAPHICS_FILL_RULE_WINDING);

BAIL:
	GraphicsPolygonFinish(&polygon);

	return status;
}

int GraphicsPathFixedIsBox(const GRAPHICS_PATH_FIXED* path, GRAPHICS_BOX* box)
{
	const GRAPHICS_PATH_BUFFER *buffer;

	if(FALSE == path->fill_is_rectilinear)
	{
		return FALSE;
	}

	if(FALSE == PathIsQuad(path))
	{
		return FALSE;
	}

	buffer = GRAPHICS_PATH_HEAD(path);
	if(PointsFromRectangle(buffer->points))
	{
		CannonicalBox(box, &buffer->points[0], &buffer->points[2]);
		return TRUE;
	}

	return FALSE;
}

int GraphicsPathFixedIsStrokeBox(
	const GRAPHICS_PATH_FIXED* path,
	GRAPHICS_BOX* box
)
{
	const GRAPHICS_PATH_BUFFER *buffer = GRAPHICS_PATH_HEAD(path);

	if(FALSE == path->fill_is_rectilinear)
	{
		return FALSE;
	}

	if(buffer->num_ops != 5)
	{
		return FALSE;
	}

	if(buffer->op[0] != GRAPHICS_PATH_MOVE_TO
		|| buffer->op[1] != GRAPHICS_PATH_LINE_TO
		|| buffer->op[2] != GRAPHICS_PATH_LINE_TO
		|| buffer->op[3] != GRAPHICS_PATH_LINE_TO
		|| buffer->op[4] != GRAPHICS_PATH_CLOSE_PATH
	)
	{
		return FALSE;
	}

	if(buffer->points[0].y == buffer->points[1].y
		&& buffer->points[1].x == buffer->points[2].x
		&& buffer->points[2].y == buffer->points[3].y
		&& buffer->points[3].x == buffer->points[0].x
	)
	{
		CannonicalBox(box, &buffer->points[0], &buffer->points[2]);
		return TRUE;
	}

	if(buffer->points[0].x == buffer->points[1].x
		&& buffer->points[1].y == buffer->points[2].y
		&& buffer->points[2].x == buffer->points[3].x
		&& buffer->points[3].y == buffer->points[0].y
	)
	{
		CannonicalBox(box, &buffer->points[0], &buffer->points[2]);
		return TRUE;
	}

	return FALSE;
}

int GraphicsPathFixedInFill(
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rule,
	FLOAT_T tolerance,
	FLOAT_T x,
	FLOAT_T y
)
{
	GRAPHICS_IN_FILL in_fill;
	eGRAPHICS_STATUS status;
	int is_inside;

	if(path->fill_is_empty)
	{
		return FALSE;
	}

	InitializeGraphicsInFill(&in_fill, tolerance, x, y);

	status = GraphicsPathFixedInterpret(path, GraphicsInFillMoveTo,
				GraphicsInFillLineTo, GraphicsInFillCurveTo, GraphicsInFillClosePath, &in_fill);
	ASSERT(status == GRAPHICS_STATUS_SUCCESS);

	GraphicsInFillClosePath(&in_fill);

	if(in_fill.on_edge)
	{
		is_inside = TRUE;
	}
	else
	{
		switch(fill_rule)
		{
		case GRAPHICS_FILL_RULE_EVEN_ODD:
			is_inside = in_fill.winding & 1;
			break;
		case GRAPHICS_FILL_RULE_WINDING:
			is_inside = in_fill.winding != 0;
			break;
		default:
			is_inside = FALSE;
			break;
		}
	}

	// GraphicsInFinish(&in_fill);

	return is_inside;
}

int GraphicsPathFixedIteratorAtEnd(const GRAPHICS_PATH_FIXED_ITERATER* iter)
{
	if(iter->buffer == NULL)
	{
		return TRUE;
	}

	return iter->num_operator == iter->buffer->num_ops;
}

typedef struct _GRAPHICS_BO_TRAP
{
	struct _GRAPHICS_BO_EDGE *right;
	int32 top;
} GRAPHICS_BO_TRAP;

typedef struct _GRAPHICS_BO_EDGE
{
	GRAPHICS_EDGE edge;
	struct _GRAPHICS_BO_EDGE *previous, *next;
	GRAPHICS_BO_TRAP deferred_trap;
} GRAPHICS_BO_EDGE;

typedef enum _eGRAPHICS_BO_EVENT_TYPE
{
	GRAPHICS_BO_EVENT_TYPE_START,
	GRAPHICS_BO_EVENT_TYPE_STOP
} eGRAPHICS_BO_EVENT_TYPE;

typedef struct _GRAPHICS_BO_EVENT
{
	eGRAPHICS_BO_EVENT_TYPE type;
	GRAPHICS_POINT point;
	GRAPHICS_BO_EDGE *edge;
} GRAPHICS_BO_EVENT;

typedef struct _GRAPHICS_BO_SWEEP_LINE
{
	GRAPHICS_BO_EVENT **events;
	GRAPHICS_BO_EDGE *head;
	GRAPHICS_BO_EDGE *stopped;
	int32 current_y;
	GRAPHICS_BO_EDGE *current_edge;
} GRAPHICS_BO_SWEEP_LINE;

static INLINE int GraphicsPointCompare(const GRAPHICS_POINT* a, const GRAPHICS_POINT* b)
{
	int compare;

	compare = a->y - b->y;
	if(LIKELY(compare))
	{
		return compare;
	}

	return a->x - b->x;
}

static INLINE int GraphicsBoEdgeCompare(const GRAPHICS_BO_EDGE* a, const GRAPHICS_BO_EDGE* b)
{
	int compare;

	compare = a->edge.line.point1.x - b->edge.line.point1.x;
	if(LIKELY(compare))
	{
		return compare;
	}

	return b->edge.bottom - a->edge.bottom;
}

static INLINE int GraphicsBoEventCompare(const GRAPHICS_BO_EVENT* a, const GRAPHICS_BO_EVENT* b)
{
	int compare;

	compare = GraphicsPointCompare(&a->point, &b->point);
	if(LIKELY(compare))
	{
		return compare;
	}

	compare = a->type - b->type;
	if(compare != 0)
	{
		return compare;
	}

	return (int)(a - b);
}

static INLINE GRAPHICS_BO_EVENT* EventDequeue(GRAPHICS_BO_SWEEP_LINE* sweep_line)
{
	return *sweep_line->events++;
}

static void GraphicsBoEventQueueSort(GRAPHICS_BO_EVENT** base, unsigned int nmemb)
{
	unsigned int gap = nmemb;
	unsigned int i, j;
	int swapped;
	do
	{
		gap = GraphicsCombsortNewGap(gap);
		swapped = gap > 1;
		for(i=0; i<nmemb-gap; i++)
		{
			j = i + gap;
			if(GraphicsBoEventCompare(base[i], base[j]) > 0)
			{
				GRAPHICS_BO_EVENT *temp;
				temp = base[i];
				base[i] = base[j];
				base[j] = temp;
				swapped = 1;
			}
		}
	} while(swapped != 0);
}

static void InitializeGraphicsBoSweepLine(
	GRAPHICS_BO_SWEEP_LINE* sweep_line,
	GRAPHICS_BO_EVENT** events,
	int num_events
)
{
	GraphicsBoEventQueueSort(events, num_events);
	events[num_events] = NULL;
	sweep_line->events = events;

	sweep_line->head = NULL;
	sweep_line->current_y = INT32_MIN;
	sweep_line->current_edge = NULL;
}

static void GraphicsBoSweepLineInsert(GRAPHICS_BO_SWEEP_LINE* sweep_line, GRAPHICS_BO_EDGE* edge)
{
	if(sweep_line->current_edge != NULL)
	{
		GRAPHICS_BO_EDGE *previous, *next;
		int compare;

		compare = GraphicsBoEdgeCompare(sweep_line->current_edge, edge);
		if(compare < 0)
		{
			previous = sweep_line->current_edge;
			next = previous->next;
			while(next != NULL && GraphicsBoEdgeCompare(next, edge) < 0)
			{
				previous = next, next = previous->next;
			}

			previous->next = edge;
			edge->previous = previous;
			edge->next = next;
			if(next != NULL)
			{
				next->previous = edge;
			}
		}
		else if(compare > 0)
		{
			next = sweep_line->current_edge;
			previous = next->previous;
			while(previous != NULL && GraphicsBoEdgeCompare(previous, edge) > 0)
			{
				next = previous, previous = next->previous;
			}

			next->previous = edge;
			edge->next = next;
			edge->previous = previous;
			if(previous != NULL)
			{
				previous->next = edge;
			}
			else
			{
				sweep_line->head = edge;
			}
		}
		else
		{
			previous = sweep_line->current_edge;
			edge->previous = previous;
			edge->next = previous->next;
			if(previous->next != NULL)
			{
				previous->next->previous = edge;
			}
			previous->next = edge;
		}
	}
	else
	{
		sweep_line->head = edge;
	}

	sweep_line->current_edge = edge;
}

static void GraphicsBoSweepLineDelete(GRAPHICS_BO_SWEEP_LINE* sweep_line, GRAPHICS_BO_EDGE* edge)
{
	if(edge->previous != NULL)
	{
		edge->previous->next = edge->next;
	}
	else
	{
		sweep_line->head = edge->next;
	}

	if(edge->next != NULL)
	{
		edge->next->previous = edge->previous;
	}

	if(sweep_line->current_edge == edge)
	{
		sweep_line->current_edge = edge->previous != NULL ? edge->previous : edge->next;
	}
}

static INLINE int EdgesColinear(const GRAPHICS_BO_EDGE* a, const GRAPHICS_BO_EDGE* b)
{
	return a->edge.line.point1.x == b->edge.line.point1.x;
}

static eGRAPHICS_STATUS GraphicsBoEdgeEndTrap(
	GRAPHICS_BO_EDGE* left,
	int32 bottom,
	int do_traps,
	void* container
)
{
	GRAPHICS_BO_TRAP *trap = &left->deferred_trap;
	eGRAPHICS_STATUS status = GRAPHICS_STATUS_SUCCESS;

	if(LIKELY(trap->top < bottom))
	{
		if(do_traps)
		{
			GraphicsTrapsAddTrap(container, trap->top, bottom,
				&left->edge.line, &trap->right->edge.line);
			status = ((GRAPHICS_TRAPS*)container)->status;
		}
		else
		{
			GRAPHICS_BOX box;

			box.point1.x = left->edge.line.point1.x;
			box.point1.y = trap->top;
			box.point2.x = trap->right->edge.line.point1.x;
			box.point2.y = bottom;
			status = GraphicsBoxesAdd(container, GRAPHICS_ANTIALIAS_DEFAULT, &box);
		}
	}

	trap->right = NULL;

	return status;
}

static INLINE eGRAPHICS_STATUS GraphicsBoEdgeStartOrContinueTrap(
	GRAPHICS_BO_EDGE* left,
	GRAPHICS_BO_EDGE* right,
	int top,
	int do_traps,
	void* container
)
{
	eGRAPHICS_STATUS status;

	if(left->deferred_trap.right == right)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	//if(left->deferred_trap.right != NULL)
	{
		if(right != NULL && EdgesColinear(left->deferred_trap.right, right))
		{
			left->deferred_trap.right = right;
			return GRAPHICS_STATUS_SUCCESS;
		}

		status = GraphicsBoEdgeEndTrap(left, top, do_traps, container);
		if(UNLIKELY(status))
		{
			return status;
		}
	}

	if(right != NULL && FALSE == EdgesColinear(left, right))
	{
		left->deferred_trap.top = top;
		left->deferred_trap.right = right;
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static INLINE eGRAPHICS_STATUS ActiveEdgesToTraps(
	GRAPHICS_BO_EDGE* left,
	int32 top,
	eGRAPHICS_FILL_RULE fill_rule,
	int do_traps,
	void* container
)
{
	GRAPHICS_BO_EDGE *right;
	eGRAPHICS_STATUS status;

	if(fill_rule == GRAPHICS_FILL_RULE_WINDING)
	{
		while(left != NULL)
		{
			int in_out;

			in_out = left->edge.direction;

			right = left->next;
			if(left->deferred_trap.right == NULL)
			{
				while(right != NULL && right->deferred_trap.right == NULL)
				{
					right = right->next;
				}
				if(right != NULL && EdgesColinear(left, right))
				{
					left->deferred_trap = right->deferred_trap;
					right->deferred_trap.right = NULL;
				}
			}

			right = left->next;
			while(right != NULL)
			{
				if(right->deferred_trap.right != NULL)
				{
					status = GraphicsBoEdgeEndTrap(right, top, do_traps, container);
					if(UNLIKELY(status))
					{
						return status;
					}
				}

				in_out += right->edge.direction;
				if(in_out == 0)
				{
					if(right == NULL || FALSE == EdgesColinear(right, right->next))
					{
						break;
					}
				}

				right = right->next;
			}

			status = GraphicsBoEdgeStartOrContinueTrap(left, right, top,
						do_traps, container);
			if(UNLIKELY(status))
			{
				return status;
			}

			left = right;
			if(left != NULL)
			{
				left = left->next;
			}
		}
	}
	else
	{
		while(left != NULL)
		{
			int in_out = 0;

			right = left->next;
			while(right != NULL)
			{
				if(right->deferred_trap.right != NULL)
				{
					status = GraphicsBoEdgeEndTrap(right, top, do_traps, container);
					if(UNLIKELY(status))
					{
						return status;
					}
				}

				if((in_out++ & 1) == 0)
				{
					GRAPHICS_BO_EDGE *next;
					int skip = FALSE;

					next = right->next;
					if(next != NULL)
					{
						skip = EdgesColinear(right, next);
					}

					if(FALSE == skip)
					{
						break;
					}
				}

				right = right->next;
			}

			status = GraphicsBoEdgeStartOrContinueTrap(left, right, top,
						do_traps, container);
			if(UNLIKELY(status))
			{
				return status;
			}

			left = right;
			if(left != NULL)
			{
				left = left->next;
			}
		}
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static INLINE GRAPHICS_BO_EVENT* GraphicsBoEventDequeue(GRAPHICS_BO_SWEEP_LINE* sweep_line)
{
	return *sweep_line->events++;
}

static eGRAPHICS_STATUS GraphicsBentleyOttmannTessellateRectilinear(
	GRAPHICS_BO_EVENT** start_events,
	int num_events,
	eGRAPHICS_FILL_RULE fill_rule,
	int do_traps,
	void* container
)
{
	GRAPHICS_BO_SWEEP_LINE sweep_line;
	GRAPHICS_BO_EVENT *event;
	eGRAPHICS_STATUS status;

	InitializeGraphicsBoSweepLine(&sweep_line, start_events, num_events);

	while((event = GraphicsBoEventDequeue(&sweep_line)) != NULL)
	{
		status = ActiveEdgesToTraps(sweep_line.head, sweep_line.current_y,
			fill_rule, do_traps, container);
		if(UNLIKELY(status))
		{
			return status;
		}

		sweep_line.current_y = event->point.y;
	}

	switch(event->type)
	{
	case GRAPHICS_BO_EVENT_TYPE_START:
		GraphicsBoSweepLineInsert(&sweep_line, event->edge);
		break;

	case GRAPHICS_BO_EVENT_TYPE_STOP:
		if(event->edge->deferred_trap.right != NULL)
		{
			status = GraphicsBoEdgeEndTrap(event->edge, sweep_line.current_y, do_traps, container);
			if(UNLIKELY(status))
			{
				return status;
			}
		}
		break;
	}

	return GRAPHICS_STATUS_SUCCESS;
}

eGRAPHICS_STATUS GraphicsBentleyOttmannTessellateRectilinearPolygonTolBoxes(
	const GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE fill_rule,
	GRAPHICS_BOXES* boxes
)
{
	eGRAPHICS_STATUS status;
	GRAPHICS_BO_EVENT stack_events[STACK_BUFFER_SIZE/sizeof(GRAPHICS_BO_EVENT)];
	GRAPHICS_BO_EVENT *events;
	GRAPHICS_BO_EVENT *stack_event_pointers[sizeof(stack_events)/sizeof(stack_events[0])+1];
	GRAPHICS_BO_EVENT **event_pointers;
	GRAPHICS_BO_EDGE stack_edges[sizeof(stack_events)/sizeof(stack_events[0])];
	GRAPHICS_BO_EDGE *edges;
	int num_events;
	int i, j;

	if(UNLIKELY(polygon->num_edges == 0))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	num_events = 2 * polygon->num_edges;

	events = stack_events;
	event_pointers = stack_event_pointers;
	edges = stack_edges;
	if(num_events > (sizeof(stack_events)/sizeof(stack_events[0])))
	{
		events = (GRAPHICS_BO_EVENT*)MEM_ALLOC_FUNC(
					num_events * (sizeof(GRAPHICS_BO_EVENT) + sizeof(GRAPHICS_BO_EDGE)+sizeof(GRAPHICS_BO_EVENT*))+sizeof(GRAPHICS_BO_EVENT*));
		if(events == NULL)
		{
			return GRAPHICS_STATUS_NO_MEMORY;
		}

		event_pointers = (GRAPHICS_BO_EVENT**)(events + num_events);
		edges = (GRAPHICS_BO_EDGE*)(event_pointers + num_events + 1);
	}

	for(i = j = 0; i < polygon->num_edges; i++)
	{
		edges[i].edge = polygon->edges[i];
		edges[i].deferred_trap.right = NULL;
		edges[i].previous = NULL;
		edges[i].next = NULL;

		event_pointers[j] = &events[j];
		events[j].type = GRAPHICS_BO_EVENT_TYPE_START;
		events[j].point.y = polygon->edges[i].top;
		events[j].point.x = polygon->edges[i].line.point1.x;
		events[j].edge = &edges[i];
		j++;

		event_pointers[j] = &events[i];
		events[j].type = GRAPHICS_BO_EVENT_TYPE_STOP;
		events[j].point.y = polygon->edges[i].bottom;
		events[j].point.x = polygon->edges[i].line.point1.x;
		events[j].edge = &edges[i];
		j++;
	}

	status = GraphicsBentleyOttmannTessellateRectilinear(event_pointers,
				j, fill_rule, FALSE, boxes);
	if(events != stack_events)
	{
		MEM_FREE_FUNC(events);
	}

	return status;
}

eGRAPHICS_STATUS GraphicsPathFixedFillToTraps(
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rule,
	FLOAT_T tolerance,
	GRAPHICS_TRAPS* traps
)
{
	GRAPHICS_POLYGON polygon;
	eGRAPHICS_STATUS status;

	if(path->fill_is_empty)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	InitializeGraphicsPolygon(&polygon, traps->limits, traps->num_limits);
	status = GraphicsPathFixedFillToPolygon(path, tolerance, &polygon);
	if(UNLIKELY(status || polygon.num_edges == 0))
	{
		goto CLEAN_UP;
	}

	status = GraphicsBentleyOttmannTessellatePolygon(traps, &polygon, fill_rule);

CLEAN_UP:
	GraphicsPolygonFinish(&polygon);
	return status;
}

eGRAPHICS_STATUS GraphicsPathFixedFillRectilinearToBoxes(
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rule,
	eGRAPHICS_ANTIALIAS antialias,
	GRAPHICS_BOXES* boxes
)
{
	GRAPHICS_PATH_FIXED_ITERATER iter;
	eGRAPHICS_STATUS status;
	GRAPHICS_BOX box;

	if(GraphicsPathFixedIsBox(path, &box))
	{
		return GraphicsBoxesAdd(boxes, antialias, &box);
	}

	InitializeGraphicsPathFixedIterator(&iter, path);
	while(GraphicsPathFixedIteratorIsFillBox(&iter, &box))
	{
		if(box.point1.y == box.point2.y || box.point1.x == box.point2.x)
		{
			continue;
		}

		if(box.point1.y > box.point2.y)
		{
			GRAPHICS_FLOAT_FIXED temp;

			temp = box.point1.y;
			box.point1.y = box.point2.y;
			box.point2.y = temp;

			temp = box.point1.x;
			box.point1.x = box.point2.x;
			box.point2.x = temp;
		}

		status = GraphicsBoxesAdd(boxes, antialias, &box);
		if(UNLIKELY(status))
		{
			return status;
		}
	}

	if(GraphicsPathFixedIteratorAtEnd(&iter))
	{
		return GraphicsBentleyOttmannTessellateBoxes(boxes, fill_rule, boxes);
	}

	GraphicsBoxesClear(boxes);
	return GraphicsPathFixedFillRectilinearTessellateToBoxes(path,
			fill_rule, antialias, boxes);
}

struct stroker
{
	const GRAPHICS_STROKE_STYLE	*style;

	const GRAPHICS_MATRIX *ctm;
	const GRAPHICS_MATRIX *ctm_inverse;
	double spline_cusp_tolerance;
	double half_line_width;
	double tolerance;
	double ctm_determinant;
	int ctm_det_positive;
	eGRAPHICS_LINE_JOIN line_join;

	GRAPHICS_TRAPS *traps;
	
	GRAPHICS_PEN pen;

	GRAPHICS_POINT first_point;

	int has_initial_sub_path;
	
	int has_current_face;
	GRAPHICS_STROKE_FACE current_face;

	int has_first_face;
	GRAPHICS_STROKE_FACE first_face;

	GRAPHICS_STROKER_DASH dash;

	int has_bounds;
	GRAPHICS_BOX tight_bounds;
	GRAPHICS_BOX line_bounds;
	GRAPHICS_BOX join_bounds;
};

static eGRAPHICS_STATUS stroker_init(
	struct stroker* stroker,
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_MATRIX* ctm,
	const GRAPHICS_MATRIX* ctm_inverse,
	double tolerance,
	GRAPHICS_TRAPS* traps
)
{
	eGRAPHICS_STATUS status;
	
	stroker->style = style;
	stroker->ctm = ctm;
	stroker->ctm_inverse = NULL;
	if (! GRAPHICS_MATRIX_IS_IDENTITY(ctm_inverse))
		stroker->ctm_inverse = ctm_inverse;
	stroker->line_join = style->line_join;
	stroker->half_line_width = style->line_width / 2.0;
	stroker->tolerance = tolerance;
	stroker->traps = traps;

	/* To test whether we need to join two segments of a spline using
	* a round-join or a bevel-join, we can inspect the angle between the
	* two segments. If the difference between the chord distance
	* (half-line-width times the cosine of the bisection angle) and the
	* half-line-width itself is greater than tolerance then we need to
	* inject a point.
	*/
	stroker->spline_cusp_tolerance = 1 - tolerance / stroker->half_line_width;
	stroker->spline_cusp_tolerance *= stroker->spline_cusp_tolerance;
	stroker->spline_cusp_tolerance *= 2;
	stroker->spline_cusp_tolerance -= 1;

	stroker->ctm_determinant = GRAPHICS_MATRIX_COMPUTE_DETERMINANT(stroker->ctm);
	stroker->ctm_det_positive = stroker->ctm_determinant >= 0.0;

	status = InitializeGraphicsPen(&stroker->pen,
		stroker->half_line_width,
		tolerance, ctm);
	if(UNLIKELY(status))
		return status;

	stroker->has_current_face = FALSE;
	stroker->has_first_face = FALSE;
	stroker->has_initial_sub_path = FALSE;

	InitializeGraphicsStrokerDash(&stroker->dash, style);

	stroker->has_bounds = traps->num_limits;
	if(stroker->has_bounds)
	{
		/* Extend the bounds in each direction to account for the maximum area
		* we might generate trapezoids, to capture line segments that are outside
		* of the bounds but which might generate rendering that's within bounds.
		*/
		double dx, dy;
		GRAPHICS_FLOAT_FIXED fdx, fdy;

		stroker->tight_bounds = traps->bounds;

		GraphicsStrokeStyleMaxDistanceFromPath(stroker->style, path,
			stroker->ctm, &dx, &dy);

		GraphicsStrokeStyleMaxLineDistanceFromPath(stroker->style, path,
			stroker->ctm, &dx, &dy);
		
		fdx = GraphicsFixedFromFloat(dx);
		fdy = GraphicsFixedFromFloat(dy);

		stroker->line_bounds = stroker->tight_bounds;
		stroker->line_bounds.point1.x -= fdx;
		stroker->line_bounds.point2.x += fdx;
		stroker->line_bounds.point1.y -= fdy;
		stroker->line_bounds.point2.y += fdy;

		GraphicsStrokeStyleMaxJoinDistanceFromPath(stroker->style, path,
			stroker->ctm, &dx, &dy);

		fdx = GraphicsFixedFromFloat(dx);
		fdy = GraphicsFixedFromFloat(dy);

		stroker->join_bounds = stroker->tight_bounds;
		stroker->join_bounds.point1.x -= fdx;
		stroker->join_bounds.point2.x += fdx;
		stroker->join_bounds.point1.y -= fdy;
		stroker->join_bounds.point2.y += fdy;
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static void stroker_fini(struct stroker *stroker)
{
	GraphicsPenFinish(&stroker->pen);
}

static void translate_point(GRAPHICS_POINT *point, GRAPHICS_POINT *offset)
{
	point->x += offset->x;
	point->y += offset->y;
}

static int join_is_clockwise(const GRAPHICS_STROKE_FACE *in, const GRAPHICS_STROKE_FACE *out)
{
	return GraphicsSlopeCompare(&in->device_vector, &out->device_vector) < 0;
}

static int slope_compare_sgn(double dx1, double dy1, double dx2, double dy2)
{
	double c = dx1 * dy2 - dx2 * dy1;
	if (c > 0) return 1;
	if (c < 0) return -1;
	return 0;
}

static int stroker_intersects_join(
	const struct stroker *stroker,
	const GRAPHICS_POINT *in,
	const GRAPHICS_POINT *out
)
{
	GRAPHICS_LINE segment;

	if (! stroker->has_bounds)
		return TRUE;
	
	segment.point1 = *in;
	segment.point2 = *out;
	return GraphicsBoxIntersectsLineSegment(&stroker->join_bounds, &segment);
}

static void join(
	struct stroker *stroker,
	GRAPHICS_STROKE_FACE *in,
	GRAPHICS_STROKE_FACE *out
)
{
	int clockwise = join_is_clockwise (out, in);
	GRAPHICS_POINT *inpt, *outpt;

	if (in->clock_wise.x == out->clock_wise.x &&
		in->clock_wise.y == out->clock_wise.y &&
		in->counter_clock_wise.x == out->counter_clock_wise.x &&
		in->counter_clock_wise.y == out->counter_clock_wise.y)
	{
		return;
	}

	if (clockwise) {
		inpt = &in->counter_clock_wise;
		outpt = &out->counter_clock_wise;
	} else {
		inpt = &in->clock_wise;
		outpt = &out->clock_wise;
	}

	if (! stroker_intersects_join (stroker, inpt, outpt))
		return;

	switch (stroker->line_join)
	{
	case GRAPHICS_LINE_JOIN_ROUND:
		/* construct a fan around the common midpoint */
		if ((in->device_slope.x * out->device_slope.x +
			in->device_slope.y * out->device_slope.y) < stroker->spline_cusp_tolerance)
		{
			int start, stop;
			GRAPHICS_POINT tri[3], edges[4];
			GRAPHICS_PEN *pen = &stroker->pen;

			edges[0] = in->clock_wise;
			edges[1] = in->counter_clock_wise;
			tri[0] = in->point;
			tri[1] = *inpt;
			if(clockwise)
			{
				GraphicsPenFindActiveCounterClockwiseVertices(pen,
					&in->device_vector, &out->device_vector,
					&start, &stop);
				while (start != stop)
				{
					tri[2] = in->point;
					translate_point (&tri[2], &pen->vertices[start].point);
					edges[2] = in->point;
					edges[3] = tri[2];
					GraphicsTrapsTessellateTriangleWithEdges(stroker->traps,
						tri, edges);
					tri[1] = tri[2];
					edges[0] = edges[2];
					edges[1] = edges[3];

					if(start-- == 0)
						start += pen->num_vertices;
				}
			}
			else
			{
				GraphicsPenFindActiveClockwiseVertices(pen,
					&in->device_vector, &out->device_vector,
					&start, &stop);
				while(start != stop)
				{
					tri[2] = in->point;
					translate_point (&tri[2], &pen->vertices[start].point);
					edges[2] = in->point;
					edges[3] = tri[2];
					GraphicsTrapsTessellateTriangleWithEdges(stroker->traps,
						tri, edges);
					tri[1] = tri[2];
					edges[0] = edges[2];
					edges[1] = edges[3];

					if (++start == pen->num_vertices)
						start = 0;
				}
			}
			tri[2] = *outpt;
			edges[2] = out->clock_wise;
			edges[3] = out->counter_clock_wise;
			GraphicsTrapsTessellateTriangleWithEdges(stroker->traps,
				tri, edges);
		} else {
			GRAPHICS_POINT t[] = { { in->point.x, in->point.y}, { inpt->x, inpt->y }, { outpt->x, outpt->y } };
			GRAPHICS_POINT e[] = { { in->clock_wise.x, in->clock_wise.y}, { in->counter_clock_wise.x, in->counter_clock_wise.y },
			{ out->clock_wise.x, out->clock_wise.y}, { out->counter_clock_wise.x, out->counter_clock_wise.y } };
			GraphicsTrapsTessellateTriangleWithEdges(stroker->traps, t, e);
		}
		break;

	case GRAPHICS_LINE_JOIN_MITER:
	default:
		{
		/* dot product of incoming slope vector with outgoing slope vector */
		double in_dot_out = (-in->user_vector.x * out->user_vector.x +
			-in->user_vector.y * out->user_vector.y);
		double ml = stroker->style->miter_limit;

		/* Check the miter limit -- lines meeting at an acute angle
		* can generate long miters, the limit converts them to bevel
		*
		* Consider the miter join formed when two line segments
		* meet at an angle psi:
		*
		*	   /.\
		*	  /. .\
		*	 /./ \.\
		*	/./psi\.\
		*
		* We can zoom in on the right half of that to see:
		*
		*		|\
		*		| \ psi/2
		*		|  \
		*		|   \
		*		|	\
		*		|	 \
		*	  miter	\
		*	 length	 \
		*		|		\
		*		|		.\
		*		|	.	 \
		*		|.   line   \
		*		 \	width  \
		*		  \		   \
		*
		*
		* The right triangle in that figure, (the line-width side is
		* shown faintly with three '.' characters), gives us the
		* following expression relating miter length, angle and line
		* width:
		*
		*	1 /sin (psi/2) = miter_length / line_width
		*
		* The right-hand side of this relationship is the same ratio
		* in which the miter limit (ml) is expressed. We want to know
		* when the miter length is within the miter limit. That is
		* when the following condition holds:
		*
		*	1/sin(psi/2) <= ml
		*	1 <= ml sin(psi/2)
		*	1 <= ml² sin²(psi/2)
		*	2 <= ml² 2 sin²(psi/2)
		*				2·sin²(psi/2) = 1-cos(psi)
		*	2 <= ml² (1-cos(psi))
		*
		*				in · out = |in| |out| cos (psi)
		*
		* in and out are both unit vectors, so:
		*
		*				in · out = cos (psi)
		*
		*	2 <= ml² (1 - in · out)
		*
		*/
		if (2 <= ml * ml * (1 - in_dot_out))
		{
			double		x1, y1, x2, y2;
			double		mx, my;
			double		dx1, dx2, dy1, dy2;
			GRAPHICS_POINT	outer;
			GRAPHICS_POINT	quad[4];
			double		ix, iy;
			double		fdx1, fdy1, fdx2, fdy2;
			double		mdx, mdy;

			/*
			* we've got the points already transformed to device
			* space, but need to do some computation with them and
			* also need to transform the slope from user space to
			* device space
			*/
			/* outer point of incoming line face */
			x1 = GraphicsFixedFromFloat(inpt->x);
			y1 = GraphicsFixedFromFloat(inpt->y);
			dx1 = in->user_vector.x;
			dy1 = in->user_vector.y;
			GraphicsMatrixTransformDistance(stroker->ctm, &dx1, &dy1);

			/* outer point of outgoing line face */
			x2 = GraphicsFixedFromFloat(outpt->x);
			y2 = GraphicsFixedFromFloat(outpt->y);
			dx2 = out->user_vector.x;
			dy2 = out->user_vector.y;
			GraphicsMatrixTransformDistance(stroker->ctm, &dx2, &dy2);

			/*
			* Compute the location of the outer corner of the miter.
			* That's pretty easy -- just the intersection of the two
			* outer edges.  We've got slopes and points on each
			* of those edges.  Compute my directly, then compute
			* mx by using the edge with the larger dy; that avoids
			* dividing by values close to zero.
			*/
			my = (((x2 - x1) * dy1 * dy2 - y2 * dx2 * dy1 + y1 * dx1 * dy2) /
				(dx1 * dy2 - dx2 * dy1));
			if (fabs (dy1) >= fabs (dy2))
				mx = (my - y1) * dx1 / dy1 + x1;
			else
				mx = (my - y2) * dx2 / dy2 + x2;

			/*
			* When the two outer edges are nearly parallel, slight
			* perturbations in the position of the outer points of the lines
			* caused by representing them in fixed point form can cause the
			* intersection point of the miter to move a large amount. If
			* that moves the miter intersection from between the two faces,
			* then draw a bevel instead.
			*/

			ix = GraphicsFixedFromFloat(in->point.x);
			iy = GraphicsFixedFromFloat(in->point.y);

			/* slope of one face */
			fdx1 = x1 - ix; fdy1 = y1 - iy;

			/* slope of the other face */
			fdx2 = x2 - ix; fdy2 = y2 - iy;

			/* slope from the intersection to the miter point */
			mdx = mx - ix; mdy = my - iy;

			/*
			* Make sure the miter point line lies between the two
			* faces by comparing the slopes
			*/
			if (slope_compare_sgn (fdx1, fdy1, mdx, mdy) !=
				slope_compare_sgn (fdx2, fdy2, mdx, mdy))
			{
				/*
				* Draw the quadrilateral
				*/
				outer.x = GraphicsFixedFromFloat(mx);
				outer.y = GraphicsFixedFromFloat(my);

				quad[0] = in->point;
				quad[1] = *inpt;
				quad[2] = outer;
				quad[3] = *outpt;

				GraphicsTrapsTessellateConvexQuad(stroker->traps, quad);
				break;
			}
		}
		/* fall through ... */
	}

	case GRAPHICS_LINE_JOIN_BEVEL:
		{
			GRAPHICS_POINT t[] = { { in->point.x, in->point.y }, { inpt->x, inpt->y }, { outpt->x, outpt->y } };
			GRAPHICS_POINT e[] = { { in->clock_wise.x, in->clock_wise.y }, { in->counter_clock_wise.x, in->counter_clock_wise.y },
			{ out->clock_wise.x, out->clock_wise.y }, { out->counter_clock_wise.x, out->counter_clock_wise.y } };
			GraphicsTrapsTessellateTriangleWithEdges(stroker->traps, t, e);
			break;
		}
	}
}

static void add_cap(struct stroker *stroker, GRAPHICS_STROKE_FACE *f)
{
	switch (stroker->style->line_cap)
	{
	case GRAPHICS_LINE_CAP_ROUND:
	{
		int start, stop;
		GRAPHICS_SLOPE in_slope, out_slope;
		GRAPHICS_POINT tri[3], edges[4];
		GRAPHICS_PEN *pen = &stroker->pen;

		in_slope = f->device_vector;
		out_slope.dx = -in_slope.dx;
		out_slope.dy = -in_slope.dy;
		GraphicsPenFindActiveClockwiseVertices(pen, &in_slope, &out_slope,
			&start, &stop);
		edges[0] = f->clock_wise;
		edges[1] = f->counter_clock_wise;
		tri[0] = f->point;
		tri[1] = f->clock_wise;
		while (start != stop)
		{
			tri[2] = f->point;
			translate_point (&tri[2], &pen->vertices[start].point);
			edges[2] = f->point;
			edges[3] = tri[2];
			GraphicsTrapsTessellateTriangleWithEdges(stroker->traps,
				tri, edges);

			tri[1] = tri[2];
			edges[0] = edges[2];
			edges[1] = edges[3];
			if (++start == pen->num_vertices)
				start = 0;
		}
		tri[2] = f->counter_clock_wise;
		edges[2] = f->clock_wise;
		edges[3] = f->counter_clock_wise;
		GraphicsTrapsTessellateTriangleWithEdges(stroker->traps,
			tri, edges);
		break;
	}

	case GRAPHICS_LINE_CAP_SQUARE: {
		double dx, dy;
		GRAPHICS_SLOPE fvector;
		GRAPHICS_POINT quad[4];

		dx = f->user_vector.x;
		dy = f->user_vector.y;
		dx *= stroker->half_line_width;
		dy *= stroker->half_line_width;
		GraphicsMatrixTransformDistance(stroker->ctm, &dx, &dy);
		fvector.dx = GraphicsFixedFromFloat(dx);
		fvector.dy = GraphicsFixedFromFloat(dy);

		quad[0] = f->clock_wise;
		quad[1].x = f->clock_wise.x + fvector.dx;
		quad[1].y = f->clock_wise.y + fvector.dy;
		quad[2].x = f->counter_clock_wise.x + fvector.dx;
		quad[2].y = f->counter_clock_wise.y + fvector.dy;
		quad[3] = f->counter_clock_wise;

		GraphicsTrapsTessellateConvexQuad(stroker->traps, quad);
		break;
	}

	case GRAPHICS_LINE_CAP_BUTT:
	default:
		break;
	}
}

static void add_leading_cap(
	struct stroker	 *stroker,
	GRAPHICS_STROKE_FACE *face
)
{
	GRAPHICS_STROKE_FACE reversed;
	GRAPHICS_POINT t;

	reversed = *face;

	/* The initial cap needs an outward facing vector. Reverse everything */
	reversed.user_vector.x = -reversed.user_vector.x;
	reversed.user_vector.y = -reversed.user_vector.y;
	reversed.device_vector.dx = -reversed.device_vector.dx;
	reversed.device_vector.dy = -reversed.device_vector.dy;
	t = reversed.clock_wise;
	reversed.clock_wise = reversed.counter_clock_wise;
	reversed.counter_clock_wise = t;

	add_cap(stroker, &reversed);
}

static void add_trailing_cap(struct stroker *stroker, GRAPHICS_STROKE_FACE *face)
{
	add_cap(stroker, face);
}

static INLINE double normalize_slope(double *dx, double *dy)
{
	double dx0 = *dx, dy0 = *dy;

	if (dx0 == 0.0 && dy0 == 0.0)
		return 0;

	if (dx0 == 0.0)
	{
		*dx = 0.0;
		if (dy0 > 0.0)
		{
			*dy = 1.0;
			return dy0;
		}
		else
		{
			*dy = -1.0;
			return -dy0;
		}
	}
	else if(dy0 == 0.0)
	{
		*dy = 0.0;
		if(dx0 > 0.0)
		{
			*dx = 1.0;
			return dx0;
		}
		else
		{
			*dx = -1.0;
			return -dx0;
		}
	}
	else
	{
		double mag = hypot(dx0, dy0);
		*dx = dx0 / mag;
		*dy = dy0 / mag;
		return mag;
	}
}

static void compute_face(
	const GRAPHICS_POINT *point,
	const GRAPHICS_SLOPE *dev_slope,
	struct stroker *stroker,
	GRAPHICS_STROKE_FACE *face
)
{
	double face_dx, face_dy;
	GRAPHICS_POINT offset_ccw, offset_cw;
	double slope_dx, slope_dy;

	slope_dx = GraphicsFixedFromFloat(dev_slope->dx);
	slope_dy = GraphicsFixedFromFloat(dev_slope->dy);
	face->length = normalize_slope(&slope_dx, &slope_dy);
	face->device_slope.x = slope_dx;
	face->device_slope.y = slope_dy;

	/*
	* rotate to get a line_width/2 vector along the face, note that
	* the vector must be rotated the right direction in device space,
	* but by 90° in user space. So, the rotation depends on
	* whether the ctm reflects or not, and that can be determined
	* by looking at the determinant of the matrix.
	*/
	if(stroker->ctm_inverse)
	{
		GraphicsMatrixTransformDistance(stroker->ctm_inverse, &slope_dx, &slope_dy);
		normalize_slope (&slope_dx, &slope_dy);

		if(stroker->ctm_det_positive)
		{
			face_dx = - slope_dy * stroker->half_line_width;
			face_dy = slope_dx * stroker->half_line_width;
		}
		else
		{
			face_dx = slope_dy * stroker->half_line_width;
			face_dy = - slope_dx * stroker->half_line_width;
		}

		/* back to device space */
		GraphicsMatrixTransformDistance(stroker->ctm, &face_dx, &face_dy);
	}
	else
	{
		face_dx = - slope_dy * stroker->half_line_width;
		face_dy = slope_dx * stroker->half_line_width;
	}

	offset_ccw.x = GraphicsFixedFromFloat(face_dx);
	offset_ccw.y = GraphicsFixedFromFloat(face_dy);
	offset_cw.x = -offset_ccw.x;
	offset_cw.y = -offset_ccw.y;

	face->counter_clock_wise = *point;
	translate_point (&face->counter_clock_wise, &offset_ccw);

	face->point = *point;

	face->clock_wise = *point;
	translate_point (&face->clock_wise, &offset_cw);

	face->user_vector.x = slope_dx;
	face->user_vector.y = slope_dy;

	face->device_vector = *dev_slope;
}

static void add_caps(struct stroker *stroker)
{
	/* check for a degenerative sub_path */
	if (stroker->has_initial_sub_path &&
		!stroker->has_first_face &&
		!stroker->has_current_face &&
		stroker->style->line_cap == GRAPHICS_LINE_CAP_ROUND)
	{
		/* pick an arbitrary slope to use */
		GRAPHICS_SLOPE slope = { GRAPHICS_FIXED_ONE, 0 };
		GRAPHICS_STROKE_FACE face;

		/* arbitrarily choose first_point
		* first_point and current_point should be the same */
		compute_face (&stroker->first_point, &slope, stroker, &face);

		add_leading_cap(stroker, &face);
		add_trailing_cap(stroker, &face);
	}

	if (stroker->has_first_face)
		add_leading_cap(stroker, &stroker->first_face);

	if (stroker->has_current_face)
		add_trailing_cap(stroker, &stroker->current_face);
}

static int stroker_intersects_edge(
	const struct stroker *stroker,
	const GRAPHICS_STROKE_FACE *start,
	const GRAPHICS_STROKE_FACE *end
)
{
	GRAPHICS_BOX box;

	if (! stroker->has_bounds)
		return TRUE;
	
	if (GraphicsBoxContainsPoint(&stroker->tight_bounds, &start->clock_wise))
		return TRUE;
	box.point2 = box.point1 = start->clock_wise;

	if (GraphicsBoxContainsPoint(&stroker->tight_bounds, &start->counter_clock_wise))
		return TRUE;
	GraphicsBoxAddPoint(&box, &start->counter_clock_wise);

	if (GraphicsBoxContainsPoint(&stroker->tight_bounds, &end->clock_wise))
		return TRUE;
	GraphicsBoxAddPoint(&box, &end->clock_wise);

	if (GraphicsBoxContainsPoint(&stroker->tight_bounds, &end->counter_clock_wise))
		return TRUE;
	GraphicsBoxAddPoint(&box, &end->counter_clock_wise);

	return (box.point2.x > stroker->tight_bounds.point1.x &&
		box.point1.x < stroker->tight_bounds.point2.x &&
		box.point2.y > stroker->tight_bounds.point1.y &&
		box.point1.y < stroker->tight_bounds.point2.y);
}

static void add_sub_edge(
	struct stroker *stroker,
	const GRAPHICS_POINT *p1, const GRAPHICS_POINT *p2,
	const GRAPHICS_SLOPE *dev_slope,
	GRAPHICS_STROKE_FACE *start, GRAPHICS_STROKE_FACE *end
)
{
	GRAPHICS_POINT rectangle[4];

	compute_face (p1, dev_slope, stroker, start);

	*end = *start;
	end->point = *p2;
	rectangle[0].x = p2->x - p1->x;
	rectangle[0].y = p2->y - p1->y;
	translate_point (&end->counter_clock_wise, &rectangle[0]);
	translate_point (&end->clock_wise, &rectangle[0]);

	if (p1->x == p2->x && p1->y == p2->y)
		return;

	if (! stroker_intersects_edge (stroker, start, end))
		return;

	rectangle[0] = start->clock_wise;
	rectangle[1] = start->counter_clock_wise;
	rectangle[2] = end->counter_clock_wise;
	rectangle[3] = end->clock_wise;

	GraphicsTrapsTessellateConvexQuad(stroker->traps, rectangle);
}

static eGRAPHICS_STATUS move_to(void *closure, const GRAPHICS_POINT *point)
{
	struct stroker *stroker = closure;

	/* Cap the start and end of the previous sub path as needed */
	add_caps (stroker);

	stroker->first_point = *point;
	stroker->current_face.point = *point;

	stroker->has_first_face = FALSE;
	stroker->has_current_face = FALSE;
	stroker->has_initial_sub_path = FALSE;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS move_to_dashed(void *closure, const GRAPHICS_POINT *point)
{
	/* reset the dash pattern for new sub paths */
	struct stroker *stroker = closure;
	
	GraphicsStrokerDashStart(&stroker->dash);
	return move_to (closure, point);
}

static eGRAPHICS_STATUS line_to(void *closure, const GRAPHICS_POINT *point)
{
	struct stroker *stroker = closure;
	GRAPHICS_STROKE_FACE start, end;
	const GRAPHICS_POINT *p1 = &stroker->current_face.point;
	const GRAPHICS_POINT *p2 = point;
	GRAPHICS_SLOPE dev_slope;

	stroker->has_initial_sub_path = TRUE;

	if (p1->x == p2->x && p1->y == p2->y)
		return GRAPHICS_STATUS_SUCCESS;

	INITIALIZE_GRAPHICS_SLOPE(&dev_slope, p1, p2);
	add_sub_edge (stroker, p1, p2, &dev_slope, &start, &end);

	if (stroker->has_current_face)
	{
		/* Join with final face from previous segment */
		join (stroker, &stroker->current_face, &start);
	}
	else if(!stroker->has_first_face)
	{
		/* Save sub path's first face in case needed for closing join */
		stroker->first_face = start;
		stroker->has_first_face = TRUE;
	}
	stroker->current_face = end;
	stroker->has_current_face = TRUE;

	return GRAPHICS_STATUS_SUCCESS;
}

/*
* Dashed lines.  Cap each dash end, join around turns when on
*/
static eGRAPHICS_STATUS line_to_dashed(void *closure, const GRAPHICS_POINT *point)
{
	struct stroker *stroker = closure;
	double mag, remain, step_length = 0;
	double slope_dx, slope_dy;
	double dx2, dy2;
	GRAPHICS_STROKE_FACE sub_start, sub_end;
	const GRAPHICS_POINT *p1 = &stroker->current_face.point;
	const GRAPHICS_POINT *p2 = point;
	GRAPHICS_SLOPE dev_slope;
	GRAPHICS_LINE segment;
	int fully_in_bounds;

	stroker->has_initial_sub_path = stroker->dash.dash_starts_on;

	if (p1->x == p2->x && p1->y == p2->y)
		return GRAPHICS_STATUS_SUCCESS;

	fully_in_bounds = TRUE;
	if (stroker->has_bounds &&
		(! GraphicsBoxContainsPoint(&stroker->join_bounds, p1) ||
			! GraphicsBoxContainsPoint(&stroker->join_bounds, p2)))
	{
		fully_in_bounds = FALSE;
	}
	
	INITIALIZE_GRAPHICS_SLOPE(&dev_slope, p1, p2);

	slope_dx = GraphicsFixedFromFloat(p2->x - p1->x);
	slope_dy = GraphicsFixedFromFloat(p2->y - p1->y);

	if (stroker->ctm_inverse)
		GraphicsMatrixTransformDistance(stroker->ctm_inverse, &slope_dx, &slope_dy);
	mag = normalize_slope (&slope_dx, &slope_dy);
	if (mag <= DBL_EPSILON)
		return GRAPHICS_STATUS_SUCCESS;

	remain = mag;
	segment.point1 = *p1;
	while (remain)
	{
		step_length = MINIMUM(stroker->dash.dash_remain, remain);
		remain -= step_length;
		dx2 = slope_dx * (mag - remain);
		dy2 = slope_dy * (mag - remain);
		GraphicsMatrixTransformDistance(stroker->ctm, &dx2, &dy2);
		segment.point2.x = GraphicsFixedFromFloat(dx2) + p1->x;
		segment.point2.y = GraphicsFixedFromFloat(dy2) + p1->y;

		if (stroker->dash.dash_on &&
			(fully_in_bounds ||
			(! stroker->has_first_face && stroker->dash.dash_starts_on) ||
				GraphicsBoxIntersectsLineSegment(&stroker->join_bounds, &segment)))
		{
			add_sub_edge(stroker,
				&segment.point1, &segment.point2,
				&dev_slope,
				&sub_start, &sub_end);

			if (stroker->has_current_face)
			{
				/* Join with final face from previous segment */
				join (stroker, &stroker->current_face, &sub_start);

				stroker->has_current_face = FALSE;
			}
			else if(! stroker->has_first_face && stroker->dash.dash_starts_on)
			{
				/* Save sub path's first face in case needed for closing join */
				stroker->first_face = sub_start;
				stroker->has_first_face = TRUE;
			}
			else
			{
				/* Cap dash start if not connecting to a previous segment */
				add_leading_cap(stroker, &sub_start);
			}

			if(remain)
			{
				/* Cap dash end if not at end of segment */
				add_trailing_cap(stroker, &sub_end);
			}
			else
			{
				stroker->current_face = sub_end;
				stroker->has_current_face = TRUE;
			}
		}
		else
		{
			if(stroker->has_current_face)
			{
				/* Cap final face from previous segment */
				add_trailing_cap(stroker, &stroker->current_face);

				stroker->has_current_face = FALSE;
			}
		}
		
		GraphicsStrokerDashStep(&stroker->dash, step_length);
		segment.point1 = segment.point2;
	}

	if (stroker->dash.dash_on && ! stroker->has_current_face)
	{
		/* This segment ends on a transition to dash_on, compute a new face
		* and add cap for the beginning of the next dash_on step.
		*
		* Note: this will create a degenerate cap if this is not the last line
		* in the path. Whether this behaviour is desirable or not is debatable.
		* On one side these degenerate caps can not be reproduced with regular
		* path stroking.
		* On the other hand, Acroread 7 also produces the degenerate caps.
		*/
		compute_face(point, &dev_slope, stroker, &stroker->current_face);

		add_leading_cap(stroker, &stroker->current_face);

		stroker->has_current_face = TRUE;
	}
	else
		stroker->current_face.point = *point;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS spline_to(
	void *closure,
	const GRAPHICS_POINT *point,
	const GRAPHICS_SLOPE *tangent
)
{
	struct stroker *stroker = closure;
	GRAPHICS_STROKE_FACE face;

	if ((tangent->dx | tangent->dy) == 0)
	{
		GRAPHICS_POINT t;

		face = stroker->current_face;

		face.user_vector.x = -face.user_vector.x;
		face.user_vector.y = -face.user_vector.y;
		face.device_slope.x = -face.device_slope.x;
		face.device_slope.y = -face.device_slope.y;
		face.device_vector.dx = -face.device_vector.dx;
		face.device_vector.dy = -face.device_vector.dy;

		t = face.clock_wise;
		face.clock_wise = face.counter_clock_wise;
		face.counter_clock_wise = t;

		join(stroker, &stroker->current_face, &face);
	}
	else
	{
		GRAPHICS_POINT rectangle[4];

		compute_face(&stroker->current_face.point, tangent, stroker, &face);
		join(stroker, &stroker->current_face, &face);

		rectangle[0] = face.clock_wise;
		rectangle[1] = face.counter_clock_wise;

		rectangle[2].x = point->x - face.point.x;
		rectangle[2].y = point->y - face.point.y;
		face.point = *point;
		translate_point (&face.counter_clock_wise, &rectangle[2]);
		translate_point (&face.clock_wise, &rectangle[2]);

		rectangle[2] = face.counter_clock_wise;
		rectangle[3] = face.clock_wise;
		
		GraphicsTrapsTessellateConvexQuad(stroker->traps, rectangle);
	}

	stroker->current_face = face;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS curve_to(
	void *closure,
	const GRAPHICS_POINT *b,
	const GRAPHICS_POINT *c,
	const GRAPHICS_POINT *d
)
{
	struct stroker *stroker = closure;
	eGRAPHICS_LINE_JOIN line_join_save;
	GRAPHICS_SPLINE spline;
	GRAPHICS_STROKE_FACE face;
	eGRAPHICS_STATUS status;
	
	if (stroker->has_bounds &&
		! GraphicsSplineIntersects(&stroker->current_face.point, b, c, d,
			&stroker->line_bounds))
		return line_to (closure, d);

	if (! InitializeGraphicsSpline(&spline, spline_to, stroker,
		&stroker->current_face.point, b, c, d))
		return line_to (closure, d);

	compute_face(&stroker->current_face.point, &spline.initial_slope,
		stroker, &face);

	if (stroker->has_current_face)
	{
		/* Join with final face from previous segment */
		join(stroker, &stroker->current_face, &face);
	}
	else
	{
		if (! stroker->has_first_face)
		{
			/* Save sub path's first face in case needed for closing join */
			stroker->first_face = face;
			stroker->has_first_face = TRUE;
		}
		stroker->has_current_face = TRUE;
	}
	stroker->current_face = face;

	/* Temporarily modify the stroker to use round joins to guarantee
	* smooth stroked curves. */
	line_join_save = stroker->line_join;
	stroker->line_join = GRAPHICS_LINE_JOIN_ROUND;
	
	status = GraphicsSplineDecompose(&spline, stroker->tolerance);

	stroker->line_join = line_join_save;

	return status;
}

static eGRAPHICS_STATUS curve_to_dashed(
	void *closure,
	const GRAPHICS_POINT *b,
	const GRAPHICS_POINT *c,
	const GRAPHICS_POINT *d
)
{
	struct stroker *stroker = closure;
	GRAPHICS_SPLINE spline;
	eGRAPHICS_LINE_JOIN line_join_save;
	GRAPHICS_SPLINE_ADD_POINT_FUNCTION func;
	eGRAPHICS_STATUS status;

	func = (GRAPHICS_SPLINE_ADD_POINT_FUNCTION)line_to_dashed;

	if(stroker->has_bounds &&
		! GraphicsSplineIntersects(&stroker->current_face.point, b, c, d,
			&stroker->line_bounds))
		return func (closure, d, NULL);

	if (! InitializeGraphicsSpline(&spline, func, stroker,
		&stroker->current_face.point, b, c, d))
		return func (closure, d, NULL);

	/* Temporarily modify the stroker to use round joins to guarantee
	* smooth stroked curves. */
	line_join_save = stroker->line_join;
	stroker->line_join = GRAPHICS_LINE_JOIN_ROUND;

	status = GraphicsSplineDecompose(&spline, stroker->tolerance);

	stroker->line_join = line_join_save;

	return status;
}

static eGRAPHICS_STATUS _close_path(struct stroker *stroker)
{
	if(stroker->has_first_face && stroker->has_current_face)
	{
		/* Join first and final faces of sub path */
		join(stroker, &stroker->current_face, &stroker->first_face);
	}
	else
	{
		/* Cap the start and end of the sub path as needed */
		add_caps(stroker);
	}

	stroker->has_initial_sub_path = FALSE;
	stroker->has_first_face = FALSE;
	stroker->has_current_face = FALSE;
	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS close_path(void *closure)
{
	struct stroker *stroker = closure;
	eGRAPHICS_STATUS status;

	status = line_to (stroker, &stroker->first_point);
	if (UNLIKELY(status))
		return status;

	return _close_path (stroker);
}

static eGRAPHICS_STATUS close_path_dashed(void *closure)
{
	struct stroker *stroker = closure;
	eGRAPHICS_STATUS status;

	status = line_to_dashed (stroker, &stroker->first_point);
	if(UNLIKELY(status))
		return status;

	return _close_path (stroker);
}

eGRAPHICS_INTEGER_STATUS GraphicsPathFixedStrokeToTraps(
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_MATRIX* matrix,
	const GRAPHICS_MATRIX* matrix_inverse,
	FLOAT_T tolerance,
	GRAPHICS_TRAPS* traps
)
{
	struct stroker stroker;
	eGRAPHICS_STATUS status;

	status = stroker_init(&stroker, path, style,
				matrix, matrix_inverse, tolerance, traps);
	if(UNLIKELY(status))
	{
		return status;
	}

	if(stroker.dash.dashed)
	{
		status = GraphicsPathFixedInterpret(path, move_to_dashed, line_to_dashed,
					curve_to_dashed, close_path_dashed, &stroker);
	}
	else
	{
		status = GraphicsPathFixedInterpret(path, move_to, line_to,
					curve_to, close_path, &stroker);
	}

	add_caps(&stroker);

	return traps->status;
}

#ifdef __cplusplus
}
#endif
