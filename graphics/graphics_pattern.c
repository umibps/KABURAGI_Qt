#include <string.h>
#include <float.h>
#include "graphics_pattern.h"
#include "graphics_matrix.h"
#include "graphics_surface.h"
#include "graphics.h"
#include "graphics_function.h"
#include "graphics_private.h"
#include "graphics_inline.h"
#include "../memory.h"

#ifdef __cplusplus
extern "C" {
#endif

void InitializeGraphicsPattern(GRAPHICS_PATTERN* pattern, eGRAPHICS_PATTERN_TYPE type, void* graphics)
{
	pattern->type = type;
	pattern->status = GRAPHICS_STATUS_SUCCESS;

	// deffered _cairo_user_data_array_init

	if(type == GRAPHICS_PATTERN_TYPE_SURFACE ||
			type == GRAPHICS_PATTERN_TYPE_RASTER_SOURCE)
	{
		pattern->extend = GRAPHICS_EXTEND_SURFACE_DEFAULT;
	}
	else
	{
		pattern->extend = GRAPHICS_EXTEND_GRADIENT_DEFAULT;
	}

	pattern->filter = GRAPHICS_FILTER_DEFAULT;
	pattern->opacity = 1.0;

	pattern->has_component_alpha = FALSE;

	InitializeGraphicsMatrixIdentity(&pattern->matrix);
	InitializeGraphicsList(&pattern->observers);

	pattern->reference_count = 1;
	pattern->own_memory = FALSE;

	pattern->graphics = graphics;
}

void InitializeGraphicsPatternSolid(GRAPHICS_SOLID_PATTERN* pattern, const GRAPHICS_COLOR* color, void* graphics)
{
	InitializeGraphicsPattern(&pattern->base, GRAPHICS_PATTERN_TYPE_SOLID, graphics);
	pattern->color = *color;
}

int GraphicsMeshPatternCoordBox(
	const GRAPHICS_MESH_PATTERN* mesh,
	GRAPHICS_FLOAT* out_x_min,
	GRAPHICS_FLOAT* out_y_min,
	GRAPHICS_FLOAT* out_x_max,
	GRAPHICS_FLOAT* out_y_max
)
{
	const GRAPHICS_MESH_PATCH *patch;
	unsigned int num_patches, i, j, k;
	GRAPHICS_FLOAT x0, y0, x1, y1;

	ASSERT(mesh->current_patch == NULL);

	num_patches = mesh->patches.num_elements;

	if(num_patches == 0)
	{
		return FALSE;
	}

	patch = (const GRAPHICS_MESH_PATCH*)GraphicsArrayIndexConst(&mesh->patches, 0);
	x0 = x1 = patch->points[0][0].x;
	y0 = y1 = patch->points[0][0].y;

	for(i=0; i<num_patches; i++)
	{
		for(j=0; j<4;j++)
		{
			for(k=0; k<4; k++)
			{
				x0 = MINIMUM(x0, patch[i].points[j][k].x);
				y0 = MINIMUM(y0, patch[i].points[j][k].y);
				x1 = MAXIMUM(x1, patch[i].points[j][k].x);
				y1 = MAXIMUM(y1, patch[i].points[j][k].y);
			}
		}
	}

	*out_x_min = x0;
	*out_x_max = x1;
	*out_y_min = y0;
	*out_y_max = y1;

	return TRUE;
}

static int SurfaceIsClear(const GRAPHICS_SURFACE_PATTERN* pattern)
{
	GRAPHICS_RECTANGLE_INT extents;

	if(GraphicsSurfaceGetExtents(pattern->surface, &extents)
		&& extents.width == 0 || extents.height == 0)
	{
		return TRUE;
	}

	return pattern->surface->is_clear && pattern->surface->content & GRAPHICS_CONTENT_ALPHA;
}

static int RasterSourceIsClear(const GRAPHICS_RASTER_SOURCE_PATTERN* pattern)
{
	return pattern->extents.width == 0 || pattern->extents.height == 0;
}

static int LinearPatternIsDegenerate(const GRAPHICS_LINEAR_PATTERN* linear)
{
	return FABS(linear->point1.x - linear->point2.x) < FLOAT_EPSILON
		&& FABS(linear->point1.y - linear->point2.y) < FLOAT_EPSILON;
}

static int RadialPatternIsDegenerate(const GRAPHICS_RADIAL_PATTERN* radial)
{
	return FABS(radial->circle1.radius - radial->circle2.radius) < FLOAT_EPSILON
		&& (MINIMUM(radial->circle1.radius, radial->circle2.radius) < FLOAT_EPSILON
			|| MAXIMUM(FABS(radial->circle1.center.x - radial->circle2.center.x),
				FABS(radial->circle1.center.y - radial->circle2.center.y)) < 2 * FLOAT_EPSILON);
}

static int GradientIsClear(const GRAPHICS_GRADIENT_PATTERN* gradient, const GRAPHICS_RECTANGLE_INT* extents)
{
	unsigned int i;

	ASSERT(gradient->base.type == GRAPHICS_PATTERN_TYPE_LINEAR ||
		gradient->base.type == GRAPHICS_PATTERN_TYPE_RADIAL);

	if(gradient->num_stops == 0 ||
		(gradient->base.extend == GRAPHICS_EXTEND_NONE
			&& gradient->stops[0].offset == gradient->stops[gradient->num_stops - 1].offset))
	{
		return TRUE;
	}
	return FALSE;
}

static int MeshIsClear(const GRAPHICS_MESH_PATTERN* mesh)
{
	GRAPHICS_FLOAT x1, y1, x2, y2;
	int is_valid;

	is_valid = GraphicsMeshPatternCoordBox(mesh, &x1, &y1, &x2, &y2);

	if(is_valid == FALSE)
	{
		return TRUE;
	}

	if(x2 - x1 < FLOAT_EPSILON || y2 - y1 < FLOAT_EPSILON)
	{
		return TRUE;
	}

	return FALSE;
}

int GraphicsPatternIsClear(const GRAPHICS_PATTERN* pattern)
{
	const GRAPHICS_PATTERN_UNION *pattern_union;

	if(pattern->has_component_alpha != FALSE)
	{
		FALSE;
	}

	pattern_union = (GRAPHICS_PATTERN_UNION*)pattern;
	switch(pattern->type)
	{
	case GRAPHICS_PATTERN_TYPE_SOLID:
		return GRAPHICS_COLOR_IS_CLEAR(&pattern_union->solid.color);
	case GRAPHICS_PATTERN_TYPE_SURFACE:
		return SurfaceIsClear(&pattern_union->surface);
	case GRAPHICS_PATTERN_TYPE_RASTER_SOURCE:
		return RasterSourceIsClear(&pattern_union->raster_source);
	case GRAPHICS_PATTERN_TYPE_LINEAR:
	case GRAPHICS_PATTERN_TYPE_RADIAL:
		return GradientIsClear(&pattern_union->gradient, NULL);
	case GRAPHICS_PATTERN_TYPE_MESH:
		return MeshIsClear(&pattern_union->mesh);
	}

	return FALSE;
}

static int SurfaceIsOpaque(const GRAPHICS_SURFACE_PATTERN* pattern, const GRAPHICS_RECTANGLE_INT* sample)
{
	GRAPHICS_RECTANGLE_INT extents;

	if(pattern->surface->content & GRAPHICS_CONTENT_ALPHA)
	{
		return FALSE;
	}

	if(pattern->base.extend != GRAPHICS_EXTEND_NONE)
	{
		return TRUE;
	}

	if(FALSE == GraphicsSurfaceGetExtents(pattern->surface, &extents))
	{
		return TRUE;
	}

	/* if(sample == NULL)
	{
		return FALSE;
	}*/

	return GraphicsRectangleContainsRectangle(&extents, sample);
}

static int RasterSourceIsOpaque(const GRAPHICS_RASTER_SOURCE_PATTERN* pattern, const GRAPHICS_RECTANGLE_INT* sample)
{
	if(pattern->content & GRAPHICS_CONTENT_ALPHA)
	{
		return FALSE;
	}

	if(pattern->base.extend != GRAPHICS_EXTEND_NONE)
	{
		return TRUE;
	}

	if(sample == NULL)
	{
		return FALSE;
	}

	return GraphicsRectangleContainsRectangle(&pattern->extents, sample);
}

static void GraphicsLinearPatternBoxToParameter(
	const GRAPHICS_LINEAR_PATTERN* linear,
	FLOAT_T x0, FLOAT_T y0,
	FLOAT_T x1, FLOAT_T y1,
	FLOAT_T range[2]
)
{
	FLOAT_T t0, tdx, tdy;
	FLOAT_T p1x, p1y, pdx, pdy, inverse_qnormnal;

	p1x = linear->point1.x;
	p1y = linear->point1.y;
	pdx = linear->point2.x - p1x;
	pdy = linear->point2.y - p1y;
	inverse_qnormnal = 1.0 / (pdx * pdx + pdy * pdy);
	pdx *= inverse_qnormnal;
	pdy *= inverse_qnormnal;

	t0 = (x0 - p1x) * pdx + (y0 - p1y) * pdy;
	tdx = (x1 - x0) * pdx;
	tdy = (y1 - y0) * pdy;

	range[0] = range[1] = t0;
	if(tdx < 0)
	{
		range[0] += tdx;
	}
	else
	{
		range[1] += tdx;
	}

	if(tdy < 0)
	{
		range[0] += tdy;
	}
	else
	{
		range[1] += tdy;
	}
}

static int GradientIsOpaque(const GRAPHICS_GRADIENT_PATTERN* gradient, const GRAPHICS_RECTANGLE_INT* sample)
{
	unsigned int i;

	if(gradient->num_stops == 0 ||
		(gradient->base.extend == GRAPHICS_EXTEND_NONE && gradient->stops[0].offset == gradient->stops[gradient->num_stops - 1].offset))
	{
		return FALSE;
	}

	if(gradient->base.type == GRAPHICS_PATTERN_TYPE_LINEAR)
	{
		if(gradient->base.extend == GRAPHICS_EXTEND_NONE)
		{
			GRAPHICS_LINEAR_PATTERN *linear = (GRAPHICS_LINEAR_PATTERN*)gradient;
			FLOAT_T t[2];

			if(LinearPatternIsDegenerate(linear))
			{
				return FALSE;
			}

			if(sample == NULL)
			{
				return FALSE;
			}

			GraphicsLinearPatternBoxToParameter(linear,
				sample->x, sample->y, sample->x + sample->width, sample->y + sample->height, t);

			if(t[0] < 0.0 || t[1] > 1.0)
			{
				return FALSE;
			}
		}
	}
	else
	{
		return FALSE;
	}

	for(i=0; i<gradient->num_stops; i++)
	{
		if(FALSE == GRAPHICS_COLOR_IS_OPAQUE(&gradient->stops[i].color))
		{
			return FALSE;
		}
	}

	return TRUE;
}

int GraphicsPatternIsOpaque(const GRAPHICS_PATTERN* abstract_pattern, const GRAPHICS_RECTANGLE_INT* sample)
{
	const GRAPHICS_PATTERN_UNION *pattern;

	if(abstract_pattern->has_component_alpha)
	{
		return FALSE;
	}

	pattern = (GRAPHICS_PATTERN_UNION*)abstract_pattern;
	switch(pattern->base.type)
	{
	case GRAPHICS_PATTERN_TYPE_SOLID:
		return GRAPHICS_COLOR_IS_OPAQUE(
				(&((GRAPHICS_SOLID_PATTERN*)abstract_pattern)->color));
	case GRAPHICS_PATTERN_TYPE_SURFACE:
		return SurfaceIsOpaque(&pattern->surface, sample);
	case GRAPHICS_PATTERN_TYPE_RASTER_SOURCE:
		return RasterSourceIsOpaque(&pattern->raster_source, sample);
	case GRAPHICS_PATTERN_TYPE_LINEAR:
	case GRAPHICS_PATTERN_TYPE_RADIAL:
		return GradientIsOpaque(&pattern->gradient, sample);
	case GRAPHICS_PATTERN_TYPE_MESH:
		return FALSE;
	}

	return FALSE;
}

void GraphicsRasterSourcePatternFinish(GRAPHICS_PATTERN* abstract_pattern)
{
	GRAPHICS_RASTER_SOURCE_PATTERN *pattern = (GRAPHICS_RASTER_SOURCE_PATTERN*)abstract_pattern;

	if(pattern->finish == NULL)
	{
		return;
	}

	pattern->finish(&pattern->base, pattern->user_data);
}

void GraphicsRasterSourcePatternRelease(
	const GRAPHICS_PATTERN* abstract_pattern,
	struct _GRAPHICS_SURFACE* surface
)
{
	GRAPHICS_RASTER_SOURCE_PATTERN *pattern =
		(GRAPHICS_RASTER_SOURCE_PATTERN*)abstract_pattern;
	if(pattern->release == NULL)
	{
		return;
	}

	pattern->release(&pattern->base, pattern->user_data, surface);
}

struct _GRAPHICS_SURFACE* GraphicsRasterSourcePatternAcquire(
	const GRAPHICS_PATTERN* abstract_pattern,
	struct _GRAPHICS_SURFACE* target,
	const GRAPHICS_RECTANGLE_INT* extents
)
{
	GRAPHICS_RASTER_SOURCE_PATTERN *pattern =
		(GRAPHICS_RASTER_SOURCE_PATTERN*)abstract_pattern;

	if(pattern->acquire == NULL)
	{
		return NULL;
	}

	if(extents == NULL)
	{
		extents = &pattern->extents;
	}

	return pattern->acquire(&pattern->base, pattern->user_data, target, extents);
}

void GraphicsPatternFinish(GRAPHICS_PATTERN* pattern)
{
	// PENDING cairo_user_data_array_fini

	switch(pattern->type)
	{
	case GRAPHICS_PATTERN_TYPE_SOLID:
		break;
	case GRAPHICS_PATTERN_TYPE_SURFACE:
		{
			GRAPHICS_SURFACE_PATTERN *surface_pattern = (GRAPHICS_SURFACE_PATTERN*)pattern;
			DestroyGraphicsSurface(surface_pattern->surface);
		}
		break;
	case GRAPHICS_PATTERN_TYPE_LINEAR:
	case GRAPHICS_PATTERN_TYPE_RADIAL:
		{
			GRAPHICS_GRADIENT_PATTERN *gradient = (GRAPHICS_GRADIENT_PATTERN*)pattern;

			if(gradient->stops && gradient->stops != gradient->stops_embedded)
			{
				MEM_FREE_FUNC(gradient->stops);
			}
		}
		break;
	case GRAPHICS_PATTERN_TYPE_MESH:
		{
			GRAPHICS_MESH_PATTERN *mesh = (GRAPHICS_MESH_PATTERN*)pattern;
			ReleaseGraphcisArray(&mesh->patches);
		}
		break;
	case GRAPHICS_PATTERN_TYPE_RASTER_SOURCE:
		GraphicsRasterSourcePatternFinish(pattern);
		break;
	}
}

INLINE int ReleaseGraphcisPattern(GRAPHICS_PATTERN* pattern)
{
	eGRAPHICS_PATTERN_TYPE type;

	if(pattern == NULL || pattern->reference_count < 0)
	{
		return FALSE;
	}

	ASSERT(pattern->reference_count >= 0);

	if(REFERENCE_COUNT_DECREMENT_AND_TEST(pattern->reference_count) == FALSE)
	{
		return FALSE;
	}

	type = pattern->type;
	GraphicsPatternFinish(pattern);

	return TRUE;
}

void DestroyGraphicsPattern(GRAPHICS_PATTERN* pattern)
{
	int free_memory;
	free_memory = ReleaseGraphcisPattern(pattern);
	if(free_memory != FALSE && pattern->own_memory)
	{
		MEM_FREE_FUNC(pattern);
	}
}

GRAPHICS_PATTERN* GraphicsPatternReference(GRAPHICS_PATTERN* pattern)
{
	if(pattern == NULL || pattern->reference_count < 0)
	{
		return pattern;
	}

	pattern->reference_count++;

	return pattern;
}

void InitializeGraphicsPatternStaticCopy(GRAPHICS_PATTERN* pattern, const GRAPHICS_PATTERN* other)
{
	int size;

	ASSERT(other->status == GRAPHICS_STATUS_SUCCESS);

	switch(other->type)
	{
	case GRAPHICS_PATTERN_TYPE_SOLID:
		size = sizeof(GRAPHICS_SOLID_PATTERN);
		break;
	case GRAPHICS_PATTERN_TYPE_SURFACE:
		size = sizeof(GRAPHICS_SURFACE_PATTERN);
		break;
	case GRAPHICS_PATTERN_TYPE_LINEAR:
		size = sizeof(GRAPHICS_LINEAR_PATTERN);
		break;
	case GRAPHICS_PATTERN_TYPE_RADIAL:
		size = sizeof(GRAPHICS_RADIAL_PATTERN);
		break;
	case GRAPHICS_PATTERN_TYPE_MESH:
		size = sizeof(GRAPHICS_MESH_PATTERN);
		break;
	case GRAPHICS_PATTERN_TYPE_RASTER_SOURCE:
		size = sizeof(GRAPHICS_RASTER_SOURCE_PATTERN);
		break;
	default:
		ASSERT(0);
	}

	(void)memcpy(pattern, other, size);

	pattern->reference_count = 0;
	// PENDING _cairo_user_data_array_init
	InitializeGraphicsList(&pattern->observers);
}

static int UseBilinear(FLOAT_T x, FLOAT_T y, FLOAT_T t)
{
	FLOAT_T h = x*x + y*y;
	if(h < 1.0 / (0.75 * 0.75))
	{
		return TRUE;
	}

	if((h > 3.99 && h < 4.01) && 0 == GraphicsFixedFromFloat(x*y)
			&& GRAPHICS_FIXED_IS_INTEGER(GraphicsFixedFromFloat(t)))
	{
		return TRUE;
	}
	return FALSE;
}

eGRAPHICS_FILTER GraphicsPatternAnalyzeFilter(const GRAPHICS_PATTERN* pattern)
{
	switch(pattern->filter)
	{
	case GRAPHICS_FILTER_GOOD:
	case GRAPHICS_FILTER_BEST:
	case GRAPHICS_FILTER_BILINEAR:
	case GRAPHICS_FILTER_FAST:
		if(GraphicsMatrixIsPixelExact(&pattern->matrix) != FALSE)
		{
			return GRAPHICS_FILTER_NEAREST;
		}
		else
		{
			if(pattern->filter == GRAPHICS_FILTER_GOOD
				&& UseBilinear(pattern->matrix.xx, pattern->matrix.xy, pattern->matrix.x0) != FALSE
				&& UseBilinear(pattern->matrix.yx, pattern->matrix.yy, pattern->matrix.y0) != FALSE
			)
			{
				return GRAPHICS_FILTER_BILINEAR;
			}
		}
		break;
	case GRAPHICS_FILTER_NEAREST:
	case GRAPHICS_FILTER_GAUSSIAN:
	default:
		break;
	}

	return pattern->filter;
}

void GraphicsPatternGetExtents(const GRAPHICS_PATTERN* pattern, GRAPHICS_RECTANGLE_INT* extents, int is_vector)
{
	FLOAT_T x1, y1, x2, y2;
	int ix1, ix2, iy1, iy2;
	int round_x = FALSE;
	int round_y = FALSE;

	switch(pattern->type)
	{
	case GRAPHICS_PATTERN_TYPE_SOLID:
		goto UNBOUNDED;
	case GRAPHICS_PATTERN_TYPE_SURFACE:
		{
			GRAPHICS_RECTANGLE_INT surface_extents;
			const GRAPHICS_SURFACE_PATTERN *surface_pattern =
				(const GRAPHICS_SURFACE_PATTERN*)pattern;
			GRAPHICS_SURFACE *surface = surface_pattern->surface;

			if(FALSE == GraphicsSurfaceGetExtents(surface, &surface_extents))
			{
				goto UNBOUNDED;
			}

			if(surface_extents.width == 0 || surface_extents.height == 0)
			{
				goto EMPTY;
			}

			if(pattern->extend != GRAPHICS_EXTEND_NONE)
			{
				goto UNBOUNDED;
			}

			x1 = surface_extents.x;
			y1 = surface_extents.y;
			x2 = surface_extents.x + (int)surface_extents.width;
			y2 = surface_extents.y + (int)surface_extents.height;

			goto HANDLE_FILTER;
		}
		break;
	case GRAPHICS_PATTERN_TYPE_RASTER_SOURCE:
		{
			const GRAPHICS_RASTER_SOURCE_PATTERN *raster =
				(const GRAPHICS_RASTER_SOURCE_PATTERN*)pattern;

			if(raster->extents.width == 0 || raster->extents.height == 0)
			{
				goto EMPTY;
			}

			if(pattern->extend != GRAPHICS_EXTEND_NONE)
			{
				goto UNBOUNDED;
			}

			x1 = raster->extents.x;
			y1 = raster->extents.y;
			x2 = raster->extents.x + (int)raster->extents.width;
			y2 = raster->extents.y + (int)raster->extents.height;
		}
 HANDLE_FILTER:
		switch(pattern->filter)
		{
		case GRAPHICS_FILTER_NEAREST:
		case GRAPHICS_FILTER_FAST:
			round_x = round_y = TRUE;
			x1 -= 0.004;
			y1 -= 0.004;
			x2 += 0.004;
			y2 += 0.004;
			break;
		case GRAPHICS_FILTER_BEST:
			break;
		case GRAPHICS_FILTER_BILINEAR:
		case GRAPHICS_FILTER_GAUSSIAN:
		case GRAPHICS_FILTER_GOOD:
		default:
			if(HYPOT(pattern->matrix.xx, pattern->matrix.yx) < 1.0)
			{
				x1 -= 0.5;
				x2 += 0.5;
				round_x = TRUE;
			}
			if(HYPOT(pattern->matrix.xy, pattern->matrix.yy) < 1.0)
			{
				y1 -= 0.5;
				y2 += 0.5;
				round_y = TRUE;
			}
			break;
		}
		break;
	case GRAPHICS_PATTERN_TYPE_RADIAL:
		{
			const GRAPHICS_RADIAL_PATTERN *radial =
				(const GRAPHICS_RADIAL_PATTERN*)pattern;
			FLOAT_T cx1, cy1;
			FLOAT_T cx2, cy2;
			FLOAT_T r1, r2;

			if(RadialPatternIsDegenerate(radial))
			{
				goto EMPTY;
			}

			if(pattern->extend != GRAPHICS_EXTEND_NONE)
			{
				goto UNBOUNDED;
			}

			cx1 = radial->circle1.center.x;
			cy1 = radial->circle1.center.y;
			r1 = radial->circle1.radius;

			cx2 = radial->circle2.center.x;
			cy2 = radial->circle2.center.y;
			r2 = radial->circle2.radius;

			x1 = MINIMUM(cx1 - r1, cx2 - r2);
			y1 = MINIMUM(cy1 - r1, cy2 - r2);
			x2 = MAXIMUM(cx1 + r1, cx2 + r2);
			y2 = MAXIMUM(cy1 + r1, cy2 + r2);
		}
		break;
	case GRAPHICS_PATTERN_TYPE_LINEAR:
		{
			const GRAPHICS_LINEAR_PATTERN *linear =
				(const GRAPHICS_LINEAR_PATTERN*)pattern;

			if(pattern->extend != GRAPHICS_EXTEND_NONE)
			{
				goto UNBOUNDED;
			}

			if(LinearPatternIsDegenerate(linear))
			{
				goto EMPTY;
			}

			if(pattern->matrix.xy != 0.0 || pattern->matrix.yx != 0.0)
			{
				goto UNBOUNDED;
			}

			if(linear->point1.x == linear->point2.x)
			{
				x1 = - HUGE_VAL;
				x2 = HUGE_VAL;
				y1 = MINIMUM(linear->point1.y, linear->point2.y);
				y2 = MAXIMUM(linear->point1.y, linear->point2.y);
			}
			else if(linear->point1.y == linear->point2.y)
			{
				x1 = MINIMUM(linear->point1.x, linear->point2.x);
				x2 = MAXIMUM(linear->point1.x, linear->point2.x);
				y1 = - HUGE_VAL;
				y2 = HUGE_VAL;
			}
			else
			{
				goto UNBOUNDED;
			}

			round_x = round_y = TRUE;
		}
		break;
	case GRAPHICS_PATTERN_TYPE_MESH:
		{
			const GRAPHICS_MESH_PATTERN *mesh =
				(const GRAPHICS_MESH_PATTERN*)pattern;
			if(FALSE == GraphicsMeshPatternCoordBox(mesh, &x1, &y1, &x2, &y2))
			{
				goto EMPTY;
			}
		}
		break;
	default:
		ASSERT(0);
	}

	if(GRAPHICS_MATRIX_IS_TRANSLATION(&pattern->matrix))
	{
		x1 -= pattern->matrix.x0,   x2 -= pattern->matrix.x0;
		y1 -= pattern->matrix.y0,   y2 -= pattern->matrix.y0;
	}
	else
	{
		GRAPHICS_MATRIX invert_matrix;
		eGRAPHICS_STATUS status;

		invert_matrix = pattern->matrix;
		status = GraphicsMatrixInvert(&invert_matrix);
		ASSERT(status == GRAPHICS_STATUS_SUCCESS);

		GraphicsMatrixTransformBoundingBox(&invert_matrix, &x1, &y1, &x2, &y2, NULL);
	}

	if(round_x == FALSE)
	{
		x1 -= 0.5;
		x2 += 0.5;
	}
	if(x1 < GRAPHICS_RECTANGLE_MIN)
	{
		ix1 = GRAPHICS_RECTANGLE_MIN;
	}
	else
	{
		ix1 = (int)floor(x1 + 0.5);
	}
	if(x2 > GRAPHICS_RECTANGLE_MAX)
	{
		ix2 = GRAPHICS_RECTANGLE_MAX;
	}
	else
	{
		ix2 = (int)floor(x2 + 0.5);
	}
	extents->x = ix1,   extents->width = ix2 - ix1;
	if(is_vector != FALSE && extents->width == 0 && x1 != x2)
	{
		extents->width += 1;
	}

	if(round_y == FALSE)
	{
		y1 -= 0.5;
		y2 += 0.5;
	}
	if(y1 < GRAPHICS_RECTANGLE_MIN)
	{
		iy1 = GRAPHICS_RECTANGLE_MIN;
	}
	else
	{
		iy1 = (int)floor(y1 + 0.5);
	}
	if(y2 > GRAPHICS_RECTANGLE_MAX)
	{
		iy2 = GRAPHICS_RECTANGLE_MAX;
	}
	else
	{
		iy2 = (int)floor(y2 + 0.5);
	}
	extents->y = iy1,   extents->height = iy2 - iy1;
	if(is_vector != FALSE && extents->height == 0 && y1 != y2)
	{
		extents->height += 1;
	}

	return;

UNBOUNDED:
	InitializeGraphicsUnboundedRectangle(extents);
	return;

EMPTY:
	extents->x = extents->y = 0;
	extents->width = extents->height = 0;
	return;
}

void GraphicsPatternSampledArea(
	const GRAPHICS_PATTERN* pattern,
	const GRAPHICS_RECTANGLE_INT* extents,
	GRAPHICS_RECTANGLE_INT* sample
)
{
	FLOAT_T x1, x2, y1, y2;
	FLOAT_T padx, pady;

	if(GRAPHICS_MATRIX_IS_IDENTITY(&pattern->matrix))
	{
		*sample = *extents;
		return;
	}

	x1 = extents->x + 0.5;
	y1 = extents->y + 0.5;
	x2 = x1 + (extents->width - 1);
	y2 = y1 + (extents->height - 1);
	GraphicsMatrixTransformBoundingBox(&pattern->matrix, &x1, &y1, &x2, &y2, NULL);

	switch(pattern->filter)
	{
	case GRAPHICS_FILTER_NEAREST:
	case GRAPHICS_FILTER_FAST:
		padx = pady = 0.004;
		break;
	case GRAPHICS_FILTER_BILINEAR:
	case GRAPHICS_FILTER_GAUSSIAN:
	default:
		padx = pady = 0.495;
		break;
	case GRAPHICS_FILTER_GOOD:
		padx = HYPOT(pattern->matrix.xx, pattern->matrix.xy);
		if(padx <= 1.0)
		{
			padx = 0.495;
		}
		else if(padx >= 16.0)
		{
			padx = 7.92;
		}
		else
		{
			padx *= 0.495;
		}

		pady = HYPOT(pattern->matrix.yx, pattern->matrix.yy);
		if(pady <= 1.0)
		{
			pady = 0.495;
		}
		else if(pady >= 16.0)
		{
			pady = 7.92;
		}
		else
		{
			pady *= 0.495;
		}
		break;
	case GRAPHICS_FILTER_BEST:
		padx = HYPOT(pattern->matrix.xx, pattern->matrix.xy) * 1.98;
		if(padx > 7.92)
		{
			padx = 7.92;
		}
		pady = HYPOT(pattern->matrix.yx, pattern->matrix.yy) * 1.98;
		if(pady > 7.92)
		{
			pady = 7.92;
		}
		break;
	}

	x1 = FLOOR(x1 - padx);
	if(x1 < GRAPHICS_RECTANGLE_INT_MIN)
	{
		x1 = GRAPHICS_RECTANGLE_INT_MIN;
	}
	sample->x = (int)x1;

	y1 = FLOOR(y1 - pady);
	if(y1 < GRAPHICS_RECTANGLE_INT_MIN)
	{
		y1 = GRAPHICS_RECTANGLE_INT_MIN;
	}
	sample->y = (int)y1;

	x2 = FLOOR(x2 + padx) + 1.0;
	if(x2 > GRAPHICS_RECTANGLE_INT_MAX)
	{
		x2 = GRAPHICS_RECTANGLE_INT_MAX;
	}
	sample->width = (int)(x2 - x1);

	y2 = FLOOR(y2 + pady) + 1.0;
	if(y2 > GRAPHICS_RECTANGLE_INT_MAX)
	{
		y2 = GRAPHICS_RECTANGLE_INT_MAX;
	}
	sample->height = (int)(y2 - y1);
}

GRAPHICS_PATTERN* CreateGraphicsPatternInError(eGRAPHICS_STATUS status, void* graphics)
{
	GRAPHICS *g = (GRAPHICS*)graphics;
	
	if(g == NULL)
	{
		return NULL;
	}
	
	if(status == GRAPHICS_STATUS_NO_MEMORY)
	{
		return &g->nil_pattern.base;
	}
	
	return &g->error_pattern;
}

int InitializeGraphicsPatternForSurface(GRAPHICS_SURFACE_PATTERN* pattern, struct _GRAPHICS_SURFACE* surface)
{
	GRAPHICS *graphics = surface->graphics;

	if(surface->status)
	{
		InitializeGraphicsPattern(&pattern->base, GRAPHICS_PATTERN_TYPE_SOLID, graphics);
		return FALSE;
	}

	InitializeGraphicsPattern(&pattern->base, GRAPHICS_PATTERN_TYPE_SURFACE, graphics);

	pattern->surface = GraphicsSurfaceReference(surface);
	pattern->base.reference_count = 1;

	return TRUE;
}

GRAPHICS_PATTERN* CreateGraphicsPatternForSurface(struct _GRAPHICS_SURFACE* surface)
{
	GRAPHICS_SURFACE_PATTERN *pattern;

	pattern = (GRAPHICS_SURFACE_PATTERN*)MEM_ALLOC_FUNC(sizeof(*pattern));
	pattern->base.own_memory = 1;

	(void)InitializeGraphicsPatternForSurface(pattern, surface);

	return &pattern->base;
}

static void GraphicsPatternNotifyObservers(struct _GRAPHICS_PATTERN* pattern, unsigned int flags)
{
	GRAPHICS_PATTERN_OBSERVER *pos;

	for(pos=(&pattern->observers)->next; pos->link != &pattern->observers; pos = pos->link->next)
	{
		pos->notify(pos, pattern, flags);
	}
}

void GraphicsPatternSetMatrix(GRAPHICS_PATTERN* pattern, const GRAPHICS_MATRIX* matrix)
{
	if(pattern->status)
	{
		return;
	}

	if(memcmp(&pattern->matrix, matrix, sizeof(*matrix)) != 0)
	{
		GRAPHICS_MATRIX inverse;
		eGRAPHICS_STATUS status;

		pattern->matrix = *matrix;
		GraphicsPatternNotifyObservers(pattern, GRAPHICS_PATTERN_NOTIFY_MATRIX);

		inverse = *matrix;
		status = GraphicsMatrixInvert(&inverse);
		if(UNLIKELY(status))
		{
			// TODO
			// _cairo_pattern_set_error(pattern, status);
		}
	}
}

void GraphicsPatternSetFilter(GRAPHICS_PATTERN* pattern,eGRAPHICS_FILTER filter)
{
	if(UNLIKELY(pattern->status))
	{
		return;
	}

	pattern->filter = filter;
	GraphicsPatternNotifyObservers(pattern, GRAPHICS_PATTERN_NOTIFY_FILTER);
}

GRAPHICS_PATTERN* CreateGraphicsPatternSolid(const GRAPHICS_COLOR* color, void* graphics)
{
	GRAPHICS_SOLID_PATTERN *pattern;

	pattern = (GRAPHICS_SOLID_PATTERN*)MEM_ALLOC_FUNC(sizeof(*pattern));
	InitializeGraphicsPatternSolid(pattern, color, graphics);
	pattern->base.own_memory = 1;
	pattern->base.reference_count = 1;

	return &pattern->base;
}

GRAPHICS_PATTERN* CreateGraphicsPatternRGBA(FLOAT_T red, FLOAT_T green, FLOAT_T blue, FLOAT_T alpha, void* graphics)
{
	GRAPHICS_COLOR color;

#if defined(GRAPHICS_COLOR_FLOAT_VALUE_UNCONTROLLED) && GRAPHICS_COLOR_FLOAT_VALUE_UNCONTROLLED != 0
	red = GraphicsRestrictValue(red, 0.0, 1.0);
	green = GraphicsRestrictValue(green, 0.0, 1.0);
	blue = GraphicsRestrictValue(blue, 0.0, 1.0);
	alpha = GraphicsRestrictValue(alpha, 0.0, 1.0);
#endif

	InitializeGraphicsColorRGBA(&color, red, green, blue, alpha);

	return CreateGraphicsPatternSolid(&color, graphics);
}

static int GraphicsLinearPatternIsDegenerate(const GRAPHICS_LINEAR_PATTERN* linear)
{
	return fabs(linear->point1.x - linear->point2.x) < FLOAT_EPSILON
			&& fabs(linear->point1.y - linear->point2.y) < FLOAT_EPSILON;
}

static void GraphicsGradientColorAverage(const GRAPHICS_GRADIENT_PATTERN* gradient, GRAPHICS_COLOR* color)
{
	FLOAT_T delta0, delta1;
	FLOAT_T r, g, b, a;
	unsigned int i, start = 1, end;

	ASSERT(gradient->num_stops > 0);
	// ASSERT(gradient->base.extend != GRAPHICS_EXTEND_NONE);

	if(gradient->num_stops == 1)
	{
		InitializeGraphicsColorRGBA(color,
			gradient->stops[0].color.red, gradient->stops[0].color.green, gradient->stops[0].color.blue, gradient->stops[0].color.alpha);
		return;
	}

	end = gradient->num_stops - 1;

	switch(gradient->base.extend)
	{
	case GRAPHICS_EXTEND_REPEAT:
		delta0 = 1.0 + gradient->stops[1].offset - gradient->stops[end].offset;
		delta1 = 1.0 + gradient->stops[0].offset - gradient->stops[end-1].offset;
		break;
	case GRAPHICS_EXTEND_REFLECT:
		delta0 = gradient->stops[0].offset + gradient->stops[1].offset;
		delta1 = 2.0 - gradient->stops[end-1].offset - gradient->stops[end].offset;
		break;
	case GRAPHICS_EXTEND_PAD:
		delta0 = delta1 = 1.0;
		start = end;
		break;
	case GRAPHICS_EXTEND_NONE:
	default:
		InitializeGraphicsColorRGBA(color, 0, 0, 0, 0);
		return;
	}

	r = delta0 * gradient->stops[0].color.red;
	g = delta0 * gradient->stops[0].color.green;
	b = delta0 * gradient->stops[0].color.blue;
	a = delta0 * gradient->stops[0].color.alpha;

	for(i = start; i < end; i++)
	{
		FLOAT_T delta = gradient->stops[i+1].offset - gradient->stops[i-1].offset;
		r += delta * gradient->stops[i].color.red;
		g += delta * gradient->stops[i].color.green;
		b += delta * gradient->stops[i].color.blue;
		a += delta * gradient->stops[i].color.alpha;
	}

	r += delta1 * gradient->stops[end].color.red;
	g += delta1 * gradient->stops[end].color.green;
	b += delta1 * gradient->stops[end].color.blue;
	a += delta1 * gradient->stops[end].color.alpha;

	InitializeGraphicsColorRGBA(color, r * 0.5, g * 0.5, b * 0.5, a * 0.5);
}

int GraphicsColorStopEqual(const GRAPHICS_COLOR_STOP* color_a, const GRAPHICS_COLOR_STOP* color_b)
{
	if(color_a == color_b)
	{
		return TRUE;
	}

	return color_a->alpha_short == color_b->alpha_short
		&& color_a->red_short == color_b->red_short
		&& color_a->green_short == color_b->green_short
		&& color_a->blue_short == color_b->blue_short;
}

void GraphicsGradientPatternFitToRange(
	const GRAPHICS_GRADIENT_PATTERN* gradient,
	FLOAT_T max_value,
	GRAPHICS_MATRIX* out_matrix,
	GRAPHICS_CIRCLE_FLOAT out_circle[2]
)
{
	FLOAT_T dim;

	if(gradient->base.type == GRAPHICS_PATTERN_TYPE_LINEAR)
	{
		GRAPHICS_LINEAR_PATTERN *linear = (GRAPHICS_LINEAR_PATTERN*)gradient;

		out_circle[0].center = linear->point1;
		out_circle[0].radius = 0;
		out_circle[1].center = linear->point2;
		out_circle[1].radius = 0;

		dim = FABS(linear->point1.x);
		dim = MAXIMUM(dim, FABS(linear->point1.y));
		dim = MAXIMUM(dim, FABS(linear->point2.x));
		dim = MAXIMUM(dim, FABS(linear->point2.y));
		dim = MAXIMUM(dim, FABS(linear->point1.x - linear->point2.x));
		dim = MAXIMUM(dim, FABS(linear->point1.y - linear->point2.y));
	}
	else
	{
		GRAPHICS_RADIAL_PATTERN *radial = (GRAPHICS_RADIAL_PATTERN*)gradient;

		out_circle[0] = radial->circle1;
		out_circle[1] = radial->circle2;

		dim = FABS(radial->circle1.center.x);
		dim = MAXIMUM(dim, radial->circle1.center.y);
		dim = MAXIMUM(dim, radial->circle1.radius);
		dim = MAXIMUM(dim, radial->circle2.center.x);
		dim = MAXIMUM(dim, radial->circle2.center.y);
		dim = MAXIMUM(dim, radial->circle2.radius);
		dim = MAXIMUM(dim, radial->circle1.center.x - radial->circle2.center.x);
		dim = MAXIMUM(dim, radial->circle1.center.y - radial->circle2.center.y);
		dim = MAXIMUM(dim, radial->circle1.radius - radial->circle2.radius);
	}

	if(UNLIKELY(dim > max_value))
	{
		GRAPHICS_MATRIX scale;

		dim = max_value / dim;

		out_circle[0].center.x *= dim;
		out_circle[0].center.y *= dim;
		out_circle[0].radius *= dim;
		out_circle[1].center.x *= dim;
		out_circle[1].center.y *= dim;
		out_circle[1].radius *= dim;

		InitializeGraphicsMatrixScale(&scale, dim, dim);
		GraphicsMatrixMultiply(out_matrix, &gradient->base.matrix, &scale);
	}
	else
	{
		*out_matrix = gradient->base.matrix;
	}
}

int GraphicsGradientPatternIsSolid(
	const GRAPHICS_GRADIENT_PATTERN* gradient,
	const GRAPHICS_RECTANGLE_INT* extents,
	GRAPHICS_COLOR* color
)
{
	unsigned int i;

	ASSERT(gradient->base.type == GRAPHICS_PATTERN_TYPE_LINEAR
		   || gradient->base.type == GRAPHICS_PATTERN_TYPE_RADIAL);

	if(gradient->base.type == GRAPHICS_PATTERN_TYPE_LINEAR
			|| gradient->base.type == GRAPHICS_PATTERN_TYPE_RADIAL)
	{
		GRAPHICS_LINEAR_PATTERN *linear = (GRAPHICS_LINEAR_PATTERN*)gradient;
		if(GraphicsLinearPatternIsDegenerate(linear))
		{
			GraphicsGradientColorAverage(gradient, color);
			return TRUE;
		}

		if(gradient->base.extend == GRAPHICS_EXTEND_NONE)
		{
			FLOAT_T t[2];

			if(extents == NULL)
			{
				return FALSE;
			}

			GraphicsLinearPatternBoxToParameter(linear, extents->x, extents->y,
				extents->x + extents->width, extents->y + extents->height, t);

			if(t[0] < 0.0 || t[1] > 1.0)
			{
				return FALSE;
			}
		}
		else
		{
			return FALSE;
		}

		for(i = 1; i < gradient->num_stops; i++)
		{
			if(FALSE == GraphicsColorStopEqual(
				&gradient->stops[0].color, &gradient->stops[i].color))
			{
				return FALSE;
			}
		}
	}

	InitializeGraphicsColorRGBA(color, gradient->stops[0].color.red,
		gradient->stops[0].color.green, gradient->stops[0].color.blue, gradient->stops[0].color.alpha);

	return TRUE;
}

void GraphicsPatternPretransform(GRAPHICS_PATTERN* pattern, const GRAPHICS_MATRIX* matrix)
{
	if(pattern->status != GRAPHICS_STATUS_SUCCESS)
	{
		return;
	}

	GraphicsMatrixMultiply(&pattern->matrix, &pattern->matrix, matrix);
}

void GraphicsPatternTransform(GRAPHICS_PATTERN* pattern, const GRAPHICS_MATRIX* matrix_inverse)
{
	if(pattern->status != GRAPHICS_STATUS_SUCCESS)
	{
		return;
	}

	GraphicsMatrixMultiply(&pattern->matrix, matrix_inverse, &pattern->matrix);
}

void GraphicsPatternSetExtend(GRAPHICS_PATTERN* pattern, eGRAPHICS_EXTENED extend)
{
	pattern->extend = extend;
	GraphicsPatternNotifyObservers(pattern, GRAPHICS_PATTERN_NOTIFY_EXTEND);
}

void InitializeGraphicsPatternGradient(GRAPHICS_GRADIENT_PATTERN* pattern, eGRAPHICS_PATTERN_TYPE type, void* graphics)
{
	InitializeGraphicsPattern(&pattern->base, type, graphics);

	pattern->num_stops = 0;
	pattern->stop_size = 0;
	pattern->stops = NULL;
	pattern->base.extend = GRAPHICS_EXTEND_GRADIENT_DEFAULT;
}

void InitializeGraphicsPatternLinear(
	GRAPHICS_LINEAR_PATTERN* pattern,
	FLOAT_T x0, FLOAT_T y0,
	FLOAT_T x1, FLOAT_T y1,
	void* graphics
)
{
	InitializeGraphicsPatternGradient(&pattern->base, GRAPHICS_PATTERN_TYPE_LINEAR, graphics);

	pattern->point1.x = x0;
	pattern->point1.y = y0;
	pattern->point2.x = x1;
	pattern->point2.y = y1;
}

GRAPHICS_PATTERN* CreateGraphicsPatternLinear(
	FLOAT_T x0, FLOAT_T y0,
	FLOAT_T x1, FLOAT_T y1,
	void* graphics
)
{
	GRAPHICS_LINEAR_PATTERN *pattern;
	pattern = (GRAPHICS_LINEAR_PATTERN*)MEM_ALLOC_FUNC(sizeof(*pattern));
	InitializeGraphicsPatternLinear(pattern, x0, y0, x1, y1, graphics);
	pattern->base.base.own_memory = TRUE;
	return &pattern->base.base;
}

void InitializeGraphicsPatternRadial(GRAPHICS_RADIAL_PATTERN* pattern,
	FLOAT_T cx0, FLOAT_T cy0, FLOAT_T radius0,
	FLOAT_T cx1, FLOAT_T cy1, FLOAT_T radius1,
	void* graphics
)
{
	InitializeGraphicsPatternGradient(&pattern->base, GRAPHICS_PATTERN_TYPE_RADIAL, graphics);

	pattern->circle1.center.x = cx0;
	pattern->circle1.center.y = cy0;
	pattern->circle1.radius = FABS(radius0);
	pattern->circle2.center.x = cx1;
	pattern->circle2.center.y = cy1;
	pattern->circle2.radius = FABS(radius1);
}

GRAPHICS_PATTERN* CreateGraphicsPatternRadial(
	FLOAT_T cx0, FLOAT_T cy0, FLOAT_T radius0,
	FLOAT_T cx1, FLOAT_T cy1, FLOAT_T radius1,
	void* graphics
)
{
	GRAPHICS_RADIAL_PATTERN *pattern;
	pattern = (GRAPHICS_RADIAL_PATTERN*)MEM_ALLOC_FUNC(sizeof(*pattern));
	InitializeGraphicsPatternRadial(pattern, cx0, cy0, radius0, cx1, cy1, radius1, graphics);
	pattern->base.base.own_memory = TRUE;
	return &pattern->base.base;
}

static eGRAPHICS_STATUS GraphicsPatternGradientGrow(GRAPHICS_GRADIENT_PATTERN* pattern)
{
	GRAPHICS_GRADIENT_STOP* new_stops;
	int old_size = pattern->stop_size;
	int embedded_size = sizeof(pattern->stops_embedded) / sizeof(pattern->stops_embedded[0]);
	int new_size = 2 * MAXIMUM(old_size, 4);

	if(old_size < embedded_size)
	{
		pattern->stops = pattern->stops_embedded;
		pattern->stop_size = embedded_size;
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(pattern->stops == pattern->stops_embedded)
	{
		new_stops = (GRAPHICS_GRADIENT_STOP*)MEM_ALLOC_FUNC(sizeof(*new_stops) * new_size);
		if(new_stops != NULL)
		{
			(void)memcpy(new_stops, pattern->stops, old_size * sizeof(*new_stops));
		}
	}
	else
	{
		new_stops = (GRAPHICS_GRADIENT_STOP*)MEM_REALLOC_FUNC(pattern->stops, new_size * sizeof(*new_stops));
	}

	if(UNLIKELY(new_stops == NULL))
	{
		return GRAPHICS_STATUS_NO_MEMORY;
	}

	pattern->stops = new_stops;
	pattern->stop_size = new_size;

	return GRAPHICS_STATUS_SUCCESS;
}

static void GraphicsPatternAddColorStop(
	GRAPHICS_GRADIENT_PATTERN* pattern,
	FLOAT_T offset,
	FLOAT_T red,
	FLOAT_T green,
	FLOAT_T blue,
	FLOAT_T alpha
)
{
	GRAPHICS_GRADIENT_STOP *stops;
	unsigned int i;

	if(pattern->num_stops >= pattern->stop_size)
	{
		eGRAPHICS_STATUS status = GraphicsPatternGradientGrow(pattern);
		if(UNLIKELY(status))
		{
			pattern->base.status = status;
			return;
		}
	}

	stops = pattern->stops;

	for(i = 0; i < pattern->num_stops; i++)
	{
		if(offset < stops[i].offset)
		{
			(void)memmove(&stops[i + 1], &stops[i], sizeof(stops[0]) * (pattern->num_stops - i));
			break;
		}
	}

	stops[i].offset = offset;

	stops[i].color.red = red;
	stops[i].color.green = green;
	stops[i].color.blue = blue;
	stops[i].color.alpha = alpha;

	stops[i].color.red_short = GRAPHICS_COLOR_FLOAT_TO_SHORT(red);
	stops[i].color.green_short = GRAPHICS_COLOR_FLOAT_TO_SHORT(green);
	stops[i].color.blue_short = GRAPHICS_COLOR_FLOAT_TO_SHORT(blue);
	stops[i].color.alpha_short = GRAPHICS_COLOR_FLOAT_TO_SHORT(alpha);

	pattern->num_stops++;
}

void GraphicsPatternAddColorStopRGB(
	GRAPHICS_PATTERN* pattern,
	FLOAT_T offset,
	FLOAT_T red,
	FLOAT_T green,
	FLOAT_T blue
)
{
	GraphicsPatternAddColorStopRGBA(pattern, offset, red, green, blue, 1.0);
}

void GraphicsPatternAddColorStopRGBA(
	GRAPHICS_PATTERN* pattern,
	FLOAT_T offset,
	FLOAT_T red,
	FLOAT_T green,
	FLOAT_T blue,
	FLOAT_T alpha
)
{
	if(UNLIKELY(pattern->status))
	{
		return;
	}

	if(pattern->type != GRAPHICS_PATTERN_TYPE_LINEAR
		&& pattern->type != GRAPHICS_PATTERN_TYPE_RADIAL)
	{
		pattern->status = GRAPHICS_STATUS_PATTERN_TYPE_MISMATCH;
		return;
	}

	offset = GraphicsRestrictValue(offset, 0.0, 1.0);
	red = GraphicsRestrictValue(red, 0.0, 1.0);
	green = GraphicsRestrictValue(green, 0.0, 1.0);
	blue = GraphicsRestrictValue(blue, 0.0, 1.0);
	alpha = GraphicsRestrictValue(alpha, 0.0, 1.0);

	GraphicsPatternAddColorStop((GRAPHICS_GRADIENT_PATTERN*)pattern,
		offset, red, green, blue, alpha);
}

#ifdef __cplusplus
}
#endif
