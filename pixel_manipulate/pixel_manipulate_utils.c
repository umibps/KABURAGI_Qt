#include "pixel_manipulate.h"
#include "pixel_manipulate_composite.h"
#include "pixel_manipulate_private.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32* PixelManipulateIteraterationInitializeBitsStride(PIXEL_MANIPULATE_ITERATION* iter, const PIXEL_MANIPULATE_ITERATION_INFO* info)
{
	PIXEL_MANIPULATE_IMAGE *image = iter->image;
	uint8 *b = (uint8*)image->bits.bits;
	int s = image->bits.row_stride;

	iter->bits = b + s * iter->y + iter->x * PIXEL_MANIPULATE_FORMAT_BPP(info->format) / 8;
	iter->stride = s;
}

uint32* PixelManipulateIteratorGetScanlineNoop(struct _PIXEL_MANIPULATE_ITERATION* iter, const uint32* mask)
{
	return iter->buffer;
}

#ifdef __cplusplus
}
#endif
