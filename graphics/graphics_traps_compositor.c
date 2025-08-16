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
	const GRAPHICS_TRAPS_COMPOSITOR* compositor,
	GRAPHICS_SURFACE* dst,
	void* closure,
	eGRAPHICS_OPERATOR op,
	GRAPHICS_SURFACE* src,
	int src_x,
	int src_y,
	int dst_x,
	int dst_y,
	const GRAPHICS_RECTANGLE_INT* extents,
	GRAPHICS_CLIP* clip
);

static void do_unaligned_row(void (*blt)(
	void *closure,
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
	if(x2 > x1)
	{
		if(! GRAPHICS_FIXED_IS_INTEGER(b->point1.x))
		{
			blt(closure, x1, y, 1, h,
				coverage * (256 - GraphicsFixedFractionalPart(b->point1.x)));
			x1++;
		}

		if(x2 > x1)
		{
			blt(closure, x1, y, x2 - x1, h, (coverage << 8) - (coverage >> 8));
		}

		if(!GRAPHICS_FIXED_IS_INTEGER(b->point2.x))
		{
			blt(closure, x2, y, 1, h,
				coverage * GraphicsFixedFractionalPart(b->point2.x));
		}
	}
	else
	{
		blt(closure, x1, y, 1, h,
			coverage * (b->point2.x - b->point1.x));
	}
}

static void do_unaligned_box(void (*blt)(
	void *closure,
	int16 x, int16 y,
	int16 w, int16 h,
	uint16 coverage),
	void *closure,
	const GRAPHICS_BOX *b, int tx, int ty
)
{
	int y1 = GraphicsFixedIntegerPart(b->point1.y) - ty;
	int y2 = GraphicsFixedIntegerPart(b->point2.y) - ty;
	if(y2 > y1)
	{
		if(! GRAPHICS_FIXED_IS_INTEGER(b->point1.y))
		{
			do_unaligned_row(blt, closure, b, tx, y1, 1,
				256 - GraphicsFixedFractionalPart(b->point1.y));
			y1++;
		}

		if(y2 > y1)
		{
			do_unaligned_row(blt, closure, b, tx, y1, y2 - y1, 256);
		}

		if(!GRAPHICS_FIXED_IS_INTEGER(b->point2.y))
		{
			do_unaligned_row(blt, closure, b, tx, y2, 1,
				GraphicsFixedFractionalPart(b->point2.y));
		}
	}
	else
	{
		do_unaligned_row(blt, closure, b, tx, y1, 1,
			b->point2.y - b->point1.y);
	}
}

struct blt_in
{
	const GRAPHICS_TRAPS_COMPOSITOR *compositor;
	GRAPHICS_SURFACE *dst;
	GRAPHICS_BOXES boxes;
};

static void blt_in(
	void *closure,
	int16 x, int16 y,
	int16 w, int16 h,
	uint16 coverage
)
{
	struct blt_in *info = closure;
	GRAPHICS_COLOR color;

	if(GRAPHICS_ALPHA_SHORT_IS_OPAQUE(coverage))
	{
		return;
	}

	GraphicsBoxFromIntegers(&info->boxes.chunks.base[0], x, y, w, h);

	InitializeGraphicsColorRGBA(&color, 0, 0, 0, coverage / (double) 0xffff);
	info->compositor->fill_boxes (info->dst,
		GRAPHICS_OPERATOR_IN, &color,
		&info->boxes);
}

static void add_rect_with_offset(GRAPHICS_BOXES* boxes, int x1, int y1, int x2, int y2, int dx, int dy)
{
	GRAPHICS_BOX box;
	eGRAPHICS_INTEGER_STATUS status;
	
	box.point1.x = GraphicsFixedFromInteger(x1 - dx);
	box.point1.y = GraphicsFixedFromInteger(y1 - dy);
	box.point2.x = GraphicsFixedFromInteger(x2 - dx);
	box.point2.y = GraphicsFixedFromInteger(y2 - dy);

	status = GraphicsBoxesAdd(boxes, GRAPHICS_ANTIALIAS_DEFAULT, &box);
	assert (status == GRAPHICS_INTEGER_STATUS_SUCCESS);
}

static eGRAPHICS_INTEGER_STATUS combine_clip_as_traps(
	const GRAPHICS_TRAPS_COMPOSITOR* compositor,
	GRAPHICS_SURFACE* mask,
	const GRAPHICS_CLIP* clip,
	const GRAPHICS_RECTANGLE_INT* extents
)
{
	GRAPHICS *graphics;
	GRAPHICS_POLYGON polygon;
	eGRAPHICS_FILL_RULE fill_rule;
	eGRAPHICS_ANTIALIAS antialias;
	GRAPHICS_TRAPS traps;
	GRAPHICS_SURFACE *src;
	GRAPHICS_BOX box;
	GRAPHICS_RECTANGLE_INT fixup;
	int src_x, src_y;
	eGRAPHICS_INTEGER_STATUS status;

	graphics = compositor->base.graphics;

	TRACE ((stderr, "%s\n", __FUNCTION__));
	
	status = GraphicsClipGetPolygon(clip, &polygon,
		&fill_rule, &antialias);
	if(status)
	{
		return status;
	}
	
	InitializeGraphicsTraps(&traps);
	status = GraphicsBentleyOttmannTessellatePolygon(&traps,
		&polygon,
		fill_rule);
	GraphicsPolygonFinish(&polygon);
	if(UNLIKELY(status))
		return status;
	
	src = compositor->pattern_to_surface (mask, NULL, FALSE,
		extents, NULL,
		&src_x, &src_y);
	if(UNLIKELY(src->status))
	{
		GraphicsTrapsFinish(&traps);
		return src->status;
	}

	status = compositor->composite_traps (mask, GRAPHICS_OPERATOR_IN, src,
		src_x, src_y,
		extents->x, extents->y,
		extents,
		antialias, &traps);
	
	GraphicsTrapsExtents(&traps, &box);
	GraphicsBoxRoundToRectangle(&box, &fixup);
	GraphicsTrapsFinish(&traps);
	DestroyGraphicsSurface(src);

	if(UNLIKELY(status))
		return status;
	
	if(! GraphicsRectangleIntersect(&fixup, extents))
		return GRAPHICS_STATUS_SUCCESS;

	if(fixup.width < extents->width || fixup.height < extents->height)
	{
		GRAPHICS_BOXES clear;
		
		InitializeGraphicsBoxes(&clear);

		/* top */
		if(fixup.y != extents->y)
		{
			add_rect_with_offset (&clear,
				extents->x, extents->y,
				extents->x + extents->width,
				fixup.y,
				extents->x, extents->y);
		}
		/* left */
		if(fixup.x != extents->x) {
			add_rect_with_offset (&clear,
				extents->x, fixup.y,
				fixup.x,
				fixup.y + fixup.height,
				extents->x, extents->y);
		}
		/* right */
		if(fixup.x + fixup.width != extents->x + extents->width) {
			add_rect_with_offset (&clear,
				fixup.x + fixup.width,
				fixup.y,
				extents->x + extents->width,
				fixup.y + fixup.height,
				extents->x, extents->y);
		}
		/* bottom */
		if(fixup.y + fixup.height != extents->y + extents->height) {
			add_rect_with_offset (&clear,
				extents->x,
				fixup.y + fixup.height,
				extents->x + extents->width,
				extents->y + extents->height,
				extents->x, extents->y);
		}

		status = compositor->fill_boxes (mask,
			GRAPHICS_OPERATOR_CLEAR,
			&graphics->color_transparent,
			&clear);

		GraphicsBoxesFinish(&clear);
	}

	return status;
}

static eGRAPHICS_STATUS __clip_to_surface(
	const GRAPHICS_TRAPS_COMPOSITOR *compositor,
	const GRAPHICS_COMPOSITE_RECTANGLES *composite,
	const GRAPHICS_RECTANGLE_INT *extents,
	GRAPHICS_SURFACE **surface)
{
	GRAPHICS *graphics;
	GRAPHICS_SURFACE *mask;
	GRAPHICS_POLYGON polygon;
	eGRAPHICS_FILL_RULE fill_rule;
	eGRAPHICS_ANTIALIAS antialias;
	GRAPHICS_TRAPS traps;
	GRAPHICS_BOXES clear;
	GRAPHICS_SURFACE *src;
	int src_x, src_y;
	eGRAPHICS_INTEGER_STATUS status;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	graphics = compositor->base.graphics;

	status = GraphicsClipGetPolygon(composite->clip, &polygon,
		&fill_rule, &antialias);
	if(status)
		return status;

	InitializeGraphicsTraps(&traps);
	status = GraphicsBentleyOttmannTessellatePolygon(&traps,
		&polygon,
		fill_rule);
	GraphicsPolygonFinish(&polygon);
	if(UNLIKELY(status))
		return status;

	mask = GraphicsSurfaceCreateScratch(composite->surface,
		GRAPHICS_CONTENT_ALPHA,
		extents->width,
		extents->height,
		NULL);
	if(UNLIKELY(mask->status)) {
		GraphicsTrapsFinish(&traps);
		return status;
	}

	src = compositor->pattern_to_surface (mask, NULL, FALSE,
		extents, NULL,
		&src_x, &src_y);
	if(UNLIKELY(status = src->status))
		goto error;

	status = compositor->acquire (mask);
	if(UNLIKELY(status))
		goto error;

	InitializeGraphicsBoxesFromRectangle(&clear,
		0, 0,
		extents->width,
		extents->height);
	status = compositor->fill_boxes (mask,
		GRAPHICS_OPERATOR_CLEAR,
		&graphics->color_transparent,
		&clear);
	if(UNLIKELY(status))
		goto error_release;

	status = compositor->composite_traps (mask, GRAPHICS_OPERATOR_ADD, src,
		src_x, src_y,
		extents->x, extents->y,
		extents,
		antialias, &traps);
	if(UNLIKELY(status))
		goto error_release;

	compositor->release (mask);
	*surface = mask;
out:
	DestroyGraphicsSurface(src);
	GraphicsTrapsFinish(&traps);
	return status;

error_release:
	compositor->release (mask);
error:
	DestroyGraphicsSurface(mask);
	goto out;
}

static GRAPHICS_SURFACE* traps_get_clip_surface(
	const GRAPHICS_TRAPS_COMPOSITOR* compositor,
	const GRAPHICS_COMPOSITE_RECTANGLES* composite,
	const GRAPHICS_RECTANGLE_INT* extents
)
{
	GRAPHICS *graphics;
	GRAPHICS_SURFACE *surface = NULL;
	eGRAPHICS_INTEGER_STATUS status;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	graphics = compositor->base.graphics;

	status = __clip_to_surface (compositor, composite, extents, &surface);
	if(status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED) {
		surface = GraphicsSurfaceCreateScratch(composite->surface,
			GRAPHICS_CONTENT_ALPHA,
			extents->width,
			extents->height,
			&graphics->color_white
		);
		if(UNLIKELY(surface->status))
			return surface;

		status = GraphicsClipCombineWithSurface(composite->clip, surface,
			extents->x, extents->y);
	}
	if(UNLIKELY(status)) {
		DestroyGraphicsSurface(surface);
		surface = NULL;
	}

	return surface;
}

static void blt_unaligned_boxes(const GRAPHICS_TRAPS_COMPOSITOR *compositor,
	GRAPHICS_SURFACE *surface,
	int dx, int dy,
	GRAPHICS_BOX *boxes,
	int num_boxes)
{
	struct blt_in info;
	int i;

	info.compositor = compositor;
	info.dst = surface;
	InitializeGraphicsBoxes(&info.boxes);
	info.boxes.num_boxes = 1;
	for (i = 0; i < num_boxes; i++) {
		GRAPHICS_BOX *b = &boxes[i];

		if(! GRAPHICS_FIXED_IS_INTEGER(b->point1.x) ||
			! GRAPHICS_FIXED_IS_INTEGER(b->point1.y) ||
			! GRAPHICS_FIXED_IS_INTEGER(b->point2.x) ||
			! GRAPHICS_FIXED_IS_INTEGER(b->point2.y))
		{
			do_unaligned_box(blt_in, &info, b, dx, dy);
		}
	}
}

static GRAPHICS_SURFACE* create_composite_mask(
	const GRAPHICS_TRAPS_COMPOSITOR* compositor,
	GRAPHICS_SURFACE* dst,
	void* draw_closure,
	draw_func_t draw_func,
	draw_func_t mask_func,
	const GRAPHICS_COMPOSITE_RECTANGLES *extents
)
{
	GRAPHICS *graphics;
	GRAPHICS_SURFACE *surface, *src;
	eGRAPHICS_INTEGER_STATUS status;
	int src_x, src_y;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	graphics = compositor->base.graphics;

	surface = GraphicsSurfaceCreateScratch(dst, GRAPHICS_CONTENT_ALPHA,
		extents->bounded.width,
		extents->bounded.height,
		NULL);
	if(UNLIKELY(surface->status))
		return surface;

	src = compositor->pattern_to_surface (surface,
		&graphics->white_pattern.base,
		FALSE,
		&extents->bounded,
		&extents->bounded,
		&src_x, &src_y);
	if(UNLIKELY(src->status))
	{
		DestroyGraphicsSurface(surface);
		return src;
	}

	status = compositor->acquire (surface);
	if(UNLIKELY(status))
	{
		DestroyGraphicsSurface(src);
		DestroyGraphicsSurface(surface);
		return NULL;
	}

	if(!surface->is_clear)
	{
		GRAPHICS_BOXES clear;

		InitializeGraphicsBoxesFromRectangle(&clear,
			0, 0,
			extents->bounded.width,
			extents->bounded.height);
		status = compositor->fill_boxes (surface,
			GRAPHICS_OPERATOR_CLEAR,
			&graphics->color_transparent,
			&clear
		);
		if(UNLIKELY(status))
			goto error;

		surface->is_clear = TRUE;
	}

	if(mask_func)
	{
		status = mask_func (compositor, surface, draw_closure,
			GRAPHICS_OPERATOR_SOURCE, src, src_x, src_y,
			extents->bounded.x, extents->bounded.y,
			&extents->bounded, extents->clip);
		if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
		{
			surface->is_clear = FALSE;
			goto out;
		}
		if(UNLIKELY(status != GRAPHICS_INTEGER_STATUS_UNSUPPORTED))
			goto error;
	}

	/* Is it worth setting the clip region here? */
	status = draw_func (compositor, surface, draw_closure,
		GRAPHICS_OPERATOR_ADD, src, src_x, src_y,
		extents->bounded.x, extents->bounded.y,
		&extents->bounded, NULL);
	if(UNLIKELY(status))
		goto error;

	surface->is_clear = FALSE;
	if(extents->clip->path != NULL)
	{
		status = combine_clip_as_traps (compositor, surface,
			extents->clip, &extents->bounded);
		if(status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED)
		{
			status = GraphicsClipCombineWithSurface(extents->clip, surface,
				extents->bounded.x,
				extents->bounded.y);
		}
		if(UNLIKELY(status))
			goto error;
	}
	else if(extents->clip->boxes)
	{
		blt_unaligned_boxes(compositor, surface,
			extents->bounded.x, extents->bounded.y,
			extents->clip->boxes, extents->clip->num_boxes);

	}

out:
	compositor->release (surface);
	DestroyGraphicsSurface(src);
	return surface;

error:
	compositor->release (surface);
	if(status != GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO)
	{
		DestroyGraphicsSurface(surface);
		surface = NULL;
	}
	DestroyGraphicsSurface(src);
	return surface;
}

/* Handles compositing with a clip surface when the operator allows
* us to combine the clip with the mask
*/
static eGRAPHICS_STATUS clip_and_composite_with_mask(
	const GRAPHICS_TRAPS_COMPOSITOR* compositor,
	const GRAPHICS_COMPOSITE_RECTANGLES* extents,
	draw_func_t draw_func,
	draw_func_t mask_func,
	void* draw_closure,
	eGRAPHICS_OPERATOR op,
	GRAPHICS_SURFACE* src,
	int src_x, int src_y
)
{
	GRAPHICS_SURFACE *dst = extents->surface;
	GRAPHICS_SURFACE *mask;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	mask = create_composite_mask(compositor, dst, draw_closure,
		draw_func, mask_func,
		extents);
	if(UNLIKELY(mask->status))
		return mask->status;

	if(mask->is_clear)
		goto skip;

	if(src != NULL || dst->content != GRAPHICS_CONTENT_ALPHA)
	{
		compositor->composite (dst, op, src, mask,
			extents->bounded.x + src_x,
			extents->bounded.y + src_y,
			0, 0,
			extents->bounded.x,	  extents->bounded.y,
			extents->bounded.width,  extents->bounded.height);
	}
	else
	{
		compositor->composite (dst, op, mask, NULL,
			0, 0,
			0, 0,
			extents->bounded.x,	  extents->bounded.y,
			extents->bounded.width,  extents->bounded.height);
	}

skip:
	DestroyGraphicsSurface(mask);
	return GRAPHICS_STATUS_SUCCESS;
}

/* Handles compositing with a clip surface when we have to do the operation
* in two pieces and combine them together.
*/
static eGRAPHICS_STATUS clip_and_composite_combine(
	const GRAPHICS_TRAPS_COMPOSITOR* compositor,
	const GRAPHICS_COMPOSITE_RECTANGLES* extents,
	draw_func_t draw_func,
	void* draw_closure,
	eGRAPHICS_OPERATOR op,
	GRAPHICS_SURFACE* src,
	int src_x, int src_y
)
{
	GRAPHICS_SURFACE *dst = extents->surface;
	GRAPHICS_SURFACE *tmp, *clip;
	eGRAPHICS_STATUS status;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	tmp = GraphicsSurfaceCreateScratch(dst, dst->content,
		extents->bounded.width,
		extents->bounded.height,
		NULL);
	if(UNLIKELY(tmp->status))
		return tmp->status;

	status = compositor->acquire (tmp);
	if(UNLIKELY(status))
	{
		DestroyGraphicsSurface(tmp);
		return status;
	}

	compositor->composite (tmp,
		dst->is_clear ? GRAPHICS_OPERATOR_CLEAR : GRAPHICS_OPERATOR_SOURCE,
		dst, NULL,
		extents->bounded.x,	  extents->bounded.y,
		0, 0,
		0, 0,
		extents->bounded.width,  extents->bounded.height);

	status = draw_func (compositor, tmp, draw_closure, op,
		src, src_x, src_y,
		extents->bounded.x, extents->bounded.y,
		&extents->bounded, NULL);

	if(UNLIKELY(status))
		goto cleanup;

	clip = traps_get_clip_surface (compositor, extents, &extents->bounded);
	if(UNLIKELY((status = clip->status)))
		goto cleanup;

	if(dst->is_clear)
	{
		compositor->composite (dst, GRAPHICS_OPERATOR_SOURCE, tmp, clip,
			0, 0,
			0, 0,
			extents->bounded.x,	  extents->bounded.y,
			extents->bounded.width,  extents->bounded.height);
	}
	else
	{
		compositor->lerp (dst, tmp, clip,
			0, 0,
			0,0,
			extents->bounded.x,	 extents->bounded.y,
			extents->bounded.width, extents->bounded.height);
	}
	DestroyGraphicsSurface(clip);

cleanup:
	compositor->release (tmp);
	DestroyGraphicsSurface(tmp);

	return status;
}

/* Handles compositing for %GRAPHICS_OPERATOR_SOURCE, which is special; it's
* defined as (src IN mask IN clip) ADD (dst OUT (mask IN clip))
*/
static eGRAPHICS_STATUS clip_and_composite_source(
	const GRAPHICS_TRAPS_COMPOSITOR* compositor,
	GRAPHICS_SURFACE* dst,
	draw_func_t draw_func,
	draw_func_t mask_func,
	void* draw_closure,
	GRAPHICS_SURFACE* src,
	int src_x,
	int src_y,
	const GRAPHICS_COMPOSITE_RECTANGLES* extents
)
{
	GRAPHICS_SURFACE *mask;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	/* Create a surface that is mask IN clip */
	mask = create_composite_mask(compositor, dst, draw_closure,
		draw_func, mask_func,
		extents);
	if(UNLIKELY(mask->status))
		return mask->status;

	if(mask->is_clear)
		goto skip;

	if(dst->is_clear)
	{
		compositor->composite (dst, GRAPHICS_OPERATOR_SOURCE, src, mask,
			extents->bounded.x + src_x, extents->bounded.y + src_y,
			0, 0,
			extents->bounded.x,	  extents->bounded.y,
			extents->bounded.width,  extents->bounded.height);
	}
	else
	{
		compositor->lerp (dst, src, mask,
			extents->bounded.x + src_x, extents->bounded.y + src_y,
			0, 0,
			extents->bounded.x,	 extents->bounded.y,
			extents->bounded.width, extents->bounded.height);
	}

skip:
	DestroyGraphicsSurface(mask);

	return GRAPHICS_STATUS_SUCCESS;
}

static int can_reduce_alpha_op (eGRAPHICS_OPERATOR op)
{
	int iop = op;
	switch (iop) {
	case GRAPHICS_OPERATOR_OVER:
	case GRAPHICS_OPERATOR_SOURCE:
	case GRAPHICS_OPERATOR_ADD:
		return TRUE;
	default:
		return FALSE;
	}
}

static int reduce_alpha_op(GRAPHICS_COMPOSITE_RECTANGLES *extents)
{
	GRAPHICS_SURFACE *dst = extents->surface;
	eGRAPHICS_OPERATOR op = extents->op;
	const GRAPHICS_PATTERN *pattern = &extents->source_pattern.base;
	return dst->is_clear &&
		dst->content == GRAPHICS_CONTENT_ALPHA &&
		GraphicsPatternIsOpaqueSolid(pattern) &&
		can_reduce_alpha_op (op);
}

static eGRAPHICS_STATUS fixup_unbounded_with_mask(
	const GRAPHICS_TRAPS_COMPOSITOR *compositor,
	const GRAPHICS_COMPOSITE_RECTANGLES *extents
)
{
	GRAPHICS_SURFACE *dst = extents->surface;
	GRAPHICS_SURFACE *mask;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	/* XXX can we avoid querying the clip surface again? */
	mask = traps_get_clip_surface (compositor, extents, &extents->unbounded);
	if(UNLIKELY(mask->status))
		return mask->status;

	/* top */
	if(extents->bounded.y != extents->unbounded.y)
	{
		int x = extents->unbounded.x;
		int y = extents->unbounded.y;
		int width = extents->unbounded.width;
		int height = extents->bounded.y - y;

		compositor->composite (dst, GRAPHICS_OPERATOR_DEST_OUT, mask, NULL,
			0, 0,
			0, 0,
			x, y,
			width, height);
	}

	/* left */
	if(extents->bounded.x != extents->unbounded.x)
	{
		int x = extents->unbounded.x;
		int y = extents->bounded.y;
		int width = extents->bounded.x - x;
		int height = extents->bounded.height;

		compositor->composite (dst, GRAPHICS_OPERATOR_DEST_OUT, mask, NULL,
			0, y - extents->unbounded.y,
			0, 0,
			x, y,
			width, height);
	}

	/* right */
	if(extents->bounded.x + extents->bounded.width != extents->unbounded.x + extents->unbounded.width)
	{
		int x = extents->bounded.x + extents->bounded.width;
		int y = extents->bounded.y;
		int width = extents->unbounded.x + extents->unbounded.width - x;
		int height = extents->bounded.height;

		compositor->composite (dst, GRAPHICS_OPERATOR_DEST_OUT, mask, NULL,
			x - extents->unbounded.x, y - extents->unbounded.y,
			0, 0,
			x, y,
			width, height);
	}

	/* bottom */
	if(extents->bounded.y + extents->bounded.height != extents->unbounded.y + extents->unbounded.height)
	{
		int x = extents->unbounded.x;
		int y = extents->bounded.y + extents->bounded.height;
		int width = extents->unbounded.width;
		int height = extents->unbounded.y + extents->unbounded.height - y;

		compositor->composite (dst, GRAPHICS_OPERATOR_DEST_OUT, mask, NULL,
			0, y - extents->unbounded.y,
			0, 0,
			x, y,
			width, height);
	}

	DestroyGraphicsSurface(mask);

	return GRAPHICS_STATUS_SUCCESS;
}

static void add_rect(GRAPHICS_BOXES* boxes, int x1, int y1, int x2, int y2)
{
	GRAPHICS_BOX box;
	eGRAPHICS_INTEGER_STATUS status;

	box.point1.x = GraphicsFixedFromInteger(x1);
	box.point1.y = GraphicsFixedFromInteger(y1);
	box.point2.x = GraphicsFixedFromInteger(x2);
	box.point2.y = GraphicsFixedFromInteger(y2);

	status = GraphicsBoxesAdd(boxes, GRAPHICS_ANTIALIAS_DEFAULT, &box);
	assert (status == GRAPHICS_INTEGER_STATUS_SUCCESS);
}

static eGRAPHICS_STATUS fixup_unbounded(
	const GRAPHICS_TRAPS_COMPOSITOR* compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_BOXES* boxes
)
{
	GRAPHICS *graphics;
	GRAPHICS_SURFACE *dst = extents->surface;
	GRAPHICS_BOXES clear, tmp;
	GRAPHICS_BOX box;
	eGRAPHICS_INTEGER_STATUS status;

	graphics = compositor->base.graphics;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	if(extents->bounded.width  == extents->unbounded.width &&
		extents->bounded.height == extents->unbounded.height)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	assert (extents->clip->path == NULL);

	/* subtract the drawn boxes from the unbounded area */
	InitializeGraphicsBoxes(&clear);

	box.point1.x = GraphicsFixedFromInteger(extents->unbounded.x + extents->unbounded.width);
	box.point1.y = GraphicsFixedFromInteger(extents->unbounded.y);
	box.point2.x = GraphicsFixedFromInteger(extents->unbounded.x);
	box.point2.y = GraphicsFixedFromInteger(extents->unbounded.y + extents->unbounded.height);

	if(boxes == NULL)
	{
		if(extents->bounded.width == 0 || extents->bounded.height == 0)
		{
			goto empty;
		}
		else
		{
			/* top */
			if(extents->bounded.y != extents->unbounded.y)
			{
				add_rect (&clear,
					extents->unbounded.x, extents->unbounded.y,
					extents->unbounded.x + extents->unbounded.width,
					extents->bounded.y);
			}
			/* left */
			if(extents->bounded.x != extents->unbounded.x)
			{
				add_rect (&clear,
					extents->unbounded.x, extents->bounded.y,
					extents->bounded.x,
					extents->bounded.y + extents->bounded.height);
			}
			/* right */
			if(extents->bounded.x + extents->bounded.width != extents->unbounded.x + extents->unbounded.width) {
				add_rect (&clear,
					extents->bounded.x + extents->bounded.width,
					extents->bounded.y,
					extents->unbounded.x + extents->unbounded.width,
					extents->bounded.y + extents->bounded.height);
			}
			/* bottom */
			if(extents->bounded.y + extents->bounded.height != extents->unbounded.y + extents->unbounded.height) {
				add_rect (&clear,
					extents->unbounded.x,
					extents->bounded.y + extents->bounded.height,
					extents->unbounded.x + extents->unbounded.width,
					extents->unbounded.y + extents->unbounded.height);
			}
		}
	}
	else if(boxes->num_boxes)
	{
		InitializeGraphicsBoxes(&tmp);

		assert (boxes->is_pixel_aligned);

		status = GraphicsBoxesAdd(&tmp, GRAPHICS_ANTIALIAS_DEFAULT, &box);
		assert (status == GRAPHICS_INTEGER_STATUS_SUCCESS);

		tmp.chunks.next = &boxes->chunks;
		tmp.num_boxes += boxes->num_boxes;
		
		status = GraphicsBentleyOttmannTessellateBoxes(&tmp,
			GRAPHICS_FILL_RULE_WINDING,
			&clear);
		tmp.chunks.next = NULL;
		if(UNLIKELY(status))
			goto error;
	}
	else
	{
empty:
		box.point1.x = GraphicsFixedFromInteger(extents->unbounded.x);
		box.point2.x = GraphicsFixedFromInteger(extents->unbounded.x + extents->unbounded.width);
		
		status = GraphicsBoxesAdd(&clear, GRAPHICS_ANTIALIAS_DEFAULT, &box);
		assert (status == GRAPHICS_INTEGER_STATUS_SUCCESS);
	}

	/* Now intersect with the clip boxes */
	if(extents->clip->num_boxes)
	{
		InitializeGraphicsBoxesForArray(&tmp,
			extents->clip->boxes,
			extents->clip->num_boxes);
		status = GraphicsBoxesIntersect(&clear, &tmp, &clear);
		if(UNLIKELY(status))
			goto error;
	}

	status = compositor->fill_boxes (dst,
		GRAPHICS_OPERATOR_CLEAR,
		&graphics->color_transparent,
		&clear
	);

error:
	GraphicsBoxesFinish(&clear);
	return status;
}

enum {
	NEED_CLIP_REGION = 0x1,
	NEED_CLIP_SURFACE = 0x2,
	FORCE_CLIP_REGION = 0x4,
};

static int need_bounded_clip(GRAPHICS_COMPOSITE_RECTANGLES* extents)
{
	unsigned int flags = 0;

	if(extents->clip->num_boxes > 1 ||
		extents->mask.width > extents->unbounded.width ||
		extents->mask.height > extents->unbounded.height)
	{
		flags |= NEED_CLIP_REGION;
	}

	if(extents->clip->num_boxes > 1 ||
		extents->mask.width > extents->bounded.width ||
		extents->mask.height > extents->bounded.height)
	{
		flags |= FORCE_CLIP_REGION;
	}

	if(! GraphicsClipIsRegion(extents->clip))
		flags |= NEED_CLIP_SURFACE;

	return flags;
}

static int need_unbounded_clip(GRAPHICS_COMPOSITE_RECTANGLES* extents)
{
	unsigned int flags = 0;
	if(! extents->is_bounded)
	{
		flags |= NEED_CLIP_REGION;
		if(! GraphicsClipIsRegion(extents->clip))
			flags |= NEED_CLIP_SURFACE;
	}
	if(extents->clip->path != NULL)
		flags |= NEED_CLIP_SURFACE;
	return flags;
}

static eGRAPHICS_STATUS clip_and_composite(
	const GRAPHICS_TRAPS_COMPOSITOR* compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	draw_func_t draw_func,
	draw_func_t mask_func,
	void* draw_closure,
	unsigned int need_clip
)
{
	GRAPHICS_SURFACE *dst = extents->surface;
	eGRAPHICS_OPERATOR op = extents->op;
	GRAPHICS_PATTERN *source = &extents->source_pattern.base;
	GRAPHICS_SURFACE *src;
	int src_x, src_y;
	GRAPHICS_REGION *clip_region = NULL;
	eGRAPHICS_STATUS status = GRAPHICS_STATUS_SUCCESS;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	if(reduce_alpha_op (extents))
	{
		op = GRAPHICS_OPERATOR_ADD;
		source = NULL;
	}

	if(op == GRAPHICS_OPERATOR_CLEAR)
	{
		op = GRAPHICS_OPERATOR_DEST_OUT;
		source = NULL;
	}

	compositor->acquire (dst);

	if(need_clip & NEED_CLIP_REGION)
	{
		const GRAPHICS_RECTANGLE_INT *limit;

		if((need_clip & FORCE_CLIP_REGION) == 0)
			limit = &extents->unbounded;
		else
			limit = &extents->destination;

		clip_region = GraphicsClipGetRegion(extents->clip);
		if(clip_region != NULL &&
			GraphicsRegionContainsRectangle(clip_region,
				limit) == GRAPHICS_REGION_OVERLAP_IN)
			clip_region = NULL;

		if(clip_region != NULL)
		{
			status = compositor->set_clip_region (dst, clip_region);
			if(UNLIKELY(status))
			{
				compositor->release (dst);
				return status;
			}
		}
	}

	if(extents->bounded.width == 0 || extents->bounded.height == 0)
		goto skip;

	src = compositor->pattern_to_surface (dst, source, FALSE,
		&extents->bounded,
		&extents->source_sample_area,
		&src_x, &src_y);
	if(UNLIKELY(status = src->status))
		goto error;

	if(op == GRAPHICS_OPERATOR_SOURCE)
	{
		status = clip_and_composite_source (compositor, dst,
			draw_func, mask_func, draw_closure,
			src, src_x, src_y,
			extents);
	}
	else
	{
		if(need_clip & NEED_CLIP_SURFACE)
		{
			if(extents->is_bounded)
			{
				status = clip_and_composite_with_mask (compositor, extents,
					draw_func, mask_func,
					draw_closure,
					op, src, src_x, src_y);
			}
			else
			{
				status = clip_and_composite_combine (compositor, extents,
					draw_func, draw_closure,
					op, src, src_x, src_y);
			}
		}
		else
		{
			status = draw_func (compositor,
				dst, draw_closure,
				op, src, src_x, src_y,
				0, 0,
				&extents->bounded,
				extents->clip);
		}
	}
	DestroyGraphicsSurface(src);

skip:
	if(status == GRAPHICS_STATUS_SUCCESS && ! extents->is_bounded)
	{
		if(need_clip & NEED_CLIP_SURFACE)
			status = fixup_unbounded_with_mask (compositor, extents);
		else
			status = fixup_unbounded (compositor, extents, NULL);
	}

error:
	if(clip_region)
		compositor->set_clip_region (dst, NULL);

	compositor->release (dst);

	return status;
}

/* meta-ops */

typedef struct
{
	GRAPHICS_TRAPS traps;
	eGRAPHICS_ANTIALIAS antialias;
} composite_traps_info_t;

static eGRAPHICS_INTEGER_STATUS composite_traps(
	const GRAPHICS_TRAPS_COMPOSITOR* compositor,
	GRAPHICS_SURFACE* dst,
	void* closure,
	eGRAPHICS_OPERATOR op,
	GRAPHICS_SURFACE* src,
	int src_x, int src_y,
	int dst_x, int dst_y,
	const GRAPHICS_RECTANGLE_INT* extents,
	GRAPHICS_CLIP* clip
)
{
	composite_traps_info_t *info = closure;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	return compositor->composite_traps (dst, op, src,
		src_x - dst_x, src_y - dst_y,
		dst_x, dst_y,
		extents,
		info->antialias, &info->traps);
}

typedef struct {
	GRAPHICS_TRISTRIP strip;
	eGRAPHICS_ANTIALIAS antialias;
} composite_tristrip_info_t;

static eGRAPHICS_INTEGER_STATUS composite_tristrip(
	const GRAPHICS_TRAPS_COMPOSITOR* compositor,
	GRAPHICS_SURFACE* dst,
	void* closure,
	eGRAPHICS_OPERATOR op,
	GRAPHICS_SURFACE* src,
	int src_x, int src_y,
	int dst_x, int dst_y,
	const GRAPHICS_RECTANGLE_INT* extents,
	GRAPHICS_CLIP* clip
)
{
	composite_tristrip_info_t *info = closure;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	return compositor->composite_tristrip (dst, op, src,
		src_x - dst_x, src_y - dst_y,
		dst_x, dst_y,
		extents,
		info->antialias, &info->strip);
}

static int is_recording_pattern(const GRAPHICS_PATTERN* pattern)
{
	GRAPHICS_SURFACE *surface;

	if(pattern->type != GRAPHICS_PATTERN_TYPE_SURFACE)
		return FALSE;
	
	surface = ((const GRAPHICS_SURFACE_PATTERN*) pattern)->surface;
	surface = GraphicsSurfaceGetSource(surface, NULL);
	return surface->backend->type == GRAPHICS_SURFACE_TYPE_RECORDING;
}

static GRAPHICS_SURFACE* recording_pattern_get_surface(const GRAPHICS_PATTERN *pattern)
{
	GRAPHICS_SURFACE *surface;

	surface = ((const GRAPHICS_SURFACE_PATTERN*) pattern)->surface;
	return GraphicsSurfaceGetSource(surface, NULL);
}

static int recording_pattern_contains_sample(
	const GRAPHICS_PATTERN* pattern,
	const GRAPHICS_RECTANGLE_INT* sample
)
{
	GRAPHICS_RECORDING_SURFACE *surface;

	if(! is_recording_pattern (pattern))
		return FALSE;

	if(pattern->extend == GRAPHICS_EXTEND_NONE)
		return TRUE;

	surface = (GRAPHICS_RECORDING_SURFACE*) recording_pattern_get_surface (pattern);
	if(surface->unbounded)
		return TRUE;
	
	return GraphicsRectangleContainsRectangle(&surface->extents, sample);
}

static int op_reduces_to_source(GRAPHICS_COMPOSITE_RECTANGLES* extents)
{
	if(extents->op == GRAPHICS_OPERATOR_SOURCE)
		return TRUE;

	if(extents->surface->is_clear)
		return extents->op == GRAPHICS_OPERATOR_OVER || extents->op == GRAPHICS_OPERATOR_ADD;

	return FALSE;
}

static eGRAPHICS_STATUS composite_aligned_boxes(
	const GRAPHICS_TRAPS_COMPOSITOR* compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_BOXES* boxes
)
{
	GRAPHICS *graphics;
	GRAPHICS_SURFACE *dst = extents->surface;
	eGRAPHICS_OPERATOR op = extents->op;
	int need_clip_mask = ! GraphicsClipIsRegion(extents->clip);
	int op_is_source;
	eGRAPHICS_STATUS status;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	graphics = compositor->base.graphics;

	if(need_clip_mask &&
		(! extents->is_bounded || extents->op == GRAPHICS_OPERATOR_SOURCE))
	{
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	}

	op_is_source = op_reduces_to_source (extents);

	/* Are we just copying a recording surface? */
	if(! need_clip_mask && op_is_source &&
		recording_pattern_contains_sample (&extents->source_pattern.base,
			&extents->source_sample_area))
	{
		GRAPHICS_CLIP *recording_clip, local_clip;
		const GRAPHICS_PATTERN *source = &extents->source_pattern.base;
		const GRAPHICS_MATRIX *m;
		GRAPHICS_MATRIX matrix;

		/* XXX could also do tiling repeat modes... */

		/* first clear the area about to be overwritten */
		if(! dst->is_clear)
		{
			status = compositor->acquire (dst);
			if(UNLIKELY(status))
				return status;

			status = compositor->fill_boxes (dst,
				GRAPHICS_OPERATOR_CLEAR,
				&graphics->color_transparent,
				boxes);
			compositor->release (dst);
			if(UNLIKELY(status))
				return status;
		}
		
		m = &source->matrix;
		if(GRAPHICS_SURFACE_HAS_DEVICE_TRANSFORM(dst))
		{
			GraphicsMatrixMultiply(&matrix,
				&source->matrix,
				&dst->device_transform);
			m = &matrix;
		}

		recording_clip = GraphicsClipFromBoxes(boxes, &local_clip);
		//status = _cairo_recording_surface_replay_with_clip (recording_pattern_get_surface (source),
		//	m, dst, recording_clip);
		GraphicsClipDestroy(recording_clip);

		return status;
	}

	status = compositor->acquire (dst);
	if(UNLIKELY(status))
		return status;

	if(! need_clip_mask &&
		(op == GRAPHICS_OPERATOR_CLEAR ||
			extents->source_pattern.base.type == GRAPHICS_PATTERN_TYPE_SOLID))
	{
		const GRAPHICS_COLOR *color;

		if(op == GRAPHICS_OPERATOR_CLEAR)
		{
			color = &graphics->color_transparent;
		}
		else
		{
			color = &((GRAPHICS_SOLID_PATTERN*) &extents->source_pattern)->color;
			if(op_is_source)
				op = GRAPHICS_OPERATOR_SOURCE;
		}

		status = compositor->fill_boxes (dst, op, color, boxes);
	}
	else
	{
		GRAPHICS_SURFACE *src, *mask = NULL;
		GRAPHICS_PATTERN *source = &extents->source_pattern.base;
		int src_x, src_y;
		int mask_x = 0, mask_y = 0;

		if(need_clip_mask) {
			mask = traps_get_clip_surface (compositor,
				extents, &extents->bounded);
			if(UNLIKELY(mask->status))
				return mask->status;

			mask_x = -extents->bounded.x;
			mask_y = -extents->bounded.y;

			if(op == GRAPHICS_OPERATOR_CLEAR) {
				source = NULL;
				op = GRAPHICS_OPERATOR_DEST_OUT;
			}
		} else if(op_is_source)
			op = GRAPHICS_OPERATOR_SOURCE;

		src = compositor->pattern_to_surface (dst, source, FALSE,
			&extents->bounded,
			&extents->source_sample_area,
			&src_x, &src_y);
		if(LIKELY(src->status == GRAPHICS_STATUS_SUCCESS)) {
			status = compositor->composite_boxes (dst, op, src, mask,
				src_x, src_y,
				mask_x, mask_y,
				0, 0,
				boxes, &extents->bounded);
			DestroyGraphicsSurface(src);
		} else
			status = src->status;

		DestroyGraphicsSurface(mask);
	}

	if(status == GRAPHICS_STATUS_SUCCESS && ! extents->is_bounded)
		status = fixup_unbounded (compositor, extents, boxes);

	compositor->release (dst);

	return status;
}

static eGRAPHICS_STATUS upload_boxes(
	const GRAPHICS_TRAPS_COMPOSITOR* compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_BOXES* boxes
)
{
	GRAPHICS_SURFACE *dst = extents->surface;
	const GRAPHICS_PATTERN *source = &extents->source_pattern.base;
	GRAPHICS_SURFACE *src;
	GRAPHICS_RECTANGLE_INT limit;
	eGRAPHICS_INTEGER_STATUS status;
	int tx, ty;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	src = GraphicsPatternGetSource((GRAPHICS_SURFACE_PATTERN *)source,
		&limit);
	if(!(src->type == GRAPHICS_SURFACE_TYPE_IMAGE || src->type == dst->type))
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;

	if(! GraphicsMatrixIsIntegerTranslation(&source->matrix, &tx, &ty))
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;

	/* Check that the data is entirely within the image */
	if(extents->bounded.x + tx < limit.x || extents->bounded.y + ty < limit.y)
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;

	if(extents->bounded.x + extents->bounded.width  + tx > limit.x + limit.width ||
		extents->bounded.y + extents->bounded.height + ty > limit.y + limit.height)
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;

	tx += limit.x;
	ty += limit.y;

	if(src->type == GRAPHICS_SURFACE_TYPE_IMAGE)
		status = compositor->draw_image_boxes(dst,
		(GRAPHICS_IMAGE_SURFACE *)src,
			boxes, extents, tx, ty);
	else
		status = compositor->copy_boxes (dst, src, boxes, &extents->bounded,
			tx, ty);

	return status;
}

static eGRAPHICS_INTEGER_STATUS trim_extents_to_traps(
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_TRAPS* traps
)
{
	GRAPHICS_BOX box;
	
	GraphicsTrapsExtents(traps, &box);
	return GraphicsCompositeRectanglesIntersectMaskExtents(extents, &box);
}

static eGRAPHICS_INTEGER_STATUS trim_extents_to_tristrip(
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_TRISTRIP* strip
)
{
	GRAPHICS_BOX box;

	GraphicsTristripExtents(strip, &box);
	return GraphicsCompositeRectanglesIntersectMaskExtents(extents, &box);
}

static eGRAPHICS_INTEGER_STATUS trim_extents_to_boxes(
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_BOXES* boxes
)
{
	GRAPHICS_BOX box;
	
	GraphicsBoxesExtents(boxes, &box);
	return GraphicsCompositeRectanglesIntersectMaskExtents(extents, &box);
}

static eGRAPHICS_INTEGER_STATUS boxes_for_traps(
	GRAPHICS_BOXES* boxes,
	GRAPHICS_TRAPS* traps,
	eGRAPHICS_ANTIALIAS antialias
)
{
	int i, j;
	
	/* first check that the traps are rectilinear */
	if(antialias == GRAPHICS_ANTIALIAS_NONE)
	{
		for(i = 0; i < traps->num_traps; i++)
		{
			const GRAPHICS_TRAPEZOID *t = &traps->traps[i];
			if(GraphicsFixedIntegerRoundDown(t->left.point1.x) !=
				GraphicsFixedIntegerRoundDown(t->left.point2.x) ||
				GraphicsFixedIntegerRoundDown(t->right.point1.x) !=
				GraphicsFixedIntegerRoundDown(t->right.point2.x))
			{
				return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
			}
		}
	}
	else
	{
		for(i = 0; i < traps->num_traps; i++)
		{
			const GRAPHICS_TRAPEZOID *t = &traps->traps[i];
			if(t->left.point1.x != t->left.point2.x || t->right.point1.x != t->right.point2.x)
				return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
		}
	}

	InitializeGraphicsBoxes(boxes);

	boxes->chunks.base  = (GRAPHICS_BOX *) traps->traps;
	boxes->chunks.size  = traps->num_traps;

	if(antialias != GRAPHICS_ANTIALIAS_NONE)
	{
		for(i = j = 0; i < traps->num_traps; i++)
		{
			/* Note the traps and boxes alias so we need to take the local copies first. */
			GRAPHICS_FLOAT_FIXED x1 = traps->traps[i].left.point1.x;
			GRAPHICS_FLOAT_FIXED x2 = traps->traps[i].right.point1.x;
			GRAPHICS_FLOAT_FIXED y1 = traps->traps[i].top;
			GRAPHICS_FLOAT_FIXED y2 = traps->traps[i].bottom;

			if(x1 == x2 || y1 == y2)
				continue;

			boxes->chunks.base[j].point1.x = x1;
			boxes->chunks.base[j].point1.y = y1;
			boxes->chunks.base[j].point2.x = x2;
			boxes->chunks.base[j].point2.y = y2;
			j++;

			if(boxes->is_pixel_aligned) {
				boxes->is_pixel_aligned =
					GRAPHICS_FIXED_IS_INTEGER(x1) && GRAPHICS_FIXED_IS_INTEGER(y1) &&
					GRAPHICS_FIXED_IS_INTEGER(x2) && GRAPHICS_FIXED_IS_INTEGER(y2);
			}
		}
	}
	else
	{
		boxes->is_pixel_aligned = TRUE;

		for(i = j = 0; i < traps->num_traps; i++)
		{
			/* Note the traps and boxes alias so we need to take the local copies first. */
			GRAPHICS_FLOAT_FIXED x1 = traps->traps[i].left.point1.x;
			GRAPHICS_FLOAT_FIXED x2 = traps->traps[i].right.point1.x;
			GRAPHICS_FLOAT_FIXED y1 = traps->traps[i].top;
			GRAPHICS_FLOAT_FIXED y2 = traps->traps[i].bottom;
			
			/* round down here to match Pixman's behavior when using traps. */
			boxes->chunks.base[j].point1.x = GRAPHICS_FIXED_ROUND_DOWN(x1);
			boxes->chunks.base[j].point1.y = GRAPHICS_FIXED_ROUND_DOWN(y1);
			boxes->chunks.base[j].point2.x = GRAPHICS_FIXED_ROUND_DOWN(x2);
			boxes->chunks.base[j].point2.y = GRAPHICS_FIXED_ROUND_DOWN(y2);
			j += (boxes->chunks.base[j].point1.x != boxes->chunks.base[j].point2.x &&
				boxes->chunks.base[j].point1.y != boxes->chunks.base[j].point2.y);
		}
	}
	boxes->chunks.count = j;
	boxes->num_boxes	= j;

	return GRAPHICS_INTEGER_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS clip_and_composite_boxes(
	const GRAPHICS_TRAPS_COMPOSITOR* compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_BOXES* boxes
);

static eGRAPHICS_STATUS clip_and_composite_polygon(
	const GRAPHICS_TRAPS_COMPOSITOR* compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	GRAPHICS_POLYGON* polygon,
	eGRAPHICS_ANTIALIAS antialias,
	eGRAPHICS_FILL_RULE fill_rule,
	int curvy
)
{
	composite_traps_info_t traps;
	GRAPHICS_SURFACE *dst = extents->surface;
	int clip_surface = ! GraphicsClipIsRegion(extents->clip);
	eGRAPHICS_INTEGER_STATUS status;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	if(polygon->num_edges == 0) {
		status = GRAPHICS_INTEGER_STATUS_SUCCESS;

		if(! extents->is_bounded) {
			GRAPHICS_REGION *clip_region = GraphicsClipGetRegion(extents->clip);

			if(clip_region &&
				GraphicsRegionContainsRectangle(clip_region,
					&extents->unbounded) == GRAPHICS_REGION_OVERLAP_IN)
				clip_region = NULL;

			if(clip_region != NULL) {
				status = compositor->set_clip_region (dst, clip_region);
				if(UNLIKELY(status))
					return status;
			}

			if(clip_surface)
				status = fixup_unbounded_with_mask (compositor, extents);
			else
				status = fixup_unbounded (compositor, extents, NULL);

			if(clip_region != NULL)
				compositor->set_clip_region (dst, NULL);
		}

		return status;
	}

	if(extents->clip->path != NULL && extents->is_bounded)
	{
		GRAPHICS_POLYGON clipper;
		eGRAPHICS_FILL_RULE clipper_fill_rule;
		eGRAPHICS_ANTIALIAS clipper_antialias;

		status = GraphicsClipGetPolygon(extents->clip,
			&clipper,
			&clipper_fill_rule,
			&clipper_antialias);
		if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
		{
			if(clipper_antialias == antialias)
			{
				status = GraphicsPolygonIntersect(polygon, fill_rule,
					&clipper, clipper_fill_rule);
				if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
				{
					GRAPHICS_CLIP local_clip;
					GRAPHICS_CLIP *clip = GraphicsClipCopyRegion(extents->clip, &local_clip);
					GraphicsClipDestroy(extents->clip);
					extents->clip = clip;

					fill_rule = GRAPHICS_FILL_RULE_WINDING;
				}
				GraphicsPolygonFinish(&clipper);
			}
		}
	}

	if(antialias == GRAPHICS_ANTIALIAS_NONE && curvy)
	{
		GRAPHICS_BOXES boxes;
		
		InitializeGraphicsBoxes(&boxes);
		status = GraphicsRasterisePolygonToBoxes(polygon, fill_rule, &boxes);
		if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
		{
			assert (boxes.is_pixel_aligned);
			status = clip_and_composite_boxes (compositor, extents, &boxes);
		}
		GraphicsBoxesFinish(&boxes);
		if((status != GRAPHICS_INTEGER_STATUS_UNSUPPORTED))
			return status;
	}

	InitializeGraphicsTraps(&traps.traps);

	if(antialias == GRAPHICS_ANTIALIAS_NONE && curvy)
	{
		status = GraphicsRasterisePolygonToTraps(polygon, fill_rule, antialias, &traps.traps);
	}
	else
	{
		status = GraphicsBentleyOttmannTessellatePolygon(&traps.traps, polygon, fill_rule);
	}
	if(UNLIKELY(status))
		goto CLEANUP_TRAPS;

	status = trim_extents_to_traps (extents, &traps.traps);
	if(UNLIKELY(status))
		goto CLEANUP_TRAPS;

	/* Use a fast path if the trapezoids consist of a set of boxes.  */
	status = GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	if(1)
	{
		GRAPHICS_BOXES boxes;

		status = boxes_for_traps (&boxes, &traps.traps, antialias);
		if(status == GRAPHICS_INTEGER_STATUS_SUCCESS)
		{
			status = clip_and_composite_boxes (compositor, extents, &boxes);
			/* XXX need to reconstruct the traps! */
			assert (status != GRAPHICS_INTEGER_STATUS_UNSUPPORTED);
		}
	}
	if(status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED)
	{
		/* Otherwise render the trapezoids to a mask and composite in the usual
		* fashion.
		*/
		unsigned int flags = 0;

		/* For unbounded operations, the X11 server will estimate the
		* affected rectangle and apply the operation to that. However,
		* there are cases where this is an overestimate (e.g. the
		* clip-fill-{eo,nz}-unbounded test).
		*
		* The clip will trim that overestimate to our expectations.
		*/
		if(! extents->is_bounded)
			flags |= FORCE_CLIP_REGION;

		traps.antialias = antialias;
		status = clip_and_composite (compositor, extents,
			composite_traps, NULL, &traps,
			need_unbounded_clip (extents) | flags);
	}

CLEANUP_TRAPS:
	GraphicsTrapsFinish(&traps.traps);

	return status;
}

struct composite_opacity_info
{
	const GRAPHICS_TRAPS_COMPOSITOR *compositor;
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
	struct composite_opacity_info *info = closure;
	const GRAPHICS_TRAPS_COMPOSITOR *compositor = info->compositor;
	GRAPHICS_SURFACE *mask;
	int mask_x, mask_y;
	GRAPHICS_COLOR color;
	GRAPHICS_SOLID_PATTERN solid;
	GRAPHICS *graphics;

	graphics = (GRAPHICS*)compositor->base.graphics;
	
	InitializeGraphicsColorRGBA(&color, 0, 0, 0, info->opacity * coverage);
	InitializeGraphicsPatternSolid(&solid, &color, compositor->base.graphics);
	mask = compositor->pattern_to_surface (info->dst, &solid.base, TRUE,
		&graphics->unbounded_rectangle,
		&graphics->unbounded_rectangle,
		&mask_x, &mask_y);
	if(LIKELY(mask->status == GRAPHICS_STATUS_SUCCESS))
	{
		if(info->src)
		{
			compositor->composite (info->dst, info->op, info->src, mask,
				x + info->src_x,  y + info->src_y,
				mask_x,		   mask_y,
				x,				y,
				w,				h);
		}
		else
		{
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
	const GRAPHICS_TRAPS_COMPOSITOR* compositor,
	GRAPHICS_SURFACE* dst,
	void* closure,
	eGRAPHICS_OPERATOR op,
	GRAPHICS_SURFACE* src,
	int src_x,
	int src_y,
	int dst_x,
	int dst_y,
	const GRAPHICS_RECTANGLE_INT* extents,
	GRAPHICS_CLIP* clip
)
{
	const GRAPHICS_SOLID_PATTERN *mask = closure;
	struct composite_opacity_info info;
	int i;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	info.compositor = compositor;
	info.op = op;
	info.dst = dst;

	info.src = src;
	info.src_x = src_x;
	info.src_y = src_y;

	info.opacity = mask->color.alpha / (double) 0xffff;

	/* XXX for lots of boxes create a clip region for the fully opaque areas */
	for (i = 0; i < clip->num_boxes; i++)
		do_unaligned_box(composite_opacity, &info,
			&clip->boxes[i], dst_x, dst_y);

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_INTEGER_STATUS composite_boxes(
	const GRAPHICS_TRAPS_COMPOSITOR* compositor,
	GRAPHICS_SURFACE* dst,
	void* closure,
	eGRAPHICS_OPERATOR op,
	GRAPHICS_SURFACE* src,
	int src_x,
	int src_y,
	int dst_x,
	int dst_y,
	const GRAPHICS_RECTANGLE_INT* extents,
	GRAPHICS_CLIP* clip
)
{
	GRAPHICS_TRAPS traps;
	eGRAPHICS_STATUS status;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	status = GraphicsTrapsInitializeBoxes(&traps, closure);
	if(UNLIKELY(status))
		return status;

	status = compositor->composite_traps (dst, op, src,
		src_x - dst_x, src_y - dst_y,
		dst_x, dst_y,
		extents,
		GRAPHICS_ANTIALIAS_DEFAULT, &traps);
	GraphicsTrapsFinish(&traps);

	return status;
}

static eGRAPHICS_STATUS
clip_and_composite_boxes (const GRAPHICS_TRAPS_COMPOSITOR *compositor,
	GRAPHICS_COMPOSITE_RECTANGLES *extents,
	GRAPHICS_BOXES *boxes)
{
	eGRAPHICS_INTEGER_STATUS status;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	if(boxes->num_boxes == 0 && extents->is_bounded)
		return GRAPHICS_STATUS_SUCCESS;

	status = trim_extents_to_boxes (extents, boxes);
	if(UNLIKELY(status))
		return status;

	if(boxes->is_pixel_aligned && extents->clip->path == NULL &&
		extents->source_pattern.base.type == GRAPHICS_PATTERN_TYPE_SURFACE &&
		(op_reduces_to_source (extents) ||
		(extents->op == GRAPHICS_OPERATOR_OVER &&
			(extents->source_pattern.surface.surface->content & GRAPHICS_CONTENT_ALPHA) == 0)))
	{
		status = upload_boxes (compositor, extents, boxes);
		if(status != GRAPHICS_INTEGER_STATUS_UNSUPPORTED)
			return status;
	}

	/* Can we reduce drawing through a clip-mask to simply drawing the clip? */
	if(extents->clip->path != NULL && extents->is_bounded) {
		GRAPHICS_POLYGON polygon;
		eGRAPHICS_FILL_RULE fill_rule;
		eGRAPHICS_ANTIALIAS antialias;
		GRAPHICS_CLIP *clip, local_clip;

		clip = GraphicsClipCopy(extents->clip, &local_clip);
		clip = GraphicsClipIntersectBoxes(clip, boxes);
		if(clip->clip_all)
			return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;

		status = GraphicsClipGetPolygon(clip, &polygon,
			&fill_rule, &antialias);
		GraphicsClipPathDestroy(clip->path);
		clip->path = NULL;
		if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS)) {
			GRAPHICS_CLIP *saved_clip = extents->clip;
			extents->clip = clip;

			status = clip_and_composite_polygon (compositor, extents, &polygon,
				antialias, fill_rule, FALSE);

			clip = extents->clip;
			extents->clip = saved_clip;

			GraphicsPolygonFinish(&polygon);
		}
		GraphicsClipDestroy(clip);

		if(status != GRAPHICS_INTEGER_STATUS_UNSUPPORTED)
			return status;
	}

	/* Use a fast path if the boxes are pixel aligned (or nearly aligned!) */
	if(boxes->is_pixel_aligned) {
		status = composite_aligned_boxes (compositor, extents, boxes);
		if(status != GRAPHICS_INTEGER_STATUS_UNSUPPORTED)
			return status;
	}

	return clip_and_composite (compositor, extents,
		composite_boxes, NULL, boxes,
		need_unbounded_clip (extents));
}

static eGRAPHICS_INTEGER_STATUS
composite_traps_as_boxes (const GRAPHICS_TRAPS_COMPOSITOR *compositor,
	GRAPHICS_COMPOSITE_RECTANGLES *extents,
	composite_traps_info_t *info)
{
	GRAPHICS_BOXES boxes;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	if(! GraphicsTrapsToBoxes(&info->traps, info->antialias, &boxes))
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;

	return clip_and_composite_boxes (compositor, extents, &boxes);
}

static eGRAPHICS_INTEGER_STATUS
clip_and_composite_traps (const GRAPHICS_TRAPS_COMPOSITOR *compositor,
	GRAPHICS_COMPOSITE_RECTANGLES *extents,
	composite_traps_info_t *info,
	unsigned flags)
{
	eGRAPHICS_INTEGER_STATUS status;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	status = trim_extents_to_traps (extents, &info->traps);
	if(UNLIKELY(status != GRAPHICS_INTEGER_STATUS_SUCCESS))
		return status;

	status = GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	if((flags & FORCE_CLIP_REGION) == 0)
		status = composite_traps_as_boxes (compositor, extents, info);
	if(status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED) {
		/* For unbounded operations, the X11 server will estimate the
		* affected rectangle and apply the operation to that. However,
		* there are cases where this is an overestimate (e.g. the
		* clip-fill-{eo,nz}-unbounded test).
		*
		* The clip will trim that overestimate to our expectations.
		*/
		if(! extents->is_bounded)
			flags |= FORCE_CLIP_REGION;

		status = clip_and_composite (compositor, extents,
			composite_traps, NULL, info,
			need_unbounded_clip (extents) | flags);
	}

	return status;
}

static eGRAPHICS_INTEGER_STATUS
clip_and_composite_tristrip (const GRAPHICS_TRAPS_COMPOSITOR *compositor,
	GRAPHICS_COMPOSITE_RECTANGLES *extents,
	composite_tristrip_info_t *info)
{
	eGRAPHICS_INTEGER_STATUS status;
	unsigned int flags = 0;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	status = trim_extents_to_tristrip (extents, &info->strip);
	if(UNLIKELY(status != GRAPHICS_INTEGER_STATUS_SUCCESS))
		return status;

	if(! extents->is_bounded)
		flags |= FORCE_CLIP_REGION;

	status = clip_and_composite (compositor, extents,
		composite_tristrip, NULL, info,
		need_unbounded_clip (extents) | flags);

	return status;
}

struct composite_mask {
	GRAPHICS_SURFACE *mask;
	int mask_x, mask_y;
};

static eGRAPHICS_INTEGER_STATUS
composite_mask (const GRAPHICS_TRAPS_COMPOSITOR *compositor,
	GRAPHICS_SURFACE			*dst,
	void				*closure,
	eGRAPHICS_OPERATOR		 op,
	GRAPHICS_SURFACE			*src,
	int				 src_x,
	int				 src_y,
	int				 dst_x,
	int				 dst_y,
	const GRAPHICS_RECTANGLE_INT	*extents,
	GRAPHICS_CLIP			*clip)
{
	struct composite_mask *data = closure;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	if(src != NULL) {
		compositor->composite (dst, op, src, data->mask,
			extents->x + src_x, extents->y + src_y,
			extents->x + data->mask_x, extents->y + data->mask_y,
			extents->x - dst_x,  extents->y - dst_y,
			extents->width,	  extents->height);
	} else {
		compositor->composite (dst, op, data->mask, NULL,
			extents->x + data->mask_x, extents->y + data->mask_y,
			0, 0,
			extents->x - dst_x,  extents->y - dst_y,
			extents->width,	  extents->height);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

struct composite_box_info {
	const GRAPHICS_TRAPS_COMPOSITOR *compositor;
	GRAPHICS_SURFACE *dst;
	GRAPHICS_SURFACE *src;
	int src_x, src_y;
	uint8 op;
};

static void composite_box(void *closure,
	int16 x, int16 y,
	int16 w, int16 h,
	uint16 coverage)
{
	struct composite_box_info *info = closure;
	const GRAPHICS_TRAPS_COMPOSITOR *compositor = info->compositor;
	GRAPHICS *graphics;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	graphics = (GRAPHICS*)compositor->base.graphics;

	if(! GRAPHICS_ALPHA_SHORT_IS_OPAQUE(coverage)) {
		GRAPHICS_SURFACE *mask;
		GRAPHICS_COLOR color;
		GRAPHICS_SOLID_PATTERN solid;
		int mask_x, mask_y;

		InitializeGraphicsColorRGBA(&color, 0, 0, 0, coverage / (double)0xffff);
		InitializeGraphicsPatternSolid(&solid, &color, compositor->base.graphics);

		mask = compositor->pattern_to_surface (info->dst, &solid.base, FALSE,
			&graphics->unbounded_rectangle,
			&graphics->unbounded_rectangle,
			&mask_x, &mask_y);

		if(LIKELY(mask->status == GRAPHICS_STATUS_SUCCESS)) {
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
composite_mask_clip_boxes (const GRAPHICS_TRAPS_COMPOSITOR *compositor,
	GRAPHICS_SURFACE		*dst,
	void				*closure,
	eGRAPHICS_OPERATOR		 op,
	GRAPHICS_SURFACE		*src,
	int				 src_x,
	int				 src_y,
	int				 dst_x,
	int				 dst_y,
	const GRAPHICS_RECTANGLE_INT	*extents,
	GRAPHICS_CLIP			*clip)
{
	struct composite_mask *data = closure;
	struct composite_box_info info;
	int i;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	info.compositor = compositor;
	info.op = GRAPHICS_OPERATOR_SOURCE;
	info.dst = dst;
	info.src = data->mask;
	info.src_x = data->mask_x;
	info.src_y = data->mask_y;

	info.src_x += dst_x;
	info.src_y += dst_y;

	for (i = 0; i < clip->num_boxes; i++)
		do_unaligned_box(composite_box, &info, &clip->boxes[i], dst_x, dst_y);

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_INTEGER_STATUS
composite_mask_clip (const GRAPHICS_TRAPS_COMPOSITOR *compositor,
	GRAPHICS_SURFACE			*dst,
	void				*closure,
	eGRAPHICS_OPERATOR			 op,
	GRAPHICS_SURFACE			*src,
	int				 src_x,
	int				 src_y,
	int				 dst_x,
	int				 dst_y,
	const GRAPHICS_RECTANGLE_INT	*extents,
	GRAPHICS_CLIP			*clip)
{
	struct composite_mask *data = closure;
	GRAPHICS_POLYGON polygon;
	eGRAPHICS_FILL_RULE fill_rule;
	composite_traps_info_t info;
	eGRAPHICS_STATUS status;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	status = GraphicsClipGetPolygon(clip, &polygon,
		&fill_rule, &info.antialias);
	if(UNLIKELY(status))
		return status;

	InitializeGraphicsTraps(&info.traps);
	status = GraphicsBentleyOttmannTessellatePolygon(&info.traps,
		&polygon,
		fill_rule);
	GraphicsPolygonFinish(&polygon);
	if(UNLIKELY(status))
		return status;

	status = composite_traps (compositor, dst, &info,
		GRAPHICS_OPERATOR_SOURCE,
		data->mask,
		data->mask_x + dst_x, data->mask_y + dst_y,
		dst_x, dst_y,
		extents, NULL);
	GraphicsTrapsFinish(&info.traps);

	return status;
}

/* high-level compositor interface */

static eGRAPHICS_INTEGER_STATUS _cairo_traps_compositor_paint(
	const GRAPHICS_COMPOSITOR *_compositor,
	GRAPHICS_COMPOSITE_RECTANGLES *extents
)
{
	GRAPHICS_TRAPS_COMPOSITOR *compositor = (GRAPHICS_TRAPS_COMPOSITOR*)_compositor;
	GRAPHICS_BOXES boxes;
	eGRAPHICS_INTEGER_STATUS status;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	status = compositor->check_composite (extents);
	if(UNLIKELY(status))
		return status;
	
	GraphicsClipStealBoxes(extents->clip, &boxes);
	status = clip_and_composite_boxes (compositor, extents, &boxes);
	GraphicsClipUnstealBoxes(extents->clip, &boxes);

	return status;
}

static eGRAPHICS_INTEGER_STATUS _cairo_traps_compositor_mask(
	const GRAPHICS_COMPOSITOR *_compositor,
	GRAPHICS_COMPOSITE_RECTANGLES *extents
)
{
	const GRAPHICS_TRAPS_COMPOSITOR *compositor = (GRAPHICS_TRAPS_COMPOSITOR*)_compositor;
	eGRAPHICS_INTEGER_STATUS status;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	status = compositor->check_composite (extents);
	if(UNLIKELY(status))
		return status;

	if(extents->mask_pattern.base.type == GRAPHICS_PATTERN_TYPE_SOLID &&
		extents->clip->path == NULL) {
		status = clip_and_composite (compositor, extents,
			composite_opacity_boxes,
			composite_opacity_boxes,
			&extents->mask_pattern,
			need_unbounded_clip (extents));
	}
	else
	{
		struct composite_mask data;

		data.mask = compositor->pattern_to_surface (extents->surface,
			&extents->mask_pattern.base,
			TRUE,
			&extents->bounded,
			&extents->mask_sample_area,
			&data.mask_x,
			&data.mask_y);
		if(UNLIKELY(data.mask->status))
			return data.mask->status;

		status = clip_and_composite (compositor, extents,
			composite_mask,
			extents->clip->path ? composite_mask_clip : composite_mask_clip_boxes,
			&data, need_bounded_clip (extents));

		DestroyGraphicsSurface(data.mask);
	}

	return status;
}

static eGRAPHICS_INTEGER_STATUS GraphicsTrapsCompositorStroke(
	const GRAPHICS_COMPOSITOR*_compositor,
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE *style,
	const GRAPHICS_MATRIX* ctm,
	const GRAPHICS_MATRIX* ctm_inverse,
	double tolerance,
	eGRAPHICS_ANTIALIAS	 antialias
)
{
	const GRAPHICS_TRAPS_COMPOSITOR *compositor = (GRAPHICS_TRAPS_COMPOSITOR *)_compositor;
	eGRAPHICS_INTEGER_STATUS status;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	status = compositor->check_composite (extents);
	if(UNLIKELY(status))
		return status;

	status = GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	if(path->stroke_is_rectilinear)
	{
		GRAPHICS_BOXES boxes;
		
		InitializeGraphicsBoxesWithClip(&boxes, extents->clip);
		status = GraphicsPathFixedStrokeRectilinearToBoxes(path,
			style,
			ctm,
			antialias,
			&boxes
		);
		if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
			status = clip_and_composite_boxes (compositor, extents, &boxes);
		GraphicsBoxesFinish(&boxes);
	}

	if(status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED && 0 &&
		GraphicsClipIsRegion(extents->clip)) /* XXX */
	{
		composite_tristrip_info_t info;
		
		info.antialias = antialias;
		InitializeGraphicsTristripWithClip(&info.strip, extents->clip);
		status = GraphicsPathFixedStrokeToTristrip(path, style,
			ctm, ctm_inverse,
			tolerance,
			&info.strip
		);
		if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
			status = clip_and_composite_tristrip (compositor, extents, &info);
		GraphicsTristripFinish(&info.strip);
	}

	if(status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED &&
		path->has_curve_to && antialias == GRAPHICS_ANTIALIAS_NONE)
	{
		GRAPHICS_POLYGON polygon;

		InitializeGraphicsPolygonWithClip(&polygon, extents->clip);
		status = GraphicsPathFixedStrokeToPolygon(path, style,
			ctm, ctm_inverse,
			tolerance,
			&polygon
		);
		if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
			status = clip_and_composite_polygon (compositor,
				extents, &polygon,
				GRAPHICS_ANTIALIAS_NONE,
				GRAPHICS_FILL_RULE_WINDING,
				TRUE);
		GraphicsPolygonFinish(&polygon);
	}
	
	if(status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED)
	{
		eGRAPHICS_INTEGER_STATUS (*func) (const GRAPHICS_PATH_FIXED	*path,
			const GRAPHICS_STROKE_STYLE	*stroke_style,
			const GRAPHICS_MATRIX	*ctm,
			const GRAPHICS_MATRIX	*ctm_inverse,
			double			 tolerance,
			GRAPHICS_TRAPS		*traps);
		composite_traps_info_t info;
		unsigned flags;
		
		if(antialias == GRAPHICS_ANTIALIAS_BEST || antialias == GRAPHICS_ANTIALIAS_GOOD)
		{
			func = GraphicsPathFixedStrokePolygonToTraps;
			flags = 0;
		}
		else
		{
			func = GraphicsPathFixedStrokeToTraps;
			flags = need_bounded_clip (extents) & ~NEED_CLIP_SURFACE;
		}

		info.antialias = antialias;
		InitializeGraphicsTrapsWithClip(&info.traps, extents->clip);
		status = func (path, style, ctm, ctm_inverse, tolerance, &info.traps);
		if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
			status = clip_and_composite_traps (compositor, extents, &info, flags);
		GraphicsTrapsFinish(&info.traps);
	}

	return status;
}

static eGRAPHICS_INTEGER_STATUS
_cairo_traps_compositor_fill (const GRAPHICS_COMPOSITOR *_compositor,
	GRAPHICS_COMPOSITE_RECTANGLES *extents,
	const GRAPHICS_PATH_FIXED	*path,
	eGRAPHICS_FILL_RULE		 fill_rule,
	double			 tolerance,
	eGRAPHICS_ANTIALIAS		 antialias)
{
	const GRAPHICS_TRAPS_COMPOSITOR *compositor = (GRAPHICS_TRAPS_COMPOSITOR *)_compositor;
	eGRAPHICS_INTEGER_STATUS status;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	status = compositor->check_composite (extents);
	if(UNLIKELY(status))
		return status;

	status = GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	if(path->fill_is_rectilinear) {
		GRAPHICS_BOXES boxes;
		
		InitializeGraphicsBoxesWithClip(&boxes, extents->clip);
		status = GraphicsPathFixedFillRectilinearToBoxes(path,
			fill_rule,
			antialias,
			&boxes);
		if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
			status = clip_and_composite_boxes (compositor, extents, &boxes);
		GraphicsBoxesFinish(&boxes);
	}

	if(status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED) {
		GRAPHICS_POLYGON polygon;

#if 0
		if(extents->mask.width  > extents->unbounded.width ||
			extents->mask.height > extents->unbounded.height)
		{
			GRAPHICS_BOX limits;
			_cairo_box_from_rectangle (&limits, &extents->unbounded);
			_cairo_polygon_init (&polygon, &limits, 1);
		}
		else
		{
			_cairo_polygon_init (&polygon, NULL, 0);
		}

		status = GraphicsPathFixedFillToPolygon(path, tolerance, &polygon);
		if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS)) {
			status = _cairo_polygon_intersect_with_boxes (&polygon, &fill_rule,
				extents->clip->boxes,
				extents->clip->num_boxes);
		}
#else
		InitializeGraphicsPolygonWithClip(&polygon, extents->clip);
		status = GraphicsPathFixedFillToPolygon(path, tolerance, &polygon);
#endif
		if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS)) {
			status = clip_and_composite_polygon (compositor, extents, &polygon,
				antialias, fill_rule, path->has_curve_to);
		}
		GraphicsPolygonFinish(&polygon);
	}

	return status;
}

#if 0
static eGRAPHICS_INTEGER_STATUS
composite_glyphs (const GRAPHICS_TRAPS_COMPOSITOR *compositor,
	GRAPHICS_SURFACE	*dst,
	void *closure,
	eGRAPHICS_OPERATOR	 op,
	GRAPHICS_SURFACE	*src,
	int src_x, int src_y,
	int dst_x, int dst_y,
	const GRAPHICS_RECTANGLE_INT *extents,
	GRAPHICS_CLIP		*clip)
{
	cairo_composite_glyphs_info_t *info = closure;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	if(op == GRAPHICS_OPERATOR_ADD && (dst->content & GRAPHICS_CONTENT_COLOR) == 0)
		info->use_mask = 0;

	return compositor->composite_glyphs (dst, op, src,
		src_x, src_y,
		dst_x, dst_y,
		info);
}

static eGRAPHICS_INTEGER_STATUS
_cairo_traps_compositor_glyphs (const GRAPHICS_COMPOSITOR	*_compositor,
	GRAPHICS_COMPOSITE_RECTANGLES	*extents,
	cairo_scaled_font_t		*scaled_font,
	cairo_glyph_t			*glyphs,
	int				 num_glyphs,
	int			 overlap)
{
	const GRAPHICS_TRAPS_COMPOSITOR *compositor = (GRAPHICS_TRAPS_COMPOSITOR *)_compositor;
	eGRAPHICS_INTEGER_STATUS status;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	status = compositor->check_composite (extents);
	if(UNLIKELY(status))
		return status;

	_cairo_scaled_font_freeze_cache (scaled_font);
	status = compositor->check_composite_glyphs (extents,
		scaled_font, glyphs,
		&num_glyphs);
	if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS)) {
		cairo_composite_glyphs_info_t info;

		info.font = scaled_font;
		info.glyphs = glyphs;
		info.num_glyphs = num_glyphs;
		info.use_mask = overlap || ! extents->is_bounded;
		info.extents = extents->bounded;

		status = clip_and_composite (compositor, extents,
			composite_glyphs, NULL, &info,
			need_bounded_clip (extents) | FORCE_CLIP_REGION);
	}
	_cairo_scaled_font_thaw_cache (scaled_font);

	return status;
}
#endif

void
_cairo_traps_compositor_init (GRAPHICS_TRAPS_COMPOSITOR *compositor,
	const GRAPHICS_COMPOSITOR  *delegate)
{
	compositor->base.delegate = delegate;

	compositor->base.paint = _cairo_traps_compositor_paint;
	compositor->base.mask = _cairo_traps_compositor_mask;
	compositor->base.fill = _cairo_traps_compositor_fill;
	compositor->base.stroke = GraphicsTrapsCompositorStroke;
	// compositor->base.glyphs = _cairo_traps_compositor_glyphs;
}

#ifdef __cplusplus
}
#endif
