#include "graphics.h"
#include "graphics_private.h"
#include "graphics_pattern.h"
#include "graphics_matrix.h"
#include "graphics_status.h"
#include "graphics_state.h"
#include "../memory.h"

#ifdef __cplusplus
extern "C" {
#endif

void GraphicsDefaultContextFinish(GRAPHICS_DEFAULT_CONTEXT* context)
{
	while(context->state != &context->state[0])
	{
		if(GraphicsStateRestore(&context->state, &context->state_free_list))
		{
			break;
		}
	}

	GraphicsStateFinish(context->state);
	context->state_free_list = context->state_free_list->next;
	while(context->state_free_list != NULL)
	{
		GRAPHICS_STATE *state = context->state_free_list;
		context->state_free_list = state->next;
		MEM_FREE_FUNC(state);
	}

	GraphicsPathFixedFinish(&context->path);

	// PENDING _cairo_fini
}

static void GraphicsDefaultContextDestroy(void* context)
{
	GRAPHICS_DEFAULT_CONTEXT *con = (GRAPHICS_DEFAULT_CONTEXT*)context;

	GraphicsDefaultContextFinish(con);

	con->base.status = GRAPHICS_STATUS_NULL_POINTER;

	// PENDING _freed_pool_put
}

static GRAPHICS_SURFACE* GraphicsDefaultContextGetOriginalTarget(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	return context->state->original_target;
}

static GRAPHICS_SURFACE* GraphicsDefaultContextGetCurrentTarget(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	return context->state->target;
}

static eGRAPHICS_STATUS GraphicsDefaultContextSave(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	return GraphicsStateSave(&context->state, &context->state_free_list);
}

static eGRAPHICS_STATUS GraphicsDefaultContextRestore(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	if(UNLIKELY(GraphicsStateIsGroup(context->state)))
	{
		return GRAPHICS_STATUS_INVALID_RESTORE;
	}
	return GraphicsStateRestore(&context->state, &context->state_free_list);
}

static eGRAPHICS_STATUS GraphicsDefaultContextPushGroup(void* abstract_context, eGRAPHICS_CONTENT content)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	GRAPHICS_SURFACE group_surface;
	GRAPHICS_CLIP *clip;
	eGRAPHICS_STATUS status;

	clip = GraphicsStateGetClip(context->state);
	if(GraphicsClipIsAllClipped(clip))
	{
		if(GraphicsImageSurfaceInitialize(&group_surface, GRAPHICS_FORMAT_ARGB32, 0, 0, 0, context->base.graphics) == FALSE)
		{
			return GRAPHICS_STATUS_INVALID;
		}
		status = group_surface.status;
	}
	if(UNLIKELY(status))
	{
		goto failed;
	}
	else
	{
		GRAPHICS_SURFACE *parent_surface;
		GRAPHICS_RECTANGLE_INT extents;
		int bounded, is_empty;

		parent_surface = GraphicsStateGetTarget(context->state);

		if(UNLIKELY(parent_surface->status))
		{
			ReleaseGraphicsSurface(&group_surface);
			return parent_surface->status;
		}
		if(UNLIKELY(parent_surface->finished))
		{
			return GRAPHICS_STATUS_SURFACE_FINISHED;
		}

		bounded = GraphicsSurfaceGetExtents(parent_surface, &extents);
		if(clip != NULL)
		{
			is_empty = GraphicsRectangleIntersect(&extents, GraphicsClip);
		}
	}

failed:
	ReleaseGraphicsSurface(&group_surface);
	return status;
}

static GRAPHICS_PATTERN* GraphicsDefaultContextPopGroup(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	GRAPHICS_SURFACE *group_surface;
	GRAPHICS_PATTERN *group_pattern;
	GRAPHICS_SURFACE *parent_surface;
	GRAPHICS_MATRIX group_matrix;
	eGRAPHICS_STATUS status;
	
	if(UNLIKELY(FALSE == GraphicsStateIsGroup(context->state)))
	{
		return CreateGraphicsPatternInError(GRAPHICS_STATUS_INVALID_POP_GROUP, context->base.graphics);
	}

	group_surface = context->state->target;
	group_surface = GraphicsSurfaceReference(group_surface);

	status = GraphicsStateRestore(&context->state, &context->state_free_list);
	ASSERT(status == GRAPHICS_STATUS_SUCCESS);

	parent_surface = GraphicsStateGetTarget(context->state);

	group_pattern = CreateGraphicsPatternForSurface(group_surface);
	status = group_pattern->status;
	if(UNLIKELY(status))
	{
		goto done;
	}

	GraphicsStateGetMatrix(context->state, &group_matrix);
	GraphicsPatternSetMatrix(group_pattern, &group_matrix);

	GraphicsPathFixedTranslate(&context->path,
		GraphicsFixedFromInteger(parent_surface->device_transform.x0 - group_surface->device_transform.x0),
		GraphicsFixedFromInteger(parent_surface->device_transform.y0 - group_surface->device_transform.y0)
	);

done:
	group_surface->reference_count--;
	//DestroyGraphicsSurface(group_surface);

	return group_pattern;
}

static eGRAPHICS_STATUS GraphicsDefaultContextSetSource(void* abstract_context, GRAPHICS_PATTERN* source)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateSetSource(context->state, source);
}

static int CurrentSourceMatchesSolid(
	const GRAPHICS_PATTERN* pattern,
	FLOAT_T red,
	FLOAT_T green,
	FLOAT_T blue,
	FLOAT_T alpha
)
{
	GRAPHICS_COLOR color;

	if(pattern->type != GRAPHICS_PATTERN_TYPE_SOLID)
	{
		return FALSE;
	}

#if defined(GRAPHICS_COLOR_FLOAT_VALUE_UNCONTROLLED) && GRAPHICS_COLOR_FLOAT_VALUE_UNCONTROLLED != 0
	red = GraphicsRestrictValue(red, 0.0, 1.0);
	green = GraphicsRestrictValue(green, 0.0, 1.0);
	blue = GraphicsRestrictValue(blue, 0.0, 1.0);
	alpha = GraphicsRestrictValue(alpha, 0.0, 1.0);
#endif
	InitializeGraphicsColorRGBA(&color, red, green, blue, alpha);
	return GraphicsColorEqual(&color, &((GRAPHICS_SOLID_PATTERN*)pattern)->color);
}

static eGRAPHICS_STATUS GraphicsDefaultContextSetSourceRGBA(
	void* abstract_context,
	FLOAT_T red,
	FLOAT_T green,
	FLOAT_T blue,
	FLOAT_T alpha
)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	GRAPHICS_PATTERN *pattern;
	eGRAPHICS_STATUS status;

	if(CurrentSourceMatchesSolid(context->state->source, red, green, blue, alpha) != FALSE)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	GraphicsDefaultContextSetSource(abstract_context, &context->base.graphics->pattern_black.base);

	pattern = CreateGraphicsPatternRGBA(red, green, blue, alpha, context->base.graphics);
	if(UNLIKELY(pattern->status))
	{
		return pattern->status;
	}

	status = GraphicsDefaultContextSetSource(context, pattern);
	pattern->reference_count--;
	//DestroyGraphicsPattern(pattern);

	return status;
}

static eGRAPHICS_STATUS GraphicsDefaultContextSetSourceSurface(
	void* absract_context,
	GRAPHICS_SURFACE* surface,
	FLOAT_T x,
	FLOAT_T y,
	struct _GRAPHICS_SURFACE_PATTERN* surface_pattern
)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)absract_context;
	GRAPHICS_PATTERN *pattern;
	GRAPHICS_MATRIX matrix;
	eGRAPHICS_STATUS status;

	GraphicsDefaultContextSetSource(context, &context->base.graphics->pattern_black);

	if(surface_pattern == NULL)
	{
		pattern = CreateGraphicsPatternForSurface(surface);
		if(UNLIKELY(pattern->status))
		{
			return pattern->status;
		}
	}
	else
	{
		pattern = surface_pattern;
		if(pattern->graphics == NULL)
		{
			InitializeGraphicsPatternForSurface(surface_pattern, surface);
		}
	}

	InitializeGraphicsMatrixTranslate(&matrix, -x, -y);
	GraphicsPatternSetMatrix(pattern, &matrix);

	status = GraphicsDefaultContextSetSource(context, pattern);
	pattern->reference_count--;

	return status;
}

static GRAPHICS_PATTERN* GraphicsDefaultContextGetSource(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateGetSource(context->state);
}

static eGRAPHICS_STATUS GraphicsDefaultContextSetTolerance(void* abstract_context, FLOAT_T tolerance)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	if(tolerance < GRAPHICS_TOLERANCE_MINIMUM)
	{
		tolerance = GRAPHICS_TOLERANCE_MINIMUM;
	}

	return GraphicsStateSetTolerance(context->state, tolerance);
}

static eGRAPHICS_STATUS GraphicsDefaultContextSetOperator(void* abstract_context, eGRAPHICS_OPERATOR op)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateSetOperator(context->state, op);
}

static eGRAPHICS_STATUS GraphicsDefaultContextSetOpacity(void* abstract_context, FLOAT_T opacity)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateSetOpacity(context->state, opacity);
}

static eGRAPHICS_STATUS GraphicsDefaultContextSetAntialias(void* abstract_context, eGRAPHICS_ANTIALIAS antialias)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateSetAntialias(context->state, antialias);
}

static eGRAPHICS_STATUS GraphicsDefalutContextSetFillRule(void* abstract_context, eGRAPHICS_FILL_RULE fill_rule)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateSetFillRule(context->state, fill_rule);
}

static eGRAPHICS_STATUS GraphicsDefaultContextSetLineWidth(void* abstract_context, FLOAT_T line_width)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateSetLineWidth(context->state, line_width);
}

static eGRAPHICS_STATUS GraphicsDefaultContextSetLineCap(void* abstract_context, eGRAPHICS_LINE_CAP line_cap)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateSetLineCap(context->state, line_cap);
}

static eGRAPHICS_STATUS GraphicsDefaultContextSetLineJoin(void* abstract_context, eGRAPHICS_LINE_JOIN line_join)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateSetLineJoin(context->state, line_join);
}

static eGRAPHICS_STATUS GraphicsDefaultContextSetDash(
	void* abstract_context,
	const GRAPHICS_FLOAT* dashes,
	int num_dashes,
	GRAPHICS_FLOAT offset
)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateSetDash(context->state, dashes, num_dashes, offset);
}

static eGRAPHICS_STATUS GraphicsDefaultContextSetFillRule(void* abstract_context, eGRAPHICS_FILL_RULE fill_rule)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateSetFillRule(context->state, fill_rule);
}

static eGRAPHICS_STATUS GraphicsDefaultContextSetMiterLimit(void* abstract_context, FLOAT_T limit)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateSetMiterLimit(context->state, limit);
}

static eGRAPHICS_ANTIALIAS GraphicsDefaultContextGetAntialias(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateGetAntialias(context->state);
}

static void GraphicsDefaultContextGetDash(
	void* abstract_context,
	GRAPHICS_FLOAT* dashes,
	int* num_dashes,
	GRAPHICS_FLOAT* offset
)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	GraphicsStateGetDash(context->state, dashes, num_dashes, offset);
}

static eGRAPHICS_FILL_RULE GraphicsDefaultContextGetFillRule(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateGetFillRule(context->state);
}

static FLOAT_T GraphicsDefaultContextGetLineWidth(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateGetLineWidth(context->state);
}

static eGRAPHICS_LINE_CAP GraphicsDefaultContextGetLineCap(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateGetLineCap(context->state);
}

static eGRAPHICS_LINE_JOIN GraphicsDefaultContextGetLineJoin(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateGetLineJoin(context->state);
}

static FLOAT_T GraphicsDefaultContextGetMiterLimit(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateGetMiterLimit(context->state);
}

static eGRAPHICS_OPERATOR GraphicsDefaultContextGetOperator(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateGetOperator(context->state);
}

static FLOAT_T GraphicsDefaultContextGetOpacity(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateGetOpacity(context->state);
}

static FLOAT_T GraphicsDefaultContextGetTolerance(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateGetTolerance(context->state);
}

static eGRAPHICS_STATUS GraphicsDefaultContextTranslate(void* abstract_context, FLOAT_T tx, FLOAT_T ty)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateTranslate(context->state, tx, ty);
}

static eGRAPHICS_STATUS GraphicsDefaultContextScale(void* abstract_context, FLOAT_T sx, FLOAT_T sy)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateScale(context->state, sx, sy);
}

static eGRAPHICS_STATUS GraphicsDefaultContextRotate(void* abstract_context, FLOAT_T angle)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateRotate(context->state, angle);
}

static eGRAPHICS_STATUS GraphicsDefaultContextTransform(void* abstract_context, const GRAPHICS_MATRIX* matrix)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateTransform(context->state, matrix);
}

static eGRAPHICS_STATUS GraphicsDefaultContextSetMatrix(void* abstract_context, const GRAPHICS_MATRIX* matrix)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateSetMatrix(context->state, matrix);
}

static eGRAPHICS_STATUS GraphicsDefaultContextSetIdentityMatrix(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	GraphicsStateIdentityMatrix(context->state);
	return GRAPHICS_STATUS_SUCCESS;
}

static void GraphicsDefaultContextGetMatrix(void* abstract_context, GRAPHICS_MATRIX* matrix)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	GraphicsStateGetMatrix(context->state, matrix);
}

static void GraphicsDefaultContextUserToDevice(void* abstract_context, FLOAT_T* x, FLOAT_T* y)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	GraphicsStateUserToDevice(context->state, x, y);
}

static void GraphicsDefaultContextUserToDeviceDistance(void* abstract_context, FLOAT_T* dx, FLOAT_T* dy)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	GraphicsStateUserToDeviceDistance(context->state, dx, dy);
}

static void GraphicsDefaultContextDeviceToUser(void* abstract_context, FLOAT_T* x, FLOAT_T* y)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	GraphicsStateDeviceToUser(context->state, x, y);
}

static void GraphicsDefaultContextDeviceToUserDistance(void* abstract_context, FLOAT_T* dx, FLOAT_T* dy)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	GraphicsStateDeviceToUserDistance(context->state, dx, dy);
}

static void GraphicsDefaultContextBackendToUser(void* abstract_context, FLOAT_T* x, FLOAT_T* y)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	GraphicsStateBackendToUser(context->state, x, y);
}

static void GraphicsDefaultContextBackendToUserDistance(void* abstract_context, FLOAT_T* dx, FLOAT_T* dy)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	GraphicsStateBackendToUserDistance(context->state, dx, dy);
}

static void GraphicsDefaultContextUserToBackend(void* abstract_context, FLOAT_T* x, FLOAT_T* y)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	GraphicsStateUserToBackend(context->state, x, y);
}

static void GraphicsDefaultContextUserToBackendDistance(void* abstract_context, FLOAT_T* dx, FLOAT_T* dy)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	GraphicsStateUserToBackendDistance(context->state, dx, dy);
}

static eGRAPHICS_STATUS GraphicsDefaultContextNewPath(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	GraphicsPathFixedFinish(&context->path);
	InitializeGraphicsPathFixed(&context->path);

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsDefaultContextNewSubPath(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	GraphicsPathFixedNewSubPath(&context->path);

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsDefaultContextMoveTo(void* abstract_context, FLOAT_T x, FLOAT_T y)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	GRAPHICS_FLOAT_FIXED x_fixed, y_fixed;

	GraphicsStateUserToBackend(context->state, &x, &y);
	x_fixed = GraphicsFixedFromFloat(x);
	y_fixed = GraphicsFixedFromFloat(y);

	return GraphicsPathFixedMoveTo(&context->path, x, y);
}

static eGRAPHICS_STATUS GraphicsDefaultContextRelativeMoveTo(void* abstract_context, FLOAT_T dx, FLOAT_T dy)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	GRAPHICS_FLOAT_FIXED dx_fixed, dy_fixed;

	GraphicsStateUserToBackendDistance(context->state, &dx, &dy);

	dx_fixed = GraphicsFixedFromFloat(dx);
	dy_fixed = GraphicsFixedFromFloat(dy);

	return GraphicsPathFixedRelativeMoveTo(&context->path, dx_fixed, dy_fixed);
}

static eGRAPHICS_STATUS GraphicsDefaultContextLineTo(void* abstract_context, FLOAT_T x, FLOAT_T y)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	GRAPHICS_FLOAT_FIXED x_fixed, y_fixed;

	GraphicsStateUserToBackend(context->state, &x, &y);
	x_fixed = GraphicsFixedFromFloat(x);
	y_fixed = GraphicsFixedFromFloat(y);

	return GraphicsPathFixedLineTo(&context->path, x_fixed, y_fixed);
}

static eGRAPHICS_STATUS GraphicsDefaultContextRelativeLineTo(void* abstract_context, FLOAT_T dx, FLOAT_T dy)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	GRAPHICS_FLOAT_FIXED dx_fixed, dy_fixed;

	GraphicsStateUserToBackendDistance(context->state, &dx, &dy);

	dx_fixed = GraphicsFixedFromFloat(dx);
	dy_fixed = GraphicsFixedFromFloat(dy);

	return GraphicsPathFixedRelativeLineTo(&context->path, dx_fixed, dy_fixed);
}

static eGRAPHICS_STATUS GraphicsDefaultContextCurveTo(
	void* abstract_context,
	FLOAT_T x1, FLOAT_T y1,
	FLOAT_T x2, FLOAT_T y2,
	FLOAT_T x3, FLOAT_T y3
)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	GRAPHICS_FLOAT_FIXED x1_fixed, y1_fixed;
	GRAPHICS_FLOAT_FIXED x2_fixed, y2_fixed;
	GRAPHICS_FLOAT_FIXED x3_fixed, y3_fixed;

	GraphicsStateUserToBackend(context->state, &x1, &y1);
	GraphicsStateUserToBackend(context->state, &x2, &y2);
	GraphicsStateUserToBackend(context->state, &x3, &y3);

	x1_fixed = GraphicsFixedFromFloat(x1);
	y1_fixed = GraphicsFixedFromFloat(y1);

	x2_fixed = GraphicsFixedFromFloat(x2);
	y2_fixed = GraphicsFixedFromFloat(y2);

	x3_fixed = GraphicsFixedFromFloat(x3);
	y3_fixed = GraphicsFixedFromFloat(y3);

	return GraphicsPathFixedCurveTo(&context->path,
		x1_fixed, y1_fixed, x2_fixed, y2_fixed, x3_fixed, y3_fixed);
}

static eGRAPHICS_STATUS GraphicsDefaultContextRelativeCurveTo(
	void* abstract_context,
	FLOAT_T dx1, FLOAT_T dy1,
	FLOAT_T dx2, FLOAT_T dy2,
	FLOAT_T dx3, FLOAT_T dy3
)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	GRAPHICS_FLOAT_FIXED dx1_fixed, dy1_fixed;
	GRAPHICS_FLOAT_FIXED dx2_fixed, dy2_fixed;
	GRAPHICS_FLOAT_FIXED dx3_fixed, dy3_fixed;

	GraphicsStateUserToBackend(context->state, &dx1, &dy1);
	GraphicsStateUserToBackend(context->state, &dx2, &dy2);
	GraphicsStateUserToBackend(context->state, &dx3, &dy3);

	dx1_fixed = GraphicsFixedFromFloat(dx1);
	dy1_fixed = GraphicsFixedFromFloat(dy1);

	dx2_fixed = GraphicsFixedFromFloat(dx2);
	dy2_fixed = GraphicsFixedFromFloat(dy2);

	dx3_fixed = GraphicsFixedFromFloat(dx3);
	dy3_fixed = GraphicsFixedFromFloat(dy3);

	return GraphicsPathFixedRelativeCurveTo(&context->path,
		dx1_fixed, dy1_fixed, dx2_fixed, dy2_fixed, dx3_fixed, dy3_fixed);
}

static eGRAPHICS_STATUS GraphicsDefaultContextArc(
	void* abstract_context,
	FLOAT_T xc,
	FLOAT_T yc,
	FLOAT_T radius,
	FLOAT_T angle1,
	FLOAT_T angle2,
	int forward
)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	eGRAPHICS_STATUS status;

	if(radius <= 0.0)
	{
		GRAPHICS_FLOAT_FIXED x_fixed, y_fixed;

		GraphicsStateUserToBackend(context->state, &xc, &yc);
		x_fixed = GraphicsFixedFromFloat(xc);
		y_fixed = GraphicsFixedFromFloat(yc);
		status = GraphicsPathFixedLineTo(&context->path, x_fixed, y_fixed);
		if(UNLIKELY(status))
		{
			return status;
		}
		return GRAPHICS_STATUS_SUCCESS;
	}

	status = GraphicsDefaultContextLineTo(context,
		xc + radius * cos(angle1), yc + radius * sin(angle1));

	if(UNLIKELY(status))
	{
		return status;
	}

	if(forward != FALSE)
	{
		GraphicsArcPath(context, xc, yc, radius, angle1, angle2);
	}
	else
	{
		GraphicsArcPathNegative(context, xc, yc, radius, angle1, angle2);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsDefaultContextClosePath(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsPathFixedClosePath(&context->path);
}

static eGRAPHICS_STATUS GraphicsDefaultContextRectangle(
	void* abstract_context,
	FLOAT_T x, FLOAT_T y,
	FLOAT_T width, FLOAT_T height
)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	eGRAPHICS_STATUS status;

	status = GraphicsDefaultContextMoveTo(context, x, y);
	if(UNLIKELY(status))
	{
		return status;
	}

	status = GraphicsDefaultContextRelativeLineTo(context, width, 0);
	if(UNLIKELY(status))
	{
		return status;
	}

	status = GraphicsDefaultContextRelativeLineTo(context, 0, height);
	if(UNLIKELY(status))
	{
		return status;
	}

	status = GraphicsDefaultContextRelativeLineTo(context, -width, 0);
	if(UNLIKELY(status))
	{
		return status;
	}

	return GraphicsDefaultContextClosePath(context);
}

static eGRAPHICS_STATUS GraphicsDefaultContext(
	void* abstract_context,
	FLOAT_T x,
	FLOAT_T y,
	FLOAT_T width,
	FLOAT_T height
)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	eGRAPHICS_STATUS status;

	status = GraphicsDefaultContextMoveTo(context, x, y);
	if(UNLIKELY(status))
	{
		return status;
	}

	status = GraphicsDefaultContextRelativeLineTo(context, width, 0);
	if(UNLIKELY(status))
	{
		return status;
	}

	status = GraphicsDefaultContextRelativeLineTo(context, 0, height);
	if(UNLIKELY(status))
	{
		return status;
	}

	status = GraphicsDefaultContextRelativeLineTo(context, - width, 0);
	if(UNLIKELY(status))
	{
		return status;
	}

	return GraphicsDefaultContextClosePath(context);
}

static void GraphicsDefaultContextPathExtents(
	void* abstract_context,
	FLOAT_T* x1,
	FLOAT_T* y1,
	FLOAT_T* x2,
	FLOAT_T* y2
)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	GraphicsStatePathExtents(context->state, &context->path, x1, y1, x2, y2);
}

static int GraphicsDefaultContextHasCurrentPoint(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return context->path.has_current_point;
}

static int GraphicsDefaultContextGetCurrentPoint(
	void* abstract_context,
	FLOAT_T* x,
	FLOAT_T* y
)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	GRAPHICS_FLOAT_FIXED x_fixed, y_fixed;

	if(GraphicsPathFixedGetCurrentPoint(&context->path, &x_fixed, &y_fixed))
	{
		*x = GRAPHICS_FIXED_TO_FLOAT(x_fixed);
		*y = GRAPHICS_FIXED_TO_FLOAT(y_fixed);
		GraphicsStateBackendToUser(context->state, x, y);

		return TRUE;
	}

	return FALSE;
}

static GRAPHICS_PATH* GraphicsDefaultContextCopyPath(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return CreateGraphicsPath(&context->path, &context->base);
}

static GRAPHICS_PATH* GraphicsDefaultContextCopyPathFlat(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return CreateGraphicsPathFlat(&context->path, &context->base);
}

static eGRAPHICS_STATUS GraphicsDefaultContextAppendPath(void* abstract_context, const GRAPHICS_PATH* path)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsPathAppendToContext(path, &context->base);
}

static eGRAPHICS_STATUS GraphicsDefaultContextPaint(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStatePaint(context->state);
}

static eGRAPHICS_STATUS GraphicsDefaultContextPaintWithAlpha(void* abstract_context, FLOAT_T alpha)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	GRAPHICS_SOLID_PATTERN pattern;
	eGRAPHICS_STATUS status;
	GRAPHICS_COLOR color;

	if(GRAPHICS_ALPHA_IS_OPAQUE(alpha))
	{
		return GraphicsStatePaint(context->state);
	}

	if(GRAPHICS_ALPHA_IS_ZERO(alpha) && GraphicsOperatorBoundedByMask(context->state->op))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	InitializeGraphicsColorRGBA(&color, 0, 0, 0, alpha);
	InitializeGraphicsPatternSolid(&pattern, &color, context->base.graphics);

	status = GraphicsStateMask(context->state, &pattern.base);
	GraphicsPatternFinish(&pattern.base);

	return status;
}

static eGRAPHICS_STATUS GraphicsDefaultContextMask(void* abstract_context, GRAPHICS_PATTERN* mask)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateMask(context->state, mask);
}

static eGRAPHICS_STATUS GraphicsDefaultContextStroke(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	eGRAPHICS_STATUS status;

	status = GraphicsStateStroke(context->state, &context->path);
	if(UNLIKELY(status))
	{
		return status;
	}

	return GraphicsDefaultContextNewPath(context);
}

static eGRAPHICS_STATUS GraphicsDefaultContextStrokePreserve(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateStroke(context->state, context->path);
}

static eGRAPHICS_STATUS GraphicsDefaultContextInStroke(
	void* abstract_context,
	double x, double y,
	int* inside
)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateInStroke(context->state, &context->path, x, y, inside);
}

static eGRAPHICS_STATUS GraphicsDefaultContextStrokeExtents(
	void* abstract_context,
	double* x1, double* y1,
	double* x2, double* y2
)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateStrokeExtents(context->state, &context->path, x1, y1, x2, y2);
}

static eGRAPHICS_STATUS GraphicsDefaultContextClip(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	eGRAPHICS_STATUS status;

	status = GraphicsStateClip(context->state, &context->path);
	if(UNLIKELY(status))
	{
		return status;
	}

	return GraphicsDefaultContextNewPath(context);
}

static eGRAPHICS_STATUS GraphicsDefaultContextClipPreserve(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateClip(context->state, &context->path);
}

static eGRAPHICS_STATUS GraphicsDefaultContextInClip(
	void* abstract_context,
	double x,
	double y,
	int* inside
)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	*inside = GraphicsStateInClip(context->state, x, y);
	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsDefaultContextResetClip(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateResetClip(context->state);
}

static GRAPHICS_RECTANGLE_LIST* GraphicsDefaultContextCopyClipRectangleList(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateCopyClipRectangleList(context->state);
}

static eGRAPHICS_STATUS GraphicsDefaultContextFill(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;
	eGRAPHICS_STATUS status;

	status = GraphicsStateFill(context->state, &context->path);
	if(UNLIKELY(status))
	{
		return status;
	}

	return GraphicsDefaultContextNewPath(context);
}

static eGRAPHICS_STATUS GraphicsDefaultContextFillPreserve(void* abstract_context)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateFill(context->state, &context->path);
}

static eGRAPHICS_STATUS GraphicsDefaultContextInFill(
	void* abstract_context,
	double x, double y,
	int* inside
)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	*inside = GraphicsStateInFill(context->state, &context->path, x, y);
	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsDefaultContextFillExtents(
	void* abstract_context,
	double* x1, double* y1,
	double* x2, double* y2
)
{
	GRAPHICS_DEFAULT_CONTEXT *context = (GRAPHICS_DEFAULT_CONTEXT*)abstract_context;

	return GraphicsStateFillExtents(context->state, &context->path, x1, y1, x2, y2);
}

void InitializeGraphicsDefaultBackend(GRAPHICS_BACKEND* backend)
{
	backend->destroy = GraphicsDefaultContextDestroy;
	backend->finish = GraphicsDefaultContextFinish;
	
	backend->get_original_target = GraphicsDefaultContextGetOriginalTarget;
	backend->get_current_target = GraphicsDefaultContextGetCurrentTarget;
	
	backend->save = GraphicsDefaultContextSave;
	backend->restore = GraphicsDefaultContextRestore;
	
	backend->push_group = GraphicsDefaultContextPushGroup;
	backend->pop_group = GraphicsDefaultContextPopGroup;

	backend->set_source_rgba = GraphicsDefaultContextSetSourceRGBA;
	backend->set_source_surface = GraphicsDefaultContextSetSourceSurface;
	backend->set_source = GraphicsDefaultContextSetSource;
	backend->get_source = GraphicsDefaultContextGetSource;

	backend->set_antialias = GraphicsDefaultContextSetAntialias;
	backend->set_dash = GraphicsDefaultContextSetDash;
	backend->set_fill_rule = GraphicsDefaultContextSetFillRule;
	backend->set_line_cap = GraphicsDefaultContextSetLineCap;
	backend->set_line_join = GraphicsDefaultContextGetLineJoin;
	backend->set_line_width = GraphicsDefaultContextSetLineWidth;
	backend->set_miter_linet = GraphicsDefaultContextSetMiterLimit;
	backend->set_opacity = GraphicsDefaultContextSetOpacity;
	backend->set_operator = GraphicsDefaultContextSetOperator;
	backend->set_tolerance = GraphicsDefaultContextSetTolerance;
	backend->get_antialias = GraphicsDefaultContextGetAntialias;
	backend->get_dash = GraphicsDefaultContextGetDash;
	backend->get_fill_rule = GraphicsDefaultContextGetFillRule;
	backend->get_line_cap = GraphicsDefaultContextGetLineCap;
	backend->get_line_join = GraphicsDefaultContextGetLineJoin;
	backend->get_line_width = GraphicsDefaultContextGetLineWidth;
	backend->get_miter_limit = GraphicsDefaultContextGetMiterLimit;
	backend->get_opacity = GraphicsDefaultContextGetOpacity;
	backend->get_operator = GraphicsDefaultContextGetOperator;
	backend->get_torlearnce = GraphicsDefaultContextGetTolerance;

	backend->translate = GraphicsDefaultContextTranslate;
	backend->scale = GraphicsDefaultContextScale;
	backend->rotate = GraphicsDefaultContextRotate;
	backend->transform = GraphicsDefaultContextTransform;
	backend->set_matrix = GraphicsDefaultContextSetMatrix;
	backend->set_identity_matrix = GraphicsDefaultContextSetIdentityMatrix;
	backend->get_matrix = GraphicsDefaultContextGetMatrix;

	backend->user_to_device = GraphicsDefaultContextUserToDevice;
	backend->user_to_device_distance = GraphicsDefaultContextUserToDeviceDistance;
	backend->device_to_user = GraphicsDefaultContextDeviceToUser;
	backend->device_to_user_distance = GraphicsDefaultContextDeviceToUserDistance;

	backend->user_to_backend = GraphicsDefaultContextUserToBackend;
	backend->user_to_backend_distance = GraphicsDefaultContextUserToBackendDistance;
	backend->backend_to_user = GraphicsDefaultContextBackendToUser;
	backend->backend_to_user_distance = GraphicsDefaultContextBackendToUserDistance;

	backend->new_path = GraphicsDefaultContextNewPath;
	backend->new_sub_path = GraphicsDefaultContextNewSubPath;
	backend->move_to = GraphicsDefaultContextMoveTo;
	backend->relative_move_to = GraphicsDefaultContextRelativeMoveTo;
	backend->line_to = GraphicsDefaultContextLineTo;
	backend->relative_line_to = GraphicsDefaultContextRelativeLineTo;
	backend->curve_to = GraphicsDefaultContextCurveTo;
	backend->relative_curve_to = GraphicsDefaultContextRelativeCurveTo;
	backend->arc_to = NULL;
	backend->relative_arc_to = NULL;
	backend->close_path = GraphicsDefaultContextClosePath;
	backend->arc = GraphicsDefaultContextArc;
	backend->rectangle = GraphicsDefaultContextRectangle;

	backend->path_extents = GraphicsDefaultContextPathExtents;
	backend->has_current_point = GraphicsDefaultContextHasCurrentPoint;
	backend->get_current_point = GraphicsDefaultContextGetCurrentPoint;

	backend->copy_path = GraphicsDefaultContextCopyPath;
	backend->copy_path_flat = GraphicsDefaultContextCopyPathFlat;
	backend->append_path = GraphicsDefaultContextAppendPath;

	backend->stroke_to_path = NULL;

	backend->clip = GraphicsDefaultContextClip;
	backend->clip_preserve = GraphicsDefaultContextClipPreserve;
	backend->in_clip = GraphicsDefaultContextInClip;
	backend->reset_clip = GraphicsDefaultContextResetClip;
	backend->clip_copy_rectangle_list = GraphicsDefaultContextCopyClipRectangleList;

	backend->paint = GraphicsDefaultContextPaint;
	backend->paint_with_alpha = GraphicsDefaultContextPaintWithAlpha;
	backend->mask = GraphicsDefaultContextMask;

	backend->stroke = GraphicsDefaultContextStroke;
	backend->stroke_preserve = GraphicsDefaultContextStrokePreserve;
	backend->in_stroke = GraphicsDefaultContextInStroke;
	backend->stroke_extents = GraphicsDefaultContextStrokeExtents;

	backend->fill = GraphicsDefaultContextFill;
	backend->fill_preserve = GraphicsDefaultContextFillPreserve;
	backend->in_fill = GraphicsDefaultContextInFill;
	backend->fill_extents = GraphicsDefaultContextFillExtents;
}

eGRAPHICS_STATUS InitializeGraphicsDefaultContext(GRAPHICS_DEFAULT_CONTEXT* context, void* target, void* graphics)
{
	GRAPHICS *g = (GRAPHICS*)graphics;
	GRAPHICS_SURFACE *surface = (GRAPHICS_SURFACE*)target;
	context->base.graphics = (GRAPHICS*)graphics;
	_InitializeGraphicsContext(&context->base, &g->context_backend);
	InitializeGraphicsPathFixed(&context->path);

	context->state = &context->state_tail[0];
	context->state_free_list = &context->state_tail[1];
	context->state_tail[1].next = NULL;

	return InitializeGraphicsState(context->state, target);
}

GRAPHICS_CONTEXT* GraphicsDefaultContextCreate(void* target, void* graphics)
{
	GRAPHICS_CONTEXT *context;
	eGRAPHICS_STATUS status;

	context = (GRAPHICS_CONTEXT*)MEM_ALLOC_FUNC(sizeof(*context));

	status = InitializeGraphicsDefaultContext(context, target, graphics);
	context->own_memory = TRUE;

	return context;
}

void _InitializeGraphicsContext(struct _GRAPHICS_CONTEXT* context, struct _GRAPHICS_BACKEND* backend)
{
	context->ref_count = 1;
	context->status = GRAPHICS_STATUS_SUCCESS;
	context->backend = backend;
}

void DestroyGraphicsContext(struct _GRAPHICS_CONTEXT* context)
{
	if(context == NULL)
	{
		return;
	}

	context->ref_count--;
	if(context->ref_count > 0)
	{
		return;
	}

	if(context->own_memory)
	{
		context->backend->destroy(context);
	}
	else
	{
		context->backend->finish(context);
	}
}

#ifdef __cplusplus
}
#endif
