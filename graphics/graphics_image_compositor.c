#include <string.h>
#include "graphics_surface.h"
#include "graphics_compositor.h"
#include "graphics_types.h"
#include "graphics_region.h"
#include "graphics_private.h"
#include "graphics_matrix.h"
#include "graphics_inline.h"
#include "../pixel_manipulate/pixel_manipulate.h"

#define TO_IMAGE_SURFACE(S) ((GRAPHICS_IMAGE_SURFACE*)(S))

#ifdef __cplusplus
extern "C" {
#endif

static INLINE PIXEL_MANIPULATE_IMAGE* ToPixelManipulateImage(GRAPHICS_SURFACE* surface)
{
	return &((GRAPHICS_IMAGE_SURFACE*)surface)->image;
}

static INLINE eGRAPHICS_INTEGER_STATUS Acquire(void* dummy)
{
	return GRAPHICS_INTEGER_STATUS_SUCCESS;
}

static eGRAPHICS_INTEGER_STATUS Release(void* dummy)
{
	return GRAPHICS_INTEGER_STATUS_SUCCESS;
}

static eGRAPHICS_INTEGER_STATUS SetClipRegion(void* _surface, GRAPHICS_REGION* region)
{
	GRAPHICS_IMAGE_SURFACE *surface = (GRAPHICS_IMAGE_SURFACE*)_surface;
	PIXEL_MANIPULATE_REGION32 *rgn = region != NULL ? &region->region : NULL;
	
	if(FALSE == PixelManipulateSetClipRegion32(&surface->image, rgn))
	{
		return GRAPHICS_STATUS_NO_MEMORY;
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS DrawImageBoxes(
	void* _dst,
	GRAPHICS_IMAGE_SURFACE* image,
	GRAPHICS_BOXES* boxes,
	int dx,
	int dy
)
{
	GRAPHICS *graphics;
	GRAPHICS_IMAGE_SURFACE *dst = (GRAPHICS_IMAGE_SURFACE*)_dst;
	GRAPHICS_BOXES_CHUNK *chunk;
	int i;

	graphics = (GRAPHICS*)dst->base.graphics;

	for(chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next)
	{
		for(i = 0; i < chunk->count; i++)
		{
			GRAPHICS_BOX *b = &chunk->base[i];
			int x = GraphicsFixedIntegerPart(b->point1.x);
			int y = GraphicsFixedIntegerPart(b->point1.y);
			int w = GraphicsFixedIntegerPart(b->point2.x) - x;
			int h = GraphicsFixedIntegerPart(b->point2.y) - y;
			if(dst->pixel_format != image->pixel_format
				|| FALSE == PixelManipulateBlockTransfer((uint32*)image->data, (uint32*)dst->data,
					image->stride / sizeof(uint32), dst->stride / sizeof(uint32),
					PIXEL_MANIPULATE_BIT_PER_PIXEL(image->pixel_format), PIXEL_MANIPULATE_BIT_PER_PIXEL(dst->pixel_format),
					x + dx, y + dy, x, y, w, h, &graphics->pixel_manipulate.implementation))
			{
				PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_CONJOINT_SRC,
					&image->image, NULL, &dst->image, x + dx, y + dy, 0, 0, x, y, w, h, &graphics->pixel_manipulate.implementation);
			}
		}
	}
	return GRAPHICS_STATUS_SUCCESS;
}

static INLINE uint32 color_to_uint32(const GRAPHICS_COLOR* color)
{
	return
		(color->alpha_short >> 8 << 24) |
		(color->red_short >> 8 << 16)   |
		(color->green_short & 0xff00)   |
		(color->blue_short >> 8);
}

static INLINE int color_to_pixel(
	const GRAPHICS_COLOR* color,
	ePIXEL_MANIPULATE_FORMAT format,
	uint32* pixel
)
{
	uint32 c;

	if(!(format == PIXEL_MANIPULATE_FORMAT_A8R8G8B8
		|| format == PIXEL_MANIPULATE_FORMAT_X8R8G8B8
		|| format == PIXEL_MANIPULATE_FORMAT_A8B8G8R8
		|| format == PIXEL_MANIPULATE_FORMAT_X8B8G8R8
		|| format == PIXEL_MANIPULATE_FORMAT_B8G8R8A8
		|| format == PIXEL_MANIPULATE_FORMAT_B8G8R8X8
		|| format == PIXEL_MANIPULATE_FORMAT_R5G6B5
		|| format == PIXEL_MANIPULATE_FORMAT_B5G6R5
		|| format == PIXEL_MANIPULATE_FORMAT_A8
	))
	{
		return FALSE;
	}

	c = color_to_uint32 (color);

	if(PIXEL_MANIPULATE_FORMAT_TYPE(format) == PIXEL_MANIPULATE_TYPE_ABGR)
	{
		c = ((c & 0xff000000) >>  0) |
			((c & 0x00ff0000) >> 16) |
			((c & 0x0000ff00) >>  0) |
			((c & 0x000000ff) << 16);
	}

	if(PIXEL_MANIPULATE_FORMAT_TYPE(format) == PIXEL_MANIPULATE_TYPE_BGRA)
	{
		c = ((c & 0xff000000) >> 24) |
			((c & 0x00ff0000) >>  8) |
			((c & 0x0000ff00) <<  8) |
			((c & 0x000000ff) << 24);
	}

	if(format == PIXEL_MANIPULATE_FORMAT_A8)
	{
		c = c >> 24;
	}
	else if(format == PIXEL_MANIPULATE_FORMAT_R5G6B5 || format == PIXEL_MANIPULATE_FORMAT_B5G6R5)
	{
		c = ((((c) >> 3) & 0x001f) |
			(((c) >> 5) & 0x07e0) |
			(((c) >> 8) & 0xf800));
	}

	*pixel = c;
	return TRUE;
}

static ePIXEL_MANIPULATE_OPERATE _PixelManipulateOperator(eGRAPHICS_OPERATOR op)
{
	switch((int) op)
	{
	case GRAPHICS_OPERATOR_CLEAR:
		return PIXEL_MANIPULATE_OPERATE_CLEAR;

	case GRAPHICS_OPERATOR_SOURCE:
		return PIXEL_MANIPULATE_OPERATE_SRC;
	case GRAPHICS_OPERATOR_OVER:
		return PIXEL_MANIPULATE_OPERATE_OVER;
	case GRAPHICS_OPERATOR_IN:
		return PIXEL_MANIPULATE_OPERATE_IN;
	case GRAPHICS_OPERATOR_OUT:
		return PIXEL_MANIPULATE_OPERATE_OUT;
	case GRAPHICS_OPERATOR_ATOP:
		return PIXEL_MANIPULATE_OPERATE_ATOP;

	case GRAPHICS_OPERATOR_DEST:
		return PIXEL_MANIPULATE_OPERATE_DST;
	case GRAPHICS_OPERATOR_DEST_OVER:
		return PIXEL_MANIPULATE_OPERATE_OVER_REVERSE;
	case GRAPHICS_OPERATOR_DEST_IN:
		return PIXEL_MANIPULATE_OPERATE_IN_REVERSE;
	case GRAPHICS_OPERATOR_DEST_OUT:
		return PIXEL_MANIPULATE_OPERATE_OUT_REVERSE;
	case GRAPHICS_OPERATOR_DEST_ATOP:
		return PIXEL_MANIPULATE_OPERATE_ATOP_REVERSE;

	case GRAPHICS_OPERATOR_XOR:
		return PIXEL_MANIPULATE_OPERATE_XOR;
	case GRAPHICS_OPERATOR_ADD:
		return PIXEL_MANIPULATE_OPERATE_ADD;
	case GRAPHICS_OPERATOR_SATURATE:
		return PIXEL_MANIPULATE_OPERATE_SATURATE;

	case GRAPHICS_OPERATOR_MULTIPLY:
		return PIXEL_MANIPULATE_OPERATE_MULTIPLY;
	case GRAPHICS_OPERATOR_SCREEN:
		return PIXEL_MANIPULATE_OPERATE_SCREEN;
	case GRAPHICS_OPERATOR_OVERLAY:
		return PIXEL_MANIPULATE_OPERATE_OVERLAY;
	case GRAPHICS_OPERATOR_DARKEN:
		return PIXEL_MANIPULATE_OPERATE_DARKEN;
	case GRAPHICS_OPERATOR_LIGHTEN:
		return PIXEL_MANIPULATE_OPERATE_LIGHTEN;
	case GRAPHICS_OPERATOR_COLOR_DODGE:
		return PIXEL_MANIPULATE_OPERATE_COLOR_DODGE;
	case GRAPHICS_OPERATOR_COLOR_BURN:
		return PIXEL_MANIPULATE_OPERATE_COLOR_BURN;
	case GRAPHICS_OPERATOR_HARD_LIGHT:
		return PIXEL_MANIPULATE_OPERATE_HARD_LIGHT;
	case GRAPHICS_OPERATOR_SOFT_LIGHT:
		return PIXEL_MANIPULATE_OPERATE_SOFT_LIGHT;
	case GRAPHICS_OPERATOR_DIFFERENCE:
		return PIXEL_MANIPULATE_OPERATE_DIFFERENCE;
	case GRAPHICS_OPERATOR_EXCLUSION:
		return PIXEL_MANIPULATE_OPERATE_EXCLUSION;
	case GRAPHICS_OPERATOR_HSL_HUE:
		return PIXEL_MANIPULATE_OPERATE_HSL_HUE;
	case GRAPHICS_OPERATOR_HSL_SATURATION:
		return PIXEL_MANIPULATE_OPERATE_HSL_SATURATION;
	case GRAPHICS_OPERATOR_HSL_COLOR:
		return PIXEL_MANIPULATE_OPERATE_HSL_COLOR;
	case GRAPHICS_OPERATOR_HSL_LUMINOSITY:
		return PIXEL_MANIPULATE_OPERATE_HSL_LUMINOSITY;

	default:
		return PIXEL_MANIPULATE_OPERATE_OVER;
	}
}

static int __fill_reduces_to_source(
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_COLOR* color,
	const GRAPHICS_IMAGE_SURFACE* dst
)
{
	if(op == GRAPHICS_OPERATOR_SOURCE || op == GRAPHICS_OPERATOR_CLEAR)
		return TRUE;
	if(op == GRAPHICS_OPERATOR_OVER && GRAPHICS_COLOR_IS_OPAQUE(color))
		return TRUE;
	if(dst->base.is_clear)
		return op == GRAPHICS_OPERATOR_OVER || op == GRAPHICS_OPERATOR_ADD;

	return FALSE;
}

static int fill_reduces_to_source(
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_COLOR* color,
	const GRAPHICS_IMAGE_SURFACE* dst,
	uint32* pixel
)
{
	if (__fill_reduces_to_source (op, color, dst)) {
		return color_to_pixel (color, dst->pixel_format, pixel);
	}

	return FALSE;
}

static eGRAPHICS_INTEGER_STATUS fill_rectangles(
	void* _dst,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_COLOR* color,
	GRAPHICS_RECTANGLE_INT* rects,
	int num_rects
)
{
	GRAPHICS *graphics;
	GRAPHICS_IMAGE_SURFACE *dst = _dst;
	uint32 pixel;
	int i;

	graphics = (GRAPHICS*)dst->base.graphics;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	if(fill_reduces_to_source (op, color, dst, &pixel))
	{
		for(i = 0; i < num_rects; i++)
		{
			PixelManipulateFill((uint32 *) dst->data, dst->stride / sizeof (uint32),
				PIXEL_MANIPULATE_FORMAT_BPP(dst->pixel_format),
				rects[i].x, rects[i].y,
				rects[i].width, rects[i].height,
				pixel, &graphics->pixel_manipulate.implementation
			);
		}
	}
	else
	{
		PIXEL_MANIPULATE_IMAGE *src = PixelManipulateImageForColor (color, dst->base.graphics);

		if(UNLIKELY (src == NULL))
			return GRAPHICS_STATUS_NO_MEMORY;

		op = _PixelManipulateOperator (op);
		for (i = 0; i < num_rects; i++)
		{
			PixelManipulateImageComposite32(op,
				src, NULL, &dst->image,
				0, 0,
				0, 0,
				rects[i].x, rects[i].y,
				rects[i].width, rects[i].height,
				&graphics->pixel_manipulate.implementation
			);
		}
		
		PixelManipulateImageUnreference(src);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_INTEGER_STATUS fill_boxes(
	void* _dst,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_COLOR* color,
	GRAPHICS_BOXES* boxes
)
{
	GRAPHICS *graphics;
	GRAPHICS_IMAGE_SURFACE *dst = _dst;
	struct _GRAPHICS_BOXES_CHUNK *chunk;
	uint32 pixel;
	int i;

	graphics = (GRAPHICS*)dst->base.graphics;
	
	TRACE ((stderr, "%s x %d\n", __FUNCTION__, boxes->num_boxes));
	
	if (fill_reduces_to_source (op, color, dst, &pixel)) {
		for (chunk = &boxes->chunks; chunk; chunk = chunk->next) {
			for (i = 0; i < chunk->count; i++) {
				int x = GraphicsFixedIntegerPart(chunk->base[i].point1.x);
				int y = GraphicsFixedIntegerPart(chunk->base[i].point1.y);
				int w = GraphicsFixedIntegerPart(chunk->base[i].point2.x) - x;
				int h = GraphicsFixedIntegerPart(chunk->base[i].point2.y) - y;
				PixelManipulateFill((uint32 *) dst->data,
					dst->stride / sizeof (uint32),
					PIXEL_MANIPULATE_BIT_PER_PIXEL(dst->pixel_format),
					x, y, w, h, pixel, &graphics->pixel_manipulate.implementation);
			}
		}
	}
	else
	{
		PIXEL_MANIPULATE_IMAGE *src = PixelManipulateImageForColor(color, dst->base.graphics);
		if (UNLIKELY(src == NULL))
			return GRAPHICS_STATUS_NO_MEMORY;

		op = _PixelManipulateOperator (op);
		for (chunk = &boxes->chunks; chunk; chunk = chunk->next) {
			for (i = 0; i < chunk->count; i++) {
				int x1 = GraphicsFixedIntegerPart(chunk->base[i].point1.x);
				int y1 = GraphicsFixedIntegerPart(chunk->base[i].point1.y);
				int x2 = GraphicsFixedIntegerPart(chunk->base[i].point2.x);
				int y2 = GraphicsFixedIntegerPart(chunk->base[i].point2.y);
				PixelManipulateImageComposite32(op,
					src, NULL, &dst->image,
					0, 0,
					0, 0,
					x1, y1,
					x2-x1, y2-y1,
					&graphics->pixel_manipulate.implementation
				);
			}
		}

		PixelManipulateImageUnreference(src);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_INTEGER_STATUS composite(
	void* _dst,
	eGRAPHICS_OPERATOR op,
	GRAPHICS_SURFACE* abstract_src,
	GRAPHICS_SURFACE* abstract_mask,
	int src_x,
	int src_y,
	int mask_x,
	int mask_y,
	int dst_x,
	int dst_y,
	unsigned int width,
	unsigned int height
)
{
	GRAPHICS *graphics;
	GRAPHICS_IMAGE_SOURCE *src = (GRAPHICS_IMAGE_SOURCE *)abstract_src;
	GRAPHICS_IMAGE_SOURCE *mask = (GRAPHICS_IMAGE_SOURCE *)abstract_mask;

	graphics = (GRAPHICS*)src->base.graphics;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	if(mask)
	{
		PixelManipulateImageComposite32(_PixelManipulateOperator (op),
			&src->image, &mask->image, ToPixelManipulateImage(_dst),
			src_x, src_y,
			mask_x, mask_y,
			dst_x, dst_y,
			width, height,
			&graphics->pixel_manipulate.implementation
		);
	}
	else
	{
		PixelManipulateImageComposite32(_PixelManipulateOperator (op),
			&src->image, NULL, ToPixelManipulateImage(_dst),
			src_x, src_y,
			0, 0,
			dst_x, dst_y,
			width, height,
			&graphics->pixel_manipulate.implementation
		);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_INTEGER_STATUS
lerp (void			*_dst,
	GRAPHICS_SURFACE		*abstract_src,
	GRAPHICS_SURFACE		*abstract_mask,
	int			src_x,
	int			src_y,
	int			mask_x,
	int			mask_y,
	int			dst_x,
	int			dst_y,
	unsigned int		width,
	unsigned int		height)
{
	GRAPHICS *graphics;
	GRAPHICS_IMAGE_SURFACE *dst = _dst;
	GRAPHICS_IMAGE_SOURCE *src = (GRAPHICS_IMAGE_SOURCE *)abstract_src;
	GRAPHICS_IMAGE_SOURCE *mask = (GRAPHICS_IMAGE_SOURCE *)abstract_mask;

	graphics = (GRAPHICS*)dst->base.graphics;

	TRACE ((stderr, "%s\n", __FUNCTION__));

#if PIXMAN_HAS_OP_LERP
	PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_LERP_SRC,
		src->image, mask->image, dst->image,
		src_x,  src_y,
		mask_x, mask_y,
		dst_x,  dst_y,
		width,  height,
		&graphics->pixel_manipulate.implementation
	);
#else
	/* Punch the clip out of the destination */
	TRACE ((stderr, "%s - OUT_REVERSE (mask=%d/%p, dst=%d/%p)\n",
		__FUNCTION__,
		mask->base.unique_id, mask->image,
		dst->base.unique_id, dst->image));
	PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_OUT_REVERSE,
		&mask->image, NULL, &dst->image,
		mask_x, mask_y,
		0,	  0,
		dst_x,  dst_y,
		width,  height,
		&graphics->pixel_manipulate.implementation
	);

	/* Now add the two results together */
	TRACE ((stderr, "%s - ADD (src=%d/%p, mask=%d/%p, dst=%d/%p)\n",
		__FUNCTION__,
		src->base.unique_id, src->image,
		mask->base.unique_id, mask->image,
		dst->base.unique_id, dst->image));
	PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_ADD,
		&src->image, &mask->image, &dst->image,
		src_x,  src_y,
		mask_x, mask_y,
		dst_x,  dst_y,
		width,  height,
		&graphics->pixel_manipulate.implementation
	);
#endif

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_INTEGER_STATUS composite_boxes(
	void* _dst,
	eGRAPHICS_OPERATOR op,
	GRAPHICS_SURFACE* abstract_src,
	GRAPHICS_SURFACE* abstract_mask,
	int src_x,
	int src_y,
	int mask_x,
	int mask_y,
	int dst_x,
	int dst_y,
	GRAPHICS_BOXES* boxes,
	const GRAPHICS_RECTANGLE_INT* extents
)
{
	GRAPHICS *graphics;
	PIXEL_MANIPULATE_IMAGE *dst = ToPixelManipulateImage(_dst);
	PIXEL_MANIPULATE_IMAGE *src = &((GRAPHICS_IMAGE_SOURCE *)abstract_src)->image;
	PIXEL_MANIPULATE_IMAGE *mask = abstract_mask ? &((GRAPHICS_IMAGE_SOURCE *)abstract_mask)->image : NULL;
	PIXEL_MANIPULATE_IMAGE *free_src = NULL;
	struct _GRAPHICS_BOXES_CHUNK *chunk;
	int i;

	/* XXX consider using a region? saves multiple prepare-composite */
	// TRACE ((stderr, "%s x %d\n", __FUNCTION__, boxes->num_boxes));

	graphics = ((GRAPHICS_IMAGE_SURFACE*)_dst)->base.graphics;

	if(((GRAPHICS_SURFACE *)_dst)->is_clear &&
		(op == GRAPHICS_OPERATOR_SOURCE ||
			op == GRAPHICS_OPERATOR_OVER ||
			op == GRAPHICS_OPERATOR_ADD)) {
		op = PIXEL_MANIPULATE_OPERATE_SRC;
	}
	else if(mask != NULL)
	{
		if(op == GRAPHICS_OPERATOR_CLEAR)
		{
#if 0 // PIXMAN_HAS_OP_LERP
			op = PIXEL_MANIPULATE_OPERATE_LERP_CLEAR;
#else
			free_src = src = PixelManipulateImageForColor(&graphics->color_white, graphics);
			if(UNLIKELY (src == NULL))
				return GRAPHICS_STATUS_NO_MEMORY;
			op = PIXEL_MANIPULATE_OPERATE_OUT_REVERSE;
#endif
		} else if (op == GRAPHICS_OPERATOR_SOURCE) {
#if PIXMAN_HAS_OP_LERP
			op = PIXEL_MANIPULATE_OPERATE_LERP_SRC;
#else
			return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
#endif
		} else {
			op = _PixelManipulateOperator (op);
		}
	} else {
		op = _PixelManipulateOperator (op);
	}

	for (chunk = &boxes->chunks; chunk; chunk = chunk->next) {
		for (i = 0; i < chunk->count; i++) {
			int x1 = GraphicsFixedIntegerPart(chunk->base[i].point1.x);
			int y1 = GraphicsFixedIntegerPart(chunk->base[i].point1.y);
			int x2 = GraphicsFixedIntegerPart(chunk->base[i].point2.x);
			int y2 = GraphicsFixedIntegerPart(chunk->base[i].point2.y);

			PixelManipulateImageComposite32(op, src, mask, dst,
				x1 + src_x, y1 + src_y,
				x1 + mask_x, y1 + mask_y,
				x1 + dst_x, y1 + dst_y,
				x2 - x1, y2 - y1,
				&graphics->pixel_manipulate.implementation
			);
		}
	}

	if(free_src)
	{
		PixelManipulateImageUnreference(free_src);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

#define GRAPHICS_FIXED_16_16_MIN GraphicsFixedFromInteger(-32768)
#define GRAPHICS_FIXED_16_16_MAX GraphicsFixedFromInteger(32767)

static int line_exceeds_16_16 (const GRAPHICS_LINE *line)
{
	return
		line->point1.x <= GRAPHICS_FIXED_16_16_MIN ||
		line->point1.x >= GRAPHICS_FIXED_16_16_MAX ||

		line->point2.x <= GRAPHICS_FIXED_16_16_MIN ||
		line->point2.x >= GRAPHICS_FIXED_16_16_MAX ||

		line->point1.y <= GRAPHICS_FIXED_16_16_MIN ||
		line->point1.y >= GRAPHICS_FIXED_16_16_MAX ||

		line->point2.y <= GRAPHICS_FIXED_16_16_MIN ||
		line->point2.y >= GRAPHICS_FIXED_16_16_MAX;
}

static void project_line_x_onto_16_16(
	const GRAPHICS_LINE* line,
	GRAPHICS_FLOAT_FIXED top,
	GRAPHICS_FLOAT_FIXED bottom,
	PIXEL_MANIPULATE_LINE_FIXED* out
)
{
	/* XXX use fixed-point arithmetic? */
	GRAPHICS_POINT_FLOAT p1, p2;
	double m;
	
	p1.x = GRAPHICS_FIXED_TO_FLOAT(line->point1.x);
	p1.y = GRAPHICS_FIXED_TO_FLOAT(line->point1.y);

	p2.x = GRAPHICS_FIXED_TO_FLOAT(line->point2.x);
	p2.y = GRAPHICS_FIXED_TO_FLOAT(line->point2.y);

	m = (p2.x - p1.x) / (p2.y - p1.y);
	out->point1.x = GraphicsFixed16_16FromFloat(p1.x + m * GRAPHICS_FIXED_TO_FLOAT(top - line->point1.y));
	out->point2.x = GraphicsFixed16_16FromFloat(p1.x + m * GRAPHICS_FIXED_TO_FLOAT(bottom - line->point1.y));
}

void _pixman_image_add_traps(
	PIXEL_MANIPULATE_IMAGE* image,
	int dst_x, int dst_y,
	GRAPHICS_TRAPS* traps
)
{
	GRAPHICS_TRAPEZOID *t = traps->traps;
	int num_traps = traps->num_traps;
	while(num_traps--)
	{
		GRAPHICS_TRAPEZOID trap;
		
		/* top/bottom will be clamped to surface bounds */
		trap.top = GraphicsFixedTo16_16(t->top);
		trap.bottom = GraphicsFixedTo16_16(t->bottom);

		/* However, all the other coordinates will have been left untouched so
		* as not to introduce numerical error. Recompute them if they
		* exceed the 16.16 limits.
		*/
		if (UNLIKELY (line_exceeds_16_16 (&t->left))) {
			project_line_x_onto_16_16 (&t->left, t->top, t->bottom, &trap.left);
			trap.left.point1.y = trap.top;
			trap.left.point2.y = trap.bottom;
		} else {
			trap.left.point1.x = GraphicsFixedTo16_16(t->left.point1.x);
			trap.left.point1.y = GraphicsFixedTo16_16(t->left.point1.y);
			trap.left.point2.x = GraphicsFixedTo16_16(t->left.point2.x);
			trap.left.point2.y = GraphicsFixedTo16_16(t->left.point2.y);
		}

		if (UNLIKELY (line_exceeds_16_16 (&t->right))) {
			project_line_x_onto_16_16 (&t->right, t->top, t->bottom, &trap.right);
			trap.right.point1.y = trap.top;
			trap.right.point2.y = trap.bottom;
		} else {
			trap.right.point1.x = GraphicsFixedTo16_16(t->right.point1.x);
			trap.right.point1.y = GraphicsFixedTo16_16(t->right.point1.y);
			trap.right.point2.x = GraphicsFixedTo16_16(t->right.point2.x);
			trap.right.point2.y = GraphicsFixedTo16_16(t->right.point2.y);
		}
		
		PixelManipulateRasterizeTrapezoid(image, &trap, -dst_x, -dst_y);
		t++;
	}
}

static eGRAPHICS_INTEGER_STATUS composite_traps(
	void* _dst,
	eGRAPHICS_OPERATOR op,
	GRAPHICS_SURFACE* abstract_src,
	int src_x,
	int src_y,
	int dst_x,
	int dst_y,
	const GRAPHICS_RECTANGLE_INT* extents,
	eGRAPHICS_ANTIALIAS	antialias,
	GRAPHICS_TRAPS* traps
)
{
	GRAPHICS *graphics;
	GRAPHICS_IMAGE_SURFACE *dst = (GRAPHICS_IMAGE_SURFACE *) _dst;
	GRAPHICS_IMAGE_SOURCE *src = (GRAPHICS_IMAGE_SOURCE *) abstract_src;
	eGRAPHICS_INTEGER_STATUS status;
	PIXEL_MANIPULATE_IMAGE *mask, mask_local;
	ePIXEL_MANIPULATE_FORMAT format;

	graphics = (GRAPHICS*)dst->base.graphics;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	/* pixman doesn't eliminate self-intersecting trapezoids/edges */
	status = GraphicsBentleyOttmannTessellateTraps(traps, GRAPHICS_FILL_RULE_WINDING);
	if (status != GRAPHICS_INTEGER_STATUS_SUCCESS)
		return status;

	/* Special case adding trapezoids onto a mask surface; we want to avoid
	* creating an intermediate temporary mask unnecessarily.
	*
	* We make the assumption here that the portion of the trapezoids
	* contained within the surface is bounded by [dst_x,dst_y,width,height];
	* the Cairo core code passes bounds based on the trapezoid extents.
	*/
	format = antialias ==
		GRAPHICS_ANTIALIAS_NONE ? PIXEL_MANIPULATE_FORMAT_A1 : PIXEL_MANIPULATE_FORMAT_A8;
	if (dst->pixel_format == format &&
		(abstract_src == NULL ||
		(op == GRAPHICS_OPERATOR_ADD && src->opaque_solid)))
	{
		_pixman_image_add_traps(&dst->image, dst_x, dst_y, traps);
		return GRAPHICS_STATUS_SUCCESS;
	}
	
	if(PixelManipulateImageInitialize(&mask_local, format, extents->width, extents->height, NULL, 0, TRUE,
		&graphics->pixel_manipulate))
	{
		mask = &mask_local;
	}
	else
	{
		mask = NULL;
	}
	//mask = CreatePixelManipulateImageBits (format,
	//	extents->width, extents->height,
	//	NULL, 0);
	if (UNLIKELY (mask == NULL))
		return GRAPHICS_STATUS_NO_MEMORY;

	_pixman_image_add_traps (mask, extents->x, extents->y, traps);
	PixelManipulateImageComposite32(_PixelManipulateOperator (op),
		&src->image, mask, &dst->image,
		extents->x + src_x, extents->y + src_y,
		0, 0,
		extents->x - dst_x, extents->y - dst_y,
		extents->width, extents->height,
		&graphics->pixel_manipulate.implementation
	);

	PixelManipulateImageUnreference(mask);

	return  GRAPHICS_STATUS_SUCCESS;
}

#if 1 // PIXMAN_VERSION >= PIXMAN_VERSION_ENCODE(0,22,0)
static void
set_point (PIXEL_MANIPULATE_POINT_FIXED *p, GRAPHICS_POINT *c)
{
	p->x = GraphicsFixedTo16_16(c->x);
	p->y = GraphicsFixedTo16_16(c->y);
}

void _pixman_image_add_tristrip(
	PIXEL_MANIPULATE_IMAGE* image,
	int dst_x, int dst_y,
	GRAPHICS_TRISTRIP* strip
)
{
	PIXEL_MANIPULATE_TRIANGLE tri;
	PIXEL_MANIPULATE_POINT_FIXED *p[3] = {&tri.point1, &tri.point2, &tri.point3 };
	int n;

	set_point (p[0], &strip->points[0]);
	set_point (p[1], &strip->points[1]);
	set_point (p[2], &strip->points[2]);
	PixelManipulateAddTriangles(image, -dst_x, -dst_y, 1, &tri);
	for (n = 3; n < strip->num_points; n++) {
		set_point (p[n%3], &strip->points[n]);
		PixelManipulateAddTriangles(image, -dst_x, -dst_y, 1, &tri);
	}
}

static eGRAPHICS_INTEGER_STATUS composite_tristrip(
	void* _dst,
	eGRAPHICS_OPERATOR op,
	GRAPHICS_SURFACE* abstract_src,
	int src_x,
	int src_y,
	int dst_x,
	int dst_y,
	const GRAPHICS_RECTANGLE_INT* extents,
	eGRAPHICS_ANTIALIAS	antialias,
	GRAPHICS_TRISTRIP* strip
)
{
	GRAPHICS *graphics;
	GRAPHICS_IMAGE_SURFACE *dst = (GRAPHICS_IMAGE_SURFACE *) _dst;
	GRAPHICS_IMAGE_SOURCE *src = (GRAPHICS_IMAGE_SOURCE *) abstract_src;
	PIXEL_MANIPULATE_IMAGE *mask, mask_local;
	ePIXEL_MANIPULATE_FORMAT format;

	graphics = (GRAPHICS*)dst->base.graphics;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	if (strip->num_points < 3)
		return GRAPHICS_STATUS_SUCCESS;

	if (1) { /* pixman doesn't eliminate self-intersecting triangles/edges */
		eGRAPHICS_INTEGER_STATUS status;
		GRAPHICS_TRAPS traps;
		int n;
		
		InitializeGraphicsTraps(&traps);
		for (n = 0; n < strip->num_points; n++) {
			GRAPHICS_POINT p[4];

			p[0] = strip->points[0];
			p[1] = strip->points[1];
			p[2] = strip->points[2];
			p[3] = strip->points[0];

			GraphicsTrapsTessellateConvexQuad(&traps, p);
		}
		status = composite_traps(_dst, op, abstract_src,
			src_x, src_y,
			dst_x, dst_y,
			extents, antialias, &traps);
		GraphicsTrapsFinish(&traps);

		return status;
	}

	format = antialias ==
		GRAPHICS_ANTIALIAS_NONE ?  PIXEL_MANIPULATE_FORMAT_A1 : PIXEL_MANIPULATE_FORMAT_A8;
	if(dst->pixel_format == format &&
		(abstract_src == NULL ||
		(op == GRAPHICS_OPERATOR_ADD && src->opaque_solid)))
	{
		_pixman_image_add_tristrip (&dst->image, dst_x, dst_y, strip);
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(PixelManipulateImageInitialize(&mask_local, format, extents->width, extents->height, NULL, 0, TRUE,
		&(((GRAPHICS*)dst->base.graphics)->pixel_manipulate)))
	{
		mask = &mask_local;
	}
	else
	{
		mask = NULL;
	}
	// mask = CreatePixelManipulateImageBits (format,
	//	extents->width, extents->height,
	//	NULL, 0);
	if(UNLIKELY(mask == NULL))
		return GRAPHICS_STATUS_NO_MEMORY;

	_pixman_image_add_tristrip (mask, extents->x, extents->y, strip);
	PixelManipulateImageComposite32(_PixelManipulateOperator (op),
		&src->image, mask, &dst->image,
		extents->x + src_x, extents->y + src_y,
		0, 0,
		extents->x - dst_x, extents->y - dst_y,
		extents->width, extents->height,
		&graphics->pixel_manipulate.implementation
	);

	PixelManipulateImageUnreference(mask);

	return  GRAPHICS_STATUS_SUCCESS;
}
#endif

#if 0
static eGRAPHICS_INTEGER_STATUS
check_composite_glyphs (const GRAPHICS_COMPOSITE_RECTANGLES *extents,
	cairo_scaled_font_t *scaled_font,
	cairo_glyph_t *glyphs,
	int *num_glyphs)
{
	return GRAPHICS_STATUS_SUCCESS;
}

#if HAS_PIXMAN_GLYPHS
static pixman_glyph_cache_t *global_glyph_cache;

static INLINE pixman_glyph_cache_t *
get_glyph_cache (void)
{
	if (!global_glyph_cache)
		global_glyph_cache = pixman_glyph_cache_create ();

	return global_glyph_cache;
}

void
_cairo_image_compositor_reset_static_data (void)
{
	CAIRO_MUTEX_LOCK (_cairo_glyph_cache_mutex);

	if (global_glyph_cache)
		pixman_glyph_cache_destroy (global_glyph_cache);
	global_glyph_cache = NULL;

	CAIRO_MUTEX_UNLOCK (_cairo_glyph_cache_mutex);
}

void _cairo_image_scaled_glyph_fini(cairo_scaled_font_t *scaled_font,
	cairo_scaled_glyph_t *scaled_glyph)
{
	CAIRO_MUTEX_LOCK (_cairo_glyph_cache_mutex);

	if (global_glyph_cache) {
		pixman_glyph_cache_remove (
			global_glyph_cache, scaled_font,
			(void *)_cairo_scaled_glyph_index (scaled_glyph));
	}

	CAIRO_MUTEX_UNLOCK (_cairo_glyph_cache_mutex);
}

static eGRAPHICS_INTEGER_STATUS
composite_glyphs (void				*_dst,
	eGRAPHICS_OPERATOR		 op,
	GRAPHICS_SURFACE		*_src,
	int				 src_x,
	int				 src_y,
	int				 dst_x,
	int				 dst_y,
	cairo_composite_glyphs_info_t *info)
{
	eGRAPHICS_INTEGER_STATUS status = GRAPHICS_INTEGER_STATUS_SUCCESS;
	pixman_glyph_cache_t *glyph_cache;
	pixman_glyph_t pglyphs_stack[CAIRO_STACK_ARRAY_LENGTH (pixman_glyph_t)];
	pixman_glyph_t *pglyphs = pglyphs_stack;
	pixman_glyph_t *pg;
	int i;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	CAIRO_MUTEX_LOCK (_cairo_glyph_cache_mutex);

	glyph_cache = get_glyph_cache();
	if (UNLIKELY (glyph_cache == NULL)) {
		status = GRAPHICS_STATUS_NO_MEMORY;
		goto out_unlock;
	}

	pixman_glyph_cache_freeze (glyph_cache);

	if (info->num_glyphs > ARRAY_LENGTH (pglyphs_stack)) {
		pglyphs = MEM_ALLOC_FUNC(info->num_glyphs * sizeof (pixman_glyph_t));
		if (UNLIKELY (pglyphs == NULL)) {
			status = GRAPHICS_STATUS_NO_MEMORY;
			goto out_thaw;
		}
	}

	pg = pglyphs;
	for (i = 0; i < info->num_glyphs; i++) {
		unsigned long index = info->glyphs[i].index;
		const void *glyph;

		glyph = pixman_glyph_cache_lookup (glyph_cache, info->font, (void *)index);
		if (!glyph) {
			cairo_scaled_glyph_t *scaled_glyph;
			GRAPHICS_IMAGE_SURFACE *glyph_surface;

			/* This call can actually end up recursing, so we have to
			* drop the mutex around it.
			*/
			CAIRO_MUTEX_UNLOCK (_cairo_glyph_cache_mutex);
			status = _cairo_scaled_glyph_lookup (info->font, index,
				CAIRO_SCALED_GLYPH_INFO_SURFACE,
				&scaled_glyph);
			CAIRO_MUTEX_LOCK (_cairo_glyph_cache_mutex);

			if (UNLIKELY (status))
				goto out_thaw;

			glyph_surface = scaled_glyph->surface;
			glyph = pixman_glyph_cache_insert (glyph_cache, info->font, (void *)index,
				glyph_surface->base.device_transform.x0,
				glyph_surface->base.device_transform.y0,
				glyph_surface->image);
			if (UNLIKELY (!glyph)) {
				status = GRAPHICS_STATUS_NO_MEMORY;
				goto out_thaw;
			}
		}

		pg->x = _cairo_lround (info->glyphs[i].x);
		pg->y = _cairo_lround (info->glyphs[i].y);
		pg->glyph = glyph;
		pg++;
	}

	if (info->use_mask) {
		ePIXEL_MANIPULATE_FORMAT mask_format;

		mask_format = pixman_glyph_get_mask_format (glyph_cache, pg - pglyphs, pglyphs);

		pixman_composite_glyphs (_PixelManipulateOperator (op),
			((GRAPHICS_IMAGE_SOURCE *)_src)->image,
			ToPixelManipulateImage(_dst),
			mask_format,
			info->extents.x + src_x, info->extents.y + src_y,
			info->extents.x, info->extents.y,
			info->extents.x - dst_x, info->extents.y - dst_y,
			info->extents.width, info->extents.height,
			glyph_cache, pg - pglyphs, pglyphs);
	} else {
		pixman_composite_glyphs_no_mask (_PixelManipulateOperator (op),
			((GRAPHICS_IMAGE_SOURCE *)_src)->image,
			ToPixelManipulateImage(_dst),
			src_x, src_y,
			- dst_x, - dst_y,
			glyph_cache, pg - pglyphs, pglyphs);
	}

out_thaw:
	pixman_glyph_cache_thaw (glyph_cache);

	if (pglyphs != pglyphs_stack)
		free(pglyphs);

out_unlock:
	CAIRO_MUTEX_UNLOCK (_cairo_glyph_cache_mutex);
	return status;
}
#else
void
_cairo_image_compositor_reset_static_data (void)
{
}

void
_cairo_image_scaled_glyph_fini (cairo_scaled_font_t *scaled_font,
	cairo_scaled_glyph_t *scaled_glyph)
{
}

static eGRAPHICS_INTEGER_STATUS
composite_one_glyph (void				*_dst,
	eGRAPHICS_OPERATOR			 op,
	GRAPHICS_SURFACE			*_src,
	int				 src_x,
	int				 src_y,
	int				 dst_x,
	int				 dst_y,
	cairo_composite_glyphs_info_t	 *info)
{
	GRAPHICS_IMAGE_SURFACE *glyph_surface;
	cairo_scaled_glyph_t *scaled_glyph;
	eGRAPHICS_STATUS status;
	int x, y;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	status = _cairo_scaled_glyph_lookup (info->font,
		info->glyphs[0].index,
		CAIRO_SCALED_GLYPH_INFO_SURFACE,
		&scaled_glyph);

	if (UNLIKELY (status))
		return status;

	glyph_surface = scaled_glyph->surface;
	if (glyph_surface->width == 0 || glyph_surface->height == 0)
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;

	/* round glyph locations to the nearest pixel */
	/* XXX: FRAGILE: We're ignoring device_transform scaling here. A bug? */
	x = _cairo_lround (info->glyphs[0].x -
		glyph_surface->base.device_transform.x0);
	y = _cairo_lround (info->glyphs[0].y -
		glyph_surface->base.device_transform.y0);

	PixelManipulateImageComposite32(_PixelManipulateOperator (op),
		((GRAPHICS_IMAGE_SOURCE *)_src)->image,
		glyph_surface->image,
		ToPixelManipulateImage(_dst),
		x + src_x,  y + src_y,
		0, 0,
		x - dst_x, y - dst_y,
		glyph_surface->width,
		glyph_surface->height);

	return GRAPHICS_INTEGER_STATUS_SUCCESS;
}

static eGRAPHICS_INTEGER_STATUS composite_glyphs_via_mask(
	void				*_dst,
	eGRAPHICS_OPERATOR		 op,
	GRAPHICS_SURFACE		*_src,
	int				 src_x,
	int				 src_y,
	int				 dst_x,
	int				 dst_y,
	cairo_composite_glyphs_info_t *info
)
{
	cairo_scaled_glyph_t *glyph_cache[64];
	PIXEL_MANIPULATE_IMAGE *white = PixelManipulateImageForColor (CAIRO_COLOR_WHITE);
	cairo_scaled_glyph_t *scaled_glyph;
	uint8 buf[2048];
	PIXEL_MANIPULATE_IMAGE *mask = NULL, mask_local;
	ePIXEL_MANIPULATE_FORMAT format;
	eGRAPHICS_STATUS status;
	int i;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	if (UNLIKELY (white == NULL))
		return GRAPHICS_STATUS_NO_MEMORY;

	/* XXX convert the glyphs to common formats a8/a8r8g8b8 to hit
	* optimised paths through pixman. Should we increase the bit
	* depth of the target surface, we should reconsider the appropriate
	* mask formats.
	*/

	status = _cairo_scaled_glyph_lookup (info->font,
		info->glyphs[0].index,
		CAIRO_SCALED_GLYPH_INFO_SURFACE,
		&scaled_glyph);
	if (UNLIKELY (status)) {
		PixelManipulateImageUnreference(white);
		return status;
	}

	memset (glyph_cache, 0, sizeof (glyph_cache));
	glyph_cache[info->glyphs[0].index % ARRAY_LENGTH (glyph_cache)] = scaled_glyph;

	format = PIXEL_MANIPULATE_FORMAT_A8;
	i = (info->extents.width + 3) & ~3;
	if (scaled_glyph->surface->base.content & CAIRO_CONTENT_COLOR) {
		format = PIXEL_MANIPULATE_FORMAT_A8R8G8B8;
		i = info->extents.width * 4;
	}

	if (i * info->extents.height > (int) sizeof (buf))
	{
		if(PixelManipulateImageInitialize(&mask_local, format, info->extents.width,
			info->extents.height, NULL, 0, TRUE)
		{
			mask = &mask_local;
		}
		//mask = CreatePixelManipulateImageBits (format,
		//	info->extents.width,
		//	info->extents.height,
		//	NULL, 0);
	}
	else
	{
		(void)memset(buf, 0, i * info->extents.height);
		if(PixelManipulateImageInitializeWithBits(&mask_local, format,
			info->extents.width, info->extents.height, (uint32*)buf, i, TRUE))
		{
			mask = &mask_local;
		}
		//mask = CreatePixelManipulateImageBits (format,
		//	info->extents.width,
		//	info->extents.height,
		//	(uint32 *)buf, i);
	}
	if(UNLIKELY(mask == NULL))
	{
		PixelManipulateImageUnreference(white);
		return GRAPHICS_STATUS_NO_MEMORY;
	}

	status = GRAPHICS_STATUS_SUCCESS;
	for (i = 0; i < info->num_glyphs; i++) {
		unsigned long glyph_index = info->glyphs[i].index;
		int cache_index = glyph_index % ARRAY_LENGTH (glyph_cache);
		GRAPHICS_IMAGE_SURFACE *glyph_surface;
		int x, y;

		scaled_glyph = glyph_cache[cache_index];
		if (scaled_glyph == NULL ||
			_cairo_scaled_glyph_index (scaled_glyph) != glyph_index)
		{
			status = _cairo_scaled_glyph_lookup (info->font, glyph_index,
				CAIRO_SCALED_GLYPH_INFO_SURFACE,
				&scaled_glyph);

			if (UNLIKELY (status)) {
				PixelManipulateImageUnreference(mask);
				PixelManipulateImageUnreference(white);
				return status;
			}

			glyph_cache[cache_index] = scaled_glyph;
		}

		glyph_surface = scaled_glyph->surface;
		if(glyph_surface->width && glyph_surface->height)
		{
			if (glyph_surface->base.content & CAIRO_CONTENT_COLOR &&
				format == PIXEL_MANIPULATE_FORMAT_A8)
			{
				PIXEL_MANIPULATE_IMAGE *ca_mask = NULL, ca_mask_local;

				format = PIXEL_MANIPULATE_FORMAT_A8R8G8B8;
				if(PixelManipulateImageInitialize(&ca_mask_loca, format, info->extents.width,
					info->extents.height, NULL, 0, TRUE))
				{
					ca_mask = &ca_mask_local;
				}
				//ca_mask = CreatePixelManipulateImageBits (format,
				//	info->extents.width,
				//	info->extents.height,
				//	NULL, 0);
				if (UNLIKELY (ca_mask == NULL)) {
					PixelManipulateImageUnreference(mask);
					PixelManipulateImageUnreference(white);
					return GRAPHICS_STATUS_NO_MEMORY;
				}

				PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_SRC,
					white, mask, ca_mask,
					0, 0,
					0, 0,
					0, 0,
					info->extents.width,
					info->extents.height);
				PixelManipulateImageUnreference(mask);
				mask = ca_mask;
			}

			/* round glyph locations to the nearest pixel */
			/* XXX: FRAGILE: We're ignoring device_transform scaling here. A bug? */
			x = _cairo_lround (info->glyphs[i].x -
				glyph_surface->base.device_transform.x0);
			y = _cairo_lround (info->glyphs[i].y -
				glyph_surface->base.device_transform.y0);

			if (glyph_surface->pixel_format == format) {
				PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_ADD,
					glyph_surface->image, NULL, mask,
					0, 0,
					0, 0,
					x - info->extents.x, y - info->extents.y,
					glyph_surface->width,
					glyph_surface->height);
			} else {
				PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_ADD,
					white, glyph_surface->image, mask,
					0, 0,
					0, 0,
					x - info->extents.x, y - info->extents.y,
					glyph_surface->width,
					glyph_surface->height);
			}
		}
	}

	if(format == PIXEL_MANIPULATE_FORMAT_A8R8G8B8)
		pixman_image_set_component_alpha (mask, TRUE);

	PixelManipulateImageComposite32(_PixelManipulateOperator (op),
		((GRAPHICS_IMAGE_SOURCE *)_src)->image,
		mask,
		ToPixelManipulateImage(_dst),
		info->extents.x + src_x, info->extents.y + src_y,
		0, 0,
		info->extents.x - dst_x, info->extents.y - dst_y,
		info->extents.width, info->extents.height);
	PixelManipulateImageUnreference(mask);
	PixelManipulateImageUnreference(white);

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_INTEGER_STATUS
composite_glyphs (void				*_dst,
	eGRAPHICS_OPERATOR		 op,
	GRAPHICS_SURFACE		*_src,
	int				 src_x,
	int				 src_y,
	int				 dst_x,
	int				 dst_y,
	cairo_composite_glyphs_info_t *info)
{
	cairo_scaled_glyph_t *glyph_cache[64];
	PIXEL_MANIPULATE_IMAGE *dst, *src;
	eGRAPHICS_STATUS status;
	int i;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	if (info->num_glyphs == 1)
		return composite_one_glyph(_dst, op, _src, src_x, src_y, dst_x, dst_y, info);

	if (info->use_mask)
		return composite_glyphs_via_mask(_dst, op, _src, src_x, src_y, dst_x, dst_y, info);

	op = _PixelManipulateOperator (op);
	dst = ToPixelManipulateImage(_dst);
	src = ((GRAPHICS_IMAGE_SOURCE *)_src)->image;

	memset (glyph_cache, 0, sizeof (glyph_cache));
	status = GRAPHICS_STATUS_SUCCESS;

	for (i = 0; i < info->num_glyphs; i++) {
		int x, y;
		GRAPHICS_IMAGE_SURFACE *glyph_surface;
		cairo_scaled_glyph_t *scaled_glyph;
		unsigned long glyph_index = info->glyphs[i].index;
		int cache_index = glyph_index % ARRAY_LENGTH (glyph_cache);

		scaled_glyph = glyph_cache[cache_index];
		if (scaled_glyph == NULL ||
			_cairo_scaled_glyph_index (scaled_glyph) != glyph_index)
		{
			status = _cairo_scaled_glyph_lookup (info->font, glyph_index,
				CAIRO_SCALED_GLYPH_INFO_SURFACE,
				&scaled_glyph);

			if (UNLIKELY (status))
				break;

			glyph_cache[cache_index] = scaled_glyph;
		}

		glyph_surface = scaled_glyph->surface;
		if (glyph_surface->width && glyph_surface->height) {
			/* round glyph locations to the nearest pixel */
			/* XXX: FRAGILE: We're ignoring device_transform scaling here. A bug? */
			x = _cairo_lround (info->glyphs[i].x -
				glyph_surface->base.device_transform.x0);
			y = _cairo_lround (info->glyphs[i].y -
				glyph_surface->base.device_transform.y0);

			PixelManipulateImageComposite32(op, src, glyph_surface->image, dst,
				x + src_x,  y + src_y,
				0, 0,
				x - dst_x, y - dst_y,
				glyph_surface->width,
				glyph_surface->height);
		}
	}

	return status;
}
#endif

#endif

static eGRAPHICS_INTEGER_STATUS
check_composite (const GRAPHICS_COMPOSITE_RECTANGLES *extents)
{
	return GRAPHICS_STATUS_SUCCESS;
}

/*
const GRAPHICS_COMPOSITOR *
_cairo_image_traps_compositor_get (void)
{
	static cairo_atomic_once_t once = CAIRO_ATOMIC_ONCE_INIT;
	static cairo_traps_compositor_t compositor;

	if (_cairo_atomic_init_once_enter(&once)) {
		_cairo_traps_compositor_init(&compositor,
			&__cairo_no_compositor);
		compositor.acquire = acquire;
		compositor.release = release;
		compositor.set_clip_region = set_clip_region;
		compositor.pattern_to_surface = _cairo_image_source_create_for_pattern;
		compositor.draw_image_boxes = draw_image_boxes;
		//compositor.copy_boxes = copy_boxes;
		compositor.fill_boxes = fill_boxes;
		compositor.check_composite = check_composite;
		compositor.composite = composite;
		compositor.lerp = lerp;
		//compositor.check_composite_boxes = check_composite_boxes;
		compositor.composite_boxes = composite_boxes;
		//compositor.check_composite_traps = check_composite_traps;
		compositor.composite_traps = composite_traps;
		//compositor.check_composite_tristrip = check_composite_traps;
#if PIXMAN_VERSION >= PIXMAN_VERSION_ENCODE(0,22,0)
		compositor.composite_tristrip = composite_tristrip;
#endif
		compositor.check_composite_glyphs = check_composite_glyphs;
		compositor.composite_glyphs = composite_glyphs;

		_cairo_atomic_init_once_leave(&once);
	}

	return &compositor.base;
}
*/
void InitializeGraphicsImageTrapsCompositor(GRAPHICS_TRAPS_COMPOSITOR* compositor, void* graphics)
{
	GRAPHICS *g = (GRAPHICS*)graphics;
		_cairo_traps_compositor_init(&compositor,
			&g->no_compositor);
		compositor->acquire = Acquire;
		compositor->release = Release;
		compositor->set_clip_region = SetClipRegion;
		compositor->pattern_to_surface = GraphicsImageSourceCreateForPattern;
		compositor->draw_image_boxes = DrawImageBoxes;
		//compositor.copy_boxes = copy_boxes;
		compositor->fill_boxes = fill_boxes;
		compositor->check_composite = check_composite;
		compositor->composite = composite;
		compositor->lerp = lerp;
		//compositor.check_composite_boxes = check_composite_boxes;
		compositor->composite_boxes = composite_boxes;
		//compositor.check_composite_traps = check_composite_traps;
		compositor->composite_traps = composite_traps;
		//compositor.check_composite_tristrip = check_composite_traps;
#if 1 // PIXMAN_VERSION >= PIXMAN_VERSION_ENCODE(0,22,0)
		compositor->composite_tristrip = composite_tristrip;
#endif
		// compositor.check_composite_glyphs = check_composite_glyphs;
		// compositor.composite_glyphs = composite_glyphs;
}

/*
const GRAPHICS_COMPOSITOR *
_cairo_image_mask_compositor_get (void)
{
	static cairo_atomic_once_t once = CAIRO_ATOMIC_ONCE_INIT;
	static cairo_mask_compositor_t compositor;

	if (_cairo_atomic_init_once_enter(&once)) {
		InitializeGraphicsMaskCompositor (&compositor,
			_cairo_image_traps_compositor_get ());
		compositor.acquire = acquire;
		compositor.release = release;
		compositor.set_clip_region = set_clip_region;
		compositor.pattern_to_surface = _cairo_image_source_create_for_pattern;
		compositor.draw_image_boxes = draw_image_boxes;
		compositor.fill_rectangles = fill_rectangles;
		compositor.fill_boxes = fill_boxes;GRAPHICS_IMAGE_SPAN_RENDERER
		compositor.check_composite = check_composite;
		compositor.composite = composite;
		//compositor.lerp = lerp;
		//compositor.check_composite_boxes = check_composite_boxes;
		compositor.composite_boxes = composite_boxes;
		compositor.check_composite_glyphs = check_composite_glyphs;
		compositor.composite_glyphs = composite_glyphs;

		_cairo_atomic_init_once_leave(&once);
	}

	return &compositor.base;
}
*/

void InitializeGraphicsImageMaskCompositor(GRAPHICS_MASK_COMPOSITOR* compositor, void* graphics)
{
	GRAPHICS *g = (GRAPHICS*)graphics;
	InitializeGraphicsMaskCompositor (&compositor, &g->mask_compositor);
	compositor->acquire = Acquire;
	compositor->release = Release;
	compositor->set_clip_region = SetClipRegion;
	compositor->pattern_to_surface = GraphicsImageSourceCreateForPattern;
	compositor->draw_image_boxes = DrawImageBoxes;
	compositor->fill_rectangles = fill_rectangles;
	compositor->fill_boxes = fill_boxes;
	compositor->check_composite = check_composite;
	compositor->composite = composite;
	//compositor.lerp = lerp;
	//compositor.check_composite_boxes = check_composite_boxes;
	compositor->composite_boxes = composite_boxes;
	// compositor.check_composite_glyphs = check_composite_glyphs;
	// compositor.composite_glyphs = composite_glyphs;
}

#if PIXEL_MANIPULATE_HAS_COMPOSITOR

static eGRAPHICS_STATUS _cairo_image_bounded_opaque_spans(
	void* abstract_renderer,
	int y, int height,
	const GRAPHICS_HALF_OPEN_SPAN* spans,
	unsigned num_spans
)
{
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;

	if (num_spans == 0)
		return GRAPHICS_STATUS_SUCCESS;

	do {
		if (spans[0].coverage)
			pixman_image_compositor_blt (r->compositor,
				spans[0].x, y,
				spans[1].x - spans[0].x, height,
				spans[0].coverage);
		spans++;
	} while (--num_spans > 1);

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS
_cairo_image_bounded_spans (void *abstract_renderer,
	int y, int height,
	const GRAPHICS_HALF_OPEN_SPAN *spans,
	unsigned num_spans)
{
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;

	if (num_spans == 0)
		return GRAPHICS_STATUS_SUCCESS;

	do {
		if (spans[0].coverage) {
			pixman_image_compositor_blt (r->compositor,
				spans[0].x, y,
				spans[1].x - spans[0].x, height,
				r->opacity * spans[0].coverage);
		}
		spans++;
	} while (--num_spans > 1);

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS
_cairo_image_unbounded_spans (void *abstract_renderer,
	int y, int height,
	const GRAPHICS_HALF_OPEN_SPAN *spans,
	unsigned num_spans)
{
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;

	ASSERT (y + height <= r->extents.height);
	if (y > r->extents.y) {
		pixman_image_compositor_blt (r->compositor,
			r->extents.x, r->extents.y,
			r->extents.width, y - r->extents.y,
			0);
	}

	if (num_spans == 0) {
		pixman_image_compositor_blt (r->compositor,
			r->extents.x, y,
			r->extents.width,  height,
			0);
	} else {
		if (spans[0].x != r->extents.x) {
			pixman_image_compositor_blt (r->compositor,
				r->extents.x, y,
				spans[0].x - r->extents.x,
				height,
				0);
		}

		do {
			ASSERT(spans[0].x < r->extents.x + r->extents.width);
			pixman_image_compositor_blt (r->compositor,
				spans[0].x, y,
				spans[1].x - spans[0].x, height,
				r->opacity * spans[0].coverage);
			spans++;
		} while (--num_spans > 1);

		if (spans[0].x != r->extents.x + r->extents.width) {
			ASSERT(spans[0].x < r->extents.x + r->extents.width);
			pixman_image_compositor_blt (r->compositor,
				spans[0].x,	 y,
				r->extents.x + r->extents.width - spans[0].x, height,
				0);
		}
	}

	r->extents.y = y + height;
	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS
_cairo_image_clipped_spans (void *abstract_renderer,
	int y, int height,
	const GRAPHICS_HALF_OPEN_SPAN *spans,
	unsigned num_spans)
{
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;

	ASSERT(num_spans);

	do {
		if (! spans[0].inverse)
			pixman_image_compositor_blt (r->compositor,
				spans[0].x, y,
				spans[1].x - spans[0].x, height,
				r->opacity * spans[0].coverage);
		spans++;
	} while (--num_spans > 1);

	r->extents.y = y + height;
	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS
_cairo_image_finish_unbounded_spans (void *abstract_renderer)
{
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;

	if (r->extents.y < r->extents.height) {
		pixman_image_compositor_blt (r->compositor,
			r->extents.x, r->extents.y,
			r->extents.width,
			r->extents.height - r->extents.y,
			0);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_INTEGER_STATUS
span_renderer_init (GRAPHICS_ABSTRACT_SPAN_RENDERER	*_r,
	const GRAPHICS_COMPOSITE_RECTANGLES *composite,
	int			 needs_clip)
{
	GRAPHICS_IMAGE_SPAN_RENDERER *r = (GRAPHICS_IMAGE_SPAN_RENDERER *)_r;
	GRAPHICS_IMAGE_SURFACE *dst = (GRAPHICS_IMAGE_SURFACE *)composite->surface;
	const GRAPHICS_PATTERN *source = &composite->source_pattern.base;
	eGRAPHICS_OPERATOR op = composite->op;
	int src_x, src_y;
	int mask_x, mask_y;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	if (op == GRAPHICS_OPERATOR_CLEAR) {
		op = PIXEL_MANIPULATE_OPERATE_LERP_CLEAR;
	} else if (dst->base.is_clear &&
		(op == GRAPHICS_OPERATOR_SOURCE ||
			op == GRAPHICS_OPERATOR_OVER ||
			op == GRAPHICS_OPERATOR_ADD)) {
		op = PIXEL_MANIPULATE_OPERATE_SRC;
	} else if (op == GRAPHICS_OPERATOR_SOURCE) {
		op = PIXEL_MANIPULATE_OPERATE_LERP_SRC;
	} else {
		op = _PixelManipulateOperator (op);
	}

	r->compositor = NULL;
	r->mask = NULL;
	r->src = PixelManipulateImageForPattern (dst, source, FALSE,
		&composite->unbounded,
		&composite->source_sample_area,
		&src_x, &src_y);
	if (UNLIKELY (r->src == NULL))
		return GRAPHICS_STATUS_NO_MEMORY;

	r->opacity = 1.0;
	if (composite->mask_pattern.base.type == GRAPHICS_PATTERN_TYPE_SOLID) {
		r->opacity = composite->mask_pattern.solid.color.alpha;
	} else {
		r->mask = PixelManipulateImageForPattern (dst,
			&composite->mask_pattern.base,
			TRUE,
			&composite->unbounded,
			&composite->mask_sample_area,
			&mask_x, &mask_y);
		if (UNLIKELY (r->mask == NULL))
			return GRAPHICS_STATUS_NO_MEMORY;
		
		/* XXX Component-alpha? */
		if ((dst->base.content & CAIRO_CONTENT_COLOR) == 0 &&
			GraphicsPatternIsOpaque(source, &composite->source_sample_area))
		{
			PixelManipulateImageUnreference(r->src);
			r->src = r->mask;
			src_x = mask_x;
			src_y = mask_y;
			r->mask = NULL;
		}
	}

	if (composite->is_bounded) {
		if (r->opacity == 1.)
			r->base.render_rows = _cairo_image_bounded_opaque_spans;
		else
			r->base.render_rows = _cairo_image_bounded_spans;
		r->base.finish = NULL;
	} else {
		if (needs_clip)
			r->base.render_rows = _cairo_image_clipped_spans;
		else
			r->base.render_rows = _cairo_image_unbounded_spans;
		r->base.finish = _cairo_image_finish_unbounded_spans;
		r->extents = composite->unbounded;
		r->extents.height += r->extents.y;
	}

	r->compositor =
		pixman_image_create_compositor (op, r->src, r->mask, dst->image,
			composite->unbounded.x + src_x,
			composite->unbounded.y + src_y,
			composite->unbounded.x + mask_x,
			composite->unbounded.y + mask_y,
			composite->unbounded.x,
			composite->unbounded.y,
			composite->unbounded.width,
			composite->unbounded.height);
	if (UNLIKELY (r->compositor == NULL))
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;

	return GRAPHICS_STATUS_SUCCESS;
}

static void
span_renderer_fini (GRAPHICS_ABSTRACT_SPAN_RENDERER *_r,
	eGRAPHICS_INTEGER_STATUS status)
{
	GRAPHICS_IMAGE_SPAN_RENDERER *r = (GRAPHICS_IMAGE_SPAN_RENDERER *) _r;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	if (status == GRAPHICS_INTEGER_STATUS_SUCCESS && r->base.finish)
		r->base.finish (r);

	if (r->compositor)
		pixman_image_compositor_destroy (r->compositor);

	if (r->src)
		PixelManipulateImageUnreference(r->src);
	if (r->mask)
		PixelManipulateImageUnreference(r->mask);
}
#else

static eGRAPHICS_STATUS
_cairo_image_spans (
	void* abstract_renderer,
	int y, int height,
	const GRAPHICS_HALF_OPEN_SPAN* spans,
	unsigned num_spans
)
{
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;
	uint8 *mask, *row;
	int len;

	if (num_spans == 0)
		return GRAPHICS_STATUS_SUCCESS;

	mask = r->u.mask.data + (y - r->u.mask.extents.y) * r->u.mask.stride;
	mask += spans[0].x - r->u.mask.extents.x;
	row = mask;

	do {
		len = spans[1].x - spans[0].x;
		if (spans[0].coverage) {
			*row++ = r->opacity * spans[0].coverage;
			if (--len)
				memset (row, row[-1], len);
		}
		row += len;
		spans++;
	} while (--num_spans > 1);

	len = row - mask;
	row = mask;
	while (--height) {
		mask += r->u.mask.stride;
		memcpy (mask, row, len);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS
_cairo_image_spans_and_zero (void *abstract_renderer,
	int y, int height,
	const GRAPHICS_HALF_OPEN_SPAN *spans,
	unsigned num_spans)
{
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;
	uint8 *mask;
	int len;

	mask = r->u.mask.data;
	if (y > r->u.mask.extents.y) {
		len = (y - r->u.mask.extents.y) * r->u.mask.stride;
		memset (mask, 0, len);
		mask += len;
	}

	r->u.mask.extents.y = y + height;
	r->u.mask.data = mask + height * r->u.mask.stride;
	if (num_spans == 0) {
		memset (mask, 0, height * r->u.mask.stride);
	} else {
		uint8 *row = mask;

		if (spans[0].x != r->u.mask.extents.x) {
			len = spans[0].x - r->u.mask.extents.x;
			memset (row, 0, len);
			row += len;
		}

		do {
			len = spans[1].x - spans[0].x;
			*row++ = r->opacity * spans[0].coverage;
			if (len > 1) {
				memset (row, row[-1], --len);
				row += len;
			}
			spans++;
		} while (--num_spans > 1);

		if (spans[0].x != r->u.mask.extents.x + r->u.mask.extents.width) {
			len = r->u.mask.extents.x + r->u.mask.extents.width - spans[0].x;
			memset (row, 0, len);
		}

		row = mask;
		while (--height) {
			mask += r->u.mask.stride;
			memcpy (mask, row, r->u.mask.extents.width);
		}
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS
_cairo_image_finish_spans_and_zero (void *abstract_renderer)
{
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;

	if (r->u.mask.extents.y < r->u.mask.extents.height)
		memset (r->u.mask.data, 0, (r->u.mask.extents.height - r->u.mask.extents.y) * r->u.mask.stride);

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS
_fill8_spans (void *abstract_renderer, int y, int h,
	const GRAPHICS_HALF_OPEN_SPAN *spans, unsigned num_spans)
{
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;

	if (num_spans == 0)
		return GRAPHICS_STATUS_SUCCESS;

	if (LIKELY(h == 1))
	{
		do
		{
			if (spans[0].coverage)
			{
				int len = spans[1].x - spans[0].x;
				uint8 *d = r->u.fill.data + r->u.fill.stride*y + spans[0].x;
				if (len == 1)
					*d = r->u.fill.pixel;
				else
					memset(d, r->u.fill.pixel, len);
			}
			spans++;
		} while (--num_spans > 1);
	}
	else
	{
		do
		{
			if (spans[0].coverage)
			{
				int yy = y, hh = h;
				do
				{
					int len = spans[1].x - spans[0].x;
					uint8 *d = r->u.fill.data + r->u.fill.stride*yy + spans[0].x;
					if (len == 1)
						*d = r->u.fill.pixel;
					else
						memset(d, r->u.fill.pixel, len);
					yy++;
				} while(--hh);
			}
			spans++;
		} while(--num_spans > 1);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS
_fill16_spans (void *abstract_renderer, int y, int h,
	const GRAPHICS_HALF_OPEN_SPAN *spans, unsigned num_spans)
{
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;

	if (num_spans == 0)
		return GRAPHICS_STATUS_SUCCESS;

	if (LIKELY(h == 1)) {
		do {
			if (spans[0].coverage) {
				int len = spans[1].x - spans[0].x;
				uint16 *d = (uint16*)(r->u.fill.data + r->u.fill.stride*y + spans[0].x*2);
				while (len-- > 0)
					*d++ = r->u.fill.pixel;
			}
			spans++;
		} while (--num_spans > 1);
	} else {
		do {
			if (spans[0].coverage) {
				int yy = y, hh = h;
				do {
					int len = spans[1].x - spans[0].x;
					uint16 *d = (uint16*)(r->u.fill.data + r->u.fill.stride*yy + spans[0].x*2);
					while (len-- > 0)
						*d++ = r->u.fill.pixel;
					yy++;
				} while (--hh);
			}
			spans++;
		} while (--num_spans > 1);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS _fill32_spans(void *abstract_renderer, int y, int h,
	const GRAPHICS_HALF_OPEN_SPAN *spans, unsigned num_spans)
{
	GRAPHICS *graphics;
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;

	graphics = (GRAPHICS*)r->composite->surface->graphics;

	if (num_spans == 0)
		return GRAPHICS_STATUS_SUCCESS;

	if (LIKELY(h == 1)) {
		do {
			if (spans[0].coverage) {
				int len = spans[1].x - spans[0].x;
				if (len > 32) {
					PixelManipulateFill((uint32 *)r->u.fill.data, r->u.fill.stride / sizeof(uint32), r->bpp,
						spans[0].x, y, len, 1, r->u.fill.pixel, &graphics->pixel_manipulate.implementation);
				} else {
					uint32 *d = (uint32*)(r->u.fill.data + r->u.fill.stride*y + spans[0].x*4);
					while (len-- > 0)
						*d++ = r->u.fill.pixel;
				}
			}
			spans++;
		} while (--num_spans > 1);
	} else {
		do {
			if (spans[0].coverage) {
				if (spans[1].x - spans[0].x > 16) {
					PixelManipulateFill((uint32 *)r->u.fill.data, r->u.fill.stride / sizeof(uint32), r->bpp,
						spans[0].x, y, spans[1].x - spans[0].x, h,
						r->u.fill.pixel, &graphics->pixel_manipulate.implementation);
				} else {
					int yy = y, hh = h;
					do {
						int len = spans[1].x - spans[0].x;
						uint32 *d = (uint32*)(r->u.fill.data + r->u.fill.stride*yy + spans[0].x*4);
						while (len-- > 0)
							*d++ = r->u.fill.pixel;
						yy++;
					} while (--hh);
				}
			}
			spans++;
		} while (--num_spans > 1);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

#if 0
static eGRAPHICS_STATUS
_fill_spans (void *abstract_renderer, int y, int h,
	const GRAPHICS_HALF_OPEN_SPAN *spans, unsigned num_spans)
{
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;

	if (num_spans == 0)
		return GRAPHICS_STATUS_SUCCESS;

	do {
		if (spans[0].coverage) {
			PixelManipulateFill((uint32 *) r->data, r->stride, r->bpp,
				spans[0].x, y,
				spans[1].x - spans[0].x, h,
				r->pixel);
		}
		spans++;
	} while (--num_spans > 1);

	return GRAPHICS_STATUS_SUCCESS;
}
#endif

static eGRAPHICS_STATUS
_blit_spans (void *abstract_renderer, int y, int h,
	const GRAPHICS_HALF_OPEN_SPAN *spans, unsigned num_spans)
{
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;
	int cpp;

	if (num_spans == 0)
		return GRAPHICS_STATUS_SUCCESS;

	cpp = r->bpp/8;
	if (LIKELY(h == 1)) {
		uint8 *src = r->u.blit.src_data + y*r->u.blit.src_stride;
		uint8 *dst = r->u.blit.data + y*r->u.blit.stride;
		do {
			if (spans[0].coverage) {
				void *s = src + spans[0].x*cpp;
				void *d = dst + spans[0].x*cpp;
				int len = (spans[1].x - spans[0].x) * cpp;
				switch (len) {
				case 1:
					*(uint8 *)d = *(uint8 *)s;
					break;
				case 2:
					*(uint16 *)d = *(uint16 *)s;
					break;
				case 4:
					*(uint32 *)d = *(uint32 *)s;
					break;
#if HAVE_UINT64_T
				case 8:
					*(uint64_t *)d = *(uint64_t *)s;
					break;
#endif
				default:
					memcpy(d, s, len);
					break;
				}
			}
			spans++;
		} while (--num_spans > 1);
	} else {
		do {
			if (spans[0].coverage) {
				int yy = y, hh = h;
				do {
					void *src = r->u.blit.src_data + yy*r->u.blit.src_stride + spans[0].x*cpp;
					void *dst = r->u.blit.data + yy*r->u.blit.stride + spans[0].x*cpp;
					int len = (spans[1].x - spans[0].x) * cpp;
					switch (len) {
					case 1:
						*(uint8 *)dst = *(uint8 *)src;
						break;
					case 2:
						*(uint16 *)dst = *(uint16 *)src;
						break;
					case 4:
						*(uint32 *)dst = *(uint32 *)src;
						break;
#if HAVE_UINT64_T
					case 8:
						*(uint64_t *)dst = *(uint64_t *)src;
						break;
#endif
					default:
						memcpy(dst, src, len);
						break;
					}
					yy++;
				} while (--hh);
			}
			spans++;
		} while (--num_spans > 1);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS _mono_spans(
	void *abstract_renderer,
	int y, int h,
	const GRAPHICS_HALF_OPEN_SPAN *spans, unsigned num_spans
)
{
	GRAPHICS *graphics;
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;

	graphics = r->composite->surface->graphics;

	if (num_spans == 0)
		return GRAPHICS_STATUS_SUCCESS;

	do {
		if (spans[0].coverage) {
			PixelManipulateImageComposite32(r->op,
				r->src, NULL, r->u.composite.dst,
				spans[0].x + r->u.composite.src_x,  y + r->u.composite.src_y,
				0, 0,
				spans[0].x, y,
				spans[1].x - spans[0].x, h,
				&graphics->pixel_manipulate.implementation
			);
		}
		spans++;
	} while (--num_spans > 1);

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS _mono_unbounded_spans(
	void *abstract_renderer,
	int y, int h,
	const GRAPHICS_HALF_OPEN_SPAN* spans,
	unsigned num_spans
)
{
	GRAPHICS *graphics;
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;

	graphics = (GRAPHICS*)r->composite->surface->graphics;

	if (num_spans == 0) {
		PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_CLEAR,
			r->src, NULL, r->u.composite.dst,
			spans[0].x + r->u.composite.src_x,  y + r->u.composite.src_y,
			0, 0,
			r->composite->unbounded.x, y,
			r->composite->unbounded.width, h,
			&graphics->pixel_manipulate.implementation
		);
		r->u.composite.mask_y = y + h;
		return GRAPHICS_STATUS_SUCCESS;
	}

	if (y != r->u.composite.mask_y) {
		PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_CLEAR,
			r->src, NULL, r->u.composite.dst,
			spans[0].x + r->u.composite.src_x,  y + r->u.composite.src_y,
			0, 0,
			r->composite->unbounded.x, r->u.composite.mask_y,
			r->composite->unbounded.width, y - r->u.composite.mask_y,
			&graphics->pixel_manipulate.implementation
		);
	}

	if (spans[0].x != r->composite->unbounded.x) {
		PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_CLEAR,
			r->src, NULL, r->u.composite.dst,
			spans[0].x + r->u.composite.src_x,  y + r->u.composite.src_y,
			0, 0,
			r->composite->unbounded.x, y,
			spans[0].x - r->composite->unbounded.x, h,
			&graphics->pixel_manipulate.implementation
		);
	}

	do {
		int op = spans[0].coverage ? r->op : PIXEL_MANIPULATE_OPERATE_CLEAR;
		PixelManipulateImageComposite32(op,
			r->src, NULL, r->u.composite.dst,
			spans[0].x + r->u.composite.src_x,  y + r->u.composite.src_y,
			0, 0,
			spans[0].x, y,
			spans[1].x - spans[0].x, h,
			&graphics->pixel_manipulate.implementation
		);
		spans++;
	} while (--num_spans > 1);

	if (spans[0].x != r->composite->unbounded.x + r->composite->unbounded.width) {
		PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_CLEAR,
			r->src, NULL, r->u.composite.dst,
			spans[0].x + r->u.composite.src_x,  y + r->u.composite.src_y,
			0, 0,
			spans[0].x, y,
			r->composite->unbounded.x + r->composite->unbounded.width - spans[0].x, h,
			&graphics->pixel_manipulate.implementation
		);
	}

	r->u.composite.mask_y = y + h;
	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS _mono_finish_unbounded_spans(void *abstract_renderer)
{
	GRAPHICS *graphics;
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;

	graphics = (GRAPHICS*)r->composite->surface->graphics;

	if(r->u.composite.mask_y < r->composite->unbounded.y + r->composite->unbounded.height)
	{
		PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_CLEAR,
			r->src, NULL, r->u.composite.dst,
			r->composite->unbounded.x + r->u.composite.src_x,  r->u.composite.mask_y + r->u.composite.src_y,
			0, 0,
			r->composite->unbounded.x, r->u.composite.mask_y,
			r->composite->unbounded.width,
			r->composite->unbounded.y + r->composite->unbounded.height - r->u.composite.mask_y,
			&graphics->pixel_manipulate.implementation
		);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_INTEGER_STATUS mono_renderer_init(
	GRAPHICS_IMAGE_SPAN_RENDERER* r,
	const GRAPHICS_COMPOSITE_RECTANGLES* composite,
	eGRAPHICS_ANTIALIAS antialias,
	int needs_clip
)
{
	GRAPHICS_IMAGE_SURFACE *dst = (GRAPHICS_IMAGE_SURFACE *)composite->surface;
	GRAPHICS *graphics;
	graphics = dst->base.graphics;

	if (antialias != GRAPHICS_ANTIALIAS_NONE)
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	
	if (!GraphicsPatternIsOpaqueSolid(&composite->mask_pattern.base))
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;

	r->base.render_rows = NULL;
	if(composite->source_pattern.base.type == GRAPHICS_PATTERN_TYPE_SOLID)
	{
		const GRAPHICS_COLOR *color;

		color = &composite->source_pattern.solid.color;
		if (composite->op == GRAPHICS_OPERATOR_CLEAR)
			color = &graphics->color_transparent;
		
		if (fill_reduces_to_source (composite->op, color, dst, &r->u.fill.pixel))
		{
			/* Use plain C for the fill operations as the span length is
			* typically small, too small to payback the startup overheads of
			* using SSE2 etc.
			*/
			switch (PIXEL_MANIPULATE_BIT_PER_PIXEL(dst->pixel_format))
			{
			case 8: r->base.render_rows = _fill8_spans; break;
			case 16: r->base.render_rows = _fill16_spans; break;
			case 32: r->base.render_rows = _fill32_spans; break;
			default: break;
			}
			r->u.fill.data = dst->data;
			r->u.fill.stride = dst->stride;
		}
	}
	else if((composite->op == GRAPHICS_OPERATOR_SOURCE ||
		(composite->op == GRAPHICS_OPERATOR_OVER &&
		(dst->base.is_clear || (dst->base.content & GRAPHICS_CONTENT_ALPHA) == 0))) &&
		composite->source_pattern.base.type == GRAPHICS_PATTERN_TYPE_SURFACE &&
		composite->source_pattern.surface.surface->backend->type == GRAPHICS_SURFACE_TYPE_IMAGE &&
		TO_IMAGE_SURFACE(composite->source_pattern.surface.surface)->format == dst->format)
	{
		GRAPHICS_IMAGE_SURFACE *src =
			TO_IMAGE_SURFACE(composite->source_pattern.surface.surface);
		int tx, ty;
		
		if (GraphicsMatrixIsIntegerTranslation(&composite->source_pattern.base.matrix,
			&tx, &ty) &&
			composite->bounded.x + tx >= 0 &&
			composite->bounded.y + ty >= 0 &&
			composite->bounded.x + composite->bounded.width +  tx <= src->width &&
			composite->bounded.y + composite->bounded.height + ty <= src->height) {

			r->u.blit.stride = dst->stride;
			r->u.blit.data = dst->data;
			r->u.blit.src_stride = src->stride;
			r->u.blit.src_data = src->data + src->stride * ty + tx * 4;
			r->base.render_rows = _blit_spans;
		}
	}

	if(r->base.render_rows == NULL)
	{
		r->src = PixelManipulateImageForPattern (dst, &composite->source_pattern.base, FALSE,
			&composite->unbounded,
			&composite->source_sample_area,
			&r->u.composite.src_x, &r->u.composite.src_y);
		if (UNLIKELY (r->src == NULL))
			return GRAPHICS_STATUS_NO_MEMORY;

		r->u.composite.dst = ToPixelManipulateImage(composite->surface);
		r->op = _PixelManipulateOperator (composite->op);
		if(composite->is_bounded == 0)
		{
			r->base.render_rows = _mono_unbounded_spans;
			r->base.finish = _mono_finish_unbounded_spans;
			r->u.composite.mask_y = composite->unbounded.y;
		} else
			r->base.render_rows = _mono_spans;
	}
	r->bpp = PIXEL_MANIPULATE_BIT_PER_PIXEL(dst->pixel_format);

	return GRAPHICS_INTEGER_STATUS_SUCCESS;
}

#define ONE_HALF 0x7f
#define RB_MASK 0x00ff00ff
#define RB_ONE_HALF 0x007f007f
#define RB_MASK_PLUS_ONE 0x01000100
#define G_SHIFT 8
static INLINE uint32
mul8x2_8 (uint32 a, uint8 b)
{
	uint32 t = (a & RB_MASK) * b + RB_ONE_HALF;
	return ((t + ((t >> G_SHIFT) & RB_MASK)) >> G_SHIFT) & RB_MASK;
}

static INLINE uint32
add8x2_8x2 (uint32 a, uint32 b)
{
	uint32 t = a + b;
	t |= RB_MASK_PLUS_ONE - ((t >> G_SHIFT) & RB_MASK);
	return t & RB_MASK;
}

static INLINE uint8
mul8_8 (uint8 a, uint8 b)
{
	uint16 t = a * (uint16)b + ONE_HALF;
	return ((t >> G_SHIFT) + t) >> G_SHIFT;
}

static INLINE uint32
lerp8x4 (uint32 src, uint8 a, uint32 dst)
{
	return (add8x2_8x2 (mul8x2_8 (src, a),
		mul8x2_8 (dst, ~a)) |
		add8x2_8x2 (mul8x2_8 (src >> G_SHIFT, a),
			mul8x2_8 (dst >> G_SHIFT, ~a)) << G_SHIFT);
}

static eGRAPHICS_STATUS
_fill_a8_lerp_opaque_spans (void *abstract_renderer, int y, int h,
	const GRAPHICS_HALF_OPEN_SPAN *spans, unsigned num_spans)
{
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;

	if (num_spans == 0)
		return GRAPHICS_STATUS_SUCCESS;

	if (LIKELY(h == 1)) {
		uint8 *d = r->u.fill.data + r->u.fill.stride*y;
		do {
			uint8 a = spans[0].coverage;
			if (a) {
				int len = spans[1].x - spans[0].x;
				if (a == 0xff) {
					memset(d + spans[0].x, r->u.fill.pixel, len);
				} else {
					uint8 s = mul8_8(a, r->u.fill.pixel);
					uint8 *dst = d + spans[0].x;
					a = ~a;
					while (len-- > 0) {
						uint8 t = mul8_8(*dst, a);
						*dst++ = t + s;
					}
				}
			}
			spans++;
		} while (--num_spans > 1);
	} else {
		do {
			uint8 a = spans[0].coverage;
			if (a) {
				int yy = y, hh = h;
				if (a == 0xff) {
					do {
						int len = spans[1].x - spans[0].x;
						uint8 *d = r->u.fill.data + r->u.fill.stride*yy + spans[0].x;
						memset(d, r->u.fill.pixel, len);
						yy++;
					} while (--hh);
				} else {
					uint8 s = mul8_8(a, r->u.fill.pixel);
					a = ~a;
					do {
						int len = spans[1].x - spans[0].x;
						uint8 *d = r->u.fill.data + r->u.fill.stride*yy + spans[0].x;
						while (len-- > 0) {
							uint8 t = mul8_8(*d, a);
							*d++ = t + s;
						}
						yy++;
					} while (--hh);
				}
			}
			spans++;
		} while (--num_spans > 1);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS _fill_xrgb32_lerp_opaque_spans(
	void* abstract_renderer,
	int y,
	int h,
	const GRAPHICS_HALF_OPEN_SPAN* spans,
	unsigned num_spans
)
{
	GRAPHICS *graphics;
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;

	graphics = (GRAPHICS*)r->composite->surface->graphics;

	if (num_spans == 0)
		return GRAPHICS_STATUS_SUCCESS;

	if(LIKELY(h == 1))
	{
		do
		{
			uint8 a = spans[0].coverage;
			if(a)
			{
				int len = spans[1].x - spans[0].x;
				uint32 *d = (uint32*)(r->u.fill.data + r->u.fill.stride*y + spans[0].x*4);
				if(a == 0xff)
				{
					if (len > 31)
					{
						PixelManipulateFill((uint32 *)r->u.fill.data, r->u.fill.stride / sizeof(uint32), 32,
							spans[0].x, y, len, 1, r->u.fill.pixel, &graphics->pixel_manipulate.implementation);
					}
					else
					{
						uint32 *d = (uint32*)(r->u.fill.data + r->u.fill.stride*y + spans[0].x*4);
						while (len-- > 0)
							*d++ = r->u.fill.pixel;
					}
				}
				else while (len-- > 0)
				{
					*d = lerp8x4(r->u.fill.pixel, a, *d);
					d++;
				}
			}
			spans++;
		} while (--num_spans > 1);
	}
	else
	{
		do {
			uint8 a = spans[0].coverage;
			if(a)
			{
				if(a == 0xff)
				{
					if(spans[1].x - spans[0].x > 16)
					{
						PixelManipulateFill((uint32 *)r->u.fill.data, r->u.fill.stride / sizeof(uint32), 32,
							spans[0].x, y, spans[1].x - spans[0].x, h,
							r->u.fill.pixel, &graphics->pixel_manipulate.implementation);
					}
					else
					{
						int yy = y, hh = h;
						do
						{
							int len = spans[1].x - spans[0].x;
							uint32 *d = (uint32*)(r->u.fill.data + r->u.fill.stride*yy + spans[0].x*4);
							while(len-- > 0)
								*d++ = r->u.fill.pixel;
							yy++;
						} while(--hh);
					}
				}
				else
				{
					int yy = y, hh = h;
					do
					{
						int len = spans[1].x - spans[0].x;
						uint32 *d = (uint32 *)(r->u.fill.data + r->u.fill.stride*yy + spans[0].x*4);
						while(len-- > 0)
						{
							*d = lerp8x4 (r->u.fill.pixel, a, *d);
							d++;
						}
						yy++;
					} while(--hh);
				}
			}
			spans++;
		} while(--num_spans > 1);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS _fill_a8_lerp_spans(
	void* abstract_renderer,
	int y,
	int h,
	const GRAPHICS_HALF_OPEN_SPAN* spans,
	unsigned num_spans
)
{
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;

	if (num_spans == 0)
		return GRAPHICS_STATUS_SUCCESS;

	if(LIKELY(h == 1))
	{
		do
		{
			uint8 a = mul8_8 (spans[0].coverage, r->bpp);
			if(a)
			{
				int len = spans[1].x - spans[0].x;
				uint8 *d = r->u.fill.data + r->u.fill.stride*y + spans[0].x;
				uint16 p = (uint16)a * r->u.fill.pixel + 0x7f;
				uint16 ia = ~a;
				while(len-- > 0)
				{
					uint16 t = *d*ia + p;
					*d++ = (t + (t>>8)) >> 8;
				}
			}
			spans++;
		} while(--num_spans > 1);
	}
	else
	{
		do
		{
			uint8 a = mul8_8 (spans[0].coverage, r->bpp);
			if(a)
			{
				int yy = y, hh = h;
				uint16 p = (uint16)a * r->u.fill.pixel + 0x7f;
				uint16 ia = ~a;
				do
				{
					int len = spans[1].x - spans[0].x;
					uint8 *d = r->u.fill.data + r->u.fill.stride*yy + spans[0].x;
					while(len-- > 0)
					{
						uint16 t = *d*ia + p;
						*d++ = (t + (t>>8)) >> 8;
					}
					yy++;
				} while(--hh);
			}
			spans++;
		} while(--num_spans > 1);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS _fill_xrgb32_lerp_spans(
	void *abstract_renderer,
	int y,
	int h,
	const GRAPHICS_HALF_OPEN_SPAN* spans, unsigned num_spans
)
{
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;

	if (num_spans == 0)
		return GRAPHICS_STATUS_SUCCESS;

	if (LIKELY(h == 1)) {
		do {
			uint8 a = mul8_8 (spans[0].coverage, r->bpp);
			if (a) {
				int len = spans[1].x - spans[0].x;
				uint32 *d = (uint32*)(r->u.fill.data + r->u.fill.stride*y + spans[0].x*4);
				while (len-- > 0) {
					*d = lerp8x4 (r->u.fill.pixel, a, *d);
					d++;
				}
			}
			spans++;
		} while (--num_spans > 1);
	} else {
		do {
			uint8 a = mul8_8 (spans[0].coverage, r->bpp);
			if (a) {
				int yy = y, hh = h;
				do {
					int len = spans[1].x - spans[0].x;
					uint32 *d = (uint32 *)(r->u.fill.data + r->u.fill.stride*yy + spans[0].x*4);
					while (len-- > 0) {
						*d = lerp8x4 (r->u.fill.pixel, a, *d);
						d++;
					}
					yy++;
				} while (--hh);
			}
			spans++;
		} while (--num_spans > 1);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS _blit_xrgb32_lerp_spans(
	void* abstract_renderer,
	int y,
	int h,
	const GRAPHICS_HALF_OPEN_SPAN* spans,
	unsigned num_spans
)
{
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;

	if (num_spans == 0)
		return GRAPHICS_STATUS_SUCCESS;

	if (LIKELY(h == 1))
	{
		uint8 *src = r->u.blit.src_data + y*r->u.blit.src_stride;
		uint8 *dst = r->u.blit.data + y*r->u.blit.stride;
		do
		{
			uint8 a = mul8_8 (spans[0].coverage, r->bpp);
			if(a)
			{
				uint32 *s = (uint32*)src + spans[0].x;
				uint32 *d = (uint32*)dst + spans[0].x;
				int len = spans[1].x - spans[0].x;
				if(a == 0xff)
				{
					if (len == 1)
						*d = *s;
					else
						memcpy(d, s, len*4);
				}
				else
				{
					while(len-- > 0)
					{
						*d = lerp8x4 (*s, a, *d);
						s++, d++;
					}
				}
			}
			spans++;
		} while(--num_spans > 1);
	}
	else
	{
		do
		{
			uint8 a = mul8_8 (spans[0].coverage, r->bpp);
			if(a)
			{
				int yy = y, hh = h;
				do
				{
					uint32 *s = (uint32 *)(r->u.blit.src_data + yy*r->u.blit.src_stride + spans[0].x * 4);
					uint32 *d = (uint32 *)(r->u.blit.data + yy*r->u.blit.stride + spans[0].x * 4);
					int len = spans[1].x - spans[0].x;
					if(a == 0xff)
					{
						if (len == 1)
							*d = *s;
						else
							memcpy(d, s, len * 4);
					}
					else
					{
						while(len-- > 0)
						{
							*d = lerp8x4 (*s, a, *d);
							s++, d++;
						}
					}
					yy++;
				} while(--hh);
			}
			spans++;
		} while(--num_spans > 1);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS _inplace_spans(
	void* abstract_renderer,
	int y, int h,
	const GRAPHICS_HALF_OPEN_SPAN* spans,
	unsigned num_spans
)
{
	GRAPHICS *graphics;
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;
	uint8 *mask;
	int x0, x1;

	graphics = (GRAPHICS*)r->composite->surface->graphics;

	if (num_spans == 0)
		return GRAPHICS_STATUS_SUCCESS;

	if (num_spans == 2 && spans[0].coverage == 0xff)
	{
		PixelManipulateImageComposite32(r->op, r->src, NULL, r->u.composite.dst,
			spans[0].x + r->u.composite.src_x,
			y + r->u.composite.src_y,
			0, 0,
			spans[0].x, y,
			spans[1].x - spans[0].x, h,
			&graphics->pixel_manipulate.implementation
		);
		return GRAPHICS_STATUS_SUCCESS;
	}

	mask = r->mask->bits.bits; // (uint8 *)pixman_image_get_data (r->mask);
	x1 = x0 = spans[0].x;
	do
	{
		int len = spans[1].x - spans[0].x;
		*mask++ = spans[0].coverage;
		if(len > 1)
		{
			if (len >= r->u.composite.run_length && spans[0].coverage == 0xff)
			{
				if (x1 != x0)
				{
					PixelManipulateImageComposite32(r->op, r->src, r->mask, r->u.composite.dst,
						x0 + r->u.composite.src_x,
						y + r->u.composite.src_y,
						0, 0,
						x0, y,
						x1 - x0, h,
						&graphics->pixel_manipulate.implementation
					);
				}
				PixelManipulateImageComposite32(r->op, r->src, NULL, r->u.composite.dst,
					spans[0].x + r->u.composite.src_x,
					y + r->u.composite.src_y,
					0, 0,
					spans[0].x, y,
					len, h,
					&graphics->pixel_manipulate.implementation
				);
				mask = r->mask->bits.bits; // (uint8 *)pixman_image_get_data (r->mask);
				x0 = spans[1].x;
			}
			else if(spans[0].coverage == 0x0 &&
				x1 - x0 > r->u.composite.run_length)
			{
				PixelManipulateImageComposite32(r->op, r->src, r->mask, r->u.composite.dst,
					x0 + r->u.composite.src_x,
					y + r->u.composite.src_y,
					0, 0,
					x0, y,
					x1 - x0, h,
					&graphics->pixel_manipulate.implementation
				);
				mask = r->mask->bits.bits; // (uint8 *)pixman_image_get_data (r->mask);
				x0 = spans[1].x;
			}
			else
			{
				memset (mask, spans[0].coverage, --len);
				mask += len;
			}
		}
		x1 = spans[1].x;
		spans++;
	} while(--num_spans > 1);

	if(x1 != x0)
	{
		PixelManipulateImageComposite32(r->op, r->src, r->mask, r->u.composite.dst,
			x0 + r->u.composite.src_x,
			y + r->u.composite.src_y,
			0, 0,
			x0, y,
			x1 - x0, h,
			&graphics->pixel_manipulate.implementation
		);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS
_inplace_opacity_spans (void *abstract_renderer, int y, int h,
	const GRAPHICS_HALF_OPEN_SPAN *spans,
	unsigned num_spans)
{
	GRAPHICS *graphics;
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;
	uint8 *mask;
	int x0, x1;

	graphics = (GRAPHICS*)r->composite->surface->graphics;

	if (num_spans == 0)
		return GRAPHICS_STATUS_SUCCESS;

	mask = r->mask->bits.bits; // (uint8 *)pixman_image_get_data (r->mask);
	x1 = x0 = spans[0].x;
	do
	{
		int len = spans[1].x - spans[0].x;
		uint8 m = mul8_8(spans[0].coverage, r->bpp);
		*mask++ = m;
		if(len > 1)
		{
			if (m == 0 &&
				x1 - x0 > r->u.composite.run_length)
			{
				PixelManipulateImageComposite32(r->op, r->src, r->mask, r->u.composite.dst,
					x0 + r->u.composite.src_x,
					y + r->u.composite.src_y,
					0, 0,
					x0, y,
					x1 - x0, h,
					&graphics->pixel_manipulate.implementation
				);
				mask = r->mask->bits.bits; // (uint8 *)pixman_image_get_data (r->mask);
				x0 = spans[1].x;
			}
			else
			{
				memset (mask, m, --len);
				mask += len;
			}
		}
		x1 = spans[1].x;
		spans++;
	} while(--num_spans > 1);

	if(x1 != x0)
	{
		PixelManipulateImageComposite32(r->op, r->src, r->mask, r->u.composite.dst,
			x0 + r->u.composite.src_x,
			y + r->u.composite.src_y,
			0, 0,
			x0, y,
			x1 - x0, h,
			&graphics->pixel_manipulate.implementation
		);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS _inplace_src_spans(
	void* abstract_renderer,
	int y, int h,
	const GRAPHICS_HALF_OPEN_SPAN* spans,
	unsigned num_spans
)
{
	GRAPHICS *graphics;
	GRAPHICS_IMAGE_SPAN_RENDERER *r = abstract_renderer;
	uint8 *m;
	int x0;

	graphics = (GRAPHICS*)r->composite->surface->graphics;

	if (num_spans == 0)
		return GRAPHICS_STATUS_SUCCESS;

	x0 = spans[0].x;
	m = r->_buf;
	do{
		int len = spans[1].x - spans[0].x;
		if (len >= r->u.composite.run_length && spans[0].coverage == 0xff)
		{
			if (spans[0].x != x0)
			{
#if PIXMAN_HAS_OP_LERP
				PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_LERP_SRC,
					r->src, r->mask, r->u.composite.dst,
					x0 + r->u.composite.src_x,
					y + r->u.composite.src_y,
					0, 0,
					x0, y,
					spans[0].x - x0, h);
#else
				PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_OUT_REVERSE,
					r->mask, NULL, r->u.composite.dst,
					0, 0,
					0, 0,
					x0, y,
					spans[0].x - x0, h,
					&graphics->pixel_manipulate.implementation
				);
				PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_ADD,
					r->src, r->mask, r->u.composite.dst,
					x0 + r->u.composite.src_x,
					y + r->u.composite.src_y,
					0, 0,
					x0, y,
					spans[0].x - x0, h,
					&graphics->pixel_manipulate.implementation
				);
#endif
			}

			PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_SRC,
				r->src, NULL, r->u.composite.dst,
				spans[0].x + r->u.composite.src_x,
				y + r->u.composite.src_y,
				0, 0,
				spans[0].x, y,
				spans[1].x - spans[0].x, h,
				&graphics->pixel_manipulate.implementation
			);

			m = r->_buf;
			x0 = spans[1].x;
		}
		else if(spans[0].coverage == 0x0)
		{
			if (spans[0].x != x0)
			{
#if PIXMAN_HAS_OP_LERP
				PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_LERP_SRC,
					r->src, r->mask, r->u.composite.dst,
					x0 + r->u.composite.src_x,
					y + r->u.composite.src_y,
					0, 0,
					x0, y,
					spans[0].x - x0, h,
					&graphics->pixel_manipulate.implementation
				);
#else
				PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_OUT_REVERSE,
					r->mask, NULL, r->u.composite.dst,
					0, 0,
					0, 0,
					x0, y,
					spans[0].x - x0, h,
					&graphics->pixel_manipulate.implementation
				);
				PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_ADD,
					r->src, r->mask, r->u.composite.dst,
					x0 + r->u.composite.src_x,
					y + r->u.composite.src_y,
					0, 0,
					x0, y,
					spans[0].x - x0, h,
					&graphics->pixel_manipulate.implementation
				);
#endif
			}

			m = r->_buf;
			x0 = spans[1].x;
		}
		else
		{
			*m++ = spans[0].coverage;
			if(len > 1)
			{
				(void)memset(m, spans[0].coverage, --len);
				m += len;
			}
		}
		spans++;
	} while(--num_spans > 1);

	if (spans[0].x != x0)
	{
#if PIXMAN_HAS_OP_LERP
		PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_LERP_SRC,
			r->src, r->mask, r->u.composite.dst,
			x0 + r->u.composite.src_x,
			y + r->u.composite.src_y,
			0, 0,
			x0, y,
			spans[0].x - x0, h,
			&graphics->pixel_manipulate.implementation
		);
#else
		PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_OUT_REVERSE,
			r->mask, NULL, r->u.composite.dst,
			0, 0,
			0, 0,
			x0, y,
			spans[0].x - x0, h,
			&graphics->pixel_manipulate.implementation
		);
		PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_ADD,
			r->src, r->mask, r->u.composite.dst,
			x0 + r->u.composite.src_x,
			y + r->u.composite.src_y,
			0, 0,
			x0, y,
			spans[0].x - x0, h,
			&graphics->pixel_manipulate.implementation
		);
#endif
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS _inplace_src_opacity_spans(
	void* abstract_renderer,
	int y, int h,
	const GRAPHICS_HALF_OPEN_SPAN* spans,
	unsigned num_spans
)
{
	GRAPHICS* graphics;
	GRAPHICS_IMAGE_SPAN_RENDERER* r = abstract_renderer;
	uint8* mask;
	int x0;

	graphics = (GRAPHICS*)r->composite->surface->graphics;

	if (num_spans == 0)
		return GRAPHICS_STATUS_SUCCESS;

	x0 = spans[0].x;
	mask = r->mask->bits.bits; // (uint8 *)pixman_image_get_data (r->mask);
	do {
		int len = spans[1].x - spans[0].x;
		uint8 m = mul8_8(spans[0].coverage, r->bpp);
		if(m == 0)
		{
			if(spans[0].x != x0)
			{
#if PIXMAN_HAS_OP_LERP
				PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_LERP_SRC,
					r->src, r->mask, r->u.composite.dst,
					x0 + r->u.composite.src_x,
					y + r->u.composite.src_y,
					0, 0,
					x0, y,
					spans[0].x - x0, h,
					&graphics->pixel_manipulate.implementation
				);
#else
				PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_OUT_REVERSE,
					r->mask, NULL, r->u.composite.dst,
					0, 0,
					0, 0,
					x0, y,
					spans[0].x - x0, h,
					&graphics->pixel_manipulate.implementation
				);
				PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_ADD,
					r->src, r->mask, r->u.composite.dst,
					x0 + r->u.composite.src_x,
					y + r->u.composite.src_y,
					0, 0,
					x0, y,
					spans[0].x - x0, h,
					&graphics->pixel_manipulate.implementation
				);
#endif
			}

			mask = r->mask->bits.bits; // (uint8 *)pixman_image_get_data (r->mask);
			x0 = spans[1].x;
		}
		else
		{
			*mask++ = m;
			if(len > 1)
			{
				memset (mask, m, --len);
				mask += len;
			}
		}
		spans++;
	} while(--num_spans > 1);

	if(spans[0].x != x0)
	{
#if PIXMAN_HAS_OP_LERP
		PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_LERP_SRC,
			r->src, r->mask, r->u.composite.dst,
			x0 + r->u.composite.src_x,
			y + r->u.composite.src_y,
			0, 0,
			x0, y,
			spans[0].x - x0, h,
			&graphics->pixel_manipulate.implementation
		);
#else
		PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_OUT_REVERSE,
			r->mask, NULL, r->u.composite.dst,
			0, 0,
			0, 0,
			x0, y,
			spans[0].x - x0, h,
			&graphics->pixel_manipulate.implementation
		);
		PixelManipulateImageComposite32(PIXEL_MANIPULATE_OPERATE_ADD,
			r->src, r->mask, r->u.composite.dst,
			x0 + r->u.composite.src_x,
			y + r->u.composite.src_y,
			0, 0,
			x0, y,
			spans[0].x - x0, h,
			&graphics->pixel_manipulate.implementation
		);
#endif
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static void free_pixels (PIXEL_MANIPULATE_IMAGE *image, void *data)
{
	free (data);
}

static eGRAPHICS_INTEGER_STATUS inplace_renderer_init(
	GRAPHICS_IMAGE_SPAN_RENDERER* r,
	const GRAPHICS_COMPOSITE_RECTANGLES* composite,
	eGRAPHICS_ANTIALIAS antialias,
	int needs_clip
)
{
	GRAPHICS_IMAGE_SURFACE *dst = (GRAPHICS_IMAGE_SURFACE *)composite->surface;
	GRAPHICS *graphics;
	uint8 *buf;

	graphics = dst->base.graphics;

	if (composite->mask_pattern.base.type != GRAPHICS_PATTERN_TYPE_SOLID)
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;

	r->base.render_rows = NULL;
	r->bpp = composite->mask_pattern.solid.color.alpha_short >> 8;

	if (composite->source_pattern.base.type == GRAPHICS_PATTERN_TYPE_SOLID)
	{
		const GRAPHICS_COLOR *color;

		color = &composite->source_pattern.solid.color;
		if (composite->op == GRAPHICS_OPERATOR_CLEAR)
			color = &graphics->color_transparent;

		if (fill_reduces_to_source (composite->op, color, dst, &r->u.fill.pixel))
		{
			/* Use plain C for the fill operations as the span length is
			* typically small, too small to payback the startup overheads of
			* using SSE2 etc.
			*/
			if(r->bpp == 0xff)
			{
				switch (dst->format)
				{
				case GRAPHICS_FORMAT_A8:
					r->base.render_rows = _fill_a8_lerp_opaque_spans;
					break;
				case GRAPHICS_FORMAT_RGB24:
				case GRAPHICS_FORMAT_ARGB32:
					r->base.render_rows = _fill_xrgb32_lerp_opaque_spans;
					break;
				case GRAPHICS_FORMAT_A1:
				// case GRAPHICS_FORMAT_RGB16_565:
				// case GRAPHICS_FORMAT_RGB30:
				case GRAPHICS_FORMAT_INVALID:
				default: break;
				}
			}
			else
			{
				switch(dst->format)
				{
				case GRAPHICS_FORMAT_A8:
					r->base.render_rows = _fill_a8_lerp_spans;
					break;
				case GRAPHICS_FORMAT_RGB24:
				case GRAPHICS_FORMAT_ARGB32:
					r->base.render_rows = _fill_xrgb32_lerp_spans;
					break;
				case GRAPHICS_FORMAT_A1:
				// case GRAPHICS_FORMAT_RGB16_565:
				// case GRAPHICS_FORMAT_RGB30:
				case GRAPHICS_FORMAT_INVALID:
				default: break;
				}
			}
			r->u.fill.data = dst->data;
			r->u.fill.stride = dst->stride;
		}
	}
	else if((dst->format == GRAPHICS_FORMAT_ARGB32 || dst->format == GRAPHICS_FORMAT_RGB24) &&
		(composite->op == GRAPHICS_OPERATOR_SOURCE ||
		(composite->op == GRAPHICS_OPERATOR_OVER &&
			(dst->base.is_clear || (dst->base.content & GRAPHICS_CONTENT_ALPHA) == 0))) &&
		composite->source_pattern.base.type == GRAPHICS_PATTERN_TYPE_SURFACE &&
		composite->source_pattern.surface.surface->backend->type == GRAPHICS_SURFACE_TYPE_IMAGE &&
		TO_IMAGE_SURFACE(composite->source_pattern.surface.surface)->format == dst->format)
	{
		GRAPHICS_IMAGE_SURFACE *src =
			TO_IMAGE_SURFACE(composite->source_pattern.surface.surface);
		int tx, ty;

		if (GraphicsMatrixIsIntegerTranslation(&composite->source_pattern.base.matrix,
			&tx, &ty) &&
			composite->bounded.x + tx >= 0 &&
			composite->bounded.y + ty >= 0 &&
			composite->bounded.x + composite->bounded.width + tx <= src->width &&
			composite->bounded.y + composite->bounded.height + ty <= src->height) {

			ASSERT(PIXEL_MANIPULATE_BIT_PER_PIXEL(dst->pixel_format) == 32);
			r->u.blit.stride = dst->stride;
			r->u.blit.data = dst->data;
			r->u.blit.src_stride = src->stride;
			r->u.blit.src_data = src->data + src->stride * ty + tx * 4;
			r->base.render_rows = _blit_xrgb32_lerp_spans;
		}
	}
	if(r->base.render_rows == NULL)
	{
		const GRAPHICS_PATTERN *src = &composite->source_pattern.base;
		unsigned int width;

		if (composite->is_bounded == 0)
			return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;

		r->base.render_rows = r->bpp == 0xff ? _inplace_spans : _inplace_opacity_spans;
		width = (composite->bounded.width + 3) & ~3;

		r->u.composite.run_length = 8;
		if (src->type == GRAPHICS_PATTERN_TYPE_LINEAR ||
			src->type == GRAPHICS_PATTERN_TYPE_RADIAL)
			r->u.composite.run_length = 256;
		if (dst->base.is_clear &&
			(composite->op == GRAPHICS_OPERATOR_SOURCE ||
				composite->op == GRAPHICS_OPERATOR_OVER ||
				composite->op == GRAPHICS_OPERATOR_ADD))
		{
			r->op = PIXEL_MANIPULATE_OPERATE_SRC;
		}
		else if(composite->op == GRAPHICS_OPERATOR_SOURCE)
		{
			r->base.render_rows = r->bpp == 0xff ? _inplace_src_spans : _inplace_src_opacity_spans;
			r->u.composite.mask_y = r->composite->unbounded.y;
			width = (composite->unbounded.width + 3) & ~3;
		}
		else if(composite->op == GRAPHICS_OPERATOR_CLEAR)
		{
			r->op = PIXEL_MANIPULATE_OPERATE_OUT_REVERSE;
			src = NULL;
		}
		else
		{
			r->op = _PixelManipulateOperator (composite->op);
		}

		r->src = PixelManipulateImageForPattern (dst, src, FALSE,
			&composite->bounded,
			&composite->source_sample_area,
			&r->u.composite.src_x, &r->u.composite.src_y);
		if (UNLIKELY (r->src == NULL))
			return GRAPHICS_STATUS_NO_MEMORY;

		/* Create an effectively unbounded mask by repeating the single line */
		buf = r->_buf;
		if(width > SZ_BUF)
		{
			buf = MEM_ALLOC_FUNC(width);
			if (UNLIKELY (buf == NULL))
			{
				PixelManipulateImageUnreference(r->src);
				return GRAPHICS_STATUS_NO_MEMORY;
			}
		}
		r->mask = CreatePixelManipulateImageBits(PIXEL_MANIPULATE_FORMAT_A8,
			width, composite->unbounded.height,
			(uint32 *)buf, 0, &graphics->pixel_manipulate);
		if(UNLIKELY (r->mask == NULL))
		{
			PixelManipulateImageUnreference(r->src);
			if (buf != r->_buf)
				free (buf);
			return GRAPHICS_STATUS_NO_MEMORY;
		}
		
		if (buf != r->_buf)
			PixelManipulateImageSetDestroyFunction(r->mask, free_pixels, buf);

		r->u.composite.dst = &dst->image;
	}

	return GRAPHICS_INTEGER_STATUS_SUCCESS;
}

static eGRAPHICS_INTEGER_STATUS span_renderer_init(
	GRAPHICS_ABSTRACT_SPAN_RENDERER* _r,
	const GRAPHICS_COMPOSITE_RECTANGLES* composite,
	eGRAPHICS_ANTIALIAS antialias,
	int needs_clip
)
{
	GRAPHICS *graphics;
	GRAPHICS_IMAGE_SPAN_RENDERER *r = (GRAPHICS_IMAGE_SPAN_RENDERER *)_r;
	GRAPHICS_IMAGE_SURFACE *dst = (GRAPHICS_IMAGE_SURFACE *)composite->surface;
	const GRAPHICS_PATTERN *source = &composite->source_pattern.base;
	eGRAPHICS_OPERATOR op = composite->op;
	eGRAPHICS_INTEGER_STATUS status;

	// TRACE ((stderr, "%s: antialias=%d, needs_clip=%d\n", __FUNCTION__,
	//	antialias, needs_clip));

	graphics = source->graphics;

	if (needs_clip)
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;

	r->composite = composite;
	r->mask = NULL;
	r->src = NULL;
	r->base.finish = NULL;

	status = mono_renderer_init (r, composite, antialias, needs_clip);
	if (status != GRAPHICS_INTEGER_STATUS_UNSUPPORTED)
		return status;

	status = inplace_renderer_init (r, composite, antialias, needs_clip);
	if (status != GRAPHICS_INTEGER_STATUS_UNSUPPORTED)
		return status;

	r->bpp = 0;

	if (op == GRAPHICS_OPERATOR_CLEAR)
	{
#if PIXMAN_HAS_OP_LERP
		op = PIXEL_MANIPULATE_OPERATE_LERP_CLEAR;
#else
		source = &graphics->white_pattern.base;
		op = PIXEL_MANIPULATE_OPERATE_OUT_REVERSE;
#endif
	} else if (dst->base.is_clear &&
		(op == GRAPHICS_OPERATOR_SOURCE ||
			op == GRAPHICS_OPERATOR_OVER ||
			op == GRAPHICS_OPERATOR_ADD)) {
		op = PIXEL_MANIPULATE_OPERATE_SRC;
	}
	else if(op == GRAPHICS_OPERATOR_SOURCE)
	{
		if (GraphicsPatternIsOpaque(&composite->source_pattern.base,
			&composite->source_sample_area))
		{
			op = PIXEL_MANIPULATE_OPERATE_OVER;
		}
		else
		{
#if PIXMAN_HAS_OP_LERP
			op = PIXEL_MANIPULATE_OPERATE_LERP_SRC;
#else
			return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
#endif
		}
	} else {
		op = _PixelManipulateOperator (op);
	}
	r->op = op;

	r->src = PixelManipulateImageForPattern (dst, source, FALSE,
		&composite->unbounded,
		&composite->source_sample_area,
		&r->u.mask.src_x, &r->u.mask.src_y);
	if(UNLIKELY (r->src == NULL))
		return GRAPHICS_STATUS_NO_MEMORY;

	r->opacity = 1.0;
	if(composite->mask_pattern.base.type == GRAPHICS_PATTERN_TYPE_SOLID)
	{
		r->opacity = composite->mask_pattern.solid.color.alpha;
	}
	else
	{
		PIXEL_MANIPULATE_IMAGE *mask;
		int mask_x, mask_y;

		mask = PixelManipulateImageForPattern (dst,
			&composite->mask_pattern.base,
			TRUE,
			&composite->unbounded,
			&composite->mask_sample_area,
			&mask_x, &mask_y);
		if (UNLIKELY (mask == NULL))
			return GRAPHICS_STATUS_NO_MEMORY;

		/* XXX Component-alpha? */
		if((dst->base.content & GRAPHICS_CONTENT_COLOR) == 0 &&
			GraphicsPatternIsOpaque(source, &composite->source_sample_area))
		{
			PixelManipulateImageUnreference(r->src);
			r->src = mask;
			r->u.mask.src_x = mask_x;
			r->u.mask.src_y = mask_y;
			mask = NULL;
		}

		if(mask != NULL)
		{
			PixelManipulateImageUnreference(mask);
			return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
		}
	}

	r->u.mask.extents = composite->unbounded;
	r->u.mask.stride = (r->u.mask.extents.width + 3) & ~3;
	if(r->u.mask.extents.height * r->u.mask.stride > SZ_BUF)
	{
		r->mask = CreatePixelManipulateImageBits(PIXEL_MANIPULATE_FORMAT_A8,
			r->u.mask.extents.width,
			r->u.mask.extents.height,
			NULL, 0, &graphics->pixel_manipulate);

		r->base.render_rows = _cairo_image_spans;
		r->base.finish = NULL;
	}
	else
	{
		r->mask = CreatePixelManipulateImageBits(PIXEL_MANIPULATE_FORMAT_A8,
			r->u.mask.extents.width,
			r->u.mask.extents.height,
			(uint32 *)r->_buf, r->u.mask.stride, &graphics->pixel_manipulate);

		r->base.render_rows = _cairo_image_spans_and_zero;
		r->base.finish = _cairo_image_finish_spans_and_zero;
	}
	if (UNLIKELY (r->mask == NULL))
		return GRAPHICS_STATUS_NO_MEMORY;

	r->u.mask.data = r->mask->bits.bits; //(uint8 *) pixman_image_get_data (r->mask);
	r->u.mask.stride = r->mask->bits.row_stride; // pixman_image_get_stride (r->mask);

	r->u.mask.extents.height += r->u.mask.extents.y;
	return GRAPHICS_STATUS_SUCCESS;
}

static void span_renderer_fini(
	GRAPHICS_ABSTRACT_SPAN_RENDERER* _r,
	eGRAPHICS_INTEGER_STATUS status
)
{
	GRAPHICS *graphics;
	GRAPHICS_IMAGE_SPAN_RENDERER *r = (GRAPHICS_IMAGE_SPAN_RENDERER *) _r;

	graphics = (GRAPHICS*)r->composite->surface->graphics;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	if (LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS)) {
		if (r->base.finish)
			r->base.finish (r);
	}
	if (LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS && r->bpp == 0)) {
		const GRAPHICS_COMPOSITE_RECTANGLES *composite = r->composite;

		PixelManipulateImageComposite32(r->op, r->src, r->mask,
			ToPixelManipulateImage(composite->surface),
			composite->unbounded.x + r->u.mask.src_x,
			composite->unbounded.y + r->u.mask.src_y,
			0, 0,
			composite->unbounded.x,
			composite->unbounded.y,
			composite->unbounded.width,
			composite->unbounded.height,
			&graphics->pixel_manipulate.implementation
		);
	}

	if (r->src)
		PixelManipulateImageUnreference(r->src);
	if (r->mask)
		PixelManipulateImageUnreference(r->mask);
}
#endif

void SetGraphicsImageCompositorCallbacks(GRAPHICS_SPANS_COMPOSITOR* compositor)
{
	compositor->fill_boxes = fill_boxes;
	compositor->draw_image_boxes = DrawImageBoxes;
	compositor->pattern_to_surface = GraphicsImageSourceCreateForPattern;
	compositor->composite_boxes = composite_boxes;
	compositor->initialize_renderer = span_renderer_init;
	compositor->render_finish = span_renderer_fini;
}

#ifdef __cplusplus
}
#endif
