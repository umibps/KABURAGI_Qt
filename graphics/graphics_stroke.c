#include <string.h>
#include <math.h>
#include "graphics.h"
#include "graphics_types.h"
#include "graphics_private.h"
#include "graphics_matrix.h"
#include "graphics_state.h"
#include "../memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

static INLINE void LerpHalf(const GRAPHICS_POINT* a, const GRAPHICS_POINT* b, GRAPHICS_POINT* result)
{
	result->x = a->x + ((b->x - a->x) >> 1);
	result->y = a->y + ((b->y - a->y) >> 1);
}

void GraphicsStrokeStyleFinish(GRAPHICS_STROKE_STYLE* style)
{
	MEM_FREE_FUNC(style->dash);
	style->dash = NULL;

	style->num_dashes = 0;
}

void GraphicsStrokeStyleInitialize(GRAPHICS_STROKE_STYLE* style)
{
	style->line_width = GRAPHICS_STATE_LINE_WIDTH_DEFAULT;
	style->line_cap = GRAPHICS_STATE_LINE_CAP_DEFAULT;
	style->line_join = GRAPHICS_STATE_LINE_JOIN_DEFAULT;
	style->miter_limit = GRAPHICS_STATE_MITER_LIMIT_DEFAULT;

	style->dash = NULL;
	style->num_dashes = 0;
	style->dash_offset = 0.0;
}

eGRAPHICS_STATUS GraphicsStrokeStyleInitializeCopy(GRAPHICS_STROKE_STYLE* style, const GRAPHICS_STROKE_STYLE* other)
{
	// PENDING CAIRO_INJET_FAULT

	style->line_width = other->line_width;
	style->line_cap = other->line_cap;
	style->line_join = other->line_join;
	style->miter_limit = other->miter_limit;

	style->num_dashes = other->num_dashes;

	if(other->dash == NULL)
	{
		style->dash = NULL;
	}
	else
	{
		style->dash = MEM_CALLOC_FUNC(style->num_dashes, sizeof(*style->dash));
		if(UNLIKELY(style->dash == NULL))
		{
			return GRAPHICS_STATUS_NO_MEMORY;
		}
	}

	(void)memcpy(style->dash, other->dash, style->num_dashes * sizeof(*style->dash));

	style->dash_offset = other->dash_offset;

	return GRAPHICS_STATUS_SUCCESS;
}

int InitializeGraphicsSpline(
	GRAPHICS_SPLINE* spline,
	GRAPHICS_SPLINE_ADD_POINT_FUNCTION add_point_function,
	void* closure,
	const GRAPHICS_POINT* a, const GRAPHICS_POINT* b,
	const GRAPHICS_POINT* c, const GRAPHICS_POINT* d
)
{
	if(a->x == b->x && a->y == b->y && c->x == d->x && c->y == d->y)
	{
		return FALSE;
	}

	spline->add_point_function = add_point_function;
	spline->closure = closure;

	spline->knots.a = *a,   spline->knots.b = *b;
	spline->knots.c = *c,   spline->knots.d = *d;

	if(a->x != b->x || a->y != b->y)
	{
		INITIALIZE_GRAPHICS_SLOPE(&spline->initial_slope, &spline->knots.a, &spline->knots.b);
	}
	else if(a->x != c->x || a->y != c->y)
	{
		INITIALIZE_GRAPHICS_SLOPE(&spline->initial_slope, &spline->knots.a, &spline->knots.c);
	}
	else if(a->x != d->x || a->y != d->y)
	{
		INITIALIZE_GRAPHICS_SLOPE(&spline->initial_slope, &spline->knots.a, &spline->knots.d);
	}
	else
	{
		return FALSE;
	}

	if(c->x != d->x || c->y != d->y)
	{
		INITIALIZE_GRAPHICS_SLOPE(&spline->final_slope, &spline->knots.c, &spline->knots.d);
	}
	else if(b->x != d->x || b->y != d->y)
	{
		INITIALIZE_GRAPHICS_SLOPE(&spline->final_slope, &spline->knots.b, &spline->knots.d);
	}
	else
	{
		return FALSE;
	}

	return TRUE;
}

static FLOAT_T GraphicsSplineErrorSquared(const GRAPHICS_SPLINE_KNOTS* knots)
{
	FLOAT_T bdx, bdy, berror;
	FLOAT_T cdx, cdy, cerror;

	bdx = GRAPHICS_FIXED_TO_FLOAT(knots->b.x - knots->a.x);
	bdy = GRAPHICS_FIXED_TO_FLOAT(knots->b.y - knots->a.y);

	cdx = GRAPHICS_FIXED_TO_FLOAT(knots->c.x - knots->a.x);
	cdy = GRAPHICS_FIXED_TO_FLOAT(knots->c.y - knots->a.y);

	if(knots->a.x != knots->d.x || knots->a.y != knots->d.y)
	{
		FLOAT_T dx, dy, u, v;

		dx = GRAPHICS_FIXED_TO_FLOAT(knots->d.x - knots->a.x);
		dy = GRAPHICS_FIXED_TO_FLOAT(knots->d.y - knots->a.y);
		v = dx * dx + dy * dy;
		u = bdx * dx + bdy * dy;
		if(u <= 0)
		{
			// Nothing to do
		}
		else if(u >= v)
		{
			bdx -= dx;
			bdy -= dy;
		}
		else
		{
			bdx -= u/v * dx;
			bdy -= u/v * dy;
		}

		u = cdx * dx + cdy * dy;
		if(u <= 0)
		{
			// Nothing to do
		}
		else if(u >= v)
		{
			cdx -= dx;
			cdy -= dy;
		}
		else
		{
			cdx -= u/v * dx;
			cdy -= u/v * dy;
		}
	}

	berror = bdx * bdx + bdy * bdy;
	cerror = cdx * cdx + cdy * cdy;
	if(berror > cerror)
	{
		return berror;
	}
	return cerror;
}

static void DeCasteljau(GRAPHICS_SPLINE_KNOTS* s1, GRAPHICS_SPLINE_KNOTS* s2)
{
	GRAPHICS_POINT ab, bc, cd;
	GRAPHICS_POINT abbc, bccd;
	GRAPHICS_POINT final;

	LerpHalf(&s1->a, &s1->b, &ab);
	LerpHalf(&s1->b, &s1->c, &bc);
	LerpHalf(&s1->c, &s1->d, &cd);
	LerpHalf(&ab, &bc, &abbc);
	LerpHalf(&bc, &cd, &bccd);
	LerpHalf(&abbc, &bccd, &final);

	s2->a = final;
	s2->b = bccd;
	s2->c = cd;
	s2->d = s1->d;

	s1->b = ab;
	s1->c = abbc;
	s1->d = final;
}

static eGRAPHICS_STATUS GraphicsSplineAddPoint(GRAPHICS_SPLINE* spline, const GRAPHICS_POINT* point, const GRAPHICS_POINT* knot)
{
	GRAPHICS_POINT *prev;
	GRAPHICS_SLOPE slope;

	prev = &spline->last_point;
	if(prev->x == point->x && prev->y == point->y)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	INITIALIZE_GRAPHICS_SLOPE(&slope, point, knot);

	spline->last_point = *point;
	return spline->add_point_function(spline->closure, point, &slope);
}

static eGRAPHICS_STATUS GraphicsSplineDecomposeInto(
	GRAPHICS_SPLINE_KNOTS* s1,
	FLOAT_T tolerance_squared,
	GRAPHICS_SPLINE* result
)
{
	GRAPHICS_SPLINE_KNOTS s2;
	eGRAPHICS_STATUS status;

	if(GraphicsSplineErrorSquared(s1) < tolerance_squared)
	{
		return GraphicsSplineAddPoint(result, &s1->a, &s1->b);
	}

	DeCasteljau(s1, &s2);

	status = GraphicsSplineDecomposeInto(s1, tolerance_squared, result);
	if(UNLIKELY(status))
	{
		return status;
	}

	return GraphicsSplineDecomposeInto(&s2, tolerance_squared, result);
}

eGRAPHICS_STATUS GraphicsSplineDecompose(GRAPHICS_SPLINE* spline, FLOAT_T tolerance)
{
	GRAPHICS_SPLINE_KNOTS s1;
	eGRAPHICS_STATUS status;

	s1 = spline->knots;
	spline->last_point = s1.a;
	status = GraphicsSplineDecomposeInto(&s1, tolerance * tolerance, spline);
	if(UNLIKELY(status))
	{
		return status;
	}

	return spline->add_point_function(spline->closure, &spline->knots.d, &spline->final_slope);
}

void GraphicsStrokeStyleMaxDistanceFromPath(
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_MATRIX* ctm,
	FLOAT_T* dx, FLOAT_T* dy
)
{
	FLOAT_T style_expansion = 0.5;

	if(style->line_cap == GRAPHICS_LINE_CAP_SQUARE)
	{
		style_expansion = M_SQRT1_2;
	}

	if(style->line_cap == GRAPHICS_LINE_JOIN_MITER
		&& FALSE == path->stroke_is_rectilinear
		&& style_expansion < M_SQRT2 * style->miter_limit
	)
	{
		style_expansion = M_SQRT2 * style->miter_limit;
	}

	style_expansion *= style->line_width;

	if(GraphicsMatrixHasUnityScale(ctm) != FALSE)
	{
		*dx = *dy = style_expansion;
	}
	else
	{
		*dx = style_expansion * HYPOT(ctm->xx, ctm->xy);
		*dy = style_expansion * HYPOT(ctm->yy, ctm->yx);
	}
}

void GraphicsStrokeStyleMaxLineDistanceFromPath(
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_MATRIX* matrix,
	FLOAT_T* dx,
	FLOAT_T* dy
)
{
	FLOAT_T style_expansion = 0.5 * style->line_width;
	if(GraphicsMatrixHasUnityScale(matrix))
	{
		*dx = *dy = style_expansion;
	}
	else
	{
		*dx = style_expansion * HYPOT(matrix->xx, matrix->xy);
		*dy = style_expansion * HYPOT(matrix->yy, matrix->yx);
	}
}

void GraphicsStrokeStyleMaxJoinDistanceFromPath(
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_MATRIX* matrix,
	FLOAT_T* dx,
	FLOAT_T* dy
)
{
	FLOAT_T style_expansion = 0.5;

	if(style->line_join == GRAPHICS_LINE_JOIN_MITER
		&& FALSE == path->stroke_is_rectilinear
		&& style_expansion < M_SQRT2 * style->miter_limit)
	{
		style_expansion = M_SQRT2 * style->miter_limit;
	}

	style_expansion *= style->line_width;

	if(GraphicsMatrixHasUnityScale(matrix))
	{
		*dx = *dy = style_expansion;
	}
	else
	{
		*dx = style_expansion * HYPOT(matrix->xx, matrix->xy);
		*dy = style_expansion * HYPOT(matrix->yy, matrix->yx);
	}
}

FLOAT_T GraphicsStrokeStyleDashPeriod(const GRAPHICS_STROKE_STYLE* style)
{
	FLOAT_T period;
	unsigned int i;
	
	for(i=0, period = 0.0; i < style->num_dashes; i++)
	{
		period += style->dash[i];
	}
	
	if(style->num_dashes & 1)
	{
		period *= 2;
	}
	
	return period;
}

#define ROUND_MINSQ_APPROXIMATION (9*M_PI/32)

FLOAT_T GraphicsStrokeStyleDashStroked(const GRAPHICS_STROKE_STYLE* style)
{
	FLOAT_T stroked, cap_scale;
	unsigned int i;
	
	switch(style->line_cap)
	{
	case GRAPHICS_LINE_CAP_BUTT:
		cap_scale = 0.0;
		break;
	case GRAPHICS_LINE_CAP_ROUND:
		cap_scale = ROUND_MINSQ_APPROXIMATION;
		break;
	case GRAPHICS_LINE_CAP_SQUARE:
		cap_scale = 1.0;
		break;
	}
	
	stroked = 0.0;
	if(style->num_dashes & 1)
	{
		for(i=0; i<style->num_dashes; i++)
		{
			stroked += style->dash[i] + cap_scale * MINIMUM(style->dash[i], style->line_width);
		}
	}
	else
	{
		for(i=0; i<style->num_dashes; i+=2)
		{
			stroked += style->dash[i] + cap_scale * MINIMUM(style->dash[i+1], style->line_width);
		}
	}
	
	return stroked;
}

int GraphicsStrokeStyleDashCanApproximate(
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_MATRIX* matrix,
	FLOAT_T tolerance
)
{
	FLOAT_T period;
	
	if(0 == style->num_dashes)
	{
		return FALSE;
	}
	
	period = GraphicsStrokeStyleDashPeriod(style);
	return GraphicsMatrixTransformedCircleMajorAxis(matrix, period) < tolerance;
}

void GraphicsStrokeStyleDashApproximate(
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_MATRIX* matrix,
	FLOAT_T tolerance,
	FLOAT_T* dash_offset,
	FLOAT_T* dashes,
	unsigned int* num_dashes
)
{
	FLOAT_T coverage, scale, offset;
	int on = TRUE;
	unsigned int i = 0;
	
	coverage = GraphicsStrokeStyleDashStroked(style) / GraphicsStrokeStyleDashPeriod(style);
	coverage = MINIMUM(coverage, 1.0);
	scale = tolerance / GraphicsMatrixTransformedCircleMajorAxis(matrix, 1.0);
	
	offset = style->dash_offset;
	while(offset > 0.0 && offset >= style->dash[i])
	{
		offset -= style->dash[i];
		on = !on;
		if(++i == style->num_dashes)
		{
			i = 0;
		}
	}
	
	*num_dashes = 2;
	
	switch(style->line_cap)
	{
	case GRAPHICS_LINE_CAP_BUTT:
		dashes[0] = scale * coverage;
		break;
	case GRAPHICS_LINE_CAP_ROUND:
		dashes[0] = MAXIMUM(scale * (coverage - ROUND_MINSQ_APPROXIMATION) / (1.0 - ROUND_MINSQ_APPROXIMATION),
			scale * coverage - ROUND_MINSQ_APPROXIMATION * style->line_width);
		break;
	case GRAPHICS_LINE_CAP_SQUARE:
		dashes[0] = MAXIMUM(0.0, scale * coverage - style->line_width);
		break;
	}
	
	dashes[1] = scale - dashes[0];
	
	*dash_offset = on ? 0.0 : dashes[0];
}

#ifdef __cplusplus
}
#endif
