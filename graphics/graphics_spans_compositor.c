#include "graphics_compositor.h"
#include "graphics_surface.h"
#include "graphics_region.h"
#include "graphics_matrix.h"
#include "graphics_private.h"
#include "graphics_inline.h"
#include "graphics_scan_converter_private.h"
#include "graphics.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _COMPOSITE_SPANS_INFO
{
	GRAPHICS_POLYGON* polygon;
	eGRAPHICS_FILL_RULE fill_rule;
	eGRAPHICS_ANTIALIAS antialias;
} COMPOSITE_SPANS_INFO;

static eGRAPHICS_INTEGER_STATUS CompositePolygon(
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
	GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE fill_rule,
	eGRAPHICS_ANTIALIAS antialias
);

static GRAPHICS_SURFACE* GetClipSurface(
	const GRAPHICS_SPANS_COMPOSITOR* compositor,
	GRAPHICS_SURFACE* destination,
	const GRAPHICS_CLIP* clip,
	const GRAPHICS_RECTANGLE_INT* extents
)
{
	GRAPHICS* graphics;
	GRAPHICS_COMPOSITE_RECTANGLES composite;
	GRAPHICS_SURFACE* surface;
	GRAPHICS_BOX box;
	GRAPHICS_POLYGON polygon;
	const GRAPHICS_CLIP_PATH* clip_path;
	eGRAPHICS_ANTIALIAS antialias;
	eGRAPHICS_FILL_RULE fill_rule;
	eGRAPHICS_INTEGER_STATUS status;

	graphics = (GRAPHICS*)compositor->base.graphics;

	surface = GraphicsSurfaceCreateScratch(destination, destination->content,
				extents->width, extents->height,&graphics->color_transparent);
	GraphicsBoxFromRectangle(&box,extents);
	InitializeGraphicsPolygon(&polygon,&box,1);

	clip_path = clip->path;
	if(clip_path == NULL)
	{
		goto CLEANUP_POLYGON;
	}
	status = GraphicsPathFixedFillToPolygon(&clip_path->path,
		clip_path->tolerance,&polygon);
	if(UNLIKELY(status))
	{
		goto CLEANUP_POLYGON;
	}

	polygon.num_limits = 0;

	antialias = clip_path->antialias;
	fill_rule = clip_path->fill_rule;

	if(clip->boxes != NULL)
	{
		GRAPHICS_POLYGON intersect;
		GRAPHICS_BOXES temp;

		InitializeGraphicsBoxesForArray(&temp,clip->boxes,clip->num_boxes);
		status = GraphicsPolygonInitializeBoxes(&intersect,&temp);
		if(UNLIKELY(status))
		{
			goto CLEANUP_POLYGON;
		}
		status = GraphicsPolygonIntersect(&polygon,fill_rule,
				&intersect,GRAPHICS_FILL_RULE_WINDING);
		GraphicsPolygonFinish(&intersect);

		if(UNLIKELY(status))
		{
			goto CLEANUP_POLYGON;
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
			if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
			{
				status = GraphicsPolygonIntersect(&polygon, fill_rule,
							&next, clip_path->fill_rule);
			}
			GraphicsPolygonFinish(&next);
			if(UNLIKELY(status))
			{
				goto CLEANUP_POLYGON;
			}

			fill_rule = GRAPHICS_FILL_RULE_WINDING;
		}

		clip_path = clip_path->prev;
	}

	GraphicsPolygonTranslate(&polygon, extents->x, -extents->y);
	status = InitializeGraphicsCompositeRectanglesForPolygon(&composite,
		surface, GRAPHICS_OPERATOR_ADD, &graphics->white_pattern, &polygon, NULL);

	if(UNLIKELY(status))
	{
		goto CLEANUP_POLYGON;
	}

	status = CompositePolygon(compositor, &composite, &polygon, fill_rule, antialias);
	GraphicsCompositeRectanglesFinish(&composite);
	GraphicsPolygonFinish(&polygon);
	if(UNLIKELY(status))
	{
		goto ERROR;
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
				if(LIKELY(status == GRAPHICS_STATUS_SUCCESS))
				{
					status = GraphicsPolygonIntersect(&polygon, fill_rule,
						&next, clip_path->fill_rule);
				}
				GraphicsPolygonFinish(&next);
				fill_rule = GRAPHICS_FILL_RULE_WINDING;
			}
			if(UNLIKELY(status))
			{
				goto ERROR;
			}
		}

		clip_path = clip_path->prev;
	}

	if(polygon.num_edges != 0)
	{
		GraphicsPolygonTranslate(&polygon, -extents->x, -extents->y);
		status = InitializeGraphicsCompositeRectanglesForPolygon(&composite, surface,
					GRAPHICS_OPERATOR_IN, &graphics->white_pattern.base, &polygon, NULL);
		if(UNLIKELY(status))
		{
			goto CLEANUP_POLYGON;
		}

		status = CompositePolygon(compositor, &composite,
					&polygon, fill_rule, antialias);
		GraphicsCompositeRectanglesFinish(&composite);
		GraphicsPolygonFinish(&polygon);
		if(UNLIKELY(status))
		{
			goto ERROR;
		}
	}

	return surface;

CLEANUP_POLYGON:
	GraphicsPolygonFinish(&polygon);
ERROR:
	DestroyGraphicsSurface(surface);
	return NULL;
}

static eGRAPHICS_INTEGER_STATUS FixupUnboundedMask(
	const GRAPHICS_SPANS_COMPOSITOR* compositor,
	const GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_BOXES* boxes
)
{
	GRAPHICS *graphics;
	GRAPHICS_COMPOSITE_RECTANGLES composite;
	GRAPHICS_SURFACE *clip;
	eGRAPHICS_INTEGER_STATUS status;

	clip = GetClipSurface(compositor, extents->surface, extents->clip, &extents->unbounded);
	if(UNLIKELY(clip->status))
	{
		if((eGRAPHICS_INTEGER_STATUS)clip->status == GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO)
		{
			return GRAPHICS_STATUS_SUCCESS;
		}
		return clip->status;
	}

	graphics = (GRAPHICS*)compositor->base.graphics;
	status = InitializeGraphicsCompositeRectanglesForBoxes(&composite, extents->surface,
				GRAPHICS_OPERATOR_CLEAR, &graphics->clear_pattern.base, boxes, NULL);

	if(UNLIKELY(status))
	{
		goto CLEANUP_CLIP;
	}

	InitializeGraphicsPatternForSurface(&composite.mask_pattern.surface, clip);
	composite.mask_pattern.base.filter = GRAPHICS_FILTER_NEAREST;
	composite.mask_pattern.base.extend = GRAPHICS_EXTEND_NONE;

	status = CompositeBoxes(compositor, &composite, boxes);

	GraphicsPatternFinish(&composite.mask_pattern.base);
	GraphicsCompositeRectanglesFinish(&composite);

CLEANUP_CLIP:
	DestroyGraphicsSurface(clip);
	return status;
}

static eGRAPHICS_INTEGER_STATUS FixupUnboundedPolygon(
	const GRAPHICS_SPANS_COMPOSITOR* compositor,
	const GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_BOXES* boxes
)
{
	GRAPHICS *graphics;
	GRAPHICS_POLYGON polygon, intersect;
	GRAPHICS_COMPOSITE_RECTANGLES composite;
	eGRAPHICS_FILL_RULE fill_rule;
	eGRAPHICS_ANTIALIAS antialias;
	eGRAPHICS_INTEGER_STATUS status;

	status = GraphicsClipGetPolygon(extents->clip, &polygon, &fill_rule, &antialias);
	if(status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED)
	{
		return status;
	}

	status = GraphicsPolygonInitializeBoxes(&intersect, boxes);
	if(UNLIKELY(status))
	{
		goto CLEANUP_POLYGON;
	}

	status = GraphicsPolygonIntersect(&polygon, fill_rule, &intersect, GRAPHICS_FILL_RULE_WINDING);
	GraphicsPolygonFinish(&intersect);

	if(UNLIKELY(status))
	{
		goto CLEANUP_POLYGON;
	}

	graphics = (GRAPHICS*)compositor->base.graphics;
	status = InitializeGraphicsCompositeRectanglesForPolygon(&composite,
				extents->surface, GRAPHICS_OPERATOR_CLEAR, &graphics->clear_pattern.base, &polygon, NULL);

	if(UNLIKELY(status))
	{
		goto CLEANUP_POLYGON;
	}

	status = CompositePolygon(compositor, &composite, &polygon, fill_rule, antialias);

	GraphicsCompositeRectanglesFinish(&composite);
CLEANUP_POLYGON:
	GraphicsPolygonFinish(&polygon);

	return status;
}

static eGRAPHICS_INTEGER_STATUS FixupUnboundedBoxes(
	const GRAPHICS_SPANS_COMPOSITOR* compositor,
	const GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_BOXES* boxes
)
{
	GRAPHICS *graphics;
	GRAPHICS_BOXES temp, clear;
	GRAPHICS_BOX box;
	eGRAPHICS_INTEGER_STATUS status;

	if(extents->bounded.width == extents->unbounded.width
		&& extents->bounded.height == extents->unbounded.height)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	graphics = (GRAPHICS*)compositor->base.graphics;
	InitializeGraphicsBoxes(&clear);

	box.point1.x = GraphicsFixedFromInteger(extents->unbounded.x + extents->unbounded.width);
	box.point1.y = GraphicsFixedFromInteger(extents->unbounded.y);
	box.point2.x = GraphicsFixedFromInteger(extents->unbounded.x);
	box.point2.y = GraphicsFixedFromInteger(extents->unbounded.y + extents->unbounded.height);

	if(boxes->num_boxes != 0)
	{
		InitializeGraphicsBoxes(&temp);

		status = GraphicsBoxesAdd(&temp, GRAPHICS_ANTIALIAS_DEFAULT, &box);

		temp.chunks.next = &boxes->chunks;
		temp.num_boxes += boxes->num_boxes;

		status = GraphicsBentleyOttmannTessellateBoxes(&temp,
					GRAPHICS_FILL_RULE_WINDING, &clear);
		temp.chunks.next = NULL;
		if(UNLIKELY(status))
		{
			goto ERROR;
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
			InitializeGraphicsBoxesForArray(&temp, extents->clip->boxes, extents->clip->num_boxes);
			status = GraphicsBoxesIntersect(&clear, &temp, &clear);
			if(UNLIKELY(status))
			{
				goto ERROR;
			}
		}

		if(clear.is_pixel_aligned)
		{
			status = compositor->fill_boxes(extents->surface, GRAPHICS_OPERATOR_CLEAR,
											&graphics->color_transparent, &clear);
		}
		else
		{
			GRAPHICS_COMPOSITE_RECTANGLES composite;

			status = InitializeGraphicsCompositeRectanglesForBoxes(&composite, extents->surface,
						GRAPHICS_OPERATOR_CLEAR, &graphics->clear_pattern.base, &clear, NULL);
			if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
			{
				status = CompositeBoxes(compositor, &composite, &clear);
				GraphicsCompositeRectanglesFinish(&composite);
			}
		}
	}

ERROR:
	GraphicsBoxesFinish(&clear);
	return status;
}

static GRAPHICS_SURFACE* UnwrapSource(const GRAPHICS_PATTERN* pattern)
{
	GRAPHICS_RECTANGLE_INT limit;
	return GraphicsPatternGetSource((GRAPHICS_SURFACE_PATTERN*)pattern, &limit);
}

static int IsRecordingPattern(const GRAPHICS_PATTERN* pattern)
{
	GRAPHICS_SURFACE *surface;

	if(pattern->type != GRAPHICS_PATTERN_TYPE_SURFACE)
	{
		return FALSE;
	}

	surface = ((const GRAPHICS_SURFACE_PATTERN*)pattern)->surface;
	return surface->backend->type == GRAPHICS_SURFACE_TYPE_RECORDING;
}

static int RecordingPatternContainsSample(const GRAPHICS_PATTERN* pattern, const GRAPHICS_RECTANGLE_INT* sample)
{
	GRAPHICS_RECORDING_SURFACE *surface;

	if(IsRecordingPattern(pattern) == FALSE)
	{
		return FALSE;
	}

	if(pattern->extend == GRAPHICS_EXTEND_NONE)
	{
		return TRUE;
	}

	surface = (GRAPHICS_RECORDING_SURFACE*)UnwrapSource(pattern);
	if(surface->unbounded)
	{
		return TRUE;
	}

	return GraphicsRectangleContainsRectangle(&surface->extents, sample);
}

static int OperatorReducesToSource(const GRAPHICS_COMPOSITE_RECTANGLES* extents, int no_mask)
{
	if(extents->op == GRAPHICS_OPERATOR_SOURCE)
	{
		return TRUE;
	}

	if(extents->surface->is_clear)
	{
		return extents->op == GRAPHICS_OPERATOR_OVER || extents->op == GRAPHICS_OPERATOR_ADD;
	}

	if(no_mask && extents->op == GRAPHICS_OPERATOR_OVER)
	{
		return GraphicsPatternIsOpaque(&extents->source_pattern.base, &extents->source_sample_area);
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
	if(FALSE == (src->type == GRAPHICS_SURFACE_TYPE_IMAGE || src->type == dst->type))
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

	tx += limit.x;
	ty += limit.y;

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

	for(i = 0; i < clip->num_boxes; i++)
	{
		const GRAPHICS_BOX *b = &clip->boxes[i];
		if(FALSE == GRAPHICS_FIXED_IS_INTEGER(b->point1.x | b->point1.y | b->point2.x | b->point2.y))
		{
			return FALSE;
		}
	}

	return TRUE;
}

static eGRAPHICS_STATUS CompositeAlignedBoxexs(
	const GRAPHICS_SPANS_COMPOSITOR* compositor,
	const GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_BOXES* boxes
)
{
	GRAPHICS *graphics;
	GRAPHICS_SURFACE *dst = extents->surface;
	eGRAPHICS_OPERATOR op = extents->op;
	const GRAPHICS_PATTERN *source = &extents->source_pattern.base;
	eGRAPHICS_INTEGER_STATUS status;
	int need_clip_mask = !(extents->clip->is_region);
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
	inplace = !need_clip_mask && op_is_source && no_mask;

	if(op == GRAPHICS_OPERATOR_SOURCE && (need_clip_mask || !no_mask))
	{
		if((compositor->base.flags & GRAPHICS_SPANS_COMPOSITOR_HAS_LERP) == 0)
		{
			return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
		}
	}

	graphics = (GRAPHICS*)compositor->base.graphics;
	if(inplace && RecordingPatternContainsSample(&extents->source_pattern.base, &extents->source_sample_area))
	{
		GRAPHICS_CLIP *recording_clip;
		GRAPHICS_CLIP result;
		const GRAPHICS_PATTERN *source = &extents->source_pattern.base;
		const GRAPHICS_MATRIX *m;
		GRAPHICS_MATRIX matrix;

		if(FALSE == dst->is_clear)
		{
			status = compositor->fill_boxes(dst, GRAPHICS_OPERATOR_CLEAR, &graphics->color_transparent, boxes);
			if(UNLIKELY(status))
			{
				return status;
			}

			dst->is_clear = TRUE;
		}

		m = &source->matrix;
		if(FALSE == GRAPHICS_MATRIX_IS_IDENTITY(&dst->device_transform))
		{
			GraphicsMatrixMultiply(&matrix, &source->matrix, &dst->device_transform);
			m = &matrix;
		}

		recording_clip = GraphicsClipFromBoxes(boxes, &result);
		// TODO _cairo_recording_surface_replay_with_clip
		// status = GraphicsRecordingSur

		GraphicsClipDestroy(recording_clip);

		return status;
	}

	status = GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	if(!need_clip_mask && no_mask && source->type == GRAPHICS_PATTERN_TYPE_SOLID)
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
			if(mask == NULL)
			{
				return GRAPHICS_STATUS_INVALID;
			}
			if(UNLIKELY(mask->status))
			{
				return mask->status;
			}

			mask_x = -extents->bounded.x;
			mask_y = -extents->bounded.y;
		}

		if(FALSE == no_mask)
		{
			src = compositor->pattern_to_surface(dst, &extents->mask_pattern.base, TRUE,
					&extents->bounded, &extents->mask_sample_area, &src_x, &src_y, graphics);
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
				&extents->source_sample_area, &src_x, &src_y, graphics);
		if(LIKELY(src->status == GRAPHICS_STATUS_SUCCESS))
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

	if(status == GRAPHICS_STATUS_SUCCESS && FALSE == extents->is_bounded)
	{
		status = FixupUnboundedBoxes(compositor, extents, boxes);
	}

	return status;
}

static int CompositeNeedsClip(const GRAPHICS_COMPOSITE_RECTANGLES* composite, const GRAPHICS_BOX* extents)
{
	return !GraphicsClipContainsRectangleBox(composite->clip, extents);
}

static eGRAPHICS_INTEGER_STATUS CompositeBoxes(
	const GRAPHICS_SPANS_COMPOSITOR* compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_BOXES* boxes
)
{
	GRAPHICS_ABSTRACT_SPAN_RENDERER renderer;
	GRAPHICS_RECTANGULAR_SCAN_CONVERTER converter;
	struct _GRAPHICS_BOXES_CHUNK *chunk;
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
		const GRAPHICS_BOX *box = chunk->base;
		int i;

		for(i = 0; i < chunk->count; i++)
		{
			status = GraphicsRectangularScanConverterAddBox(&converter, &box[i], 1);
			if(UNLIKELY(status))
			{
				goto CLEANUP_CONVERTER;
			}
		}
	}

	status = compositor->initialize_renderer(&renderer, extents, GRAPHICS_ANTIALIAS_DEFAULT, FALSE);
	if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
	{
		status = converter.base.generate(&converter.base, &renderer.renderer_union.base);
	}
	compositor->render_finish(&renderer, status);

CLEANUP_CONVERTER:
	converter.base.destroy(&converter.base);
	return status;
}

static eGRAPHICS_INTEGER_STATUS CompositePolygon(
	const GRAPHICS_SPANS_COMPOSITOR* compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE fill_rule,
	eGRAPHICS_ANTIALIAS antialias
)
{
	GRAPHICS_ABSTRACT_SPAN_RENDERER renderer = {0};
	GRAPHICS_SCAN_CONVERTERS converter = {0};
	int needs_clip;
	eGRAPHICS_INTEGER_STATUS status;

	if(extents->is_bounded)
	{
		needs_clip = extents->clip->path != NULL;
	}
	else
	{
		needs_clip = !ClipIsRegion(extents->clip) || extents->clip->num_boxes > 1;
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
			status = InitializeGraphicsTor22ScanConverter(&converter.tor22_scan_converter, r->x, r->y,
						r->x + r->width, r->y + r->height, fill_rule, antialias);
			if(UNLIKELY(status))
			{
				return status;
			}
			status = GraphicsTor22ScanConverterAddPolygon(&converter.tor22_scan_converter, polygon);
		}
		else if(antialias == GRAPHICS_ANTIALIAS_NONE)
		{
			status = InitializeGraphicsMonoScanConverter(&converter.mono_scan_converter, r->x, r->y,
						r->x + r->width, r->y + r->height, fill_rule);
			if(UNLIKELY(status))
			{
				return status;
			}
			status = GraphicsMonoScanConvereterAddPolygon(&converter.mono_scan_converter, polygon);
		}
		else
		{
			status = InitializeGraphicsTorScanConverter(&converter.tor_scan_converter, r->x, r->y,
						r->x + r->width, r->y + r->height, fill_rule, antialias);
			if(UNLIKELY(status))
			{
				return status;
			}
			status = GraphicsTorScanConverterAddPolygon(&converter.tor_scan_converter, polygon);
		}
	}

	if(UNLIKELY(status))
	{
		goto CLEANUP_CONVERTER;
	}

	status = compositor->initialize_renderer(&renderer, extents, antialias, needs_clip);
	if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
	{
		status = converter.converter.generate(&converter, &renderer.renderer_union.base);
	}
	compositor->render_finish(&renderer, status);

CLEANUP_CONVERTER:
	converter.converter.destroy(&converter);
	return status;
}

static eGRAPHICS_STATUS TrimExtentsToBoxes(GRAPHICS_COMPOSITE_RECTANGLES* extents, GRAPHICS_BOXES* boxes)
{
	GRAPHICS_BOX box;

	GraphicsBoxesExtents(boxes, &box);
	return GraphicsCompositeRectanglesIntersectMaskExtents(extents, &box);
}

static eGRAPHICS_INTEGER_STATUS TrimExtentsToPolygon(GRAPHICS_COMPOSITE_RECTANGLES* extents, GRAPHICS_POLYGON* polygon)
{
	return GraphicsCompositeRectanglesIntersectMaskExtents(extents, &polygon->extents);
}

static eGRAPHICS_INTEGER_STATUS ClipAndCompositeBoxes(
	const GRAPHICS_SPANS_COMPOSITOR* compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_BOXES* boxes
)
{
	eGRAPHICS_INTEGER_STATUS status;
	GRAPHICS_POLYGON polygon;

	status = TrimExtentsToBoxes(extents, boxes);
	if(UNLIKELY(status))
	{
		return status;
	}

	if(boxes->num_boxes == 0)
	{
		if(extents->is_bounded)
		{
			return GRAPHICS_INTEGER_STATUS_SUCCESS;
		}
		return FixupUnboundedBoxes(compositor, extents, boxes);
	}

	if(extents->clip->path != NULL && extents->is_bounded)
	{
		GRAPHICS_POLYGON polygon;
		eGRAPHICS_FILL_RULE fill_rule;
		eGRAPHICS_ANTIALIAS antialias;
		GRAPHICS_CLIP *clip, local_clip;

		clip = GraphicsClipCopy(&local_clip, extents->clip);
		clip = GraphicsClipIntersectBoxes(clip, boxes);
		if(clip->clip_all)
		{
			return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
		}

		status = GraphicsClipGetPolygon(clip, &polygon, &fill_rule, &antialias);
		GraphicsClipPathDestroy(clip->path);
		clip->path = NULL;
		if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
		{
			GRAPHICS_CLIP *saved_clip = extents->clip;
			extents->clip = clip;

			status = ClipAndCompositePolygon(compositor, extents, &polygon, fill_rule, antialias);

			clip = extents->clip;
			extents->clip = saved_clip;

			GraphicsPolygonFinish(&polygon);
		}
		GraphicsClipDestroy(clip);

		if(status != GRAPHICS_INTEGER_STATUS_UNSUPPORTED)
		{
			return status;
		}
	}

	if(boxes->is_pixel_aligned)
	{
		status = CompositeAlignedBoxexs(compositor, extents, boxes);
		if(status != GRAPHICS_INTEGER_STATUS_UNSUPPORTED)
		{
			return status;
		}
	}

	status = CompositeBoxes(compositor, extents, boxes);
	if(status != GRAPHICS_INTEGER_STATUS_UNSUPPORTED)
	{
		return status;
	}

	status = GraphicsPolygonInitializeBoxes(&polygon, boxes);
	if(UNLIKELY(status))
	{
		return status;
	}

	status = CompositePolygon(compositor, extents, &polygon, GRAPHICS_FILL_RULE_WINDING,
				GRAPHICS_ANTIALIAS_DEFAULT);
	GraphicsPolygonFinish(&polygon);

	return status;
}

static eGRAPHICS_INTEGER_STATUS ClipAndCompositePolygon(
	const GRAPHICS_SPANS_COMPOSITOR* compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE fill_rule,
	eGRAPHICS_ANTIALIAS antialias
)
{
	eGRAPHICS_INTEGER_STATUS status;

	status = TrimExtentsToPolygon(extents, polygon);
	if(UNLIKELY(status))
	{
		return status;
	}

	if(GraphicsPolygonIsEmpty(polygon))
	{
		GRAPHICS_BOXES boxes;

		if(extents->is_bounded)
		{
			return GRAPHICS_INTEGER_STATUS_SUCCESS;
		}

		InitializeGraphicsBoxes(&boxes);
		extents->bounded.width = extents->bounded.height = 0;
		return FixupUnboundedBoxes(compositor, extents, &boxes);
	}

	if(extents->is_bounded && extents->clip->path != NULL)
	{
		GRAPHICS_POLYGON clipper;
		eGRAPHICS_ANTIALIAS clip_antialias;
		eGRAPHICS_FILL_RULE clip_fill_rule;

		status = GraphicsClipGetPolygon(extents->clip, &clipper, &clip_fill_rule, &clip_antialias);
		if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
		{
			GRAPHICS_CLIP *old_clip, local_clip;

			if(clip_antialias == antialias)
			{
				status = GraphicsPolygonIntersect(polygon, fill_rule, &clipper, clip_fill_rule);
				GraphicsPolygonFinish(polygon);
				if(UNLIKELY(status))
				{
					return status;
				}

				old_clip = extents->clip;
				extents->clip = GraphicsClipCopyRegion(extents->clip, &local_clip);
				GraphicsClipDestroy(old_clip);

				status = TrimExtentsToPolygon(extents, polygon);
				if(UNLIKELY(status))
				{
					return status;
				}

				fill_rule = GRAPHICS_FILL_RULE_WINDING;
			}
			else
			{
				GraphicsPolygonFinish(&clipper);
			}
		}
	}

	return CompositePolygon(compositor, extents, polygon, fill_rule, antialias);
}

static eGRAPHICS_INTEGER_STATUS GraphicsSpansCompositorPaint(
	const GRAPHICS_COMPOSITOR* _compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents
)
{
	const GRAPHICS_SPANS_COMPOSITOR *compositor = (const GRAPHICS_SPANS_COMPOSITOR*)_compositor;
	GRAPHICS_BOXES boxes;
	eGRAPHICS_INTEGER_STATUS status;

	GraphicsClipStealBoxes(extents->clip, &boxes);
	status = ClipAndCompositeBoxes(compositor, extents, &boxes);
	GraphicsClipUnstealBoxes(extents->clip, &boxes);

	return status;
}

static eGRAPHICS_INTEGER_STATUS GraphicsSpansCompositorMask(
	const GRAPHICS_COMPOSITOR* _compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents
)
{
	const GRAPHICS_SPANS_COMPOSITOR *compositor = (const GRAPHICS_SPANS_COMPOSITOR*)_compositor;
	GRAPHICS_BOXES boxes;
	eGRAPHICS_INTEGER_STATUS status;

	GraphicsClipStealBoxes(extents->clip, &boxes);
	status = ClipAndCompositeBoxes(compositor, extents, &boxes);
	GraphicsClipUnstealBoxes(extents->clip, &boxes);

	return status;
}

static eGRAPHICS_INTEGER_STATUS GraphicsSpansCompositorStroke(
	const GRAPHICS_COMPOSITOR* _compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_MATRIX* matrix,
	const GRAPHICS_MATRIX* matrix_inverse,
	FLOAT_T tolerance,
	eGRAPHICS_ANTIALIAS antialias
)
{
	const GRAPHICS_SPANS_COMPOSITOR *compositor = (const GRAPHICS_SPANS_COMPOSITOR*)_compositor;
	eGRAPHICS_INTEGER_STATUS status;

	status = GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	if(path->stroke_is_rectilinear)
	{
		GRAPHICS_BOXES boxes;

		InitializeGraphicsBoxes(&boxes);
		if(FALSE == GraphicsClipContainsRectangle(extents->clip, &extents->mask))
		{
			GraphicsBoxesLimit(&boxes, extents->clip->boxes, extents->clip->num_boxes);
		}

		status = GraphicsPathFixedStrokeRectilinearToBoxes(path, style, matrix, antialias, &boxes);
		if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
		{
			status = ClipAndCompositeBoxes(compositor, extents, &boxes);
		}
		GraphicsBoxesFinish(&boxes);
	}

	if(status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED)
	{
		GRAPHICS_POLYGON polygon;
		eGRAPHICS_FILL_RULE fill_rule = GRAPHICS_FILL_RULE_WINDING;

		if(FALSE == GraphicsRectangleContainsRectangle(&extents->unbounded, &extents->mask))
		{
			if(extents->clip->num_boxes == 1)
			{
				InitializeGraphicsPolygon(&polygon, extents->clip->boxes, 1);
			}
			else
			{
				GRAPHICS_BOX limits;
				GraphicsBoxFromRectangle(&limits, &extents->unbounded);
				InitializeGraphicsPolygon(&polygon, &limits, 1);
			}
		}
		else
		{
			InitializeGraphicsPolygon(&polygon, NULL, 0);
		}
		status = GraphicsPathFixedStrokeToPolygon(path, style, matrix, matrix_inverse, tolerance, &polygon);

		polygon.num_limits = 0;

		if(status == GRAPHICS_INTEGER_STATUS_SUCCESS && extents->clip->num_boxes > 1)
		{
			status = GraphicsPolygonIntersect(&polygon, &fill_rule, extents->clip->boxes, extents->clip->num_boxes);
		}
		if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
		{
			GRAPHICS_CLIP *saved_clip = extents->clip, local_clip = {0};

			if(extents->is_bounded)
			{
				extents->clip = GraphicsClipCopyPath(extents->clip, &local_clip);
				extents->clip = GraphicsClipIntersectBox(extents->clip, &polygon.extents);
			}

			status = ClipAndCompositePolygon(compositor, extents, &polygon, fill_rule, antialias);

			if(extents->is_bounded)
			{
				GraphicsClipDestroy(extents->clip);
				extents->clip = saved_clip;
			}
		}
		GraphicsPolygonFinish(&polygon);
	}

	return status;
}

static eGRAPHICS_INTEGER_STATUS GraphicsSpansCompositorFill(
	const GRAPHICS_COMPOSITOR* _compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rule,
	FLOAT_T tolerance,
	eGRAPHICS_ANTIALIAS antialias
)
{
	const GRAPHICS_SPANS_COMPOSITOR *compositor = (const GRAPHICS_SPANS_COMPOSITOR*)_compositor;
	eGRAPHICS_INTEGER_STATUS status;

	status = GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	if(path->fill_is_rectilinear)
	{
		GRAPHICS_BOXES boxes;

		InitializeGraphicsBoxes(&boxes);
		if(FALSE == GraphicsClipContainsRectangle(extents->clip, &extents->mask))
		{
			GraphicsBoxesLimit(&boxes, extents->clip->boxes, extents->clip->num_boxes);
		}
		status = GraphicsPathFixedFillRectilinearToBoxes(path, fill_rule, antialias, &boxes);
		if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
		{
			status = ClipAndCompositeBoxes(compositor, extents, &boxes);
		}
		GraphicsBoxesFinish(&boxes);
	}
	if(status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED)
	{
		GRAPHICS_POLYGON polygon;

		if(FALSE == GraphicsRectangleContainsRectangle(&extents->unbounded, &extents->mask))
		{
			if(extents->clip->num_boxes == 1)
			{
				InitializeGraphicsPolygon(&polygon, extents->clip->boxes, 1);
			}
			else
			{
				GRAPHICS_BOX limits;
				GraphicsBoxFromRectangle(&limits, &extents->unbounded);
				InitializeGraphicsPolygon(&polygon, &limits, 1);
			}
		}
		else
		{
			InitializeGraphicsPolygon(&polygon, NULL, 0);
		}

		status = GraphicsPathFixedFillToPolygon(path, tolerance, &polygon);
		polygon.num_limits = 0;

		if(status == GRAPHICS_INTEGER_STATUS_SUCCESS && extents->clip->num_boxes > 1)
		{
			status = GraphicsPolygonIntersectWithBoxes(&polygon, &fill_rule,
						extents->clip->boxes, extents->clip->num_boxes);
		}

		if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
		{
			GRAPHICS_CLIP* saved_clip = extents->clip, local_clip = {0};

			if(extents->is_bounded)
			{
				extents->clip = GraphicsClipCopyPath(extents->clip, &local_clip);
				extents->clip = GraphicsClipIntersectBox(extents->clip, &polygon.extents);
			}

			status = ClipAndCompositePolygon(compositor, extents, &polygon, fill_rule, antialias);

			if(extents->is_bounded)
			{
				GraphicsClipDestroy(extents->clip);
				extents->clip = saved_clip;
			}
		}
		GraphicsPolygonFinish(&polygon);
	}

	return status;
}

void InitialzieGraphicsSpansCompositor(
	GRAPHICS_SPANS_COMPOSITOR* compositor,
	const GRAPHICS_COMPOSITOR* delegate,
	void* graphics
)
{
	SetGraphicsImageCompositorCallbacks(compositor);

	compositor->base.graphics = graphics;
	compositor->base.delegate = delegate;

	compositor->base.paint = GraphicsSpansCompositorPaint;
	compositor->base.mask = GraphicsSpansCompositorMask;
	compositor->base.fill = GraphicsSpansCompositorFill;
	compositor->base.stroke = GraphicsSpansCompositorStroke;
}

#ifdef __cplusplus
}
#endif
