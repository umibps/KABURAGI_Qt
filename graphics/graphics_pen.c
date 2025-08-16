#include "graphics_pen.h"
#include "graphics_private.h"
#include "graphics_matrix.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

int GraphicsPenVerticesNeeded(FLOAT_T tolerance, FLOAT_T radius, const GRAPHICS_MATRIX* matrix)
{
	FLOAT_T major_axis = GraphicsMatrixTransformedCircleMajorAxis(matrix, radius);
	int num_vertices;

	if(tolerance >= 4 * major_axis)
	{
		num_vertices = 1;
	}
	else if(tolerance >= major_axis)
	{
		num_vertices = 4;
	}
	else
	{
		num_vertices = CEIL(2 * M_PI / ACOS(1 - tolerance / major_axis));
		if(num_vertices % 2)
		{
			num_vertices++;
		}

		if(num_vertices < 4)
		{
			num_vertices = 4;
		}
	}

	return num_vertices;
}

static void GraphicsPenComputeSlopes(GRAPHICS_PEN* pen)
{
	int i, i_prev;
	GRAPHICS_PEN_VERTEX *prev, *v, *next;

	for(i=0, i_prev = pen->num_vertices - 1; i < pen->num_vertices; i_prev = i++)
	{
		prev = &pen->vertices[i_prev];
		v = &pen->vertices[i];
		next = &pen->vertices[(i + 1) % pen->num_vertices];

		INITIALIZE_GRAPHICS_SLOPE(&v->slope_cw, &prev->point, &v->point);
		INITIALIZE_GRAPHICS_SLOPE(&v->slope_ccw, &v->point, &next->point);
	}
}

eGRAPHICS_STATUS InitializeGraphicsPen(
	GRAPHICS_PEN* pen,
	FLOAT_T radius,
	FLOAT_T tolerance,
	const GRAPHICS_MATRIX* matrix
)
{
	const GRAPHICS_PEN local_pen = {0};
	int i;
	int reflect;

	*pen = local_pen;

	pen->radius = radius;
	pen->tolerance = tolerance;

	reflect = GRAPHICS_MATRIX_COMPUTE_DETERMINANT(matrix) < 0.0;

	pen->num_vertices = GraphicsPenVerticesNeeded(tolerance, radius, matrix);

	if(pen->num_vertices > (sizeof(pen->vertices_embedded) / sizeof(pen->vertices_embedded[0])))
	{
		pen->vertices = (GRAPHICS_PEN_VERTEX*)MEM_ALLOC_FUNC(
					sizeof(*pen->vertices) * pen->num_vertices);
	}
	else
	{
		pen->vertices = pen->vertices_embedded;
	}

	for(i=0; i<pen->num_vertices; i++)
	{
		GRAPHICS_PEN_VERTEX *v = &pen->vertices[i];
		FLOAT_T theta = 2 * M_PI * i / (FLOAT_T)pen->num_vertices, dx, dy;
		if(reflect != FALSE)
		{
			theta = - theta;
		}
		dx = radius * cos(theta);
		dy = radius * sin(theta);
		GraphicsMatrixTransformDistance(matrix, &dx, &dy);
		v->point.x = GraphicsFixedFromFloat(dx);
		v->point.y = GraphicsFixedFromFloat(dy);
	}

	GraphicsPenComputeSlopes(pen);

	return GRAPHICS_STATUS_SUCCESS;
}

void GraphicsPenFindActiveClockwiseVertices(
	const struct _GRAPHICS_PEN* pen,
	const struct _GRAPHICS_SLOPE* in,
	const struct _GRAPHICS_SLOPE* out,
	int* start,
	int* stop
)
{
	int low = 0, high = pen->num_vertices;
	int i;

	i = (low + high) >> 1;
	do
	{
		if(GraphicsSlopeCompare(&pen->vertices[i].slope_cw, in) < 0)
		{
			low = i;
		}
		else
		{
			high = i;
		}
		i = (low + high) >> 1;
	} while(high - low > 1);

	if(GraphicsSlopeCompare(&pen->vertices[i].slope_cw, in) < 0)
	{
		if(++i == pen->num_vertices)
		{
			i = 0;
		}
	}
	*start = i;

	if(GraphicsSlopeCompare(out, &pen->vertices[i].slope_ccw) >= 0)
	{
		low = i;
		high = i + pen->num_vertices;
		i = (low + high) >> 1;
		do
		{
			int j = i;
			if(j >= pen->num_vertices)
			{
				j -= pen->num_vertices;
			}
			if(GraphicsSlopeCompare(&pen->vertices[j].slope_cw, out) > 0)
			{
				high = i;
			}
			else
			{
				low = i;
			}
			i = (low + high) >> 1;
		} while(high - low > 1);
		if(i >= pen->num_vertices)
		{
			i -= pen->num_vertices;
		}
	}
	*stop = i;
}

int GraphicsPenFindActiveClockwiseVertexIndex(const struct _GRAPHICS_PEN* pen, const GRAPHICS_SLOPE* slope)
{
	int i;

	for(i = 0; i < pen->num_vertices; i++)
	{
		if((GraphicsSlopeCompare(slope, &pen->vertices[i].slope_ccw) < 0)
			&& (GraphicsSlopeCompare(slope, &pen->vertices[i].slope_cw) >= 0))
		{
			break;
		}
	}

	if(i == pen->num_vertices)
	{
		i = 0;
	}

	return i;
}

int GraphicsPenFindActiveCounterClockwiseVertexIndex(const struct _GRAPHICS_PEN* pen, const GRAPHICS_SLOPE* slope)
{
	GRAPHICS_SLOPE slope_reverse;
	int i;

	slope_reverse = *slope;
	slope_reverse.dx = - slope_reverse.dx;
	slope_reverse.dy = - slope_reverse.dy;

	for(i = pen->num_vertices - 1; i >= 0; i--)
	{
		if((GraphicsSlopeCompare(&pen->vertices[i].slope_ccw, &slope_reverse) >= 0)
			&& (GraphicsSlopeCompare(&pen->vertices[i].slope_cw, &slope_reverse) < 0))
		{
			break;
		}
	}

	if(i < 0)
	{
		i = pen->num_vertices - 1;
	}

	return i;
}

void GraphicsPenFindActiveCounterClockwiseVertices(
	const struct _GRAPHICS_PEN* pen,
	const struct _GRAPHICS_SLOPE* in,
	const struct _GRAPHICS_SLOPE* out,
	int* start,
	int* stop
)
{
	int low = 0, high = pen->num_vertices;
	int i;

	i = (low + high) >> 1;
	do
	{
		if(GraphicsSlopeCompare(in, &pen->vertices[i].slope_ccw) < 0)
		{
			low = i;
		}
		else
		{
			high = i;
		}
		i = (low + high) >> 1;
	} while(high - low > 1);

	if(GraphicsSlopeCompare(in, &pen->vertices[i].slope_ccw) < 0)
	{
		if(++i == pen->num_vertices)
		{
			i =0 ;
		}
	}
	*start = i;

	if(GraphicsSlopeCompare(&pen->vertices[i].slope_cw, out) <= 0)
	{
		low = i;
		high = i + pen->num_vertices;
		i = (low + high)  >> i;
		do
		{
			int j = i;
			if(j >= pen->num_vertices)
			{
				j -= pen->num_vertices;
			}
			if(GraphicsSlopeCompare(out, &pen->vertices[j].slope_ccw) > 0)
			{
				high = i;
			}
			else
			{
				low = i;
			}
			i = (low + high) >> 1;
		} while(high - low > 1);

		if(i >= pen->num_vertices)
		{
			i -= pen->num_vertices;
		}
	}

	*stop = i;
}

void GraphicsPenFinish(GRAPHICS_PEN* pen)
{
	if(pen->vertices != pen->vertices_embedded)
	{
		MEM_FREE_FUNC(pen->vertices);
	}
}

#ifdef __cplusplus
}
#endif
