#include <string.h>
#include "pixel_manipulate.h"
#include "pixel_manipulate_region.h"
#include "pixel_manipulate_private.h"
#include "pixel_manipulate_region.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

static INLINE void ImagePropertyChanged(PIXEL_MANIPULATE_IMAGE* image)
{
	image->common.dirty = TRUE;
}

static void InitializePixelManipulateImageInternal(PIXEL_MANIPULATE_IMAGE* image)
{
	PIXEL_MANIPULATE_IMAGE_COMMON *common = &image->common;

	InitializePixelManipulateRegion32(&common->clip_region);

	common->alpha_count = 0;
	common->have_clip_region = FALSE;
	common->clip_sources = FALSE;
	common->transform = NULL;
	common->repeat = PIXEL_MANIPULATE_REPEAT_TYPE_NONE;
	common->filter = PIXEL_MANIPULATE_FILTER_TYPE_NEAREST;
	common->filter_parameters = NULL;
	common->num_filter_parameters = 0;
	common->alpha_map = FALSE;
	common->component_alpha = FALSE;
	common->reference_count = 1;
	common->property_changed_function = NULL;
	common->delete_function = NULL;
	common->delete_data = NULL;
	common->dirty = TRUE;

	PixelManipulateSetupAccessors(image);
}

int PixelManipulateSetClipRegion32(PIXEL_MANIPULATE_IMAGE* image, PIXEL_MANIPULATE_REGION32* region)
{
	PIXEL_MANIPULATE_IMAGE_COMMON* common = (PIXEL_MANIPULATE_IMAGE_COMMON*)image;
	int result = FALSE;

	if(region != NULL)
	{
		if((result = PixelManipulateRegion32Copy(&common->clip_region, region)))
		{
			image->common.have_clip_region = TRUE;
		}
	}
	else
	{
		PixelManipulateResetClipRegion(image);

		result = TRUE;
	}

	ImagePropertyChanged(image);

	return result;
}

int PixelManipulateImageSetTransform(
	PIXEL_MANIPULATE_IMAGE* image,
	const PIXEL_MANIPULATE_TRANSFORM* transform
)
	{
		static const PIXEL_MANIPULATE_TRANSFORM id =
		{
			{ { PIXEL_MANIPULATE_FIXED_1, 0, 0 },
		{ 0, PIXEL_MANIPULATE_FIXED_1, 0 },
		{ 0, 0, PIXEL_MANIPULATE_FIXED_1 } }
	};

	PIXEL_MANIPULATE_IMAGE_COMMON* common = (PIXEL_MANIPULATE_IMAGE_COMMON*)image;
	int result;

	if(common->transform == transform)
		return TRUE;

	if(!transform || memcmp(&id, transform, sizeof(PIXEL_MANIPULATE_TRANSFORM)) == 0)
	{
		free (common->transform);
		common->transform = NULL;
		result = TRUE;

		goto out;
	}

	if(common->transform &&
		memcmp(common->transform, transform, sizeof(PIXEL_MANIPULATE_TRANSFORM)) == 0)
	{
		return TRUE;
	}

	if(common->transform == NULL)
		common->transform = malloc (sizeof(PIXEL_MANIPULATE_TRANSFORM));

	if(common->transform == NULL)
	{
		result = FALSE;

		goto out;
	}

	(void)memcpy(common->transform, transform, sizeof(PIXEL_MANIPULATE_TRANSFORM));

	result = TRUE;
	
out:
	ImagePropertyChanged(image);

	return result;
}

void PixelManipulateImageSetRepeat(
	PIXEL_MANIPULATE_IMAGE* image,
	ePIXEL_MANIPULATE_REPEAT_TYPE repeat
)
{
	if(image->common.repeat == repeat)
		return;

	image->common.repeat = repeat;

	ImagePropertyChanged(image);
}

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
	stride >>= 5;
	stride *= sizeof(uint32);

	buffer_size = (size_t)height * stride;

	*row_stride_bytes = stride;

	if(clear)
	{
		return (uint32*)MEM_CALLOC_FUNC(buffer_size, 1);
	}
	return (uint32*)MEM_ALLOC_FUNC(buffer_size);
}

int InitializePixelManipulateBitsImage(
	PIXEL_MANIPULATE_IMAGE* image,
	ePIXEL_MANIPULATE_FORMAT format,
	int width,
	int height,
	uint32* bits,
	int row_stride,
	int clear
)
{
	uint32* free_memory = NULL;

	if(PIXEL_MANIPULATE_BIT_PER_PIXEL(format) == 128)
	{
		if(row_stride % 4 != 0)
		{
			return FALSE;
		}
	}

	if(bits == NULL && width != 0 && height != 0)
	{
		int row_stride_bytes;

		free_memory = bits = CreateBits(format, width, height, &row_stride_bytes, clear);
		if(bits == NULL)
		{
			return FALSE;
		}

		row_stride = row_stride_bytes / (int)sizeof(uint32);
	}

	InitializePixelManipulateImageInternal(image);

	image->type = PIXEL_MANIPULATE_IMAGE_TYPE_BITS;
	image->bits.format = format;
	image->bits.width = width;
	image->bits.height = height;
	image->bits.bits = bits;
	image->bits.to_free = free_memory;
	image->bits.dither = PIXEL_MANIPULATE_DITHER_NONE;
	image->bits.dither_offset_x = 0;
	image->bits.dither_offset_y = 0;
	image->bits.row_stride = row_stride / sizeof(uint32);
	image->bits.indexed = NULL;

	image->common.property_changed_function = PixelManipulateSetupAccessors;

	PixelManipulateResetClipRegion(image);

	return TRUE;
}

int PixelManipulateImageSetFilter(
	PIXEL_MANIPULATE_IMAGE* image,
	ePIXEL_MANIPULATE_FILTER_TYPE filter,
const PIXEL_MANIPULATE_FIXED* params,
int n_params
)
{
PIXEL_MANIPULATE_IMAGE_COMMON* common = (PIXEL_MANIPULATE_IMAGE_COMMON*)image;
PIXEL_MANIPULATE_FIXED* new_params;

if(params == common->filter_parameters && filter == common->filter)
{
	return TRUE;
}

if(filter == PIXEL_MANIPULATE_FILTER_TYPE_CONVOLUTION)
{
	int width = PIXEL_MANIPULATE_FIXED_TO_INTEGER(params[0]);
	int height = PIXEL_MANIPULATE_FIXED_TO_INTEGER(params[1]);
	int x_phase_bits = PIXEL_MANIPULATE_FIXED_TO_INTEGER(params[2]);
	int y_phase_bits = PIXEL_MANIPULATE_FIXED_TO_INTEGER(params[3]);
	int n_x_phases = (1 << x_phase_bits);
	int n_y_phases = (1 << y_phase_bits);

	if(n_params != 4 + n_x_phases * width + n_y_phases * height)
	{
		return FALSE;
	}
}

new_params = NULL;
if(params != NULL)
{
	new_params = MEM_ALLOC_FUNC(n_params * sizeof(PIXEL_MANIPULATE_FIXED));
	if(new_params == NULL)
	{
		return FALSE;
	}

	(void)memcpy(new_params, params, n_params * sizeof(PIXEL_MANIPULATE_FIXED));
}

common->filter = filter;

if(common->filter_parameters != NULL)
{
	MEM_FREE_FUNC(common->filter_parameters);
}

common->filter_parameters = new_params;
common->num_filter_parameters = n_params;

ImagePropertyChanged(image);
return TRUE;
}

void PixelManipulateImageSetComponentAlpha(PIXEL_MANIPULATE_IMAGE* image, int component_alpha)
{
	if(image->common.component_alpha == component_alpha)
	{
		return;
	}

	image->common.component_alpha = component_alpha;
	ImagePropertyChanged(image);
}

void PixelManipulateSetDestroyFunction(
	PIXEL_MANIPULATE_IMAGE* image,
	void (*destroy_function)(PIXEL_MANIPULATE_IMAGE*),
	void* data
)
{
	image->common.delete_function = destroy_function;
}

static void ComputeImageInfo(PIXEL_MANIPULATE_IMAGE* image)
{
	ePIXEL_MANIPULATE_FORMAT code;
	uint32 flags = 0;

	if(FALSE == image->common.transform)
	{
		flags |= (FAST_PATH_ID_TRANSFORM
			| FAST_PATH_X_UNIT_POSITIVE
			| FAST_PATH_Y_UNIT_ZERO
			| FAST_PATH_AFFINE_TRANSFORM);
	}
	else
	{
		flags |= FAST_PATH_HAS_TRANSFORM;

		if(image->common.transform->matrix[2][0] == 0
			&& image->common.transform->matrix[2][1] == 0
			&& image->common.transform->matrix[2][2] == PIXEL_MANIPULATE_FIXED_1)
		{
			flags |= FAST_PATH_AFFINE_TRANSFORM;

			if(image->common.transform->matrix[0][1] == 0
				&& image->common.transform->matrix[1][0] == 0)
			{
				if(image->common.transform->matrix[0][0] == -PIXEL_MANIPULATE_FIXED_1
					&& image->common.transform->matrix[1][1] == PIXEL_MANIPULATE_FIXED_1)
				{
					PIXEL_MANIPULATE_FIXED m01 = image->common.transform->matrix[0][1];
					PIXEL_MANIPULATE_FIXED m10 = image->common.transform->matrix[1][0];

					if(m01 == -PIXEL_MANIPULATE_FIXED_1)
					{
						flags |= FAST_PATH_ROTATE_90_TRANSFORM;
					}
					else if(m01 == PIXEL_MANIPULATE_FIXED_1 && m10 == -PIXEL_MANIPULATE_FIXED_1)
					{
						flags |= FAST_PATH_ROTATE_270_TRANSFORM;
					}
				}
			}

			if(image->common.transform->matrix[0][0] > 0)
			{
				flags |= FAST_PATH_X_UNIT_POSITIVE;
			}

			if(image->common.transform->matrix[1][0] == 0)
			{
				flags |= FAST_PATH_Y_UNIT_ZERO;
			}
		}
	}

	switch(image->common.filter)
	{
	case PIXEL_MANIPULATE_FILTER_TYPE_NEAREST:
	case PIXEL_MANIPULATE_FILTER_TYPE_FAST:
		flags |= (FAST_PATH_NEAREST_FILTER | FAST_PATH_NO_CONVOLUTION_FILTER);
		break;
	case PIXEL_MANIPULATE_FILTER_TYPE_GOOD:
	case PIXEL_MANIPULATE_FILTER_TYPE_BEST:
		flags |= (FAST_PATH_BILINEAR_FILTER | FAST_PATH_NO_CONVOLUTION_FILTER);

		if(flags & FAST_PATH_ID_TRANSFORM)
		{
			flags |= FAST_PATH_NEAREST_FILTER;
		}
		else if(flags & FAST_PATH_AFFINE_TRANSFORM)
		{
			PIXEL_MANIPULATE_FIXED (*t)[3] = image->common.transform->matrix;

			if((PIXEL_MANIPULATE_FIXED_FRAC(t[0][0] | t[0][1] | t[0][2]
				| t[1][0] | t[1][1] | t[1][2]) == 0)
				&& (PIXEL_MANIPULATE_FIXED_TO_INTEGER((t[0][0] + t[0][1]) & (t[1][0] + t[1][1])) % 2) == 1)
			{
				PIXEL_MANIPULATE_FIXED magic_limit = PIXEL_MANIPULATE_FIXED_TO_INTEGER(30000);
				if(image->common.transform->matrix[0][2] <= magic_limit
					&& image->common.transform->matrix[1][2] <= magic_limit
					&& image->common.transform->matrix[0][2] >= -magic_limit
					&& image->common.transform->matrix[1][2] >= -magic_limit)
				{
					flags |= FAST_PATH_NEAREST_FILTER;
				}
			}
		}
		break;

	case PIXEL_MANIPULATE_FILTER_TYPE_CONVOLUTION:
		break;

	case PIXEL_MANIPULATE_FILTER_TYPE_SEPARABLE_CONVOLUTION:
		flags |= FAST_PATH_SEPARABLE_CONVOLUTION_FILTER;
		break;

	default:
		flags |= FAST_PATH_NO_CONVOLUTION_FILTER;
		break;
	}

	switch(image->common.repeat)
	{
	case PIXEL_MANIPULATE_REPEAT_TYPE_NONE:
		flags |= FAST_PATH_NO_REFLECT_REPEAT
			| FAST_PATH_NO_PAD_REPEAT
			| FAST_PATH_NO_NORMAL_REPEAT;
		break;

	case PIXEL_MANIPULATE_REPEAT_TYPE_REFLECT:
		flags |= FAST_PATH_NO_PAD_REPEAT
			| FAST_PATH_NO_NONE_REPEAT
			| FAST_PATH_NO_NORMAL_REPEAT;
		break;

	case PIXEL_MANIPULATE_REPEAT_TYPE_PAD:
		flags |= FAST_PATH_NO_REFLECT_REPEAT
			| FAST_PATH_NO_NONE_REPEAT
			| FAST_PATH_NO_NORMAL_REPEAT;
		break;

	default:
		flags |= FAST_PATH_NO_REFLECT_REPEAT
			| FAST_PATH_NO_PAD_REPEAT
			| FAST_PATH_NO_NONE_REPEAT;
		break;
	}

	if(image->common.component_alpha)
	{
		flags |= FAST_PATH_COMPONENT_ALPHA;
	}
	else
	{
		flags |= FAST_PATH_UNIFIED_ALPHA;
	}

	flags |= (FAST_PATH_NO_ACCESSORS | FAST_PATH_NARROW_FORMAT);

	switch(image->type)
	{
	case PIXEL_MANIPULATE_IMAGE_TYPE_SOLID:
		code = PIXEL_MANIPULATE_SOLID;

		if(image->solid.color.alpha == 0xFFFF)
		{
			flags |= FAST_PATH_IS_OPAQUE;
		}
		break;

	case PIXEL_MANIPULATE_IMAGE_TYPE_BITS:
		if(image->bits.width == 1
			&& image->bits.height == 1
			&& image->common.repeat != PIXEL_MANIPULATE_REPEAT_TYPE_NONE)
		{
			code = PIXEL_MANIPULATE_SOLID;
		}
		else
		{
			code = image->bits.format;
			flags |= FAST_PATH_BITS_IMAGE;
		}

		if(FALSE == PIXEL_MANIPULATE_FORMAT_A(image->bits.format)
			&& PIXEL_MANIPULATE_FORMAT_TYPE(image->bits.format) != PIXEL_MANIPULATE_TYPE_GRAY
			&& PIXEL_MANIPULATE_FORMAT_TYPE(image->bits.format) != PIXEL_MANIPULATE_TYPE_COLOR)
		{
			flags |= FAST_PATH_SAMPLES_OPAQUE;

			if(image->common.repeat != PIXEL_MANIPULATE_REPEAT_TYPE_NONE)
			{
				flags |= FAST_PATH_IS_OPAQUE;
			}
		}

		// if(image->bits.read_func || image->bits.write_func)

		// if(PIXEL_MANIPULATE_FORMAT_IS_WIDE)

		break;
	case PIXEL_MANIPULATE_IMAGE_TYPE_RADIAL:
		code = PIXEL_MANIPULATE_FORMAT_UNKOWN;

		if(image->radial.a >= 0)
		{
			break;
	}

	case PIXEL_MANIPULATE_IMAGE_TYPE_CONICAL:
	case PIXEL_MANIPULATE_IMAGE_TYPE_LINEAR:
		code = PIXEL_MANIPULATE_FORMAT_UNKOWN;

		if(image->common.repeat != PIXEL_MANIPULATE_REPEAT_TYPE_NONE)
		{
			int i;

			flags |= FAST_PATH_IS_OPAQUE;
			for(i = 0; i < image->gradient.num_stops; ++i)
			{
				if(image->gradient.stops[i].color.alpha != 0xFFFF)
				{
					flags &= ~FAST_PATH_IS_OPAQUE;
					break;
				}
			}
		}
		break;
	default:
		code = PIXEL_MANIPULATE_FORMAT_UNKOWN;
		break;
	}

	if(NULL == image->common.alpha_map || image->type != PIXEL_MANIPULATE_IMAGE_TYPE_BITS)
	{
		flags |= FAST_PATH_NO_ALPHA_MAP;
	}
	else
	{
		// if(PIXEL_MANIPULATE_FORMAT_IS_WIDE(image->common.alpha_map->format))
	}

	if(image->common.alpha_map != NULL
		|| image->common.filter == PIXEL_MANIPULATE_FILTER_TYPE_CONVOLUTION
		|| image->common.filter == PIXEL_MANIPULATE_FILTER_TYPE_SEPARABLE_CONVOLUTION
		|| image->common.component_alpha != NULL)
	{
		flags &= ~(FAST_PATH_IS_OPAQUE | FAST_PATH_SAMPLES_OPAQUE);
	}

	image->common.flags = flags;
	image->common.extended_format_code = code;
}

void PixelManipulateImageValidate(PIXEL_MANIPULATE_IMAGE* image)
{
	if(image->common.dirty)
	{
		ComputeImageInfo(image);

		if(image->common.property_changed_function != NULL)
		{
			image->common.property_changed_function(&image->common);
		}

		image->common.dirty = FALSE;
	}

	if(image->common.alpha_map != NULL)
	{
		PixelManipulateImageValidate((PIXEL_MANIPULATE_IMAGE*)image->common.alpha_map);
	}
}

#ifdef __cplusplus
}
#endif
