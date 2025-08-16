#include "pixel_manipulate.h"
#include "pixel_manipulate_region.h"
#include "pixel_manipulate_composite.h"
#include "pixel_manipulate_inline.h"
#include "pixel_manipulate_private.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

static uint32 ColorToUint32(const PIXEL_MANIPULATE_COLOR* color)
{
	return
		((unsigned int) color->alpha >> 8 << 24) |
		((unsigned int) color->red >> 8 << 16) |
		((unsigned int) color->green & 0xff00) |
		((unsigned int) color->blue >> 8);
}

static PIXEL_MANIPULATE_ARGB ColorToFloat(const PIXEL_MANIPULATE_COLOR* color)
{
	PIXEL_MANIPULATE_ARGB result;

	result.a = PixelManipulateUnormToFloat(color->alpha, 16);
	result.r = PixelManipulateUnormToFloat(color->red, 16);
	result.g = PixelManipulateUnormToFloat(color->green, 16);
	result.b = PixelManipulateUnormToFloat(color->blue, 16);

	return result;
}

void InitializePixelManipulateImageSolidFill(PIXEL_MANIPULATE_IMAGE* image, const PIXEL_MANIPULATE_COLOR* color)
{
	InitializePixelManipulateImage(image);

	image->type = PIXEL_MANIPULATE_IMAGE_TYPE_SOLID;
	image->solid.color = *color;
	image->solid.color_32 = ColorToUint32(color);
	image->solid.color_float = ColorToFloat(color);

	image->common.dirty = TRUE;
}

PIXEL_MANIPULATE_IMAGE* PixelManipulateImageCreateSolidFill(const PIXEL_MANIPULATE_COLOR* color)
{
	PIXEL_MANIPULATE_IMAGE *image = PixelManipulateAllocate();

	image->type = PIXEL_MANIPULATE_IMAGE_TYPE_SOLID;
	image->solid.color = *color;
	image->solid.color_32 = ColorToUint32(color);
	image->solid.color_float =ColorToFloat(color);

	image->common.dirty = TRUE;

	return image;
}

#ifdef __cplusplus
}
#endif
