#include "graphics_surface.h"
#include "graphics_compositor.h"
#include "graphics_types.h"
#include "graphics_region.h"
#include "graphics_private.h"
#include "graphics_inline.h"
#include "graphics_path.h"
#include "graphics_matrix.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef eGRAPHICS_INTEGER_STATUS (*draw_func_t)(
	const GRAPHICS_MASK_COMPOSITOR *compositor,
	GRAPHICS_SURFACE			*dst,
	void				*closure,
	eGRAPHICS_OPERATOR		 op,
	const GRAPHICS_PATTERN		*src,
	const GRAPHICS_RECTANGLE_INT	*src_sample,
	int				 dst_x,
	int				 dst_y,
	const GRAPHICS_RECTANGLE_INT	*extents,
	GRAPHICS_CLIP			*clip
);

static void do_unaligned_row(void (*blt)(void *closure,
	int16 x, int16 y,
	int16 w, int16 h,
	uint16 coverage),
	void *closure,
	const GRAPHICS_BOX *b,
	int tx, int y, int h,
	uint16 coverage)
{
	int x1 = GraphicsFixedIntegerPart(b->point1.x) - tx;
	int x2 = GraphicsFixedIntegerPart(b->point2.x) - tx;
	if (x2 > x1) {
		if (! GRAPHICS_FIXED_IS_INTEGER(b->point1.x)) {
			blt(closure, x1, y, 1, h,
				coverage * (256 - GraphicsFixedFractionalPart(b->point1.x)));
			x1++;
		}

		if (x2 > x1)
			blt(closure, x1, y, x2-x1, h, (coverage << 8) - (coverage >> 8));

		if (! GRAPHICS_FIXED_IS_INTEGER(b->point2.x))
			blt(closure, x2, y, 1, h,
				coverage * GraphicsFixedFractionalPart(b->point2.x));
	} else
		blt(closure, x1, y, 1, h,
			coverage * (b->point2.x - b->point1.x));
}

static void do_unaligned_box(void (*blt)(void *closure,
	int16 x, int16 y,
	int16 w, int16 h,
	uint16 coverage),
	void *closure,
	const GRAPHICS_BOX *b, int tx, int ty)
{
	int y1 = GraphicsFixedIntegerPart(b->point1.y) - ty;
	int y2 = GraphicsFixedIntegerPart(b->point2.y) - ty;
	if (y2 > y1) {
		if (! GRAPHICS_FIXED_IS_INTEGER(b->point1.y)) {
			do_unaligned_row(blt, closure, b, tx, y1, 1,
				256 - GraphicsFixedFractionalPart(b->point1.y));
			y1++;
		}

		if (y2 > y1)
			do_unaligned_row(blt, closure, b, tx, y1, y2-y1, 256);

		if (! GRAPHICS_FIXED_IS_INTEGER(b->point2.y))
			do_unaligned_row(blt, closure, b, tx, y2, 1,
				GraphicsFixedFractionalPart(b->point2.y));
	} else
		do_unaligned_row(blt, closure, b, tx, y1, 1,
			b->point2.y - b->point1.y);
}

struct blt_in {
	const GRAPHICS_MASK_COMPOSITOR *compositor;
	GRAPHICS_SURFACE *dst;
};

static void blt_in(void *closure,
	int16 x, int16 y,
	int16 w, int16 h,
	uint16 coverage)
{
	struct blt_in *info = closure;
	GRAPHICS_COLOR color;
	GRAPHICS_RECTANGLE_INT rect;

	if (coverage == 0xffff)
		return;

	rect.x = x;
	rect.y = y;
	rect.width  = w;
	rect.height = h;
	
	InitializeGraphicsColorRGBA(&color, 0, 0, 0, coverage / (double) 0xffff);
	info->compositor->fill_rectangles (info->dst, GRAPHICS_OPERATOR_IN,
		&color, &rect, 1);
}

static GRAPHICS_SURFACE* create_composite_mask(
	const GRAPHICS_MASK_COMPOSITOR *compositor,
	GRAPHICS_SURFACE		*dst,
	void			*draw_closure,
	draw_func_t		 draw_func,
	draw_func_t		 mask_func,
	const GRAPHICS_COMPOSITE_RECTANGLES *extents
)
{
	GRAPHICS *graphics;
	GRAPHICS_SURFACE *surface;
	eGRAPHICS_INTEGER_STATUS status;
	struct blt_in info;
	int i;

	graphics = compositor->base.graphics;

	surface = GraphicsSurfaceCreateScratch(dst, GRAPHICS_CONTENT_ALPHA,
		extents->bounded.width,
		extents->bounded.height,
		NULL);
	if (UNLIKELY(surface->status))
		return surface;

	status = compositor->acquire (surface);
	if (UNLIKELY(status)) {
		DestroyGraphicsSurface(surface);
		return NULL;
	}

	if (!surface->is_clear) {
		GRAPHICS_RECTANGLE_INT rect;

		rect.x = rect.y = 0;
		rect.width = extents->bounded.width;
		rect.height = extents->bounded.height;

		status = compositor->fill_rectangles (surface, GRAPHICS_OPERATOR_CLEAR,
			&graphics->color_transparent,
			&rect, 1);
		if (UNLIKELY(status))
			goto error;
	}

	if (mask_func) {
		status = mask_func (compositor, surface, draw_closure,
			GRAPHICS_OPERATOR_SOURCE, NULL, NULL,
			extents->bounded.x, extents->bounded.y,
			&extents->bounded, extents->clip);
		if (LIKELY(status != GRAPHICS_INTEGER_STATUS_UNSUPPORTED))
			goto out;
	}

	/* Is it worth setting the clip region here? */
	status = draw_func (compositor, surface, draw_closure,
		GRAPHICS_OPERATOR_ADD, NULL, NULL,
		extents->bounded.x, extents->bounded.y,
		&extents->bounded, NULL);
	if (UNLIKELY(status))
		goto error;
	
	info.compositor = compositor;
	info.dst = surface;
	for(i = 0; i < extents->clip->num_boxes; i++)
	{
		GRAPHICS_BOX *b = &extents->clip->boxes[i];

		if (! GRAPHICS_FIXED_IS_INTEGER(b->point1.x) ||
			! GRAPHICS_FIXED_IS_INTEGER(b->point1.y) ||
			! GRAPHICS_FIXED_IS_INTEGER(b->point2.x) ||
			! GRAPHICS_FIXED_IS_INTEGER(b->point2.y))
		{
			do_unaligned_box(blt_in, &info, b,
				extents->bounded.x,
				extents->bounded.y);
		}
	}
	
	if (extents->clip->path != NULL)
	{
		status = GraphicsClipCombineWithSurface(extents->clip, surface,
			extents->bounded.x,
			extents->bounded.y);
		if (UNLIKELY(status))
			goto error;
	}
	
out:
	compositor->release (surface);
	surface->is_clear = FALSE;
	return surface;

error:
	compositor->release (surface);
	if (status != GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO)
	{
		DestroyGraphicsSurface(surface);
		surface = NULL;
	}
	return surface;
}

/* Handles compositing with a clip surface when the operator allows
* us to combine the clip with the mask
*/
static eGRAPHICS_STATUS clip_and_composite_with_mask(
	const GRAPHICS_MASK_COMPOSITOR *compositor,
	void			*draw_closure,
	draw_func_t		 draw_func,
	draw_func_t		 mask_func,
	eGRAPHICS_OPERATOR		 op,
	GRAPHICS_PATTERN		*pattern,
	const GRAPHICS_COMPOSITE_RECTANGLES*extents)
{
	GRAPHICS_SURFACE *dst = extents->surface;
	GRAPHICS_SURFACE *mask, *src;
	int src_x, src_y;

	mask = create_composite_mask (compositor, dst, draw_closure,
		draw_func, mask_func,
		extents);
	if (UNLIKELY(mask->status))
		return mask->status;

	if (pattern != NULL || dst->content != GRAPHICS_CONTENT_ALPHA) {
		src = compositor->pattern_to_surface (dst,
			&extents->source_pattern.base,
			FALSE,
			&extents->bounded,
			&extents->source_sample_area,
			&src_x, &src_y);
		if (UNLIKELY(src->status)) {
			DestroyGraphicsSurface(mask);
			return src->status;
		}

		compositor->composite (dst, op, src, mask,
			extents->bounded.x + src_x,
			extents->bounded.y + src_y,
			0, 0,
			extents->bounded.x,	  extents->bounded.y,
			extents->bounded.width,  extents->bounded.height);

		DestroyGraphicsSurface(src);
	} else {
		compositor->composite (dst, op, mask, NULL,
			0, 0,
			0, 0,
			extents->bounded.x,	  extents->bounded.y,
			extents->bounded.width,  extents->bounded.height);
	}
	DestroyGraphicsSurface(mask);

	return GRAPHICS_STATUS_SUCCESS;
}

static GRAPHICS_SURFACE* get_clip_source(
	const GRAPHICS_MASK_COMPOSITOR *compositor,
	GRAPHICS_CLIP *clip,
	GRAPHICS_SURFACE *dst,
	const GRAPHICS_RECTANGLE_INT *bounds,
	int *out_x, int *out_y
)
{
	GRAPHICS_SURFACE_PATTERN pattern;
	GRAPHICS_RECTANGLE_INT r;
	GRAPHICS_SURFACE *surface;
	
	surface = GraphicsClipGetImage(clip, dst, bounds);
	if (UNLIKELY(surface->status))
		return surface;

	InitializeGraphicsPatternForSurface(&pattern, surface);
	pattern.base.filter = GRAPHICS_FILTER_NEAREST;
	DestroyGraphicsSurface(surface);

	r.x = r.y = 0;
	r.width  = bounds->width;
	r.height = bounds->height;

	surface = compositor->pattern_to_surface (dst, &pattern.base, TRUE,
		&r, &r, out_x, out_y);
	GraphicsPatternFinish(&pattern.base);

	*out_x += -bounds->x;
	*out_y += -bounds->y;
	return surface;
}

/* Handles compositing with a clip surface when we have to do the operation
* in two pieces and combine them together.
*/
static eGRAPHICS_STATUS clip_and_composite_combine(
	const GRAPHICS_MASK_COMPOSITOR *compositor,
	void			*draw_closure,
	draw_func_t		 draw_func,
	eGRAPHICS_OPERATOR		 op,
	const GRAPHICS_PATTERN	*pattern,
	const GRAPHICS_COMPOSITE_RECTANGLES*extents
)
{
	GRAPHICS_SURFACE *dst = extents->surface;
	GRAPHICS_SURFACE *tmp, *clip;
	eGRAPHICS_STATUS status;
	int clip_x, clip_y;

	
	tmp = GraphicsSurfaceCreateScratch(dst, dst->content,
		extents->bounded.width,
		extents->bounded.height,
		NULL);
	if (UNLIKELY(tmp->status))
		return tmp->status;

	compositor->composite (tmp, GRAPHICS_OPERATOR_SOURCE, dst, NULL,
		extents->bounded.x,	  extents->bounded.y,
		0, 0,
		0, 0,
		extents->bounded.width,  extents->bounded.height);

	status = draw_func (compositor, tmp, draw_closure, op,
		pattern, &extents->source_sample_area,
		extents->bounded.x, extents->bounded.y,
		&extents->bounded, NULL);
	if (UNLIKELY(status))
		goto cleanup;

	clip = get_clip_source (compositor,
		extents->clip, dst, &extents->bounded,
		&clip_x, &clip_y);
	if (UNLIKELY((status = clip->status)))
		goto cleanup;

	if (dst->is_clear) {
		compositor->composite (dst, GRAPHICS_OPERATOR_SOURCE, tmp, clip,
			0, 0,
			clip_x, clip_y,
			extents->bounded.x,	  extents->bounded.y,
			extents->bounded.width,  extents->bounded.height);
	} else {
		/* Punch the clip out of the destination */
		compositor->composite (dst, GRAPHICS_OPERATOR_DEST_OUT, clip, NULL,
			clip_x, clip_y,
			0, 0,
			extents->bounded.x,	 extents->bounded.y,
			extents->bounded.width, extents->bounded.height);

		/* Now add the two results together */
		compositor->composite (dst, GRAPHICS_OPERATOR_ADD, tmp, clip,
			0, 0,
			clip_x, clip_y,
			extents->bounded.x,	 extents->bounded.y,
			extents->bounded.width, extents->bounded.height);
	}
	DestroyGraphicsSurface(clip);

cleanup:
	DestroyGraphicsSurface(tmp);
	return status;
}

/* Handles compositing for %GRAPHICS_OPERATOR_SOURCE, which is special; it's
* defined as (src IN mask IN clip) ADD (dst OUT (mask IN clip))
*/
static eGRAPHICS_STATUS
clip_and_composite_source (const GRAPHICS_MASK_COMPOSITOR	*compositor,
	void				*draw_closure,
	draw_func_t			 draw_func,
	draw_func_t			 mask_func,
	GRAPHICS_PATTERN		*pattern,
	const GRAPHICS_COMPOSITE_RECTANGLES	*extents)
{
	GRAPHICS_SURFACE *dst = extents->surface;
	GRAPHICS_SURFACE *mask, *src;
	int src_x, src_y;

	/* Create a surface that is mask IN clip */
	mask = create_composite_mask (compositor, dst, draw_closure,
		draw_func, mask_func,
		extents);
	if (UNLIKELY(mask->status))
		return mask->status;

	src = compositor->pattern_to_surface (dst,
		pattern,
		FALSE,
		&extents->bounded,
		&extents->source_sample_area,
		&src_x, &src_y);
	if (UNLIKELY(src->status)) {
		DestroyGraphicsSurface(mask);
		return src->status;
	}

	if (dst->is_clear) {
		compositor->composite (dst, GRAPHICS_OPERATOR_SOURCE, src, mask,
			extents->bounded.x + src_x, extents->bounded.y + src_y,
			0, 0,
			extents->bounded.x,	  extents->bounded.y,
			extents->bounded.width,  extents->bounded.height);
	} else {
		/* Compute dest' = dest OUT (mask IN clip) */
		compositor->composite (dst, GRAPHICS_OPERATOR_DEST_OUT, mask, NULL,
			0, 0, 0, 0,
			extents->bounded.x,	 extents->bounded.y,
			extents->bounded.width, extents->bounded.height);

		/* Now compute (src IN (mask IN clip)) ADD dest' */
		compositor->composite (dst, GRAPHICS_OPERATOR_ADD, src, mask,
			extents->bounded.x + src_x, extents->bounded.y + src_y,
			0, 0,
			extents->bounded.x,	 extents->bounded.y,
			extents->bounded.width, extents->bounded.height);
	}

	DestroyGraphicsSurface(src);
	DestroyGraphicsSurface(mask);

	return GRAPHICS_STATUS_SUCCESS;
}

static int can_reduce_alpha_op (eGRAPHICS_OPERATOR op)
{
	int iop = op;
	switch (iop)
	{
	case GRAPHICS_OPERATOR_OVER:
	case GRAPHICS_OPERATOR_SOURCE:
	case GRAPHICS_OPERATOR_ADD:
		return TRUE;
	default:
		return FALSE;
	}
}

static int reduce_alpha_op(
	GRAPHICS_SURFACE *dst,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN *pattern
)
{
	return dst->is_clear &&
		dst->content == GRAPHICS_CONTENT_ALPHA &&
		GraphicsPatternIsOpaqueSolid(pattern) &&
		can_reduce_alpha_op (op);
}

static eGRAPHICS_STATUS fixup_unbounded(
	const GRAPHICS_MASK_COMPOSITOR *compositor,
	GRAPHICS_SURFACE *dst,
	const GRAPHICS_COMPOSITE_RECTANGLES *extents
)
{
	GRAPHICS *graphics;
	GRAPHICS_RECTANGLE_INT rects[4];
	int n;

	graphics = (GRAPHICS*)compositor->base.graphics;

	if (extents->bounded.width  == extents->unbounded.width &&
		extents->bounded.height == extents->unbounded.height)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	n = 0;
	if (extents->bounded.width == 0 || extents->bounded.height == 0) {
		rects[n].x = extents->unbounded.x;
		rects[n].width = extents->unbounded.width;
		rects[n].y = extents->unbounded.y;
		rects[n].height = extents->unbounded.height;
		n++;
	} else {
		/* top */
		if (extents->bounded.y != extents->unbounded.y) {
			rects[n].x = extents->unbounded.x;
			rects[n].width = extents->unbounded.width;
			rects[n].y = extents->unbounded.y;
			rects[n].height = extents->bounded.y - extents->unbounded.y;
			n++;
		}
		/* left */
		if (extents->bounded.x != extents->unbounded.x) {
			rects[n].x = extents->unbounded.x;
			rects[n].width = extents->bounded.x - extents->unbounded.x;
			rects[n].y = extents->bounded.y;
			rects[n].height = extents->bounded.height;
			n++;
		}
		/* right */
		if (extents->bounded.x + extents->bounded.width != extents->unbounded.x + extents->unbounded.width) {
			rects[n].x = extents->bounded.x + extents->bounded.width;
			rects[n].width = extents->unbounded.x + extents->unbounded.width - rects[n].x;
			rects[n].y = extents->bounded.y;
			rects[n].height = extents->bounded.height;
			n++;
		}
		/* bottom */
		if (extents->bounded.y + extents->bounded.height != extents->unbounded.y + extents->unbounded.height) {
			rects[n].x = extents->unbounded.x;
			rects[n].width = extents->unbounded.width;
			rects[n].y = extents->bounded.y + extents->bounded.height;
			rects[n].height = extents->unbounded.y + extents->unbounded.height - rects[n].y;
			n++;
		}
	}

	return compositor->fill_rectangles (dst, GRAPHICS_OPERATOR_CLEAR,
		&graphics->color_transparent,
		rects, n);
}

static eGRAPHICS_STATUS fixup_unbounded_with_mask (
	const GRAPHICS_MASK_COMPOSITOR *compositor,
	GRAPHICS_SURFACE *dst,
	const GRAPHICS_COMPOSITE_RECTANGLES *extents
)
{
	GRAPHICS_SURFACE *mask;
	int mask_x, mask_y;

	mask = get_clip_source (compositor,
		extents->clip, dst, &extents->unbounded,
		&mask_x, &mask_y);
	if (UNLIKELY(mask->status))
		return mask->status;

	/* top */
	if (extents->bounded.y != extents->unbounded.y) {
		int x = extents->unbounded.x;
		int y = extents->unbounded.y;
		int width = extents->unbounded.width;
		int height = extents->bounded.y - y;

		compositor->composite (dst, GRAPHICS_OPERATOR_DEST_OUT, mask, NULL,
			x + mask_x, y + mask_y,
			0, 0,
			x, y,
			width, height);
	}

	/* left */
	if (extents->bounded.x != extents->unbounded.x) {
		int x = extents->unbounded.x;
		int y = extents->bounded.y;
		int width = extents->bounded.x - x;
		int height = extents->bounded.height;

		compositor->composite (dst, GRAPHICS_OPERATOR_DEST_OUT, mask, NULL,
			x + mask_x, y + mask_y,
			0, 0,
			x, y,
			width, height);
	}

	/* right */
	if (extents->bounded.x + extents->bounded.width != extents->unbounded.x + extents->unbounded.width) {
		int x = extents->bounded.x + extents->bounded.width;
		int y = extents->bounded.y;
		int width = extents->unbounded.x + extents->unbounded.width - x;
		int height = extents->bounded.height;

		compositor->composite (dst, GRAPHICS_OPERATOR_DEST_OUT, mask, NULL,
			x + mask_x, y + mask_y,
			0, 0,
			x, y,
			width, height);
	}

	/* bottom */
	if (extents->bounded.y + extents->bounded.height != extents->unbounded.y + extents->unbounded.height) {
		int x = extents->unbounded.x;
		int y = extents->bounded.y + extents->bounded.height;
		int width = extents->unbounded.width;
		int height = extents->unbounded.y + extents->unbounded.height - y;

		compositor->composite (dst, GRAPHICS_OPERATOR_DEST_OUT, mask, NULL,
			x + mask_x, y + mask_y,
			0, 0,
			x, y,
			width, height);
	}

	DestroyGraphicsSurface(mask);

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS fixup_unbounded_boxes(
	const GRAPHICS_MASK_COMPOSITOR *compositor,
	const GRAPHICS_COMPOSITE_RECTANGLES *extents,
	GRAPHICS_BOXES *boxes
)
{
	GRAPHICS *graphics;
	GRAPHICS_SURFACE *dst = extents->surface;
	GRAPHICS_BOXES clear;
	GRAPHICS_REGION *clip_region;
	GRAPHICS_BOX box;
	eGRAPHICS_STATUS status;
	GRAPHICS_BOXES_CHUNK *chunk;
	int i;

	ASSERT(boxes->is_pixel_aligned);

	graphics = (GRAPHICS*)compositor->base.graphics;

	clip_region = NULL;
	if (GraphicsClipIsRegion(extents->clip) &&
		(clip_region = extents->clip->region) &&
		GraphicsRegionContainsRectangle(clip_region,
			&extents->bounded) == GRAPHICS_REGION_OVERLAP_IN)
		clip_region = NULL;


	if (boxes->num_boxes <= 1 && clip_region == NULL)
		return fixup_unbounded (compositor, dst, extents);

	InitializeGraphicsBoxes(&clear);
	
	box.point1.x = GraphicsFixedFromInteger(extents->unbounded.x + extents->unbounded.width);
	box.point1.y = GraphicsFixedFromInteger(extents->unbounded.y);
	box.point2.x = GraphicsFixedFromInteger(extents->unbounded.x);
	box.point2.y = GraphicsFixedFromInteger(extents->unbounded.y + extents->unbounded.height);

	if(clip_region == NULL)
	{
		GRAPHICS_BOXES tmp;

		InitializeGraphicsBoxes(&tmp);

		status = GraphicsBoxesAdd(&tmp, GRAPHICS_ANTIALIAS_DEFAULT, &box);
		ASSERT(status == GRAPHICS_STATUS_SUCCESS);

		tmp.chunks.next = &boxes->chunks;
		tmp.num_boxes += boxes->num_boxes;
		
		status = GraphicsBentleyOttmannTessellateBoxes(&tmp,
			GRAPHICS_FILL_RULE_WINDING,
			&clear);

		tmp.chunks.next = NULL;
	}
	else
	{
		PIXEL_MANIPULATE_BOX32 *pbox;
		
		pbox = PixelManipulateRegion32Rectangles(&clip_region->region, &i);
		GraphicsBoxesLimit(&clear, (GRAPHICS_BOX *) pbox, i);

		status = GraphicsBoxesAdd(&clear, GRAPHICS_ANTIALIAS_DEFAULT, &box);
		ASSERT(status == GRAPHICS_STATUS_SUCCESS);

		for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
			for (i = 0; i < chunk->count; i++) {
				status = GraphicsBoxesAdd(&clear,
					GRAPHICS_ANTIALIAS_DEFAULT,
					&chunk->base[i]);
				if (UNLIKELY(status)) {
					GraphicsBoxesFinish(&clear);
					return status;
				}
			}
		}

		status = GraphicsBentleyOttmannTessellateBoxes(&clear,
			GRAPHICS_FILL_RULE_WINDING,
			&clear);
	}

	if (LIKELY(status == GRAPHICS_STATUS_SUCCESS)) {
		status = compositor->fill_boxes (dst,
			GRAPHICS_OPERATOR_CLEAR,
			&graphics->color_transparent,
			&clear);
	}

	GraphicsBoxesFinish(&clear);

	return status;
}

enum {
	NEED_CLIP_REGION = 0x1,
	NEED_CLIP_SURFACE = 0x2,
	FORCE_CLIP_REGION = 0x4,
};

static int need_bounded_clip (GRAPHICS_COMPOSITE_RECTANGLES *extents)
{
	unsigned int flags = NEED_CLIP_REGION;
	if (! GraphicsClipIsRegion(extents->clip))
		flags |= NEED_CLIP_SURFACE;
	return flags;
}

static int need_unbounded_clip (GRAPHICS_COMPOSITE_RECTANGLES *extents)
{
	unsigned int flags = 0;
	if (! extents->is_bounded) {
		flags |= NEED_CLIP_REGION;
		if (! GraphicsClipIsRegion(extents->clip))
			flags |= NEED_CLIP_SURFACE;
	}
	if (extents->clip->path != NULL)
		flags |= NEED_CLIP_SURFACE;
	return flags;
}

static eGRAPHICS_STATUS clip_and_composite(
	const GRAPHICS_MASK_COMPOSITOR *compositor,
	draw_func_t			 draw_func,
	draw_func_t			 mask_func,
	void			*draw_closure,
	GRAPHICS_COMPOSITE_RECTANGLES*extents,
	unsigned int need_clip
)
{
	GRAPHICS_SURFACE *dst = extents->surface;
	eGRAPHICS_OPERATOR op = extents->op;
	GRAPHICS_PATTERN *src = &extents->source_pattern.base;
	GRAPHICS_REGION *clip_region = NULL;
	eGRAPHICS_STATUS status;

	compositor->acquire (dst);

	if (need_clip & NEED_CLIP_REGION)
	{
		clip_region = extents->clip->region; // _cairo_clip_get_region (extents->clip);
		if((need_clip & FORCE_CLIP_REGION) == 0 &&
			GraphicsCompositeRectanglesCanReduceClip(extents,
				extents->clip))
			clip_region = NULL;
		if(clip_region != NULL)
		{
			status = compositor->set_clip_region(dst, clip_region);
			if(UNLIKELY(status))
			{
				compositor->release(dst);
				return status;
			}
		}
	}

	if (reduce_alpha_op (dst, op, &extents->source_pattern.base)) {
		op = GRAPHICS_OPERATOR_ADD;
		src = NULL;
	}

	if (op == GRAPHICS_OPERATOR_SOURCE) {
		status = clip_and_composite_source (compositor,
			draw_closure, draw_func, mask_func,
			src, extents);
	} else {
		if (op == GRAPHICS_OPERATOR_CLEAR) {
			op = GRAPHICS_OPERATOR_DEST_OUT;
			src = NULL;
		}

		if (need_clip & NEED_CLIP_SURFACE) {
			if (extents->is_bounded) {
				status = clip_and_composite_with_mask (compositor,
					draw_closure,
					draw_func,
					mask_func,
					op, src, extents);
			} else {
				status = clip_and_composite_combine (compositor,
					draw_closure,
					draw_func,
					op, src, extents);
			}
		} else {
			status = draw_func (compositor,
				dst, draw_closure,
				op, src, &extents->source_sample_area,
				0, 0,
				&extents->bounded,
				extents->clip);
		}
	}

	if (status == GRAPHICS_STATUS_SUCCESS && ! extents->is_bounded) {
		if (need_clip & NEED_CLIP_SURFACE)
			status = fixup_unbounded_with_mask (compositor, dst, extents);
		else
			status = fixup_unbounded (compositor, dst, extents);
	}

	if (clip_region)
		compositor->set_clip_region (dst, NULL);

	compositor->release (dst);

	return status;
}

static eGRAPHICS_INTEGER_STATUS
trim_extents_to_boxes (GRAPHICS_COMPOSITE_RECTANGLES *extents,
	GRAPHICS_BOXES *boxes)
{
	GRAPHICS_BOX box;
	
	GraphicsBoxesExtents(boxes, &box);
	return GraphicsCompositeRectanglesIntersectMaskExtents(extents, &box);
}

static eGRAPHICS_STATUS upload_boxes(
	const GRAPHICS_MASK_COMPOSITOR *compositor,
	GRAPHICS_COMPOSITE_RECTANGLES *extents,
	GRAPHICS_BOXES *boxes
)
{
	GRAPHICS_SURFACE *dst = extents->surface;
	const GRAPHICS_PATTERN *source = &extents->source_pattern.base;
	GRAPHICS_SURFACE *src;
	GRAPHICS_RECTANGLE_INT limit;
	eGRAPHICS_INTEGER_STATUS status;
	int tx, ty;
	
	src = GraphicsPatternGetSource((GRAPHICS_SURFACE_PATTERN *)source, &limit);
	if (!(src->type == GRAPHICS_SURFACE_TYPE_IMAGE || src->type == dst->type))
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;

	if (! GraphicsMatrixIsIntegerTranslation(&source->matrix, &tx, &ty))
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;

	/* Check that the data is entirely within the image */
	if (extents->bounded.x + tx < limit.x || extents->bounded.y + ty < limit.y)
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;

	if (extents->bounded.x + extents->bounded.width  + tx > limit.x + limit.width ||
		extents->bounded.y + extents->bounded.height + ty > limit.y + limit.height)
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;

	tx += limit.x;
	ty += limit.y;

	if (src->type == GRAPHICS_SURFACE_TYPE_IMAGE)
		status = compositor->draw_image_boxes (dst,
		(GRAPHICS_IMAGE_SURFACE *)src,
			boxes, tx, ty);
	else
		status = compositor->copy_boxes (dst, src, boxes, &extents->bounded,
			tx, ty);

	return status;
}

static eGRAPHICS_STATUS composite_boxes(
	const GRAPHICS_MASK_COMPOSITOR *compositor,
	const GRAPHICS_COMPOSITE_RECTANGLES *extents,
	GRAPHICS_BOXES *boxes
)
{
	GRAPHICS_SURFACE *dst = extents->surface;
	eGRAPHICS_OPERATOR op = extents->op;
	const GRAPHICS_PATTERN *source = &extents->source_pattern.base;
	int need_clip_mask = extents->clip->path != NULL;
	eGRAPHICS_STATUS status;

	if (need_clip_mask &&
		(! extents->is_bounded || op == GRAPHICS_OPERATOR_SOURCE))
	{
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	}

	status = compositor->acquire (dst);
	if (UNLIKELY(status))
		return status;

	if (! need_clip_mask && source->type == GRAPHICS_PATTERN_TYPE_SOLID) {
		const GRAPHICS_COLOR *color;

		color = &((GRAPHICS_SOLID_PATTERN*)source)->color;
		status = compositor->fill_boxes (dst, op, color, boxes);
	} else {
		GRAPHICS_SURFACE *src, *mask = NULL;
		int src_x, src_y;
		int mask_x = 0, mask_y = 0;

		if (need_clip_mask) {
			mask = get_clip_source (compositor,
				extents->clip, dst, &extents->bounded,
				&mask_x, &mask_y);
			if (UNLIKELY(mask->status))
				return mask->status;

			if (op == GRAPHICS_OPERATOR_CLEAR) {
				source = NULL;
				op = GRAPHICS_OPERATOR_DEST_OUT;
			}
		}

		if (source || mask == NULL) {
			src = compositor->pattern_to_surface (dst, source, FALSE,
				&extents->bounded,
				&extents->source_sample_area,
				&src_x, &src_y);
		} else {
			src = mask;
			src_x = mask_x;
			src_y = mask_y;
			mask = NULL;
		}

		status = compositor->composite_boxes (dst, op, src, mask,
			src_x, src_y,
			mask_x, mask_y,
			0, 0,
			boxes, &extents->bounded);

		DestroyGraphicsSurface(src);
		DestroyGraphicsSurface(mask);
	}

	if (status == GRAPHICS_STATUS_SUCCESS && ! extents->is_bounded)
		status = fixup_unbounded_boxes (compositor, extents, boxes);

	compositor->release (dst);

	return status;
}

static eGRAPHICS_STATUS clip_and_composite_boxes(
	const GRAPHICS_MASK_COMPOSITOR *compositor,
	GRAPHICS_COMPOSITE_RECTANGLES *extents,
	GRAPHICS_BOXES *boxes
)
{
	GRAPHICS_SURFACE *dst = extents->surface;
	eGRAPHICS_INTEGER_STATUS status;

	if (boxes->num_boxes == 0) {
		if (extents->is_bounded)
			return GRAPHICS_STATUS_SUCCESS;

		return fixup_unbounded_boxes (compositor, extents, boxes);
	}

	if (! boxes->is_pixel_aligned)
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;

	status = trim_extents_to_boxes (extents, boxes);
	if (UNLIKELY(status))
		return status;

	if (extents->source_pattern.base.type == GRAPHICS_PATTERN_TYPE_SURFACE &&
		extents->clip->path == NULL &&
		(extents->op == GRAPHICS_OPERATOR_SOURCE ||
		(dst->is_clear && (extents->op == GRAPHICS_OPERATOR_OVER ||
			extents->op == GRAPHICS_OPERATOR_ADD))))
	{
		status = upload_boxes (compositor, extents, boxes);
		if (status != GRAPHICS_INTEGER_STATUS_UNSUPPORTED)
			return status;
	}

	return composite_boxes (compositor, extents, boxes);
}

/* high-level compositor interface */

static eGRAPHICS_INTEGER_STATUS GraphicsMaskCompositorPaint(
	const GRAPHICS_COMPOSITOR *_compositor,
	GRAPHICS_COMPOSITE_RECTANGLES *extents
)
{
	GRAPHICS_MASK_COMPOSITOR *compositor = (GRAPHICS_MASK_COMPOSITOR*)_compositor;
	GRAPHICS_BOXES boxes;
	eGRAPHICS_INTEGER_STATUS status;

	status = compositor->check_composite (extents);
	if (UNLIKELY(status))
		return status;
	
	GraphicsClipStealBoxes(extents->clip, &boxes);
	status = clip_and_composite_boxes (compositor, extents, &boxes);
	GraphicsClipUnstealBoxes(extents->clip, &boxes);

	return status;
}

struct composite_opacity_info {
	const GRAPHICS_MASK_COMPOSITOR *compositor;
	uint8 op;
	GRAPHICS_SURFACE *dst;
	GRAPHICS_SURFACE *src;
	int src_x, src_y;
	double opacity;
};

static void composite_opacity(
	void *closure,
	int16 x, int16 y,
	int16 w, int16 h,
	uint16 coverage
)
{
	GRAPHICS *graphics;
	struct composite_opacity_info *info = closure;
	const GRAPHICS_MASK_COMPOSITOR *compositor = info->compositor;
	GRAPHICS_SURFACE *mask;
	int mask_x, mask_y;
	GRAPHICS_COLOR color;
	GRAPHICS_SOLID_PATTERN solid;

	graphics = compositor->base.graphics;
	InitializeGraphicsColorRGBA(&color, 0, 0, 0, info->opacity * coverage);
	InitializeGraphicsPatternSolid(&solid, &color, graphics);
	mask = compositor->pattern_to_surface (info->dst, &solid.base, TRUE,
		&graphics->unbounded_rectangle,
		&graphics->unbounded_rectangle,
		&mask_x, &mask_y);
	if (LIKELY(mask->status == GRAPHICS_STATUS_SUCCESS)) {
		if (info->src) {
			compositor->composite (info->dst, info->op, info->src, mask,
				x + info->src_x,  y + info->src_y,
				mask_x,		   mask_y,
				x,				y,
				w,				h);
		} else {
			compositor->composite (info->dst, info->op, mask, NULL,
				mask_x,			mask_y,
				0,				 0,
				x,				 y,
				w,				 h);
		}
	}

	DestroyGraphicsSurface(mask);
}

static eGRAPHICS_INTEGER_STATUS composite_opacity_boxes(
	const GRAPHICS_MASK_COMPOSITOR *compositor,
	GRAPHICS_SURFACE		*dst,
	void				*closure,
	eGRAPHICS_OPERATOR		 op,
	const GRAPHICS_PATTERN		*src_pattern,
	const GRAPHICS_RECTANGLE_INT	*src_sample,
	int				 dst_x,
	int				 dst_y,
	const GRAPHICS_RECTANGLE_INT	*extents,
	GRAPHICS_CLIP			*clip
)
{
	const GRAPHICS_SOLID_PATTERN *mask_pattern = closure;
	struct composite_opacity_info info;
	int i;

	ASSERT(clip);

	info.compositor = compositor;
	info.op = op;
	info.dst = dst;

	if (src_pattern != NULL) {
		info.src = compositor->pattern_to_surface (dst, src_pattern, FALSE,
			extents, src_sample,
			&info.src_x, &info.src_y);
		if (UNLIKELY(info.src->status))
			return info.src->status;
	} else
		info.src = NULL;

	info.opacity = mask_pattern->color.alpha / (double) 0xffff;

	/* XXX for lots of boxes create a clip region for the fully opaque areas */
	for (i = 0; i < clip->num_boxes; i++)
		do_unaligned_box(composite_opacity, &info,
			&clip->boxes[i], dst_x, dst_y);
	DestroyGraphicsSurface(info.src);

	return GRAPHICS_STATUS_SUCCESS;
}

struct composite_box_info {
	const GRAPHICS_MASK_COMPOSITOR *compositor;
	GRAPHICS_SURFACE *dst;
	GRAPHICS_SURFACE *src;
	int src_x, src_y;
	uint8 op;
};

static void composite_box(
	void *closure,
	int16 x, int16 y,
	int16 w, int16 h,
	uint16 coverage
)
{
	GRAPHICS *graphics;
	struct composite_box_info *info = closure;
	const GRAPHICS_MASK_COMPOSITOR *compositor = info->compositor;

	graphics = (GRAPHICS*)compositor->base.graphics	;

	if (! GRAPHICS_ALPHA_SHORT_IS_OPAQUE(coverage))
	{
		GRAPHICS_SURFACE *mask;
		GRAPHICS_COLOR color;
		GRAPHICS_SOLID_PATTERN solid;
		int mask_x, mask_y;

		InitializeGraphicsColorRGBA(&color, 0, 0, 0, coverage / (double)0xffff);
		InitializeGraphicsPatternSolid(&solid, &color, graphics);

		mask = compositor->pattern_to_surface (info->dst, &solid.base, FALSE,
			&graphics->unbounded_rectangle,
			&graphics->unbounded_rectangle,
			&mask_x, &mask_y);

		if (LIKELY(mask->status == GRAPHICS_STATUS_SUCCESS)) {
			compositor->composite (info->dst, info->op, info->src, mask,
				x + info->src_x,  y + info->src_y,
				mask_x,		   mask_y,
				x,				y,
				w,				h);
		}

		DestroyGraphicsSurface(mask);
	} else {
		compositor->composite (info->dst, info->op, info->src, NULL,
			x + info->src_x,  y + info->src_y,
			0,				0,
			x,				y,
			w,				h);
	}
}

static eGRAPHICS_INTEGER_STATUS
composite_mask_clip_boxes (const GRAPHICS_MASK_COMPOSITOR *compositor,
	GRAPHICS_SURFACE		*dst,
	void				*closure,
	eGRAPHICS_OPERATOR		 op,
	const GRAPHICS_PATTERN	*src_pattern,
	const GRAPHICS_RECTANGLE_INT	*src_sample,
	int				 dst_x,
	int				 dst_y,
	const GRAPHICS_RECTANGLE_INT	*extents,
	GRAPHICS_CLIP			*clip)
{
	GRAPHICS_COMPOSITE_RECTANGLES *composite = closure;
	struct composite_box_info info;
	int i;

	ASSERT(src_pattern == NULL);
	ASSERT(op == GRAPHICS_OPERATOR_SOURCE);

	info.compositor = compositor;
	info.op = GRAPHICS_OPERATOR_SOURCE;
	info.dst = dst;
	info.src = compositor->pattern_to_surface (dst,
		&composite->mask_pattern.base,
		FALSE, extents,
		&composite->mask_sample_area,
		&info.src_x, &info.src_y);
	if (UNLIKELY(info.src->status))
		return info.src->status;

	info.src_x += dst_x;
	info.src_y += dst_y;

	for (i = 0; i < clip->num_boxes; i++)
		do_unaligned_box(composite_box, &info, &clip->boxes[i], dst_x, dst_y);

	DestroyGraphicsSurface(info.src);

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_INTEGER_STATUS composite_mask(
	const GRAPHICS_MASK_COMPOSITOR *compositor,
	GRAPHICS_SURFACE			*dst,
	void				*closure,
	eGRAPHICS_OPERATOR		 op,
	const GRAPHICS_PATTERN		*src_pattern,
	const GRAPHICS_RECTANGLE_INT	*src_sample,
	int				 dst_x,
	int				 dst_y,
	const GRAPHICS_RECTANGLE_INT	*extents,
	GRAPHICS_CLIP			*clip
)
{
	GRAPHICS_COMPOSITE_RECTANGLES *composite = closure;
	GRAPHICS_SURFACE *src, *mask;
	int src_x, src_y;
	int mask_x, mask_y;

	if (src_pattern != NULL) {
		src = compositor->pattern_to_surface (dst, src_pattern, FALSE,
			extents, src_sample,
			&src_x, &src_y);
		if (UNLIKELY(src->status))
			return src->status;

		mask = compositor->pattern_to_surface (dst, &composite->mask_pattern.base, TRUE,
			extents, &composite->mask_sample_area,
			&mask_x, &mask_y);
		if (UNLIKELY(mask->status)) {
			DestroyGraphicsSurface(src);
			return mask->status;
		}

		compositor->composite (dst, op, src, mask,
			extents->x + src_x,  extents->y + src_y,
			extents->x + mask_x, extents->y + mask_y,
			extents->x - dst_x,  extents->y - dst_y,
			extents->width,	  extents->height);

		DestroyGraphicsSurface(mask);
		DestroyGraphicsSurface(src);
	} else {
		src = compositor->pattern_to_surface (dst, &composite->mask_pattern.base, FALSE,
			extents, &composite->mask_sample_area,
			&src_x, &src_y);
		if (UNLIKELY(src->status))
			return src->status;

		compositor->composite (dst, op, src, NULL,
			extents->x + src_x,  extents->y + src_y,
			0, 0,
			extents->x - dst_x,  extents->y - dst_y,
			extents->width,	  extents->height);

		DestroyGraphicsSurface(src);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_INTEGER_STATUS GraphicsMaskCompositorMask(
	const GRAPHICS_COMPOSITOR *_compositor,
	GRAPHICS_COMPOSITE_RECTANGLES *extents
)
{
	const GRAPHICS_MASK_COMPOSITOR *compositor = (GRAPHICS_MASK_COMPOSITOR*)_compositor;
	eGRAPHICS_INTEGER_STATUS status = GRAPHICS_INTEGER_STATUS_UNSUPPORTED;

	status = compositor->check_composite (extents);
	if (UNLIKELY(status))
		return status;

	if (extents->mask_pattern.base.type == GRAPHICS_PATTERN_TYPE_SOLID &&
		extents->clip->path == NULL &&
		GraphicsClipIsRegion(extents->clip)) {
		status = clip_and_composite (compositor,
			composite_opacity_boxes,
			composite_opacity_boxes,
			&extents->mask_pattern.solid,
			extents, need_unbounded_clip (extents));
	} else {
		status = clip_and_composite (compositor,
			composite_mask,
			extents->clip->path == NULL ? composite_mask_clip_boxes : NULL,
			extents,
			extents, need_bounded_clip (extents));
	}

	return status;
}

static eGRAPHICS_INTEGER_STATUS GraphicsMaskCompositorStroke(
	const GRAPHICS_COMPOSITOR *_compositor,
	GRAPHICS_COMPOSITE_RECTANGLES *extents,
	const GRAPHICS_PATH_FIXED	*path,
	const GRAPHICS_STROKE_STYLE	*style,
	const GRAPHICS_MATRIX	*ctm,
	const GRAPHICS_MATRIX	*ctm_inverse,
	double		 tolerance,
	eGRAPHICS_ANTIALIAS	 antialias
)
{
	GRAPHICS *graphics;
	const GRAPHICS_MASK_COMPOSITOR *compositor = (GRAPHICS_MASK_COMPOSITOR*)_compositor;
	GRAPHICS_SURFACE *mask;
	GRAPHICS_SURFACE_PATTERN pattern;
	eGRAPHICS_INTEGER_STATUS status = GRAPHICS_INTEGER_STATUS_UNSUPPORTED;

	graphics = (GRAPHICS*)compositor->base.graphics;

	status = compositor->check_composite (extents);
	if(UNLIKELY(status))
		return status;
	
	if(path->stroke_is_rectilinear)
	{
		GRAPHICS_BOXES boxes;

		InitializeGraphicsBoxesWithClip(&boxes, extents->clip);
		status = GraphicsPathFixedStrokeRectilinearToBoxes(path,
			style,
			ctm,
			antialias,
			&boxes);
		if (LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
			status = clip_and_composite_boxes (compositor, extents, &boxes);
		GraphicsBoxesFinish(&boxes);
	}

	if (status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED) {
		mask = GraphicsSurfaceCreateSimilarImage(extents->surface,
			GRAPHICS_FORMAT_A8,
			extents->bounded.width,
			extents->bounded.height);
		if (UNLIKELY(mask->status))
			return mask->status;
		
		status = GraphicsSurfaceOffsetStroke(mask,
			extents->bounded.x,
			extents->bounded.y,
			GRAPHICS_OPERATOR_ADD,
			&graphics->white_pattern.base,
			path, style, ctm, ctm_inverse,
			tolerance, antialias,
			extents->clip);
		if (UNLIKELY(status)) {
			DestroyGraphicsSurface(mask);
			return status;
		}

		InitializeGraphicsPatternForSurface(&pattern, mask);
		DestroyGraphicsSurface(mask);

		InitializeGraphicsMatrixTranslate(&pattern.base.matrix,
			-extents->bounded.x,
			-extents->bounded.y);
		pattern.base.filter = GRAPHICS_FILTER_NEAREST;
		pattern.base.extend = GRAPHICS_EXTEND_NONE;
		status = GraphicsSurfaceMask(extents->surface,
			extents->op,
			&extents->source_pattern.base,
			&pattern.base,
			extents->clip);
		GraphicsPatternFinish(&pattern.base);
	}

	return status;
}

static eGRAPHICS_INTEGER_STATUS GraphicsMaskCompositorFill(
	const GRAPHICS_COMPOSITOR *_compositor,
	GRAPHICS_COMPOSITE_RECTANGLES *extents,
	const GRAPHICS_PATH_FIXED	*path,
	eGRAPHICS_FILL_RULE	 fill_rule,
	double			 tolerance,
	eGRAPHICS_ANTIALIAS	 antialias
)
{
	GRAPHICS *graphics;
	const GRAPHICS_MASK_COMPOSITOR *compositor = (GRAPHICS_MASK_COMPOSITOR*)_compositor;
	GRAPHICS_SURFACE *mask;
	GRAPHICS_SURFACE_PATTERN pattern;
	eGRAPHICS_INTEGER_STATUS status = GRAPHICS_INTEGER_STATUS_UNSUPPORTED;

	graphics = (GRAPHICS*)compositor->base.graphics;

	status = compositor->check_composite (extents);
	if (UNLIKELY(status))
		return status;
	
	if(path->fill_is_rectilinear)
	{
		GRAPHICS_BOXES boxes;
		
		InitializeGraphicsBoxesWithClip(&boxes, extents->clip);
		status = GraphicsPathFixedFillRectilinearToBoxes(path,
			fill_rule,
			antialias,
			&boxes);
		if (LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
			status = clip_and_composite_boxes (compositor, extents, &boxes);
		GraphicsBoxesFinish(&boxes);
	}

	if (status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED)
	{
		mask = GraphicsSurfaceCreateSimilarImage(extents->surface,
			GRAPHICS_FORMAT_A8,
			extents->bounded.width,
			extents->bounded.height);
		if (UNLIKELY(mask->status))
			return mask->status;
		
		status = GraphicsSurfaceOffsetFill(mask,
			extents->bounded.x,
			extents->bounded.y,
			GRAPHICS_OPERATOR_ADD,
			&graphics->white_pattern.base,
			path, fill_rule, tolerance, antialias,
			extents->clip);
		if (UNLIKELY(status)) {
			DestroyGraphicsSurface(mask);
			return status;
		}
		
		InitializeGraphicsPatternForSurface(&pattern, mask);
		DestroyGraphicsSurface(mask);

		InitializeGraphicsMatrixTranslate(&pattern.base.matrix,
			-extents->bounded.x,
			-extents->bounded.y);
		pattern.base.filter = GRAPHICS_FILTER_NEAREST;
		pattern.base.extend = GRAPHICS_EXTEND_NONE;
		status = GraphicsSurfaceMask(extents->surface,
			extents->op,
			&extents->source_pattern.base,
			&pattern.base,
			extents->clip);
		GraphicsPatternFinish(&pattern.base);
	}

	return status;
}

#if 0
static eGRAPHICS_INTEGER_STATUS
_cairo_mask_compositor_glyphs (const GRAPHICS_COMPOSITOR *_compositor,
	GRAPHICS_COMPOSITE_RECTANGLES *extents,
	cairo_scaled_font_t	*scaled_font,
	cairo_glyph_t		*glyphs,
	int			 num_glyphs,
	int		 overlap)
{
	const GRAPHICS_MASK_COMPOSITOR *compositor = (GRAPHICS_MASK_COMPOSITOR*)_compositor;
	GRAPHICS_SURFACE *mask;
	GRAPHICS_SURFACE_PATTERN pattern;
	eGRAPHICS_INTEGER_STATUS status;

	status = compositor->check_composite (extents);
	if (UNLIKELY(status))
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;

	mask = GraphicsSurfaceCreateSimilarImage(extents->surface,
		CAIRO_FORMAT_A8,
		extents->bounded.width,
		extents->bounded.height);
	if (UNLIKELY(mask->status))
		return mask->status;

	status = _cairo_surface_offset_glyphs (mask,
		extents->bounded.x,
		extents->bounded.y,
		GRAPHICS_OPERATOR_ADD,
		&_cairo_pattern_white.base,
		scaled_font, glyphs, num_glyphs,
		extents->clip);
	if (UNLIKELY(status)) {
		DestroyGraphicsSurface(mask);
		return status;
	}

	InitializeGraphicsPatternForSurface(&pattern, mask);
	DestroyGraphicsSurface(mask);
	
	InitializeGraphicsMatrixTranslate(&pattern.base.matrix,
		-extents->bounded.x,
		-extents->bounded.y);
	pattern.base.filter = GRAPHICS_FILTER_NEAREST;
	pattern.base.extend = CAIRO_EXTEND_NONE;
	status = GraphicsSurfaceMask(extents->surface,
		extents->op,
		&extents->source_pattern.base,
		&pattern.base,
		extents->clip);
	GraphicsPatternFinish(&pattern.base);

	return status;
}
#endif

void InitializeGraphicsMaskCompositor(
	GRAPHICS_MASK_COMPOSITOR *compositor,
	const GRAPHICS_COMPOSITOR *delegate
)
{
	compositor->base.delegate = delegate;

	compositor->base.paint = GraphicsMaskCompositorPaint;
	compositor->base.mask  = GraphicsMaskCompositorMask;
	compositor->base.fill  = GraphicsMaskCompositorFill;
	compositor->base.stroke = GraphicsMaskCompositorStroke;
	// compositor->base.glyphs = _cairo_mask_compositor_glyphs;
}

#ifdef __cplusplus
}
#endif
