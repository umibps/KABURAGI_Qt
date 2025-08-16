#include <math.h>
#include "graphics.h"
#include "graphics_private.h"
#include "graphics_types.h"
#include "graphics_surface.h"
#include "graphics_matrix.h"
#include "graphics_state.h"
#include "graphics_compositor.h"
#include "types.h"

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

#ifdef __cplusplus
extern "C" {
#endif

int InitializeGraphics(GRAPHICS* graphics)
{
	const GRAPHICS_COLOR color_white = {1, 1, 1, 1, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
	const GRAPHICS_COLOR color_balck = {0, 0, 0, 1, 0x0, 0x0, 0x0, 0xFFFF};
	const GRAPHICS_COLOR color_transparent = {0, 0, 0, 0, 0x0, 0x0, 0x0, 0x0};
	const GRAPHICS_RECTANGLE_INT empty_rectangle = { 0 };
	graphics->color_white = color_white;
	graphics->color_black = color_balck;
	graphics->color_transparent = color_transparent;
	InitializePixelManipulate(&graphics->pixel_manipulate);
	InitializeGraphicsImageSurfaceBackend(&graphics->image_surface_backend);
	//graphics->spans_compositor.flags = 0;
	//todo : initialize shape compositor (_cairo_shape_mask_compositor_init(&graphics->shape, _cairo_image_traps_compositor_get()))
	{
		GRAPHICS_SOLID_PATTERN nil_pattern =
		{
			{
				-1,
				GRAPHICS_STATUS_NO_MEMORY,
				{NULL, NULL},
			
				GRAPHICS_PATTERN_TYPE_SOLID,
				GRAPHICS_FILTER_DEFAULT,
				GRAPHICS_EXTEND_GRADIENT_DEFAULT,
				FALSE,
				FALSE,
				{1, 0, 0, 1, 0, 0},
				1.0
			},
			{0, 0, 0, 0}
		};
		graphics->nil_pattern = nil_pattern;
		graphics->nil_pattern.base.graphics = graphics;
	}

	{
		const GRAPHICS_SOLID_PATTERN clear_pattern =
		{
			{
				0,	/* ref_count */
				GRAPHICS_STATUS_SUCCESS,		/* status */
				{ NULL, NULL },			/* observers */

				GRAPHICS_PATTERN_TYPE_SOLID,		/* type */
				GRAPHICS_FILTER_NEAREST,		/* filter */
				GRAPHICS_EXTEND_REPEAT,		/* extend */
				FALSE,				/* has component alpha */
				FALSE,				/* own memory */
				{ 1., 0., 0., 1., 0., 0., },	/* matrix */
				1.0							   /* opacity */
			},
			{ 0., 0., 0., 0., 0, 0, 0, 0 }
		};
		graphics->clear_pattern = clear_pattern;
		graphics->clear_pattern.base.graphics = graphics;
	}

	{
		const GRAPHICS_SOLID_PATTERN white_pattern =
		{
			{
				0,
				GRAPHICS_STATUS_SUCCESS,
				{NULL, NULL},
				GRAPHICS_PATTERN_TYPE_SOLID,
				GRAPHICS_FILTER_NEAREST,
				GRAPHICS_EXTEND_REPEAT,
				FALSE,
				FALSE,
				{1., 0., 0., 1., 0., 0.,},
				1.0
			},
			{ 1, 1, 1, 1, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF }
		};
		graphics->white_pattern = white_pattern;
		graphics->white_pattern.base.graphics = graphics;
	}

	{
		const GRAPHICS_SOLID_PATTERN black_pattern =
		{
			{
				0,
				GRAPHICS_STATUS_SUCCESS,
		{NULL, NULL},
		GRAPHICS_PATTERN_TYPE_SOLID,
		GRAPHICS_FILTER_NEAREST,
		GRAPHICS_EXTEND_REPEAT,
		FALSE,
		FALSE,
		{1., 0., 0., 1., 0., 0.,},
		1.0
			},
		{ 0, 0, 0, 1, 0x0, 0x0, 0x0, 0xFFFF }
		};
		graphics->pattern_black = black_pattern;
		graphics->pattern_black.base.graphics = graphics;
	}

	InitializeGraphicsDefaultBackend(&graphics->context_backend);
	InitializeGraphicsImageSurfaceBackend(&graphics->image_surface_backend);
	InitializeGraphicsSubsurfaceBackend(&graphics->subsurface_backend);
	InitializeGraphicsImageSourceBackend(&graphics->image_source_backend);

	GraphicsShapeMaskCompositorInitialize(&graphics->shape_mask_compositor, NULL, graphics);
	InitialzieGraphicsSpansCompositor(&graphics->spans_compositor, &graphics->shape_mask_compositor, graphics);

	InitializeGraphicsUnboundedRectangle(&graphics->unbounded_rectangle);
	graphics->empty_rectangle = empty_rectangle;

	return TRUE;
}

INLINE static int GraphicsStatusIsError(eGRAPHICS_STATUS status)
{
	return (status != GRAPHICS_STATUS_SUCCESS && status < NUM_GRAPHICS_STATUS);
}

INLINE eGRAPHICS_STATUS GraphicsError(eGRAPHICS_STATUS status)
{
	ASSERT(GraphicsStatusIsError(status));
	return status;
}

void GraphicsSetError(GRAPHICS_CONTEXT* context, eGRAPHICS_STATUS status)
{
	context->status = GraphicsError(status);
	if(context->graphics->error_message != NULL)
	{
		context->graphics->error_message(context->graphics->error_message_data);
	}
}

void GraphicsPaint(GRAPHICS_CONTEXT* context)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->paint(context);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsPaintWithAlpha(GRAPHICS_CONTEXT* context, FLOAT_T alpha)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->paint_with_alpha(context, alpha);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsFill(GRAPHICS_CONTEXT* context)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->fill(context);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsFillPreserve(GRAPHICS_CONTEXT* context)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->fill_preserve(context);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsMask(GRAPHICS_CONTEXT* context, GRAPHICS_PATTERN* pattern)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	if(UNLIKELY(pattern == NULL))
	{
		return;
	}

	if(UNLIKELY(pattern->status))
	{
		return;
	}

	status = context->backend->mask(context, pattern);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsMaskSurface(
	GRAPHICS_CONTEXT* context,
	GRAPHICS_SURFACE* surface,
	FLOAT_T surface_x,
	FLOAT_T surface_y
)
{
	GRAPHICS_SURFACE_PATTERN pattern;
	GRAPHICS_MATRIX matrix;

	if(UNLIKELY(context->status))
	{
		return;
	}

	if(InitializeGraphicsPatternForSurface(&pattern, surface) == FALSE)
	{
		return;
	}

	InitializeGraphicsMatrixTranslate(&matrix, - surface_x, - surface_y);
	GraphicsPatternSetMatrix(&pattern.base, &matrix);

	GraphicsMask(context, &pattern.base);
}

void GraphicsStroke(GRAPHICS_CONTEXT* context)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->stroke(context);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsStrokePreserve(GRAPHICS_CONTEXT* context)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->stroke_preserve(context);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsGetMatrix(GRAPHICS_CONTEXT* context, GRAPHICS_MATRIX* matrix)
{
	if(UNLIKELY(context->status))
	{
		InitializeGraphicsMatrixIdentity(matrix);
		return;
	}

	context->backend->get_matrix(context, matrix);
}

FLOAT_T GraphicsGetTolerance(GRAPHICS_CONTEXT* context)
{
	if(UNLIKELY(context->status))
	{
		return GRAPHICS_STATE_DEFAULT_TOLERANCE;
	}

	return context->backend->get_torlearnce(context);
}

void GraphicsMoveTo(GRAPHICS_CONTEXT* context, FLOAT_T x, FLOAT_T y)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->move_to(context, x, y);

	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsLineTo(GRAPHICS_CONTEXT* context, FLOAT_T x, FLOAT_T y)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->line_to(context, x, y);

	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsCurveTo(
	GRAPHICS_CONTEXT* context,
	FLOAT_T x1, FLOAT_T y1,
	FLOAT_T x2, FLOAT_T y2,
	FLOAT_T x3, FLOAT_T y3
)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->curve_to(
		context, x1, y1, x2, y2, x3, y3);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsArc(
	GRAPHICS_CONTEXT* context,
	FLOAT_T xc,
	FLOAT_T yc,
	FLOAT_T radius,
	FLOAT_T angle1,
	FLOAT_T angle2
)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	if(angle2 < angle1)
	{
		angle2 = fmod(angle2 - angle1, 2 * M_PI);

		if(angle2 < 0)
		{
			angle2 += 2 * M_PI;
		}
		angle2 += angle1;
	}

	status = context->backend->arc(context, xc, yc, radius, angle1, angle2, TRUE);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsArcNegative(
	GRAPHICS_CONTEXT* context,
	FLOAT_T xc,
	FLOAT_T yc,
	FLOAT_T radius,
	FLOAT_T angle1,
	FLOAT_T angle2
)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	if(angle2 > angle1)
	{
		angle2 = fmod(angle2 - angle1, 2 * M_PI);

		if(angle2 > 0)
		{
			angle2 -= 2 * M_PI;
		}
		angle2 += angle1;
	}

	status = context->backend->arc(context, xc, yc, radius, angle1, angle2, FALSE);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsClosePath(GRAPHICS_CONTEXT* context)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->close_path(context);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsPathExtents(
	GRAPHICS_CONTEXT* context,
	FLOAT_T* x1,FLOAT_T* y1,
	FLOAT_T* x2,FLOAT_T* y2
)
{
	if(UNLIKELY(context->status))
	{
		*x1 = 0.0;
		*y1 = 0.0;
		*x2 = 0.0;
		*y2 = 0.0;

		return;
	}

	context->backend->path_extents(context, x1, y1, x2, y2);
}

eGRAPHICS_CONTENT GraphicsFormatToContent(eGRAPHICS_FORMAT format)
{
	switch(format)
	{
	case GRAPHICS_FORMAT_ARGB32:
		return GRAPHICS_CONTENT_COLOR_ALPHA;
	case GRAPHICS_FORMAT_RGB24:
		return GRAPHICS_CONTENT_COLOR;
	case GRAPHICS_FORMAT_A8:
	case GRAPHICS_FORMAT_A1:
		return GRAPHICS_CONTENT_ALPHA;
	}

	return GRAPHICS_CONTENT_INVALID;
}

int GraphicsContentDepth(eGRAPHICS_CONTENT content)
{
	switch(content)
	{
	case GRAPHICS_CONTENT_COLOR:
		return 24;
	case GRAPHICS_CONTENT_ALPHA:
		return 8;
	case GRAPHICS_CONTENT_COLOR_ALPHA:
		return 32;
	}
	return 0;
}

void GraphicsSetSource(GRAPHICS_CONTEXT* context, GRAPHICS_PATTERN* source)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	if(UNLIKELY(source == NULL))
	{
		return;
	}

	if(UNLIKELY(source->status))
	{
		return;
	}

	status = context->backend->set_source(context, source);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsSetSourceRGB(GRAPHICS_CONTEXT* context, FLOAT_T red, FLOAT_T green, FLOAT_T blue)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->set_source_rgba(context, red, green, blue, 1);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsSetSourceRGBA(GRAPHICS_CONTEXT* context, FLOAT_T red, FLOAT_T green, FLOAT_T blue, FLOAT_T alpha)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->set_source_rgba(context, red, green, blue, alpha);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsSetSourceSurface(GRAPHICS_CONTEXT* context, GRAPHICS_SURFACE* surface, FLOAT_T x, FLOAT_T y, GRAPHICS_SURFACE_PATTERN* pattern)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	if(UNLIKELY(surface == NULL && pattern == NULL))
	{
		GraphicsSetError(context, GRAPHICS_STATUS_NULL_POINTER);
		return;
	}

	status = context->backend->set_source_surface(context, surface, x, y, pattern);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsRectangle(GRAPHICS_CONTEXT* context, FLOAT_T x, FLOAT_T y, FLOAT_T width, FLOAT_T height)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->rectangle(context, x, y, width, height);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsSetOperator(GRAPHICS_CONTEXT* context, eGRAPHICS_OPERATOR op)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->set_operator(context, op);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsTranslate(GRAPHICS_CONTEXT* context, FLOAT_T tx, FLOAT_T ty)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->translate(context, tx, ty);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsScale(GRAPHICS_CONTEXT* context, FLOAT_T sx, FLOAT_T sy)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->scale(context, sx, sy);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsRotate(GRAPHICS_CONTEXT* context,FLOAT_T angle)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->rotate(context, angle);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsSetLineWidth(GRAPHICS_CONTEXT* context,FLOAT_T width)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	if(width < 0)
	{
		width = 0;
	}

	status = context->backend->set_line_width(context, width);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsSetLineCap(GRAPHICS_CONTEXT* context,eGRAPHICS_LINE_CAP line_cap)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->set_line_cap(context, line_cap);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsSetLineJoin(GRAPHICS_CONTEXT* context,eGRAPHICS_LINE_JOIN line_join)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->set_line_join(context, line_join);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsSetAntialias(GRAPHICS_CONTEXT* context, eGRAPHICS_ANTIALIAS antialias)
{
	eGRAPHICS_STATUS status;
	
	if(UNLIKELY(context->status))
	{
		return;
	}
	
	status = context->backend->set_antialias(context, antialias);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsSave(GRAPHICS_CONTEXT* context)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->save(context);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

void GraphicsRestore(GRAPHICS_CONTEXT* context)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->restore(context);
	if(UNLIKELY(status))
	{
		GraphicsSetError(context, status);
	}
}

eGRAPHICS_LINE_CAP GraphicsGetLineCap(GRAPHICS_CONTEXT* context)
{
	if(UNLIKELY(context->status))
	{
		return GRAPHICS_LINE_CAP_DEFAULT;
	}
	
	return context->backend->get_line_cap(context);
}

#ifdef __cplusplus
}
#endif
