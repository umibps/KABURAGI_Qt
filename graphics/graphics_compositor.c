#include <string.h>
#include "graphics_types.h"
#include "graphics_compositor.h"
#include "graphics_surface.h"
#include "graphics.h"
#include "graphics_flags.h"
#include "graphics_matrix.h"
#include "graphics_pen.h"
#include "graphics_region.h"
#include "graphics_private.h"
#include "graphics_scan_converter_private.h"
#include "graphics_clip_tor_scan_converter.h"

#ifdef __cplusplus
extern "C" {
#endif

static eGRAPHICS_INTEGER_STATUS GraphicsCompositeRectanglesIntersect(GRAPHICS_COMPOSITE_RECTANGLES* extents, const GRAPHICS_CLIP* clip);

void GraphicsCompositeRectanglesFinish(GRAPHICS_COMPOSITE_RECTANGLES* extents)
{
	GraphicsClipDestroy(extents->clip);
}

static void GraphicsCompositeReducePattern(const GRAPHICS_PATTERN* src, GRAPHICS_PATTERN_UNION* dst)
{
	int tx, ty;

	InitializeGraphicsPatternStaticCopy(&dst->base, src);
	if(dst->base.type == GRAPHICS_PATTERN_TYPE_SOLID)
	{
		return;
	}

	dst->base.filter = GraphicsPatternAnalyzeFilter(&dst->base);

	tx = ty = 0;
	if(GraphicsMatrixIsPixelManipulateTranslation(&dst->base.matrix,
		dst->base.filter, &tx, &ty) != FALSE)
	{
		dst->base.matrix.x0 = tx;
		dst->base.matrix.y0 = ty;
	}
}

static INLINE int InitializeGraphicsCompositeRectangles(
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_CLIP* clip
)
{
	if(GraphicsClipIsAllClipped(clip))
	{
		return FALSE;
	}

	extents->surface = surface;
	extents->op = op;

	GraphicsSurfaceGetExtents(surface, &extents->destination);
	extents->clip = NULL;

	extents->unbounded = extents->destination;
	if(clip != NULL && clip->clip_all == FALSE)
	{
		if(GraphicsRectangleIntersect(&extents->unbounded, &clip->extents) == FALSE)
		{
			return FALSE;
		}
	}

	extents->bounded = extents->unbounded;
	extents->is_bounded = GraphicsOperatorBoundedByEither(op);

	extents->original_source_pattern = source;
	GraphicsCompositeReducePattern(source, &extents->source_pattern);

	GraphicsPatternGetExtents(&extents->source_pattern.base, &extents->source, surface->is_vector);
	if((extents->is_bounded & GRAPHICS_OPERATOR_BOUNDED_BY_SOURCE) != 0)
	{
		if(FALSE == GraphicsRectangleIntersect(&extents->bounded, &extents->source))
		{
			return FALSE;
		}
	}

	extents->original_mask_pattern = NULL;
	extents->mask_pattern.base.type = GRAPHICS_PATTERN_TYPE_SOLID;
	extents->mask_pattern.solid.color.alpha = 1.0;
	extents->mask_pattern.solid.color.alpha_short = 0xFFFF;

	return TRUE;
}

eGRAPHICS_INTEGER_STATUS InitializeGraphicsCompositeRectanglesForBoxes(
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_BOXES* boxes,
	const GRAPHICS_CLIP* clip
)
{
	GRAPHICS_BOX box;

	if(FALSE == InitializeGraphicsCompositeRectangles(extents,
		surface, op, source, clip))
	{
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}

	GraphicsBoxesExtents(boxes, &box);
	GraphicsBoxRoundToRectangle(&box, &extents->mask);
	return GraphicsCompositeRectanglesIntersect(extents, clip);
}

eGRAPHICS_STATUS InitializeGraphicsCompositeRectanglesForPolygon(
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_POLYGON* polygon,
	const GRAPHICS_CLIP* clip
)
{
	if(FALSE == InitializeGraphicsCompositeRectangles(extents,
		surface, op, source, clip))
	{
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}

	GraphicsBoxRoundToRectangle(&polygon->extents, &extents->mask);
	return GraphicsCompositeRectanglesIntersect(extents, clip);
}

eGRAPHICS_STATUS InitializeGraphicsCompositeRectanglesForPaint(
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_CLIP* clip
)
{
	if(FALSE == InitializeGraphicsCompositeRectangles(extents, surface, op, source, clip))
	{
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}

	extents->mask = extents->destination;

	extents->clip = GraphicsClipReduceForComposite(clip, extents);
	if(GraphicsClipIsAllClipped(extents->clip))
	{
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}

	if(FALSE == GraphicsRectangleIntersect(&extents->unbounded,
		GraphicsClipGetExtents(extents->clip, surface->graphics)))
	{
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}

	if(extents->source_pattern.base.type != GRAPHICS_PATTERN_TYPE_SOLID)
	{
		GraphicsPatternSampledArea(&extents->source_pattern.base,
			&extents->bounded, &extents->source_sample_area);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static INLINE int InitializeGraphicsCompositeRectanglesForMask(
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATTERN* mask,
	const GRAPHICS_CLIP* clip
)
{
	if(FALSE == InitializeGraphicsCompositeRectangles(extents,
		surface, op, source, clip))
	{
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}

	extents->original_mask_pattern = mask;
	GraphicsCompositeReducePattern(mask, &extents->mask_pattern);
	GraphicsPatternGetExtents(&extents->mask_pattern.base, &extents->mask, surface->is_vector);

	return GraphicsCompositeRectanglesIntersect(extents, clip);
}

static eGRAPHICS_INTEGER_STATUS GraphicsCompositeRectanglesIntersect(GRAPHICS_COMPOSITE_RECTANGLES* extents, const GRAPHICS_CLIP* clip)
{
	if((FALSE == GraphicsRectangleIntersect(&extents->bounded, &extents->mask))
		&& (extents->is_bounded & GRAPHICS_OPERATOR_BOUNDED_BY_MASK))
	{
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}

	if(extents->is_bounded == (GRAPHICS_OPERATOR_BOUNDED_BY_MASK | GRAPHICS_OPERATOR_BOUNDED_BY_SOURCE))
	{
		extents->unbounded = extents->bounded;
	}
	else if(extents->is_bounded & GRAPHICS_OPERATOR_BOUNDED_BY_MASK)
	{
		if(FALSE == GraphicsRectangleIntersect(&extents->unbounded, &extents->mask))
		{
			return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
		}
	}

	extents->clip = GraphicsClipReduceForComposite(clip, extents);
	if(extents->clip->clip_all)
	{
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}

	if(FALSE == GraphicsRectangleIntersect(&extents->unbounded, &extents->clip->extents))
	{
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}

	if(FALSE == GraphicsRectangleIntersect(&extents->bounded, &extents->clip->extents)
		&& extents->is_bounded & GRAPHICS_OPERATOR_BOUNDED_BY_MASK)
	{
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}

	if(extents->source_pattern.base.type != GRAPHICS_PATTERN_TYPE_SOLID)
	{
		GraphicsPatternSampledArea(&extents->source_pattern.base,
			&extents->bounded, &extents->source_sample_area);
	}
	if(extents->mask_pattern.base.type != GRAPHICS_PATTERN_TYPE_SOLID)
	{
		GraphicsPatternSampledArea(&extents->mask_pattern.base,
			&extents->bounded, &extents->mask_sample_area);
		if(extents->mask_sample_area.width == 0 || extents->mask_sample_area.height == 0)
		{
			GraphicsCompositeRectanglesFinish(extents);
			return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
		}
	}
	return GRAPHICS_INTEGER_STATUS_SUCCESS;
}

eGRAPHICS_INTEGER_STATUS GraphicsCompositeRectanglesIntersectMaskExtents(
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	const GRAPHICS_BOX* box
)
{
	GRAPHICS_RECTANGLE_INT mask;
	GRAPHICS_CLIP* clip;

	GraphicsBoxRoundToRectangle(box, &mask);
	if(mask.x == extents->mask.x && mask.y == extents->mask.y
		&& mask.width == extents->mask.width && mask.height == extents->mask.height)
	{
		return GRAPHICS_INTEGER_STATUS_SUCCESS;
	}

	GraphicsRectangleIntersect(&extents->mask, &mask);

	mask = extents->bounded;
	if(!GraphicsRectangleIntersect(&extents->bounded, &extents->mask)
		&& extents->is_bounded & GRAPHICS_OPERATOR_BOUNDED_BY_MASK)
	{
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}

	if(mask.width == extents->bounded.width
		&& mask.height == extents->bounded.height)
	{
		return GRAPHICS_INTEGER_STATUS_SUCCESS;
	}

	if(extents->is_bounded == (GRAPHICS_OPERATOR_BOUNDED_BY_MASK | GRAPHICS_OPERATOR_BOUNDED_BY_SOURCE))
	{
		extents->unbounded = extents->bounded;
	}
	else if(extents->is_bounded & GRAPHICS_OPERATOR_BOUNDED_BY_MASK)
	{
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}

	clip = extents->clip;
	extents->clip = GraphicsClipReduceForComposite(clip, extents);
	if(clip != extents->clip)
	{
		GraphicsClipDestroy(clip);
	}

	if(GraphicsClipIsAllClipped(extents->clip))
	{
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}

	if(FALSE == GraphicsRectangleIntersect(&extents->unbounded, &extents->clip->extents))
	{
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}

	if(extents->source_pattern.base.type != GRAPHICS_PATTERN_TYPE_SOLID)
	{
		GraphicsPatternSampledArea(&extents->source_pattern.base, &extents->bounded, &extents->source_sample_area);
	}
	if(extents->mask_pattern.base.type != GRAPHICS_PATTERN_TYPE_SOLID)
	{
		GraphicsPatternSampledArea(&extents->mask_pattern.base, &extents->bounded, &extents->mask_sample_area);
		if(extents->mask_sample_area.width == 0 || extents->mask_sample_area.height == 0)
		{
			return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
		}
	}

	return GRAPHICS_INTEGER_STATUS_SUCCESS;
}

eGRAPHICS_INTEGER_STATUS InitializeGraphicsCompositorRecatanglesForPaint(
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_CLIP* clip
)
{
	if(FALSE == InitializeGraphicsCompositeRectangles (extents, surface, op, source, clip))
	{
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}

	extents->mask = extents->destination;

	extents->clip = GraphicsClipReduceForComposite(clip, extents);
	if(extents->clip->clip_all)
	{
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}

	if(FALSE == GraphicsRectangleIntersect(&extents->unbounded,
		&extents->clip->extents))
	{
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}

	if(extents->source_pattern.base.type != GRAPHICS_PATTERN_TYPE_SOLID)
	{
		GraphicsPatternSampledArea(&extents->source_pattern.base, &extents->bounded, &extents->source_sample_area);
	}

	return GRAPHICS_INTEGER_STATUS_SUCCESS;
}

eGRAPHICS_INTEGER_STATUS InitializeGraphicsCompositeRectanglesForStroke(
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_MATRIX* ctm,
	const GRAPHICS_CLIP* clip
)
{
	if(FALSE == InitializeGraphicsCompositeRectangles(extents, surface, op, source, clip))
	{
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}

	GraphicsPathFixedApproximateStrokeExtents(path, style, ctm, surface->is_vector, &extents->mask);

	return GraphicsCompositeRectanglesIntersect(extents, clip);
}

eGRAPHICS_INTEGER_STATUS InitializeGraphicsCompositeRectanglesForFill(
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_CLIP* clip
)
{
	if(FALSE == InitializeGraphicsCompositeRectangles(extents, surface, op, source, clip))
	{
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}

	GraphicsPathFixedApproximateFillExtents(path, &extents->mask);

	return GraphicsCompositeRectanglesIntersect(extents, clip);
}

eGRAPHICS_INTEGER_STATUS GraphicsCompositorPaint(
	const GRAPHICS_COMPOSITOR* compositor,
	struct _GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_CLIP* clip
)
{
	GRAPHICS_COMPOSITE_RECTANGLES extents;
	eGRAPHICS_INTEGER_STATUS status;

	status = InitializeGraphicsCompositorRecatanglesForPaint(
		&extents, surface, op, source, clip);
	if(UNLIKELY(status))
	{
		return status;
	}

	do
	{
		while(compositor->paint == NULL)
		{
			compositor = compositor->delegate;
		}
		status = compositor->paint(compositor, &extents);
		compositor = compositor->delegate;
	} while(status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED);

	if(status == GRAPHICS_INTEGER_STATUS_SUCCESS && surface->damage != NULL)
	{
		surface->damage = GraphicsDamageAddRectangle(surface->damage, &extents.unbounded);
	}
	GraphicsCompositeRectanglesFinish(&extents);

	return status;
}

eGRAPHICS_INTEGER_STATUS GraphicsCompositorMask(
	const GRAPHICS_COMPOSITOR* compositor,
	struct _GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATTERN* mask,
	const GRAPHICS_CLIP* clip
)
{
	GRAPHICS_COMPOSITE_RECTANGLES extents;
	eGRAPHICS_INTEGER_STATUS status;

	status = InitializeGraphicsCompositeRectanglesForMask(
		&extents, surface, op, source, mask, clip);

	if(UNLIKELY(status))
	{
		return status;
	}

	do
	{
		while(compositor->mask == NULL)
		{
			compositor = compositor->delegate;
		}

		status = compositor->mask(compositor, &extents);
		compositor = compositor->delegate;
	} while(status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED);

	if(status == GRAPHICS_INTEGER_STATUS_SUCCESS && surface->damage)
	{
		surface->damage = GraphicsDamageAddRectangle(surface->damage, &extents.unbounded);
	}

	GraphicsCompositeRectanglesFinish(&extents);

	return status;
}

eGRAPHICS_INTEGER_STATUS GraphicsCompositorStroke(
	const GRAPHICS_COMPOSITOR* compositor,
	struct _GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_MATRIX* ctm,
	const GRAPHICS_MATRIX* ctm_inverse,
	FLOAT_T tolerance,
	eGRAPHICS_ANTIALIAS antialias,
	const GRAPHICS_CLIP* clip
)
{
	GRAPHICS_COMPOSITE_RECTANGLES extents;
	eGRAPHICS_INTEGER_STATUS status;

	if(GraphicsPenVerticesNeeded(tolerance, style->line_width * 0.5, ctm) <= 1)
	{
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}

	status = InitializeGraphicsCompositeRectanglesForStroke(&extents, surface,
		op, source, path, style, ctm, clip);
	if(UNLIKELY(status))
	{
		return status;
	}

	do
	{
		while(compositor->stroke == NULL)
		{
			compositor = compositor->delegate;
		}
		status = compositor->stroke(compositor, &extents, path, style, ctm, ctm_inverse, tolerance, antialias);
		compositor = compositor->delegate;
	} while(status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED);

	if(status == GRAPHICS_INTEGER_STATUS_SUCCESS && surface->damage != NULL)
	{
		surface->damage = GraphicsDamageAddRectangle(surface->damage, &extents.unbounded);
	}

	GraphicsCompositeRectanglesFinish(&extents);

	return status;
}

eGRAPHICS_INTEGER_STATUS GraphicsCompositorFill(
	const GRAPHICS_COMPOSITOR* compositor,
	struct _GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rule,
	FLOAT_T tolerance,
	eGRAPHICS_ANTIALIAS antialias,
	const GRAPHICS_CLIP* clip
)
{
	GRAPHICS_COMPOSITE_RECTANGLES extents;
	eGRAPHICS_INTEGER_STATUS status;

	status = InitializeGraphicsCompositeRectanglesForFill(&extents, surface, op, source, path, clip);

	if(UNLIKELY(status))
	{
		return status;
	}

	do
	{
		while(compositor->fill == NULL)
		{
			compositor = compositor->delegate;
		}

		status = compositor->fill(compositor, &extents, path, fill_rule, tolerance, antialias);

		compositor = compositor->delegate;
	} while(status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED);

	if(status == GRAPHICS_INTEGER_STATUS_SUCCESS && surface->damage)
	{
		surface->damage = GraphicsDamageAddRectangle(surface->damage, &extents.unbounded);
	}

	GraphicsCompositeRectanglesFinish(&extents);

	return status;
}

static eGRAPHICS_INTEGER_STATUS GraphicsShapeMaskCompositorStroke(
	const GRAPHICS_COMPOSITOR* compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_MATRIX* matrix,
	const GRAPHICS_MATRIX* matrix_inverse,
	FLOAT_T tolerance,
	eGRAPHICS_ANTIALIAS antialias
)
{
	GRAPHICS_SURFACE *mask;
	GRAPHICS_SURFACE_PATTERN pattern;
	eGRAPHICS_INTEGER_STATUS status;
	GRAPHICS_CLIP *clip, local_clip;

	if(FALSE == extents->is_bounded)
	{
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	}

	mask = GraphicsSurfaceCreateScratch(extents->surface, GRAPHICS_CONTENT_ALPHA,
		extents->bounded.width, extents->bounded.height, NULL);
	if(UNLIKELY(mask->status))
	{
		return mask->status;
	}

	clip = extents->clip;
	if(FALSE == GraphicsClipIsRegion(clip))
	{
		clip = GraphicsClipCopyRegion(clip, &local_clip);
	}

	if(FALSE == mask->is_clear)
	{
		status = GraphicsSurfaceOffsetPaint(mask, extents->bounded.x, extents->bounded.y,
			GRAPHICS_OPERATOR_CLEAR, &((GRAPHICS*)mask->graphics)->clear_pattern.base, clip);
		if(UNLIKELY(status))
		{
			goto error;
		}
	}

	InitializeGraphicsPatternForSurface(&pattern, mask);
	InitializeGraphicsMatrixTranslate(&pattern.base.matrix,
		- extents->bounded.x, - extents->bounded.y);
	pattern.base.filter = GRAPHICS_FILTER_NEAREST;
	pattern.base.extend = GRAPHICS_EXTEND_NONE;
	if(extents->op == GRAPHICS_OPERATOR_SOURCE)
	{
		status = GraphicsSurfaceMask(extents->surface, GRAPHICS_OPERATOR_DEST_OUT,
			&((GRAPHICS*)mask->graphics)->white_pattern.base, &pattern.base, clip);
		if(status == GRAPHICS_INTEGER_STATUS_SUCCESS)
		{
			status = GraphicsSurfaceMask(extents->surface, GRAPHICS_OPERATOR_ADD,
				&extents->source_pattern.base, &pattern.base, clip);
		}
	}
	else
	{
		status = GraphicsSurfaceMask(extents->surface, extents->op,
			&extents->source_pattern.base, &pattern.base, clip);
	}
	GraphicsPatternFinish(&pattern.base);

error:
	DestroyGraphicsSurface(mask);
	if(clip != extents->clip)
	{
		GraphicsClipDestroy(clip);
	}
	return status;
}

const GRAPHICS_COMPOSITOR* GetGraphicsImageSpansCompositor(struct _GRAPHICS* graphics)
{
	if(IS_GRAPHICS_COMPOSITOR_INITIALIZED(&graphics->shape_mask_compositor) == FALSE)
	{
		//GraphicsShapeMaskCompositorInitialize(&graphics->spans_shape_compositor, todo);
		GRAPHICS_COMPOSITOR_INITIALIZED_FLAG_ON(&graphics->shape_mask_compositor);
	}
}

static eGRAPHICS_INTEGER_STATUS GraphicsShapeMaskCompositorFill(
	const GRAPHICS_COMPOSITOR* compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rule,
	FLOAT_T tolerance,
	eGRAPHICS_ANTIALIAS antialias
)
{
	GRAPHICS_SURFACE *mask;
	GRAPHICS_SURFACE_PATTERN pattern;
	eGRAPHICS_INTEGER_STATUS status;
	GRAPHICS_CLIP *clip, local_clip;

	if(FALSE == extents->is_bounded)
	{
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	}

	mask = GraphicsSurfaceCreateScratch(extents->surface, GRAPHICS_CONTENT_ALPHA,
		extents->bounded.width, extents->bounded.height, NULL);

	if(UNLIKELY(mask->status))
	{
		return mask->status;
	}

	clip = extents->clip;
	if(FALSE == GraphicsClipIsRegion(clip))
	{
		clip = GraphicsClipCopyRegion(clip, &local_clip);
	}

	if(FALSE == mask->is_clear)
	{
		status = GraphicsSurfaceOffsetPaint(mask, extents->bounded.x, extents->bounded.y,
			GRAPHICS_OPERATOR_CLEAR, &((GRAPHICS*)extents->surface->graphics)->clear_pattern.base, clip);
		if(UNLIKELY(status))
		{
			goto error;
		}
	}

	status = GraphicsSurfaceOffsetFill(mask, extents->bounded.x, extents->bounded.y, GRAPHICS_OPERATOR_ADD,
		&((GRAPHICS*)extents->surface->graphics)->white_pattern.base, path, fill_rule, tolerance, antialias, clip);
	if(UNLIKELY(status))
	{
		goto error;
	}

	if(clip != extents->clip)
	{
		status = GraphicsClipCombineWithSurface(extents->clip, mask, extents->bounded.x, extents->bounded.y);
		if(UNLIKELY(status))
		{
			goto error;
		}
	}

	InitializeGraphicsPatternForSurface(&pattern, mask);
	InitializeGraphicsMatrixTranslate(&pattern.base.matrix, - extents->bounded.x, - extents->bounded.y);
	pattern.base.filter = GRAPHICS_FILTER_NEAREST;
	pattern.base.extend = GRAPHICS_EXTEND_NONE;
	if(extents->op == GRAPHICS_OPERATOR_SOURCE)
	{
		status = GraphicsSurfaceMask(extents->surface, GRAPHICS_OPERATOR_DEST_OUT,
					&((GRAPHICS*)extents->surface->graphics)->white_pattern.base, &pattern.base, clip);
		if(status == GRAPHICS_INTEGER_STATUS_SUCCESS)
		{
			status = GraphicsSurfaceMask(extents->surface, GRAPHICS_OPERATOR_ADD, &extents->source_pattern.base,
						&pattern.base, clip);
		}
	}
	else
	{
		status = GraphicsSurfaceMask(extents->surface, extents->op, &extents->source_pattern.base,
					&pattern.base, clip);
	}
	GraphicsPatternFinish(&pattern.base);

error:
	if(clip != extents->clip)
	{
		GraphicsClipDestroy(clip);
	}
	DestroyGraphicsSurface(mask);
	return status;
}

static eGRAPHICS_INTEGER_STATUS GraphicsShapeCompositorStroke(
	const GRAPHICS_COMPOSITOR* compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_MATRIX* matrix,
	const GRAPHICS_MATRIX* matrix_inverse,
	double tolerance,
	eGRAPHICS_ANTIALIAS antialias
)
{
	GRAPHICS_SURFACE *mask;
	GRAPHICS_SURFACE_PATTERN pattern;
	eGRAPHICS_INTEGER_STATUS status;
	GRAPHICS_CLIP *clip;

	//if(extents->bounded == FALSE)
	{
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	}

	//mask =
}

void GraphicsShapeMaskCompositorInitialize(GRAPHICS_COMPOSITOR* compositor, const GRAPHICS_COMPOSITOR* delegate, void* graphics)
{
	compositor->delegate = delegate;

	compositor->paint = NULL;
	compositor->mask = NULL;
	compositor->fill = GraphicsShapeMaskCompositorFill;
	compositor->stroke = GraphicsShapeMaskCompositorStroke;

	compositor->graphics = graphics;
}

typedef struct _COMPOSITE_SPANS_INFO
{
	GRAPHICS_POLYGON *polygon;
	eGRAPHICS_FILL_RULE fill_rule;
	eGRAPHICS_ANTIALIAS antialias;
} COMPOSITE_SPANS_INFO;

static eGRAPHICS_STATUS CompositePolygon(
	const GRAPHICS_SPANS_COMPOSITOR* compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE fill_rule,
	eGRAPHICS_ANTIALIAS antialias
);

static eGRAPHICS_INTEGER_STATUS CompositeBoxes(
	const GRAPHICS_SPANS_COMPOSITOR* compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_BOXES* boxes
);

static eGRAPHICS_INTEGER_STATUS ClipAndCompositePolygon(
	const GRAPHICS_SPANS_COMPOSITOR* compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	eGRAPHICS_FILL_RULE fill_rule,
	eGRAPHICS_ANTIALIAS antialias
);

static GRAPHICS_SURFACE* GetClipSurface(
	const GRAPHICS_SPANS_COMPOSITOR* compositor,
	GRAPHICS_SURFACE* dst,
	const GRAPHICS_CLIP* clip,
	const GRAPHICS_RECTANGLE_INT* extents
)
{
	GRAPHICS_COMPOSITE_RECTANGLES composite;
	GRAPHICS_SURFACE *surface;
	GRAPHICS_BOX box;
	GRAPHICS_POLYGON polygon;
	const GRAPHICS_CLIP_PATH *clip_path;
	eGRAPHICS_ANTIALIAS antialias;
	eGRAPHICS_FILL_RULE fill_rule;
	eGRAPHICS_INTEGER_STATUS status;
	const GRAPHICS_COLOR color_transparent = {0, 0, 0, 0, 0x0, 0x0, 0x0, 0x0};

	ASSERT(clip->path != NULL);

	surface = GraphicsSurfaceCreateScratch(dst, GRAPHICS_CONTENT_ALPHA, extents->width,
		extents->height, &color_transparent);

	GraphicsBoxFromRectangle(&box, extents);
	InitializeGraphicsPolygon(&polygon, &box, 1);

	clip_path = clip->path;
	status = GraphicsPathFixedFillToPolygon(&clip_path->path, clip_path->tolerance, &polygon);
	if(UNLIKELY(status))
	{
		goto cleanup_polygon;
	}

	clip_path = clip->path;
	status = GraphicsPathFixedFillToPolygon(&clip_path->path,
		clip_path->tolerance, &polygon);
	if(UNLIKELY(status))
	{
		goto cleanup_polygon;
	}

	polygon.num_limits = 0;

	antialias = clip_path->antialias;
	fill_rule = clip_path->fill_rule;

	if(clip->boxes != NULL)
	{
		GRAPHICS_POLYGON intersect;
		GRAPHICS_BOXES tmp;

		InitializeGraphicsBoxesForArray(&tmp, clip->boxes, clip->num_boxes);
		status = GraphicsPolygonInitializeBoxes(&intersect, &tmp);
		if(UNLIKELY(status))
		{
			goto cleanup_polygon;
		}

		status = GraphicsPolygonIntersect(&polygon, fill_rule,
						&intersect, GRAPHICS_FILL_RULE_WINDING);
		GraphicsPolygonFinish(&intersect);

		if(UNLIKELY(status))
		{
			goto cleanup_polygon;
		}

		fill_rule = GRAPHICS_FILL_RULE_WINDING;
	}

	polygon.limits = NULL;
	polygon.num_limits = 0;

	clip_path = clip_path->prev;
	while(clip_path != NULL)
	{
		if(clip_path->antialias == antialias)
		{
			GRAPHICS_POLYGON next;

			InitializeGraphicsPolygon(&next, NULL, 0);
			status = GraphicsPathFixedFillToPolygon(&clip_path->path,
						clip_path->tolerance, &next);
			if(status == GRAPHICS_INTEGER_STATUS_SUCCESS)
			{
				status = GraphicsPolygonIntersect(&polygon, fill_rule,
								&next, clip_path->fill_rule);
			}
			GraphicsPolygonFinish(&next);
			if(UNLIKELY(status))
			{
				goto cleanup_polygon;
			}

			fill_rule = GRAPHICS_FILL_RULE_WINDING;
		}

		clip_path = clip_path->prev;
	}

	GraphicsPolygonTranslate(&polygon, -extents->x, -extents->y);
	status = InitializeGraphicsCompositeRectanglesForPolygon(&composite, surface,
		GRAPHICS_OPERATOR_ADD, &((GRAPHICS*)surface->graphics)->white_pattern.base, &polygon, NULL);
	if(UNLIKELY(status))
	{
		goto cleanup_polygon;
	}

	status = CompositePolygon(compositor, &composite, &polygon, fill_rule, antialias);
	GraphicsCompositeRectanglesFinish(&composite);
	GraphicsPolygonFinish(&polygon);
	if(UNLIKELY(status))
	{
		goto error;
	}

	InitializeGraphicsPolygon(&polygon, &box, 1);

	clip_path = clip->path;
	antialias = clip_path->antialias == GRAPHICS_ANTIALIAS_DEFAULT ? GRAPHICS_ANTIALIAS_NONE : GRAPHICS_ANTIALIAS_DEFAULT;
	clip_path = clip_path->prev;
	while(clip_path != NULL)
	{
		if(clip_path->antialias == antialias)
		{
			if(polygon.num_edges == 0)
			{
				status = GraphicsPathFixedFillToPolygon(&clip_path->path,
							clip_path->tolerance, &polygon);

				fill_rule = clip_path->fill_rule;
				polygon.limits = NULL;
				polygon.num_limits = 0;
			}
			else
			{
				GRAPHICS_POLYGON next;

				InitializeGraphicsPolygon(&next, NULL, 0);
				status = GraphicsPathFixedFillToPolygon(&clip_path->path,
							clip_path->tolerance, &next);
				if(status == GRAPHICS_INTEGER_STATUS_SUCCESS)
				{
					status = GraphicsPolygonIntersect(&polygon, fill_rule,
								&next, clip_path->fill_rule);
				}
				GraphicsPolygonFinish(&next);
				fill_rule = GRAPHICS_FILL_RULE_WINDING;
			}

			if(UNLIKELY(status))
			{
				goto error;
			}
		}

		clip_path = clip_path->prev;
	}

	if(polygon.num_edges != 0)
	{
		GraphicsPolygonTranslate(&polygon, -extents->x, -extents->y);
		status = InitializeGraphicsCompositeRectanglesForPolygon(&composite,
			surface, GRAPHICS_OPERATOR_IN, &((GRAPHICS*)surface->graphics)->white_pattern.base, &polygon, NULL);
		if(UNLIKELY(status))
		{
			goto cleanup_polygon;
		}

		status = CompositePolygon(compositor, &composite,
					&polygon, fill_rule, antialias);
		GraphicsCompositeRectanglesFinish(&composite);
		GraphicsPolygonFinish(&polygon);
		if(UNLIKELY(status))
		{
			goto error;
		}
	}

	return surface;

cleanup_polygon:
	GraphicsPolygonFinish(&polygon);
error:
	DestroyGraphicsSurface(surface);
	return NULL;
}

static eGRAPHICS_INTEGER_STATUS FixupUnboundedMask(
	const GRAPHICS_SPANS_COMPOSITOR* compositor,
	const GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_BOXES* boxes
)
{
	GRAPHICS_COMPOSITE_RECTANGLES composite;
	GRAPHICS_SURFACE *clip;
	eGRAPHICS_INTEGER_STATUS status;

	clip = GetClipSurface(compositor, extents->surface, extents->clip,
				&extents->unbounded);
	if(UNLIKELY(clip->status))
	{
		if((eGRAPHICS_INTEGER_STATUS)clip->status == GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO)
		{
			return GRAPHICS_STATUS_SUCCESS;
		}
	}

	status = InitializeGraphicsCompositeRectanglesForBoxes(&composite,
		extents->surface, GRAPHICS_OPERATOR_CLEAR,
			&((GRAPHICS*)extents->surface->graphics)->clear_pattern.base, boxes, NULL);
	if(UNLIKELY(status))
	{
		goto cleanup_clip;
	}

	InitializeGraphicsPatternForSurface(&composite.mask_pattern.surface, clip);
	composite.mask_pattern.base.filter = GRAPHICS_FILTER_NEAREST;
	composite.mask_pattern.base.extend = GRAPHICS_EXTEND_NONE;

	status = CompositeBoxes(compositor, &composite, boxes);

	GraphicsPatternFinish(&composite.mask_pattern.base);
	GraphicsCompositeRectanglesFinish(&composite);

cleanup_clip:
	DestroyGraphicsSurface(clip);
	return status;
}

static eGRAPHICS_INTEGER_STATUS FixupUnboundedPolygon(
	const GRAPHICS_SPANS_COMPOSITOR* compositor,
	const GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_BOXES* boxes	
)
{
	GRAPHICS_POLYGON polygon, intersect;
	GRAPHICS_COMPOSITE_RECTANGLES composite;
	eGRAPHICS_FILL_RULE fill_rule;
	eGRAPHICS_ANTIALIAS antialias;
	eGRAPHICS_INTEGER_STATUS status;

	GraphicsClipGetPolygon(extents->clip, &polygon, &fill_rule, &antialias);
	if(status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED)
	{
		return status;
	}

	status = GraphicsPolygonInitializeBoxes(&intersect, boxes);
	if(UNLIKELY(status))
	{
		goto cleanup_polygon;
	}

	status = GraphicsPolygonIntersect(&polygon, fill_rule,
				&intersect, GRAPHICS_FILL_RULE_WINDING);
	GraphicsPolygonFinish(&intersect);

	if(UNLIKELY(status))
	{
		goto cleanup_polygon;
	}

	status = InitializeGraphicsCompositeRectanglesForPolygon(&composite,
		extents->surface, GRAPHICS_OPERATOR_CLEAR, &((GRAPHICS*)extents->surface->graphics)->clear_pattern.base, &polygon, NULL);
	if(UNLIKELY(status))
	{
		goto cleanup_polygon;
	}

	status = CompositePolygon(compositor, &composite, &polygon, fill_rule, antialias);

	GraphicsCompositeRectanglesFinish(&composite);

cleanup_polygon:
	GraphicsPolygonFinish(&polygon);

	return status;
}

static eGRAPHICS_INTEGER_STATUS FixupUnboundedBoxes(
	const GRAPHICS_SPANS_COMPOSITOR* compositor,
	const GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_BOXES* boxes
)
{
	GRAPHICS_BOXES tmp, clear;
	GRAPHICS_BOX box;
	eGRAPHICS_INTEGER_STATUS status;

	if(extents->bounded.width == extents->unbounded.width
		&& extents->bounded.height == extents->unbounded.height)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	InitializeGraphicsBoxes(&clear);
	
	box.point1.x = GraphicsFixedFromInteger(extents->unbounded.x + extents->unbounded.width);
	box.point1.y = GraphicsFixedFromInteger(extents->unbounded.y);
	box.point2.x = GraphicsFixedFromInteger(extents->unbounded.x);
	box.point2.y = GraphicsFixedFromInteger(extents->unbounded.y + extents->unbounded.height);

	if(boxes->num_boxes != 0)
	{
		InitializeGraphicsBoxes(&tmp);

		status = GraphicsBoxesAdd(&tmp, GRAPHICS_ANTIALIAS_DEFAULT, &box);

		tmp.chunks.next = &boxes->chunks;
		tmp.num_boxes += boxes->num_boxes;

		status = GraphicsBentleyOttmannTessellateBoxes(&tmp, GRAPHICS_FILL_RULE_WINDING, &clear);
		tmp.chunks.next = NULL;
		if(UNLIKELY(status))
		{
			goto error;
		}
	}
	else
	{
		box.point1.x = GraphicsFixedFromInteger(extents->unbounded.x);
		box.point2.x = GraphicsFixedFromInteger(extents->unbounded.x + extents->unbounded.width);

		status = GraphicsBoxesAdd(&clear, GRAPHICS_ANTIALIAS_DEFAULT, &box);
	}

	if(extents->clip->path != NULL)
	{
		status = FixupUnboundedPolygon(compositor, extents, &clear);
		if(status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED)
		{
			status = FixupUnboundedMask(compositor, extents, &clear);
		}
	}
	else
	{
		if(extents->clip->num_boxes != 0)
		{
			InitializeGraphicsBoxesForArray(&tmp, extents->clip->boxes, extents->clip->num_boxes);
			status = GraphicsBoxesIntersect(&clear, &tmp, &clear);
			if(UNLIKELY(status))
			{
				goto error;
			}
		}

		if(clear.is_pixel_aligned)
		{
			status = compositor->fill_boxes(extents->surface,
				GRAPHICS_OPERATOR_CLEAR, &((GRAPHICS*)extents->surface->graphics)->color_transparent, &clear);
		}
		else
		{
			GRAPHICS_COMPOSITE_RECTANGLES composite;

			status = InitializeGraphicsCompositeRectanglesForBoxes(&composite, extents->surface,
				GRAPHICS_OPERATOR_CLEAR, &((GRAPHICS*)extents->surface->graphics)->clear_pattern.base, &clear, NULL);
			if(status == GRAPHICS_STATUS_SUCCESS)
			{
				status = CompositeBoxes(compositor, &composite, &clear);
				GraphicsCompositeRectanglesFinish(&composite);
			}
		}
	}

error:
	GraphicsBoxesFinish(&clear);
	return status;
}

static GRAPHICS_SURFACE* UnwrapSource(const GRAPHICS_PATTERN* pattern)
{
	GRAPHICS_RECTANGLE_INT limit;

	return GraphicsPatternGetSource((GRAPHICS_SURFACE_PATTERN*)pattern, &limit);
}

static int OperatorReducesToSource(const GRAPHICS_COMPOSITE_RECTANGLES* extents, int no_mask)
{
	if(extents->op == GRAPHICS_OPERATOR_SOURCE)
	{
		return TRUE;
	}

	if(extents->surface->is_clear)
	{
		return extents->op == GRAPHICS_OPERATOR_OVER
			|| extents->op == GRAPHICS_OPERATOR_ADD;
	}

	if(no_mask && extents->op == GRAPHICS_OPERATOR_OVER)
	{
		return GraphicsPatternIsOpaque(&extents->source_pattern.base,
				&extents->source_sample_area);
	}

	return FALSE;
}

static eGRAPHICS_STATUS UploadBoxes(
	const GRAPHICS_SPANS_COMPOSITOR* compositor,
	const GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_BOXES* boxes
)
{
	GRAPHICS_SURFACE *dst = extents->surface;
	const GRAPHICS_SURFACE_PATTERN *source = &extents->source_pattern.surface;
	GRAPHICS_SURFACE *src;
	GRAPHICS_RECTANGLE_INT limit;
	eGRAPHICS_INTEGER_STATUS status;
	int tx, ty;

	src = GraphicsPatternGetSource(source, &limit);
	if(FALSE == (src->type == GRAPHICS_SURFACE_TYPE_IMAGE) || src->type == dst->type)
	{
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	}

	if(FALSE == GraphicsMatrixIsIntegerTranslation(&source->base.matrix, &tx, &ty))
	{
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	}

	if(extents->bounded.x + tx < limit.x || extents->bounded.y + ty < limit.y)
	{
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	}

	if(extents->bounded.x + extents->bounded.width + tx > limit.x + limit.width
		|| extents->bounded.y + extents->bounded.height + ty > limit.y + limit.height)
	{
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	}

	tx += limit.x;	ty += limit.y;

	if(src->type == GRAPHICS_SURFACE_TYPE_IMAGE)
	{
		status = compositor->draw_image_boxes(dst, (GRAPHICS_IMAGE_SURFACE*)src, boxes, tx, ty);
	}
	else
	{
		status = compositor->copy_boxes(dst, src, boxes, &extents->bounded, tx, ty);
	}

	return status;
}

static int ClipIsRegion(const GRAPHICS_CLIP* clip)
{
	int i;

	if(clip->is_region)
	{
		return TRUE;
	}

	if(clip->path != NULL)
	{
		return FALSE;
	}

	for(i=0; i<clip->num_boxes; i++)
	{
		const GRAPHICS_BOX *b = &clip->boxes[i];
		if(FALSE == GRAPHICS_FIXED_IS_INTEGER(b->point1.x | b->point1.y | b->point2.x | b->point2.y))
		{
			return FALSE;
		}
	}

	return TRUE;
}

static eGRAPHICS_INTEGER_STATUS CompositeAlignedBoxes(
	const GRAPHICS_SPANS_COMPOSITOR* compositor,
	const GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_BOXES* boxes
)
{
	GRAPHICS_SURFACE *dst = extents->surface;
	eGRAPHICS_OPERATOR op = extents->op;
	const GRAPHICS_PATTERN *source = &extents->source_pattern.base;
	eGRAPHICS_INTEGER_STATUS status;
	int need_clip_mask = ! ClipIsRegion(extents->clip);
	int op_is_source;
	int no_mask;
	int inplace;

	if(need_clip_mask && FALSE == extents->is_bounded)
	{
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	}

	no_mask = extents->mask_pattern.base.type == GRAPHICS_PATTERN_TYPE_SOLID
		&& GRAPHICS_COLOR_IS_OPAQUE(&extents->mask_pattern.solid.color);
	op_is_source = OperatorReducesToSource(extents, no_mask);
	inplace = ! need_clip_mask && op_is_source && no_mask;

	if(op == GRAPHICS_OPERATOR_SOURCE && (need_clip_mask || ! no_mask))
	{
		if((compositor->base.flags & GRAPHICS_SPANS_COMPOSITOR_HAS_LERP) == 0)
		{
			return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
		}
	}

	/* RecordingPatternCoontainsSample */

	status = GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	if(! need_clip_mask && no_mask && source->type == GRAPHICS_PATTERN_TYPE_SOLID)
	{
		const GRAPHICS_COLOR *color;

		color = &((GRAPHICS_SOLID_PATTERN*)source)->color;
		if(op_is_source)
		{
			op = GRAPHICS_OPERATOR_SOURCE;
		}
		status = compositor->fill_boxes(dst, op, color, boxes);
	}
	else if(inplace && source->type == GRAPHICS_PATTERN_TYPE_SURFACE)
	{
		status = UploadBoxes(compositor, extents, boxes);
	}
	if(status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED)
	{
		GRAPHICS_SURFACE *src;
		GRAPHICS_SURFACE *mask = NULL;
		int src_x, src_y;
		int mask_x = 0, mask_y = 0;

		if(need_clip_mask)
		{
			mask = GetClipSurface(compositor, dst, extents->clip, &extents->bounded);
			if(UNLIKELY(mask->status))
			{
				return mask->status;
			}

			mask_x = - extents->bounded.x;
			mask_y = - extents->bounded.y;
		}

		if(FALSE == no_mask)
		{
			src = compositor->pattern_to_surface(dst, &extents->mask_pattern.base, TRUE,
					&extents->bounded, &extents->mask_sample_area, &src_x, &src_y, source->graphics);
			if(UNLIKELY(src->status))
			{
				DestroyGraphicsSurface(mask);
				return src->status;
			}

			if(mask != NULL)
			{
				status = compositor->composite_boxes(mask, GRAPHICS_OPERATOR_IN, src, NULL,
							src_x, src_y, 0, 0, mask_x, mask_y, boxes, &extents->bounded);
				DestroyGraphicsSurface(src);
			}
			else
			{
				mask = src;
				mask_x = src_x;
				mask_y = src_y;
			}
		}

		src = compositor->pattern_to_surface(dst, source, FALSE, &extents->bounded,
					&extents->source_sample_area, &src_x, &src_y, source->graphics);
		if(src->status == GRAPHICS_STATUS_SUCCESS)
		{
			status = compositor->composite_boxes(dst, op, src, mask, src_x, src_y,
						mask_x, mask_y, 0, 0, boxes, &extents->bounded);
			DestroyGraphicsSurface(src);
		}
		else
		{
			status = src->status;
		}

		DestroyGraphicsSurface(mask);
	}

	if(status == GRAPHICS_INTEGER_STATUS_SUCCESS && FALSE == extents->is_bounded)
	{
		status = FixupUnboundedBoxes(compositor, extents, boxes);
	}

	return status;
}

static eGRAPHICS_INTEGER_STATUS CompositeBoxes(
	const GRAPHICS_SPANS_COMPOSITOR* compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_BOXES* boxes
)
{
	GRAPHICS_ABSTRACT_SPAN_RENDERER renderer;
	GRAPHICS_RECTANGULAR_SCAN_CONVERTER converter;
	const struct _GRAPHICS_BOXES_CHUNK *chunk;
	eGRAPHICS_INTEGER_STATUS status;
	GRAPHICS_BOX box;

	GraphicsBoxFromRectangle(&box, &extents->unbounded);
	if(CompositeNeedsClip(extents, &box))
	{
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	}

	InitializeGraphicsRectangularScanConverter(&converter, &extents->unbounded);
	for(chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next)
	{
		const GRAPHICS_BOX *b = chunk->base;
		int i;

		for(i=0; i<chunk->count; i++)
		{
			status = GraphicsRectangularScanConverterAddBox(&converter, &b[i], 1);
			if(UNLIKELY(status))
			{
				goto cleanup_converter;
			}
		}
	}

	status = compositor->initialize_renderer(&renderer, extents,
				GRAPHICS_ANTIALIAS_DEFAULT, FALSE);
	if(status == GRAPHICS_INTEGER_STATUS_SUCCESS)
	{
		status = converter.base.generate(&converter.base, &renderer.renderer_union.base);
	}
	compositor->render_finish(&renderer, status);

cleanup_converter:
	converter.base.destroy(&converter.base);
	return status;
}

static eGRAPHICS_STATUS CompositePolygon(
	const GRAPHICS_SPANS_COMPOSITOR* compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE fill_rule,
	eGRAPHICS_ANTIALIAS antialias
)
{
	GRAPHICS_ABSTRACT_SPAN_RENDERER renderer;
	GRAPHICS_SCAN_CONVERTERS converter;
	int needs_clip;
	eGRAPHICS_INTEGER_STATUS status;

	if(extents->is_bounded)
	{
		needs_clip = extents->clip->path != NULL;
	}
	else
	{
		needs_clip = ! ClipIsRegion(extents->clip) || extents->clip->num_boxes > 1;
	}
	if(needs_clip)
	{
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	}
	else
	{
		const GRAPHICS_RECTANGLE_INT *r = &extents->unbounded;

		if(antialias == GRAPHICS_ANTIALIAS_FAST)
		{
			if(UNLIKELY((status = InitializeGraphicsTor22ScanConverter(&converter.tor22_scan_converter,
				r->x, r->y, r->x + r->width, r->y + r->height, fill_rule, antialias))))
			{
				goto cleanup_converter;
			}
		}
		else if(antialias == GRAPHICS_ANTIALIAS_NONE)
		{
			if(UNLIKELY((status = InitializeGraphicsMonoScanConverter(&converter.mono_scan_converter,
				r->x, r->y, r->x + r->width, r->y + r->height, fill_rule))))
			{
				goto cleanup_converter;
			}
		}
		else
		{
			if(UNLIKELY((status = InitializeGraphicsTorScanConverter(&converter.tor_scan_converter,
				r->x, r->y, r->x + r->width, r->y + r->height, fill_rule, antialias))))
			{
				goto cleanup_converter;
			}
			if ((status = GraphicsTorScanConverterAddPolygon(&converter.tor_scan_converter, polygon)) != GRAPHICS_STATUS_SUCCESS)
			{
				goto cleanup_converter;
			}
		}
	}

	if(LIKELY((status = compositor->initialize_renderer(&renderer, extents,
		antialias, needs_clip)) == GRAPHICS_STATUS_SUCCESS))
	{
		status = converter.converter.generate(&converter, &renderer.renderer_union.base);
	}
	compositor->render_finish(&renderer, status);

cleanup_converter:
	converter.converter.destroy(&converter);
	return status;
}

int GraphicsCompositeRectanglesCanReduceClip(
	GRAPHICS_COMPOSITE_RECTANGLES* composite,
	GRAPHICS_CLIP* clip
)
{
	GRAPHICS_RECTANGLE_INT extents;
	GRAPHICS_BOX box;

	if(clip == NULL)
	{
		return TRUE;
	}

	extents = composite->destination;
	if(composite->is_bounded & GRAPHICS_OPERATOR_BOUNDED_BY_SOURCE)
	{
		GraphicsRectangleIntersect(&extents, &composite->source);
	}
	if(composite->is_bounded & GRAPHICS_OPERATOR_BOUNDED_BY_MASK)
	{
		GraphicsRectangleIntersect(&extents, &composite->mask);
	}

	GraphicsBoxFromRectangle(&box, &extents);
	return GraphicsClipContainsBox(clip, &box);
}

#ifdef __cplusplus
}
#endif
