#include <string.h>
#include "pixel_manipulate.h"
#include "pixel_manipulate_region.h"
#include "pixel_manipulate_composite.h"
#include "pixel_manipulate_inline.h"
#include "pixel_manipulate_private.h"
#include "../memory.h"

#ifdef __cplusplus
extern "C" {
#endif

static uint32* CreateBits(
	ePIXEL_MANIPULATE_FORMAT format,
	int width,
	int height,
	int* row_stride_bytes,
	int clear
)
{
	int stride;
	size_t buffer_size;
	int bpp;

	bpp = PIXEL_MANIPULATE_BIT_PER_PIXEL(format);

	stride = width * bpp;

	stride += 0x1F;
	stride >> 5;

	stride *= sizeof(uint32);

	buffer_size = (size_t)height * stride;

	if(row_stride_bytes != NULL)
	{
		*row_stride_bytes = stride;
	}

	if(clear == FALSE)
	{
		return (uint32*)MEM_ALLOC_FUNC(buffer_size);
	}
	return (uint32*)MEM_CALLOC_FUNC(buffer_size, 1);
}

static void BitsImagePropertyChanged(PIXEL_MANIPULATE_IMAGE* image)
{
	PixelManipulateSetupAccessors(&image->bits);
}

int PixelManipulateImageInitialize(
	PIXEL_MANIPULATE_IMAGE* image,
	ePIXEL_MANIPULATE_FORMAT format,
	int width,
	int height,
	uint32* bits,
	int rowstride_bytes,
	int clear,
	PIXEL_MANIPULATE* manipulate
)
{
	uint32 *to_free = NULL;
	int result;

	if(bits == NULL && width > 0 && height > 0)
	{
		bits = to_free = CreateBits(format, width, height, &rowstride_bytes, clear);
	}

	result = InitializePixelManipulateBitsImage(image, format, width, height, bits, rowstride_bytes, clear);
	image->common.pixel_manipulate = manipulate;

	return result;

	// image->type = PIXEL_MANIPULATE_IMAGE_TYPE_BITS;
	// image->bits.format = format;
	// image->bits.width = width;
	// image->bits.height = height;
	// image->bits.bits = bits;
	// image->common.to_free = to_free;
	// image->bits.dither = PIXEL_MANIPULATE_DITHER_NONE;
	// image->bits.dither_offset_x = 0;
	// image->bits.dither_offset_y = 0;
	// image->bits.row_stride = rowstride_bytes;
	// image->bits.indexed = NULL;
	// image->common.pixel_manipulate = manipulate;

	// image->common.property_changed_function = BitsImagePropertyChanged;
	// PixelManipulateResetClipRegion(image);
	// PixelManipulateSetupAccessors(image);

	// return TRUE;
}

int PixelManipulateImageInitializeWithBits(
	PIXEL_MANIPULATE_IMAGE* image,
	ePIXEL_MANIPULATE_FORMAT format,
	int width,
	int height,
	uint32* bits,
	int rowstride_bytes,
	int clear,
	PIXEL_MANIPULATE* manipulate
)
{
	if(bits == NULL || rowstride_bytes % sizeof(*bits) != 0)
	{
		return FALSE;
	}

	if(format >= NUM_PIXEL_MANIPULATE_FORMAT || format < 0)
	{
		return FALSE;
	}

	if(image == NULL)
	{
		return FALSE;
	}

	return PixelManipulateImageInitialize(image, format,
		width, height, bits, rowstride_bytes, clear, manipulate);
}

void InitializePixelManipulateImage(PIXEL_MANIPULATE_IMAGE* image)
{
	const PIXEL_MANIPULATE_IMAGE zero_image = {0};
	const PIXEL_MANIPULATE_BOX32 empty_box = {0};
	*image = zero_image;

	image->common.reference_count = 1;
	image->common.clip_region.data = (PIXEL_MANIPULATE_REGION_DATA*)PIXEL_MANIPULATE_EMPTY_DATA;
	image->common.clip_region.extents = empty_box;
	image->common.repeat = PIXEL_MANIPULATE_REPEAT_TYPE_NONE;
	image->common.filter = PIXEL_MANIPULATE_FILTER_TYPE_NEAREST;
	image->common.reference_count = 1;
	image->common.dirty = TRUE;
	image->common.self_allocated = TRUE;
}

PIXEL_MANIPULATE_IMAGE* PixelManipulateAllocate(void)
{
	PIXEL_MANIPULATE_IMAGE *image;
	const PIXEL_MANIPULATE_BOX32 empty_box = {0};

	image = (PIXEL_MANIPULATE_IMAGE*)MEM_ALLOC_FUNC(sizeof(*image));

	InitializePixelManipulateImage(image);

	return image;
}

PIXEL_MANIPULATE_IMAGE* CreatePixelManipulateImageBits(
	ePIXEL_MANIPULATE_FORMAT format,
	int width,
	int height,
	uint32* bits,
	int row_stride,
	PIXEL_MANIPULATE* manipulate
)
{
	PIXEL_MANIPULATE_IMAGE *image;

	if(bits == NULL || row_stride % sizeof(uint32) != 0)
	{
		return NULL;
	}

	image = PixelManipulateAllocate();

	if(image == NULL)
	{
		return NULL;
	}

	if(PixelManipulateImageInitializeWithBits(image, format, width, height, bits, row_stride, TRUE, manipulate) == FALSE)
	{
		MEM_FREE_FUNC(image);
		return NULL;
	}

	return image;
}

int PixelManipulateImageFinish(PIXEL_MANIPULATE_IMAGE* image)
{
	PIXEL_MANIPULATE_IMAGE_COMMON *common = (PIXEL_MANIPULATE_IMAGE_COMMON*)image;

	common->reference_count--;
	if(common->reference_count <= 0)
	{
		if(image->common.delete_function != NULL)
		{
			image->common.delete_function(image, image->common.delete_data);
		}
	}

	PixelManipulateRegion32Finish(&common->clip_region);

	MEM_FREE_FUNC(common->transform);
	MEM_FREE_FUNC(common->filter_parameters);

	if(common->alpha_map != NULL)
	{
		PixelManipulateImageUnreference(common->alpha_map);
	}

	if(image->type == PIXEL_MANIPULATE_IMAGE_TYPE_LINEAR || image->type == PIXEL_MANIPULATE_IMAGE_TYPE_RADIAL
			|| image->type == PIXEL_MANIPULATE_IMAGE_TYPE_CONICAL)
	{
		if(image->gradient.stops != NULL)
		{
			MEM_FREE_FUNC(image->gradient.stops - 1);
		}
	}

	if(image->type == PIXEL_MANIPULATE_IMAGE_TYPE_BITS && image->common.to_free != NULL)
	{
		MEM_FREE_FUNC(image->common.to_free);
		return TRUE;
	}

	return FALSE;
}

int PixelManipulateFill(
	uint32* bits,
	int stride,
	int bpp,
	int x,
	int y,
	int width,
	int height,
	uint32 filler,
	PIXEL_MANIPULATE_IMPLEMENTATION* implementation
)
{
	return PixelManipulateImplementationFill(implementation,
		bits, stride, bpp, x, y, width, height, filler);
}

int PixelManipulateBlockTransfer(
	uint32* src_bits,
	uint32* dst_bits,
	int src_stride,
	int dst_stride,
	int src_bpp,
	int dst_bpp,
	int src_x,
	int src_y,
	int dest_x,
	int dest_y,
	int width,
	int height,
	PIXEL_MANIPULATE_IMPLEMENTATION* implementation
)
{
	return PixelManipulateImplementationBlockTransfer(implementation,
			src_bits, dst_bits, src_stride, dst_stride, src_bpp, dst_bpp,
				src_x, src_y, dest_x, dest_y, width, height);
}

typedef struct operator_info_t operator_info_t;

struct operator_info_t
{
	uint8 opaque_info[4];
};

#define PACK(neither, src, dest, both)			\
	{{		(uint8)PIXEL_MANIPULATE_OPERATE_ ## neither,		\
		(uint8)PIXEL_MANIPULATE_OPERATE_ ## src,			\
		(uint8)PIXEL_MANIPULATE_OPERATE_ ## dest,		\
		(uint8)PIXEL_MANIPULATE_OPERATE_ ## both		}}

static const operator_info_t operator_table[] =
{
	/*	Neither Opaque		 Src Opaque			 Dst Opaque			 Both Opaque */
	PACK (CLEAR,				 CLEAR,				 CLEAR,				 CLEAR),
	PACK (SRC,				   SRC,				   SRC,				   SRC),
	PACK (DST,				   DST,				   DST,				   DST),
	PACK (OVER,				  SRC,				   OVER,				  SRC),
	PACK (OVER_REVERSE,		  OVER_REVERSE,		  DST,				   DST),
	PACK (IN,					IN,					SRC,				   SRC),
	PACK (IN_REVERSE,			DST,				   IN_REVERSE,			DST),
	PACK (OUT,				   OUT,				   CLEAR,				 CLEAR),
	PACK (OUT_REVERSE,		   CLEAR,				 OUT_REVERSE,		   CLEAR),
	PACK (ATOP,				  IN,					OVER,				  SRC),
	PACK (ATOP_REVERSE,		  OVER_REVERSE,		  IN_REVERSE,			DST),
	PACK (XOR,				   OUT,				   OUT_REVERSE,		   CLEAR),
	PACK (ADD,				   ADD,				   ADD,				   ADD),
	PACK (SATURATE,			  OVER_REVERSE,		  DST,				   DST),

	{{ 0 /* 0x0e */ }},
	{{ 0 /* 0x0f */ }},

	PACK (CLEAR,				 CLEAR,				 CLEAR,				 CLEAR),
	PACK (SRC,				   SRC,				   SRC,				   SRC),
	PACK (DST,				   DST,				   DST,				   DST),
	PACK (DISJOINT_OVER,		 DISJOINT_OVER,		 DISJOINT_OVER,		 DISJOINT_OVER),
	PACK (DISJOINT_OVER_REVERSE, DISJOINT_OVER_REVERSE, DISJOINT_OVER_REVERSE, DISJOINT_OVER_REVERSE),
	PACK (DISJOINT_IN,		   DISJOINT_IN,		   DISJOINT_IN,		   DISJOINT_IN),
	PACK (DISJOINT_IN_REVERSE,   DISJOINT_IN_REVERSE,   DISJOINT_IN_REVERSE,   DISJOINT_IN_REVERSE),
	PACK (DISJOINT_OUT,		  DISJOINT_OUT,		  DISJOINT_OUT,		  DISJOINT_OUT),
	PACK (DISJOINT_OUT_REVERSE,  DISJOINT_OUT_REVERSE,  DISJOINT_OUT_REVERSE,  DISJOINT_OUT_REVERSE),
	PACK (DISJOINT_ATOP,		 DISJOINT_ATOP,		 DISJOINT_ATOP,		 DISJOINT_ATOP),
	PACK (DISJOINT_ATOP_REVERSE, DISJOINT_ATOP_REVERSE, DISJOINT_ATOP_REVERSE, DISJOINT_ATOP_REVERSE),
	PACK (DISJOINT_XOR,		  DISJOINT_XOR,		  DISJOINT_XOR,		  DISJOINT_XOR),

	{{ 0 /* 0x1c */ }},
	{{ 0 /* 0x1d */ }},
	{{ 0 /* 0x1e */ }},
	{{ 0 /* 0x1f */ }},

	PACK (CLEAR,				 CLEAR,				 CLEAR,				 CLEAR),
	PACK (SRC,				   SRC,				   SRC,				   SRC),
	PACK (DST,				   DST,				   DST,				   DST),
	PACK (CONJOINT_OVER,		 CONJOINT_OVER,		 CONJOINT_OVER,		 CONJOINT_OVER),
	PACK (CONJOINT_OVER_REVERSE, CONJOINT_OVER_REVERSE, CONJOINT_OVER_REVERSE, CONJOINT_OVER_REVERSE),
	PACK (CONJOINT_IN,		   CONJOINT_IN,		   CONJOINT_IN,		   CONJOINT_IN),
	PACK (CONJOINT_IN_REVERSE,   CONJOINT_IN_REVERSE,   CONJOINT_IN_REVERSE,   CONJOINT_IN_REVERSE),
	PACK (CONJOINT_OUT,		  CONJOINT_OUT,		  CONJOINT_OUT,		  CONJOINT_OUT),
	PACK (CONJOINT_OUT_REVERSE,  CONJOINT_OUT_REVERSE,  CONJOINT_OUT_REVERSE,  CONJOINT_OUT_REVERSE),
	PACK (CONJOINT_ATOP,		 CONJOINT_ATOP,		 CONJOINT_ATOP,		 CONJOINT_ATOP),
	PACK (CONJOINT_ATOP_REVERSE, CONJOINT_ATOP_REVERSE, CONJOINT_ATOP_REVERSE, CONJOINT_ATOP_REVERSE),
	PACK (CONJOINT_XOR,		  CONJOINT_XOR,		  CONJOINT_XOR,		  CONJOINT_XOR),

	{{ 0 /* 0x2c */ }},
	{{ 0 /* 0x2d */ }},
	{{ 0 /* 0x2e */ }},
	{{ 0 /* 0x2f */ }},

	PACK (MULTIPLY,			  MULTIPLY,			  MULTIPLY,			  MULTIPLY),
	PACK (SCREEN,				SCREEN,				SCREEN,				SCREEN),
	PACK (OVERLAY,			   OVERLAY,			   OVERLAY,			   OVERLAY),
	PACK (DARKEN,				DARKEN,				DARKEN,				DARKEN),
	PACK (LIGHTEN,			   LIGHTEN,			   LIGHTEN,			   LIGHTEN),
	PACK (COLOR_DODGE,		   COLOR_DODGE,		   COLOR_DODGE,		   COLOR_DODGE),
	PACK (COLOR_BURN,			COLOR_BURN,			COLOR_BURN,			COLOR_BURN),
	PACK (HARD_LIGHT,			HARD_LIGHT,			HARD_LIGHT,			HARD_LIGHT),
	PACK (SOFT_LIGHT,			SOFT_LIGHT,			SOFT_LIGHT,			SOFT_LIGHT),
	PACK (DIFFERENCE,			DIFFERENCE,			DIFFERENCE,			DIFFERENCE),
	PACK (EXCLUSION,			 EXCLUSION,			 EXCLUSION,			 EXCLUSION),
	PACK (HSL_HUE,			   HSL_HUE,			   HSL_HUE,			   HSL_HUE),
	PACK (HSL_SATURATION,		HSL_SATURATION,		HSL_SATURATION,		HSL_SATURATION),
	PACK (HSL_COLOR,			 HSL_COLOR,			 HSL_COLOR,			 HSL_COLOR),
	PACK (HSL_LUMINOSITY,		HSL_LUMINOSITY,		HSL_LUMINOSITY,		HSL_LUMINOSITY),
};

/*
* Optimize the current operator based on opacity of source or destination
* The output operator should be mathematically equivalent to the source.
*/
static ePIXEL_MANIPULATE_OPERATE optimize_operator(
	ePIXEL_MANIPULATE_OPERATE op,
	uint32 src_flags,
	uint32 mask_flags,
	uint32 dst_flags
)
{
	int is_source_opaque, is_dest_opaque;

#define OPAQUE_SHIFT 13

	is_dest_opaque = (dst_flags & FAST_PATH_IS_OPAQUE);
	is_source_opaque = ((src_flags & mask_flags) & FAST_PATH_IS_OPAQUE);

	is_dest_opaque >>= OPAQUE_SHIFT - 1;
	is_source_opaque >>= OPAQUE_SHIFT;

	return operator_table[op].opaque_info[is_dest_opaque | is_source_opaque];
}

typedef struct box_48_16 box_48_16_t;

struct box_48_16
{
	PIXEL_MANIPULATE_FIXED_48_16 x1;
	PIXEL_MANIPULATE_FIXED_48_16 y1;
	PIXEL_MANIPULATE_FIXED_48_16 x2;
	PIXEL_MANIPULATE_FIXED_48_16 y2;
};

#define PIXEL_MANIPULATE_MAXIMUM_FIXED_48_16 ((PIXEL_MANIPULATE_FIXED_48_16)0x7fffffff)
#define PIXEL_MANIPULATE_MINIMUM_FIXED_48_16 (-((PIXEL_MANIPULATE_FIXED_48_16)1 << 31))

#define IS_16BIT(x) (((x) >= INT16_MIN) && ((x) <= INT16_MAX))
#define ABS(f)	  (((f) < 0)?  (-(f)) : (f))
#define IS_16_16(f) (((f) >= PIXEL_MANIPULATE_MINIMUM_FIXED_48_16 && ((f) <= PIXEL_MANIPULATE_MAXIMUM_FIXED_48_16)))

static int compute_transformed_extents(
	PIXEL_MANIPULATE_TRANSFORM* transform,
	const PIXEL_MANIPULATE_BOX32* extents,
	box_48_16_t* transformed
)
{
	PIXEL_MANIPULATE_FIXED_48_16 tx1, ty1, tx2, ty2;
	PIXEL_MANIPULATE_FIXED x1, y1, x2, y2;
	int i;
	
	x1 = PIXEL_MANIPULATE_FIXED_TO_INTEGER(extents->x1) + PIXEL_MANIPULATE_FIXED_1 / 2;
	y1 = PIXEL_MANIPULATE_FIXED_TO_INTEGER(extents->y1) + PIXEL_MANIPULATE_FIXED_1 / 2;
	x2 = PIXEL_MANIPULATE_FIXED_TO_INTEGER(extents->x2) - PIXEL_MANIPULATE_FIXED_1 / 2;
	y2 = PIXEL_MANIPULATE_FIXED_TO_INTEGER(extents->y2) - PIXEL_MANIPULATE_FIXED_1 / 2;

	if (!transform)
	{
		transformed->x1 = x1;
		transformed->y1 = y1;
		transformed->x2 = x2;
		transformed->y2 = y2;

		return TRUE;
	}

	tx1 = ty1 = INT64_MAX;
	tx2 = ty2 = INT64_MIN;

	for (i = 0; i < 4; ++i)
	{
		PIXEL_MANIPULATE_FIXED_48_16 tx, ty;
		PIXEL_MANIPULATE_VECTOR v;

		v.vector[0] = (i & 0x01)? x1 : x2;
		v.vector[1] = (i & 0x02)? y1 : y2;
		v.vector[2] = PIXEL_MANIPULATE_FIXED_1;
		
		if (!PixelManipulateTransformPoint(transform, &v))
			return FALSE;

		tx = (PIXEL_MANIPULATE_FIXED_48_16)v.vector[0];
		ty = (PIXEL_MANIPULATE_FIXED_48_16)v.vector[1];

		if (tx < tx1)
			tx1 = tx;
		if (ty < ty1)
			ty1 = ty;
		if (tx > tx2)
			tx2 = tx;
		if (ty > ty2)
			ty2 = ty;
	}

	transformed->x1 = tx1;
	transformed->y1 = ty1;
	transformed->x2 = tx2;
	transformed->y2 = ty2;

	return TRUE;
}

static int analyze_extent(
	PIXEL_MANIPULATE_IMAGE* image,
	const PIXEL_MANIPULATE_BOX32* extents,
	uint32* flags
)
{
	PIXEL_MANIPULATE_TRANSFORM *transform;
	PIXEL_MANIPULATE_FIXED x_off, y_off;
	PIXEL_MANIPULATE_FIXED width, height;
	PIXEL_MANIPULATE_FIXED *params;
	box_48_16_t transformed;
	PIXEL_MANIPULATE_BOX32 exp_extents;

	if (!image)
		return TRUE;

	/* Some compositing functions walk one step
	* outside the destination rectangle, so we
	* check here that the expanded-by-one source
	* extents in destination space fits in 16 bits
	*/
	if (!IS_16BIT (extents->x1 - 1)		||
		!IS_16BIT (extents->y1 - 1)		||
		!IS_16BIT (extents->x2 + 1)		||
		!IS_16BIT (extents->y2 + 1))
	{
		return FALSE;
	}

	transform = image->common.transform;
	if (image->common.type == PIXEL_MANIPULATE_IMAGE_TYPE_BITS)
	{
		/* During repeat mode calculations we might convert the
		* width/height of an image to fixed 16.16, so we need
		* them to be smaller than 16 bits.
		*/
		if (image->bits.width >= 0x7fff	|| image->bits.height >= 0x7fff)
			return FALSE;

		if ((image->common.flags & FAST_PATH_ID_TRANSFORM) == FAST_PATH_ID_TRANSFORM &&
			extents->x1 >= 0 &&
			extents->y1 >= 0 &&
			extents->x2 <= image->bits.width &&
			extents->y2 <= image->bits.height)
		{
			*flags |= FAST_PATH_SAMPLES_COVER_CLIP_NEAREST;
			return TRUE;
		}

		switch (image->common.filter)
		{
		case PIXEL_MANIPULATE_FILTER_TYPE_CONVOLUTION:
			params = image->common.filter_parameters;
			x_off = - PIXEL_MANIPULATE_FIXED_E - ((params[0] - PIXEL_MANIPULATE_FIXED_1) >> 1);
			y_off = - PIXEL_MANIPULATE_FIXED_E - ((params[1] - PIXEL_MANIPULATE_FIXED_1) >> 1);
			width = params[0];
			height = params[1];
			break;

		case PIXEL_MANIPULATE_FILTER_TYPE_SEPARABLE_CONVOLUTION:
			params = image->common.filter_parameters;
			x_off = - PIXEL_MANIPULATE_FIXED_E - ((params[0] - PIXEL_MANIPULATE_FIXED_1) >> 1);
			y_off = - PIXEL_MANIPULATE_FIXED_E - ((params[1] - PIXEL_MANIPULATE_FIXED_1) >> 1);
			width = params[0];
			height = params[1];
			break;

		case PIXEL_MANIPULATE_FILTER_TYPE_GOOD:
		case PIXEL_MANIPULATE_FILTER_TYPE_BEST:
		case PIXEL_MANIPULATE_FILTER_TYPE_BILINEAR:
			x_off = - PIXEL_MANIPULATE_FIXED_1 / 2;
			y_off = - PIXEL_MANIPULATE_FIXED_1 / 2;
			width = PIXEL_MANIPULATE_FIXED_1;
			height = PIXEL_MANIPULATE_FIXED_1;
			break;

		case PIXEL_MANIPULATE_FILTER_TYPE_FAST:
		case PIXEL_MANIPULATE_FILTER_TYPE_NEAREST:
			x_off = - PIXEL_MANIPULATE_FIXED_E;
			y_off = - PIXEL_MANIPULATE_FIXED_E;
			width = 0;
			height = 0;
			break;

		default:
			return FALSE;
		}
	}
	else
	{
		x_off = 0;
		y_off = 0;
		width = 0;
		height = 0;
	}

	if (!compute_transformed_extents (transform, extents, &transformed))
		return FALSE;
	
	if (image->common.type == PIXEL_MANIPULATE_IMAGE_TYPE_BITS)
	{
		if (PIXEL_MANIPULATE_FIXED_TO_INTEGER(transformed.x1 - PIXEL_MANIPULATE_FIXED_E) >= 0				&&
			PIXEL_MANIPULATE_FIXED_TO_INTEGER(transformed.y1 - PIXEL_MANIPULATE_FIXED_E) >= 0				&&
			PIXEL_MANIPULATE_FIXED_TO_INTEGER(transformed.x2 - PIXEL_MANIPULATE_FIXED_E) < image->bits.width &&
			PIXEL_MANIPULATE_FIXED_TO_INTEGER(transformed.y2 - PIXEL_MANIPULATE_FIXED_E) < image->bits.height)
		{
			*flags |= FAST_PATH_SAMPLES_COVER_CLIP_NEAREST;
		}

		if (PIXEL_MANIPULATE_FIXED_TO_INTEGER(transformed.x1 - PIXEL_MANIPULATE_FIXED_1 / 2) >= 0		  &&
			PIXEL_MANIPULATE_FIXED_TO_INTEGER(transformed.y1 - PIXEL_MANIPULATE_FIXED_1 / 2) >= 0		  &&
			PIXEL_MANIPULATE_FIXED_TO_INTEGER(transformed.x2 + PIXEL_MANIPULATE_FIXED_1 / 2) < image->bits.width &&
			PIXEL_MANIPULATE_FIXED_TO_INTEGER(transformed.y2 + PIXEL_MANIPULATE_FIXED_1 / 2) < image->bits.height)
		{
			*flags |= FAST_PATH_SAMPLES_COVER_CLIP_BILINEAR;
		}
	}

	/* Check we don't overflow when the destination extents are expanded by one.
	* This ensures that compositing functions can simply walk the source space
	* using 16.16 variables without worrying about overflow.
	*/
	exp_extents = *extents;
	exp_extents.x1 -= 1;
	exp_extents.y1 -= 1;
	exp_extents.x2 += 1;
	exp_extents.y2 += 1;

	if (!compute_transformed_extents (transform, &exp_extents, &transformed))
		return FALSE;

	if (!IS_16_16 (transformed.x1 + x_off - 8 * PIXEL_MANIPULATE_FIXED_E)	||
		!IS_16_16 (transformed.y1 + y_off - 8 * PIXEL_MANIPULATE_FIXED_E)	||
		!IS_16_16 (transformed.x2 + x_off + 8 * PIXEL_MANIPULATE_FIXED_E + width)	||
		!IS_16_16 (transformed.y2 + y_off + 8 * PIXEL_MANIPULATE_FIXED_E + height))
	{
		return FALSE;
	}

	return TRUE;
}

void PixelManipulateImageComposite32(
	ePIXEL_MANIPULATE_OPERATE op,
	PIXEL_MANIPULATE_IMAGE* src,
	PIXEL_MANIPULATE_IMAGE* mask,
	PIXEL_MANIPULATE_IMAGE* dest,
	int32 src_x,
	int32 src_y,
	int32 mask_x,
	int32 mask_y,
	int32 dest_x,
	int32 dest_y,
	int32 width,
	int32 height,
	PIXEL_MANIPULATE_IMPLEMENTATION* implementation
)
{
	ePIXEL_MANIPULATE_FORMAT src_format, mask_format, dest_format;
	PIXEL_MANIPULATE_REGION32 region;
	PIXEL_MANIPULATE_BOX32 extents;
	PIXEL_MANIPULATE_IMPLEMENTATION *imp;
	pixel_manipulate_composite_function function;
	PIXEL_MANIPULATE_COMPOSITE_INFO info;
	const PIXEL_MANIPULATE_BOX32 *pbox;
	int n;

	PixelManipulateImageValidate(src);
	if(mask != NULL)
	{
		PixelManipulateImageValidate(mask);
	}
	PixelManipulateImageValidate(dest);

	src_format = src->common.extended_format_code;
	info.src_flags = src->common.flags;

	if(mask != NULL && FALSE == (mask->common.flags & PIXEL_MANIPULATE_FAST_PATH_IS_OPAQUE))
	{
		mask_format = mask->common.extended_format_code;
		info.mask_flags = mask->common.flags;
	}
	else
	{
		mask_format = PIXEL_MANIPULATE_null;
		info.mask_flags = FAST_PATH_IS_OPAQUE | FAST_PATH_NO_ALPHA_MAP;
	}

	dest_format = dest->common.extended_format_code;
	info.dest_flags = dest->common.flags;

	if((mask_format == PIXEL_MANIPULATE_FORMAT_A8R8G8B8 || mask_format == PIXEL_MANIPULATE_FORMAT_A8B8G8R8)
		&& (src->type == PIXEL_MANIPULATE_IMAGE_TYPE_BITS && src->bits.bits == mask->bits.bits)
		&& (src->common.repeat == mask->common.repeat)
		&& (info.src_flags & info.mask_flags & FAST_PATH_ID_TRANSFORM)
		&& (src_x == mask_x && src_y == mask_y))
	{
		if(src_format == PIXEL_MANIPULATE_FORMAT_X8B8G8R8)
		{
			src_format = mask_format = PIXEL_MANIPULATE_PIXBUF;
		}
		else if(src_format == PIXEL_MANIPULATE_FORMAT_X8R8G8B8)
		{
			src_format = mask_format = PIXEL_MANIPULATE_RPIXBUF;
		}
	}

	InitializePixelManipulateRegion32(&region);

	if(FALSE == PixelManipulateComputeCompositeRegion32(&region, src, mask, dest,
					src_x, src_y, mask_x, mask_y, dest_x, dest_y, width, height))
	{
		goto OUT;
	}

	extents = region.extents;

	extents.x1 -= dest_x - src_x;
	extents.y1 -= dest_y - src_y;
	extents.x2 -= dest_x - src_x;
	extents.y2 -= dest_y - src_y;

	if(FALSE == analyze_extent(mask, &extents, &info.mask_flags))
	{
		goto OUT;
	}

#define NEAREST_OPAQUE	(FAST_PATH_SAMPLES_OPAQUE |			\
			 FAST_PATH_NEAREST_FILTER |			\
			 FAST_PATH_SAMPLES_COVER_CLIP_NEAREST)
#define BILINEAR_OPAQUE	(FAST_PATH_SAMPLES_OPAQUE |			\
			 FAST_PATH_BILINEAR_FILTER |			\
			 FAST_PATH_SAMPLES_COVER_CLIP_BILINEAR)

	if((info.src_flags & NEAREST_OPAQUE) == NEAREST_OPAQUE ||
		(info.src_flags & BILINEAR_OPAQUE) == BILINEAR_OPAQUE)
	{
		info.src_flags |= FAST_PATH_IS_OPAQUE;
	}

	if((info.mask_flags & NEAREST_OPAQUE) == NEAREST_OPAQUE ||
		(info.mask_flags & BILINEAR_OPAQUE) == BILINEAR_OPAQUE)
	{
		info.mask_flags |= FAST_PATH_IS_OPAQUE;
	}

	/*
	* Check if we can replace our operator by a simpler one
	* if the src or dest are opaque. The output operator should be
	* mathematically equivalent to the source.
	*/
	info.op = optimize_operator(op, info.src_flags, info.mask_flags, info.dest_flags);

	PixelManipulateImplementationLookupComposite(
		implementation, info.op,
		src_format, info.src_flags,
		mask_format, info.mask_flags,
		dest_format, info.dest_flags,
		&imp, &function
	);

	info.src_image = src;
	info.mask_image = mask;
	info.dest_image = dest;

	pbox = PixelManipulateRegion32Rectangles(&region, &n);

	while(n--)
	{
		info.src_x = pbox->x1 + src_x - dest_x;
		info.src_y = pbox->y1 + src_y - dest_y;
		info.mask_x = pbox->x1 + mask_x - dest_x;
		info.mask_y = pbox->y1 + mask_y - dest_y;
		info.dest_x = pbox->x1;
		info.dest_y = pbox->y1;
		info.width = pbox->x2 - pbox->x1;
		info.height = pbox->y2 - pbox->y1;

		function(imp, &info);
		pbox++;
	}

OUT:
	PixelManipulateRegion32Finish(&region);
}

void PixelManipulateImageSetDestroyFunction(
	PIXEL_MANIPULATE_IMAGE* image,
	void (*function)(struct _PIXEL_MANIPULATE_IMAGE_COMMON*, void*),
	void* data
)
{
	image->common.delete_function = function;
	image->common.delete_data = data;
}

void PixelManipulateChooseImplementation(PIXEL_MANIPULATE_IMPLEMENTATION** implementation)
{
	*implementation = PixelManipulateImplementationCreateGeneral();
	*implementation = PixelManipulateX86GetImplementations(*implementation);
	*implementation = PixelManipulateImplementationCreateNoOperator(*implementation);
}

void InitializePixelManipulate(PIXEL_MANIPULATE* pixel_manipulate)
{
	PIXEL_MANIPULATE_IMPLEMENTATION *imp = &pixel_manipulate->implementation;
	PixelManipulateChooseImplementation(&imp);
	pixel_manipulate->implementation = *imp;
}

#ifdef __cplusplus
}
#endif
