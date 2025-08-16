#include <limits.h>
#include "pixel_manipulate.h"
#include "pixel_manipulate_composite.h"
#include "pixel_manipulate_region.h"
#include "pixel_manipulate_matrix.h"
#include "pixel_manipulate_private.h"

#ifdef __cplusplus
extern "C" {
#endif

static PIXEL_MANIPULATE_IMPLEMENTATION* GetImplementation(void)
{
	static PIXEL_MANIPULATE_IMPLEMENTATION *implementation = NULL;

	if(implementation == NULL)
	{
		implementation = PixelManipulateImplementationCreateGeneral();
		implementation = PixelManipulateX86GetImplementations(implementation);
	}

	return implementation;
}

typedef struct _OPERATOR_INFO
{
	uint8 opaque_info[4];
} OPERATOR_INFO;

static ePIXEL_MANIPULATE_OPERATE OptimizeOperator(
	ePIXEL_MANIPULATE_OPERATE op,
	uint32 src_flags,
	uint32 mask_flags,
	uint32 dest_flags
)
{
#define PACK(neither, src, dest, both)  \
	{{ (uint8)PIXEL_MANIPULATE_OPERATE_ ## neither,	\
		(uint8)PIXEL_MANIPULATE_OPERATE_ ## src,	\
		(uint8)PIXEL_MANIPULATE_OPERATE_ ## dest,   \
		(uint8)PIXEL_MANIPULATE_OPERATE_ ## both	}}
#define OPAQUE_SHIFT 13
	int is_source_opaque, is_dest_opaque;
	const OPERATOR_INFO operator_table[] =
	{
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
		   PACK (HSL_LUMINOSITY,		HSL_LUMINOSITY,		HSL_LUMINOSITY,		HSL_LUMINOSITY)
	};

	is_dest_opaque = (dest_flags & PIXEL_MANIPULATE_FAST_PATH_IS_OPAQUE);
	is_source_opaque = ((src_flags & mask_flags) & PIXEL_MANIPULATE_FAST_PATH_IS_OPAQUE);

	is_dest_opaque >>= OPAQUE_SHIFT - 1;
	is_source_opaque >>= OPAQUE_SHIFT;

	return operator_table[op].opaque_info[is_dest_opaque | is_source_opaque];
}

typedef struct _BOX_48_16
{
	PIXEL_MANIPULATE_FIXED_48_16 x1;
	PIXEL_MANIPULATE_FIXED_48_16 y1;
	PIXEL_MANIPULATE_FIXED_48_16 x2;
	PIXEL_MANIPULATE_FIXED_48_16 y2;
} BOX_48_16;

static int ComputeTransformedExtents(PIXEL_MANIPULATE_TRANSFORM* transform, const PIXEL_MANIPULATE_BOX32* extents, BOX_48_16* transformed)
{
	PIXEL_MANIPULATE_FIXED_48_16 tx1, ty1, tx2, ty2;
	PIXEL_MANIPULATE_FIXED x1, y1, x2, y2;
	int i;

	x1 = PIXEL_MANIPULATE_INTEGER_TO_FIXED(extents->x1) + PIXEL_MANIPULATE_FIXED_1 / 2;
	y1 = PIXEL_MANIPULATE_INTEGER_TO_FIXED(extents->y1) + PIXEL_MANIPULATE_FIXED_1 / 2;
	x2 = PIXEL_MANIPULATE_INTEGER_TO_FIXED(extents->x2) + PIXEL_MANIPULATE_FIXED_1 / 2;
	y2 = PIXEL_MANIPULATE_INTEGER_TO_FIXED(extents->y2) + PIXEL_MANIPULATE_FIXED_1 / 2;

	if(transform == NULL)
	{
		transformed->x1 = x1;
		transformed->y1 = y1;
		transformed->x2 = x2;
		transformed->y2 = y2;

		return TRUE;
	}

	tx1 = ty1 = 0x7FFFFFFFFFFFFFFF;
	tx2 = ty2 = 0xFFFFFFFFFFFFFFFF;

	for(i=0; i<4; i++)
	{
		PIXEL_MANIPULATE_FIXED_48_16 tx, ty;
		PIXEL_MANIPULATE_VECTOR v;

		v.vector[0] = (i & 0x01) ? x1 : x2;
		v.vector[1] = (i & 0x02) ? y1 : y2;
		v.vector[2] = PIXEL_MANIPULATE_FIXED_1;

		if(FALSE == PixelManipulateTransformPoint(transform, &v))
		{
			return FALSE;
		}

		tx = (PIXEL_MANIPULATE_FIXED_48_16)v.vector[0];
		ty = (PIXEL_MANIPULATE_FIXED_48_16)v.vector[1];

		if(tx < tx1)
		{
			tx1 = tx;
		}
		if(ty < ty1)
		{
			ty1 = ty;
		}
		if(tx > tx2)
		{
			tx2 = tx;
		}
		if(ty > ty2)
		{
			ty2 = ty;
		}
	}

	transformed->x1 = tx1;
	transformed->y1 = ty1;
	transformed->x2 = tx2;
	transformed->y2 = ty2;

	return TRUE;
}

#define IS_16BIT(x) (((x) >= SHRT_MIN) && ((x) <= SHRT_MAX))
#define ABS(f) (((f) < 0) ? (-(f)) : (f))
#define IS_16_16(f) (((f) >= PIXEL_MANIPULATE_FIXED_48_16MIN && ((f) <= PIXEL_MANIPULATE_FIXED_48_16MAX)))

static int AnalyzeExtent(PIXEL_MANIPULATE_IMAGE* image, const PIXEL_MANIPULATE_BOX32* extents, uint32* flags)
{
	PIXEL_MANIPULATE_TRANSFORM *transform;
	PIXEL_MANIPULATE_FIXED x_offset, y_offset;
	PIXEL_MANIPULATE_FIXED width, height;
	PIXEL_MANIPULATE_FIXED *parameters;
	BOX_48_16 transformed;
	PIXEL_MANIPULATE_BOX32 exp_extents;

	if(image == NULL)
	{
		return TRUE;
	}

	if(FALSE == IS_16BIT(extents->x1 - 1)
		|| FALSE == IS_16BIT(extents->y1 - 1)
		|| FALSE == IS_16BIT(extents->x2 + 1)
		|| FALSE == IS_16BIT(extents->y2 + 1)
	)
	{
		return FALSE;
	}

	transform = image->common.transform;
	if(image->common.type == PIXEL_MANIPULATE_IMAGE_TYPE_BITS)
	{
		if(image->bits.width >= 0x7FFF || image->bits.height >= 0x7FFF)
		{
			return FALSE;
		}

		if((image->common.flags & PIXEL_MANIPULATE_FAST_PATH_ID_TRANSFORM) == PIXEL_MANIPULATE_FAST_PATH_ID_TRANSFORM
			&& extents->x1 >= 0
			&& extents->y1 >= 0
			&& extents->x2 <= image->bits.width
			&& extents->y2 <= image->bits.height
		)
		{
			*flags |= PIXEL_MANIPULATE_FAST_PATH_SAMPLES_COVER_CLIP_NEAREST;
			return TRUE;
		}

		switch(image->common.filter)
		{
		case PIXEL_MANIPULATE_FILTER_TYPE_CONVOLUTION:
			parameters = image->common.filter_parameters;
			x_offset = - PIXEL_MANIPULATE_FIXED_E - ((parameters[0] - PIXEL_MANIPULATE_FIXED_1) >> 1);
			y_offset = - PIXEL_MANIPULATE_FIXED_E - ((parameters[1] - PIXEL_MANIPULATE_FIXED_1) >> 1);
			width = parameters[0];
			height = parameters[1];
			break;
		case PIXEL_MANIPULATE_FILTER_TYPE_SEPARABLE_CONVOLUTION:
			parameters = image->common.filter_parameters;
			x_offset = - PIXEL_MANIPULATE_FIXED_E - ((parameters[0] - PIXEL_MANIPULATE_FIXED_1) >> 1);
			y_offset = - PIXEL_MANIPULATE_FIXED_E - ((parameters[1] - PIXEL_MANIPULATE_FIXED_1) >> 1);
			width = parameters[0];
			height = parameters[1];
			break;
		case PIXEL_MANIPULATE_DITHER_BEST:
		case PIXEL_MANIPULATE_FILTER_TYPE_BEST:
		case PIXEL_MANIPULATE_FILTER_TYPE_BILINEAR:
			x_offset = - PIXEL_MANIPULATE_FIXED_1 / 2;
			y_offset = - PIXEL_MANIPULATE_FIXED_1 / 2;
			width = - PIXEL_MANIPULATE_FIXED_1 / 2;
			height = - PIXEL_MANIPULATE_FIXED_1 / 2;
			width = PIXEL_MANIPULATE_FIXED_1;
			height = PIXEL_MANIPULATE_FIXED_1;
			break;

		default:
			return FALSE;
		}
	}
	else
	{
		x_offset = 0;
		y_offset = 0;
		width = 0;
		height = 0;
	}

	if(FALSE == ComputeTransformedExtents(transform, extents, &transformed))
	{
		return FALSE;
	}

	if(image->common.type == PIXEL_MANIPULATE_IMAGE_TYPE_BITS)
	{
		if(PIXEL_MANIPULATE_FIXED_TO_INTEGER(transformed.x1 - PIXEL_MANIPULATE_FIXED_E) >= 0
			&& PIXEL_MANIPULATE_FIXED_TO_INTEGER(transformed.y1 - PIXEL_MANIPULATE_FIXED_E) >= 0
			&& PIXEL_MANIPULATE_FIXED_TO_INTEGER(transformed.x2 - PIXEL_MANIPULATE_FIXED_E) < image->bits.width
			&& PIXEL_MANIPULATE_FIXED_TO_INTEGER(transformed.y2 - PIXEL_MANIPULATE_FIXED_E) < image->bits.height
		)
		{
			*flags |= PIXEL_MANIPULATE_FAST_PATH_SAMPLES_COVER_CLIP_NEAREST;
		}

		if(PIXEL_MANIPULATE_FIXED_TO_INTEGER(transformed.x1 - PIXEL_MANIPULATE_FIXED_E / 2) >= 0
			&& PIXEL_MANIPULATE_FIXED_TO_INTEGER(transformed.y1 - PIXEL_MANIPULATE_FIXED_E / 2) >= 0
			&& PIXEL_MANIPULATE_FIXED_TO_INTEGER(transformed.x2 - PIXEL_MANIPULATE_FIXED_E / 2) < image->bits.width
			&& PIXEL_MANIPULATE_FIXED_TO_INTEGER(transformed.y2 - PIXEL_MANIPULATE_FIXED_E / 2) < image->bits.height
		)
		{
			*flags |= PIXEL_MANIPULATE_FAST_PATH_SAMPLES_COVER_CLIP_BILINEAR;
		}
	}

	exp_extents = *extents;
	exp_extents.x1 -= 1;
	exp_extents.y1 -= 1;
	exp_extents.x2 += 1;
	exp_extents.y2 += 1;

	if(FALSE == ComputeTransformedExtents(transform, &exp_extents, &transformed))
	{
		return FALSE;
	}

	if(FALSE == IS_16_16(transformed.x1 + x_offset - 8 * PIXEL_MANIPULATE_FIXED_E)
		|| FALSE == IS_16_16(transformed.y1 + y_offset - 8 * PIXEL_MANIPULATE_FIXED_E)
		|| FALSE == IS_16_16(transformed.x2 + x_offset + 8 * PIXEL_MANIPULATE_FIXED_E + width)
		|| FALSE == IS_16_16(transformed.y2 + y_offset + 8 * PIXEL_MANIPULATE_FIXED_E + height)
	)
	{
		return FALSE;
	}

	return TRUE;
}

void PixelManipulateComposite32(
	ePIXEL_MANIPULATE_OPERATE op,
	union _PIXEL_MANIPULATE_IMAGE* src,
	union _PIXEL_MANIPULATE_IMAGE* mask,
	union _PIXEL_MANIPULATE_IMAGE* dest,
	int32 src_x,
	int32 src_y,
	int32 mask_x,
	int32 mask_y,
	int32 dest_x,
	int32 dest_y,
	int32 width,
	int32 height
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

	if(mask != NULL && 0 == (mask->common.flags & PIXEL_MANIPULATE_FAST_PATH_IS_OPAQUE))
	{
		mask_format = mask->common.extended_format_code;
		info.mask_flags = mask->common.flags;
	}
	else
	{
		mask_format = PIXEL_MANIPULATE_FORMAT_NULL;
		info.mask_flags = PIXEL_MANIPULATE_FAST_PATH_IS_OPAQUE | PIXEL_MANIPULATE_FAST_PATH_NO_ALPHA_MAP;
	}

	dest_format = dest->common.extended_format_code;
	info.dest_flags = dest->common.flags;

	if((mask_format == PIXEL_MANIPULATE_FORMAT_A8R8G8B8 || mask_format == PIXEL_MANIPULATE_FORMAT_A8B8G8R8)
			&& (src->type == PIXEL_MANIPULATE_IMAGE_TYPE_BITS && src->bits.bits == mask->bits.bits)
			&& (src->common.repeat == mask->common.repeat)
			&& (info.src_flags & info.mask_flags & PIXEL_MANIPULATE_FAST_PATH_ID_TRANSFORM)
			&& (src_x == mask_x && src_y == mask_y)
	)
	{
		if(src_format == PIXEL_MANIPULATE_FORMAT_X8B8G8R8)
		{
			src_format = mask_format = PIXEL_MANIPULATE_FORMAT_BUFFER;
		}
		else if(src_format == PIXEL_MANIPULATE_FORMAT_X8R8G8B8)
		{
			src_format = mask_format = PIXEL_MANIPULATE_FORMAT_R_BUFFER;
		}
	}

	InitializePixelManipulateRegion32(&region);

	if(FALSE == PixelManipulateComputeCompositeRegion32(&region,
		src, mask, dest, src_x, src_y, mask_x, mask_y, dest_x, dest_y, width, height))
	{
		goto out;
	}

	extents = region.extents;

	extents.x1 -= src_x - mask_x;
	extents.y1 -= src_y - mask_y;
	extents.x2 -= src_x - mask_x;
	extents.y2 -= src_y - mask_y;

	if(FALSE == AnalyzeExtent(mask, &extents, &info.mask_flags))
	{
		goto out;
	}

#define NEAREST_OPAQUE (PIXEL_MANIPULATE_FAST_PATH_SAMPLES_OPAQUE \
	| PIXEL_MANIPULATE_FAST_PATH_NEAREST_FILTER \
	| PIXEL_MANIPULATE_FAST_PATH_SAMPLES_COVER_CLIP_NEAREST)
#define BILINEAR_OPAQUE (PIXEL_MANIPULATE_FAST_PATH_SAMPLES_OPAQUE \
	| PIXEL_MANIPULATE_FAST_PATH_BILINEAR_FILTER \
	| PIXEL_MANIPULATE_FAST_PATH_SAMPLES_COVER_CLIP_BILINEAR)

	if((info.src_flags & NEAREST_OPAQUE) == NEAREST_OPAQUE
		|| (info.src_x & BILINEAR_OPAQUE) == BILINEAR_OPAQUE)
	{
		info.src_flags |= PIXEL_MANIPULATE_FAST_PATH_IS_OPAQUE;
	}

	if((info.mask_flags & NEAREST_OPAQUE) == NEAREST_OPAQUE
		|| (info.mask_flags & BILINEAR_OPAQUE) == BILINEAR_OPAQUE)
	{
		info.mask_flags |= PIXEL_MANIPULATE_FAST_PATH_IS_OPAQUE;
	}

	info.op = OptimizeOperator(op, info.src_flags, info.mask_flags, info.dest_flags);

	PixelManipulateImplementationLookupComposite(
		GetImplementation(), info.op,
				src_format, info.src_flags, mask_format, info.mask_flags,
				dest_format, info.dest_flags, &imp, &function
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

out:
	PixelManipulateRegion32Finish(&region);
}

int PixelManipulateComputeCompositeRegion32(
	PIXEL_MANIPULATE_REGION32* region,
	PIXEL_MANIPULATE_IMAGE* src_image,
	PIXEL_MANIPULATE_IMAGE* mask_image,
	PIXEL_MANIPULATE_IMAGE* dest_image,
	int32 src_x,
	int32 src_y,
	int32 mask_x,
	int32 mask_y,
	int32 dest_x,
	int32 dest_y,
	int32 width,
	int32 height
)
{
	region->extents.x1 = dest_x;
	region->extents.x2 = dest_x + width;
	region->extents.y1 = dest_y;
	region->extents.y2 = dest_y + height;

	region->extents.x1 = MAXIMUM(region->extents.x1, 0);
	region->extents.y1 = MAXIMUM(region->extents.y1, 0);
	region->extents.x2 = MINIMUM(region->extents.x2, dest_image->bits.width);
	region->extents.y2 = MINIMUM(region->extents.y2, dest_image->bits.height);

	region->data = NULL;

	if(region->extents.x1 >= region->extents.x2
		|| region->extents.y1 >= region->extents.y2)
	{
		region->extents.x1 = region->extents.y1 = 0;
		region->extents.x2 = region->extents.y2 = 0;
		return FALSE;
	}

	if(dest_image->common.have_clip_region)
	{
		if(FALSE == ClipGeneralImage(region, &dest_image->common.clip_region, 0, 0))
		{
			return FALSE;
		}
	}

	if(dest_image->common.alpha_map != NULL)
	{
		if(FALSE == PixelManipulateRegion32IntersectRectangle(region, region,
			dest_image->common.alpha_origin_x, dest_image->common.alpha_origin_y, dest_image->common.alpha_map->width, dest_image->common.alpha_map->height))
		{
			return FALSE;
		}
	}
	if(FALSE == PixelManipulateRegion32NotEmpty(region))
	{
		return FALSE;
	}
	if(dest_image->common.alpha_map != NULL && dest_image->common.alpha_map->common.have_clip_region)
	{
		if(FALSE == ClipGeneralImage(region, &dest_image->common.alpha_map->common.clip_region,
			-dest_image->common.alpha_origin_x, -dest_image->common.alpha_origin_y))
		{
			return FALSE;
		}
	}

	if(src_image->common.have_clip_region)
	{
		if(FALSE == ClipSourceImage(region, src_image, dest_x - src_x, dest_y - src_y))
		{
			return FALSE;
		}
	}
	if(src_image->common.alpha_map && src_image->common.alpha_map->common.have_clip_region)
	{
		if(FALSE == ClipSourceImage(region, (PIXEL_MANIPULATE_IMAGE*)src_image->common.alpha_map,
			dest_x - (src_x - src_image->common.alpha_origin_x), dest_y - (src_y - src_image->common.alpha_origin_y)))
		{
			return FALSE;
		}
	}

	if(mask_image != NULL && mask_image->common.have_clip_region)
	{
		if(FALSE == ClipSourceImage(region, (PIXEL_MANIPULATE_IMAGE*)mask_image->common.alpha_map,
			dest_x - (mask_x - mask_image->common.alpha_origin_x), dest_y - (mask_y - mask_image->common.alpha_origin_y)))
		{
			return FALSE;
		}
	}

	return TRUE;
}

uint32 PixelManipulateImageGetSolid(
	struct _PIXEL_MANIPULATE_IMPLEMENTATION* implementation,
	union _PIXEL_MANIPULATE_IMAGE* image,
	ePIXEL_MANIPULATE_FORMAT format
)
{
	uint32 result;

	if(image->type == PIXEL_MANIPULATE_IMAGE_TYPE_SOLID)
	{
		result = image->solid.color_32;
	}
	else if(image->type == PIXEL_MANIPULATE_IMAGE_TYPE_BITS)
	{
		if(image->bits.format == PIXEL_MANIPULATE_FORMAT_A8R8G8B8)
		{
			result = image->bits.bits[0];
		}
		else if(image->bits.format == PIXEL_MANIPULATE_FORMAT_X8R8G8B8)
		{
			result = image->bits.bits[0] | 0xFF000000;
		}
		else if(image->bits.format == PIXEL_MANIPULATE_FORMAT_A8)
		{
			result = (uint32)(*(uint8*)image->bits.bits) << 24;
		}
		else
		{
			goto otherwise;
		}
	}
	else
	{
		PIXEL_MANIPULATE_ITERATION iter;
otherwise:
		PixelManipulateImplementationIterationInitialize(implementation,
			&iter, image, 0, 0, 1, 1, (uint8*)&result, PIXEL_MANIPULATE_ITERATION_NARROW | PIXEL_MANIPULATE_ITERATION_SRC, image->common.flags);

		result = *iter.get_scanline(&iter, NULL);

		if(iter.finish != NULL)
		{
			iter.finish(&iter);
		}
	}

	if(PIXEL_MANIPULATE_FORMAT_TYPE(format) != PIXEL_MANIPULATE_TYPE_ARGB)
	{
		result = ((result & 0xFF000000) >> 0)
				| ((result & 0x00FF0000) >> 16)
				| ((result & 0x0000FF00) >> 0)
				| ((result & 0x000000FF) >> 16);
	}

	return result;
}

#ifdef __cplusplus
}
#endif
