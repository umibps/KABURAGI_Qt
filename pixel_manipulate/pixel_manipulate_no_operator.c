
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <stdlib.h>
#include "pixel_manipulate_composite.h"
#include "pixel_manipulate.h"
#include "pixel_manipulate_private.h"
#include "pixel_manipulate_inline.h"

typedef enum
{
	ITER_NARROW =			   (1 << 0),
	ITER_WIDE =				 (1 << 1),

	/* "Localized alpha" is when the alpha channel is used only to compute
	* the alpha value of the destination. This means that the computation
	* of the RGB values of the result is independent of the alpha value.
	*
	* For example, the OVER operator has localized alpha for the
	* destination, because the RGB values of the result can be computed
	* without knowing the destination alpha. Similarly, ADD has localized
	* alpha for both source and destination because the RGB values of the
	* result can be computed without knowing the alpha value of source or
	* destination.
	*
	* When he destination is xRGB, this is useful knowledge, because then
	* we can treat it as if it were ARGB, which means in some cases we can
	* avoid copying it to a temporary buffer.
	*/
	ITER_LOCALIZED_ALPHA =	(1 << 2),
	ITER_IGNORE_ALPHA =		(1 << 3),
	ITER_IGNORE_RGB =		(1 << 4),

	/* These indicate whether the iterator is for a source
	* or a destination image
	*/
	ITER_SRC =			(1 << 5),
	ITER_DEST =			(1 << 6)
} iter_flags_t;

static void noop_composite(
	PIXEL_MANIPULATE_IMPLEMENTATION *imp,
	PIXEL_MANIPULATE_COMPOSITE_INFO *info
)
{
	return;
}

static uint32* noop_get_scanline(PIXEL_MANIPULATE_ITERATION *iter, const uint32 *mask)
{
	uint32 *result = iter->buffer;

	iter->buffer += iter->image->bits.row_stride;

	return result;
}

static void noop_init_solid_narrow(PIXEL_MANIPULATE_ITERATION *iter,
	const PIXEL_MANIPULATE_ITERATION_INFO *info)
{ 
	PIXEL_MANIPULATE_IMAGE *image = iter->image;
	uint32 *buffer = iter->buffer;
	uint32 *end = buffer + iter->width;
	uint32 color;

	if (iter->image->type == PIXEL_MANIPULATE_IMAGE_TYPE_SOLID)
		color = image->solid.color_32;
	else
		color = image->bits.fetch_pixel_32 (&image->bits, 0, 0);

	while (buffer < end)
		*(buffer++) = color;
}

static void noop_init_solid_wide(
	PIXEL_MANIPULATE_ITERATION *iter,
	const PIXEL_MANIPULATE_ITERATION_INFO *info
)
{
	PIXEL_MANIPULATE_IMAGE *image = iter->image;
	PIXEL_MANIPULATE_ARGB *buffer = (PIXEL_MANIPULATE_ARGB*)iter->buffer;
	PIXEL_MANIPULATE_ARGB *end = buffer + iter->width;
	PIXEL_MANIPULATE_ARGB color;

	if (iter->image->type == PIXEL_MANIPULATE_IMAGE_TYPE_SOLID)
		color = image->solid.color_float;
	else
		color = image->bits.fetch_pixel_float (&image->bits, 0, 0);

	while (buffer < end)
		*(buffer++) = color;
}

static void
noop_init_direct_buffer (PIXEL_MANIPULATE_ITERATION *iter, const PIXEL_MANIPULATE_ITERATION_INFO *info)
{
	PIXEL_MANIPULATE_IMAGE *image = iter->image;

	iter->buffer =
		image->bits.bits + iter->y * image->bits.row_stride + iter->x;
}

static void
dest_write_back_direct (PIXEL_MANIPULATE_ITERATION *iter)
{
	iter->buffer += iter->image->bits.row_stride;
}

static const PIXEL_MANIPULATE_ITERATION_INFO noop_iters[] =
{
	/* Source iters */
	{ PIXEL_MANIPULATE_FORMAT_ANY,
	0, ITER_IGNORE_ALPHA | ITER_IGNORE_RGB | ITER_SRC,
	NULL,
	PixelManipulateIteratorGetScanlineNoop,
	NULL
	},
{ PIXEL_MANIPULATE_SOLID,
FAST_PATH_NO_ALPHA_MAP, ITER_NARROW | ITER_SRC,
noop_init_solid_narrow,
PixelManipulateIteratorGetScanlineNoop,
NULL,
},
{ PIXEL_MANIPULATE_SOLID,
FAST_PATH_NO_ALPHA_MAP, ITER_WIDE | ITER_SRC,
noop_init_solid_wide,
PixelManipulateIteratorGetScanlineNoop,
NULL
},
{ PIXEL_MANIPULATE_FORMAT_A8R8G8B8,
FAST_PATH_STANDARD_FLAGS | FAST_PATH_ID_TRANSFORM |
FAST_PATH_BITS_IMAGE | FAST_PATH_SAMPLES_COVER_CLIP_NEAREST,
ITER_NARROW | ITER_SRC,
noop_init_direct_buffer,
noop_get_scanline,
NULL
},
/* Dest iters */
{ PIXEL_MANIPULATE_FORMAT_A8R8G8B8,
FAST_PATH_STD_DEST_FLAGS, ITER_NARROW | ITER_DEST,
noop_init_direct_buffer,
PixelManipulateIteratorGetScanlineNoop,
dest_write_back_direct
},
{ PIXEL_MANIPULATE_FORMAT_X8R8G8B8,
FAST_PATH_STD_DEST_FLAGS, ITER_NARROW | ITER_DEST | ITER_LOCALIZED_ALPHA,
noop_init_direct_buffer,
PixelManipulateIteratorGetScanlineNoop,
dest_write_back_direct
},
{ PIXEL_MANIPULATE_null },
};

static const PIXEL_MANIPULATE_FAST_PATH noop_fast_paths[] =
{
	{ PIXEL_MANIPULATE_OPERATE_DST, PIXEL_MANIPULATE_FORMAT_ANY, 0,
		PIXEL_MANIPULATE_FORMAT_ANY, 0, PIXEL_MANIPULATE_FORMAT_ANY, 0, noop_composite },
{ PIXEL_MANIPULATE_OPERATE_NONE },
};

PIXEL_MANIPULATE_IMPLEMENTATION* PixelManipulateImplementationCreateNoOperator(PIXEL_MANIPULATE_IMPLEMENTATION *fallback)
{
	PIXEL_MANIPULATE_IMPLEMENTATION *imp =
		PixelManipulateImplementationCreate(fallback, noop_fast_paths);

	imp->iteration_info = noop_iters;

	return imp;
}
