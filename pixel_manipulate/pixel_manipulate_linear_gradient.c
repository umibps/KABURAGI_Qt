#include "pixel_manipulate.h"
#include "pixel_manipulate_region.h"
#include "pixel_manipulate_composite.h"
#include "pixel_manipulate_inline.h"
#include "pixel_manipulate_private.h"
#include "../memory.h"

#ifdef __cplusplus
extern "C" {
#endif

static int linear_gradient_is_horizontal(
	PIXEL_MANIPULATE_IMAGE* image,
	int x,
	int y,
	int width,
	int height
)
{
	PIXEL_MANIPULATE_LINEAR_GRADIENT *linear = (PIXEL_MANIPULATE_LINEAR_GRADIENT *)image;
	PIXEL_MANIPULATE_VECTOR v;
	PIXEL_MANIPULATE_FIXED_32_32 l;
	PIXEL_MANIPULATE_FIXED_48_16 dx, dy;
	double inc;
	
	if (image->common.transform)
	{
		/* projective transformation */
		if (image->common.transform->matrix[2][0] != 0 ||
			image->common.transform->matrix[2][1] != 0 ||
			image->common.transform->matrix[2][2] == 0)
		{
			return FALSE;
		}

		v.vector[0] = image->common.transform->matrix[0][1];
		v.vector[1] = image->common.transform->matrix[1][1];
		v.vector[2] = image->common.transform->matrix[2][2];
	}
	else
	{
		v.vector[0] = 0;
		v.vector[1] = PIXEL_MANIPULATE_FIXED_1;
		v.vector[2] = PIXEL_MANIPULATE_FIXED_1;
	}

	dx = linear->p2.x - linear->p1.x;
	dy = linear->p2.y - linear->p1.y;

	l = dx * dx + dy * dy;

	if (l == 0)
		return FALSE;

	/*
	* compute how much the input of the gradient walked changes
	* when moving vertically through the whole image
	*/
	inc = height * (double) PIXEL_MANIPULATE_FIXED_1 * PIXEL_MANIPULATE_FIXED_1 *
		(dx * v.vector[0] + dy * v.vector[1]) /
		(v.vector[2] * (double) l);

	/* check that casting to integer would result in 0 */
	if (-1 < inc && inc < 1)
		return TRUE;

	return FALSE;
}

static uint32* linear_get_scanline(
	PIXEL_MANIPULATE_ITERATION* iter,
	const uint32* mask,
	int bpp,
	PIXEL_MANIPULATE_GRADIENT_WALKER_WRITE write_pixel,
	PIXEL_MANIPULATE_GRADIENT_WALKER_FILL fill_pixel
)
{
	PIXEL_MANIPULATE_IMAGE *image  = iter->image;
	int			 x	  = iter->x;
	int			 y	  = iter->y;
	int			 width  = iter->width;
	uint32 *	  buffer = iter->buffer;

	PIXEL_MANIPULATE_VECTOR v, unit;
	PIXEL_MANIPULATE_FIXED_32_32 l;
	PIXEL_MANIPULATE_FIXED_48_16 dx, dy;
	PIXEL_MANIPULATE_GRADIENT *gradient = (PIXEL_MANIPULATE_GRADIENT *)image;
	PIXEL_MANIPULATE_LINEAR_GRADIENT *linear = (PIXEL_MANIPULATE_LINEAR_GRADIENT *)image;
	uint32 *end = buffer + width * (bpp / 4);
	PIXEL_MANIPULATE_GRADIENT_WALKER walker;
	
	InitializePixelManipulateGradient(&walker, gradient, image->common.repeat);
	
	/* reference point is the center of the pixel */
	v.vector[0] = PIXEL_MANIPULATE_INTEGER_TO_FIXED(x) + PIXEL_MANIPULATE_FIXED_1 / 2;
	v.vector[1] = PIXEL_MANIPULATE_INTEGER_TO_FIXED(y) + PIXEL_MANIPULATE_FIXED_1 / 2;
	v.vector[2] = PIXEL_MANIPULATE_FIXED_1;

	if (image->common.transform)
	{
		if (!PixelManipulateTransformPoint3d(image->common.transform, &v))
			return iter->buffer;

		unit.vector[0] = image->common.transform->matrix[0][0];
		unit.vector[1] = image->common.transform->matrix[1][0];
		unit.vector[2] = image->common.transform->matrix[2][0];
	}
	else
	{
		unit.vector[0] = PIXEL_MANIPULATE_FIXED_1;
		unit.vector[1] = 0;
		unit.vector[2] = 0;
	}

	dx = linear->p2.x - linear->p1.x;
	dy = linear->p2.y - linear->p1.y;

	l = dx * dx + dy * dy;

	if (l == 0 || unit.vector[2] == 0)
	{
		/* affine transformation only */
		PIXEL_MANIPULATE_FIXED_32_32 t, next_inc;
		double inc;

		if (l == 0 || v.vector[2] == 0)
		{
			t = 0;
			inc = 0;
		}
		else
		{
			double invden, v2;

			invden = PIXEL_MANIPULATE_FIXED_1 * (double) PIXEL_MANIPULATE_FIXED_1 /
				(l * (double) v.vector[2]);
			v2 = v.vector[2] * (1. / PIXEL_MANIPULATE_FIXED_1);
			t = ((dx * v.vector[0] + dy * v.vector[1]) -
				(dx * linear->p1.x + dy * linear->p1.y) * v2) * invden;
			inc = (dx * unit.vector[0] + dy * unit.vector[1]) * invden;
		}
		next_inc = 0;

		if (((PIXEL_MANIPULATE_FIXED_32_32 )(inc * width)) == 0)
		{
			fill_pixel (&walker, t, buffer, end);
		}
		else
		{
			int i;

			i = 0;
			while (buffer < end)
			{
				if (!mask || *mask++)
				{
					write_pixel (&walker, t + next_inc, buffer);
				}
				i++;
				next_inc = inc * i;
				buffer += (bpp / 4);
			}
		}
	}
	else
	{
		/* projective transformation */
		double t;

		t = 0;

		while (buffer < end)
		{
			if (!mask || *mask++)
			{
				if (v.vector[2] != 0)
				{
					double invden, v2;

					invden = PIXEL_MANIPULATE_FIXED_1 * (double) PIXEL_MANIPULATE_FIXED_1 /
						(l * (double) v.vector[2]);
					v2 = v.vector[2] * (1. / PIXEL_MANIPULATE_FIXED_1);
					t = ((dx * v.vector[0] + dy * v.vector[1]) -
						(dx * linear->p1.x + dy * linear->p1.y) * v2) * invden;
				}

				write_pixel (&walker, t, buffer);
			}

			buffer += (bpp / 4);

			v.vector[0] += unit.vector[0];
			v.vector[1] += unit.vector[1];
			v.vector[2] += unit.vector[2];
		}
	}

	iter->y++;

	return iter->buffer;
}

static uint32* linear_get_scanline_narrow(PIXEL_MANIPULATE_ITERATION  *iter, const uint32 *mask)
{
	return linear_get_scanline (iter, mask, 4,
		PixelManipulateGradientWalkerWriteNarrow,
		PixelManipulateGradientWalkerFillNarrow);
}


static uint32 *
linear_get_scanline_wide (PIXEL_MANIPULATE_ITERATION *iter, const uint32 *mask)
{
	return linear_get_scanline (iter, NULL, 16,
		PixelManipulateGradientWalkerWriteWide,
		PixelManipulateGradientWalkerFillWide);
}

void IniltializePixelManipulateGradientLinearGradientIteration(PIXEL_MANIPULATE_IMAGE *image, PIXEL_MANIPULATE_ITERATION  *iter)
{
	if (linear_gradient_is_horizontal (
		iter->image, iter->x, iter->y, iter->width, iter->height))
	{
		if (iter->iterate_flags & PIXEL_MANIPULATE_ITERATION_NARROW)
			linear_get_scanline_narrow (iter, NULL);
		else
			linear_get_scanline_wide (iter, NULL);

		iter->get_scanline = PixelManipulateIteratorGetScanlineNoop;
	}
	else
	{
		if (iter->iterate_flags & PIXEL_MANIPULATE_ITERATION_NARROW)
			iter->get_scanline = linear_get_scanline_narrow;
		else
			iter->get_scanline = linear_get_scanline_wide;
	}
}

void InitializePixelManipulateLinearGradient(
	PIXEL_MANIPULATE_IMAGE* image,
	const PIXEL_MANIPULATE_POINT_FIXED* p1,
	const PIXEL_MANIPULATE_POINT_FIXED* p2,
	const PIXEL_MANIPULATE_GRADIENT_STOP* stops,
	int n_stops
)
{
	PIXEL_MANIPULATE_LINEAR_GRADIENT* linear;

	InitializePixelManipulateImage(image);

	linear = &image->linear;

	if (!InitializePixelManipulateGradient(&linear->common, stops, n_stops))
	{
		return;
	}

	linear->p1 = *p1;
	linear->p2 = *p2;

	image->type = PIXEL_MANIPULATE_IMAGE_TYPE_LINEAR;
}

PIXEL_MANIPULATE_IMAGE* PixelManipulateCreateLinearGradient(
	const PIXEL_MANIPULATE_POINT_FIXED*  p1,
	const PIXEL_MANIPULATE_POINT_FIXED*  p2,
	const PIXEL_MANIPULATE_GRADIENT_STOP* stops,
	int n_stops
)
{
	PIXEL_MANIPULATE_IMAGE *image;
	PIXEL_MANIPULATE_LINEAR_GRADIENT *linear;

	image = PixelManipulateAllocate();

	if (!image)
		return NULL;

	linear = &image->linear;

	if (!InitializePixelManipulateGradient(&linear->common, stops, n_stops))
	{
		free (image);
		return NULL;
	}

	linear->p1 = *p1;
	linear->p2 = *p2;

	image->type = PIXEL_MANIPULATE_IMAGE_TYPE_LINEAR;

	return image;
}


#ifdef __cplusplus
}
#endif
