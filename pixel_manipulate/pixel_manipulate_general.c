#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pixel_manipulate_composite.h"
#include "pixel_manipulate.h"
#include "pixel_manipulate_private.h"
#include "pixel_manipulate_inline.h"
#include "pixel_manipulate_gradient.h"
#include "dither/blue-noise-64x64.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

static FORCE_INLINE void
fetch_pixel_no_alpha_32 (PIXEL_MANIPULATE_BITS_IMAGE *image,
			 int x, int y, int check_bounds,
			 void *out)
{
	uint32 *ret = out;

	if (check_bounds &&
	(x < 0 || x >= image->width || y < 0 || y >= image->height))
	*ret = 0;
	else
	*ret = image->fetch_pixel_32 (image, x, y);
}

static FORCE_INLINE void
fetch_pixel_no_alpha_float (PIXEL_MANIPULATE_BITS_IMAGE *image,
				int x, int y, int check_bounds,
				void *out)
{
	PIXEL_MANIPULATE_ARGB *ret = out;

	if (check_bounds &&
	(x < 0 || x >= image->width || y < 0 || y >= image->height))
	ret->a = ret->r = ret->g = ret->b = 0.f;
	else
	*ret = image->fetch_pixel_float (image, x, y);
}

typedef void (* get_pixel_t) (PIXEL_MANIPULATE_BITS_IMAGE *image,
				  int x, int y, int check_bounds, void *out);

static FORCE_INLINE void
bits_image_fetch_pixel_nearest (PIXEL_MANIPULATE_BITS_IMAGE   *image,
				PIXEL_MANIPULATE_FIXED  x,
				PIXEL_MANIPULATE_FIXED  y,
				get_pixel_t	get_pixel,
				void		   *out)
{
	int x0 = PIXEL_MANIPULATE_FIXED_TO_INTEGER (x - PIXEL_MANIPULATE_FIXED_E);
	int y0 = PIXEL_MANIPULATE_FIXED_TO_INTEGER (y - PIXEL_MANIPULATE_FIXED_E);

	if (image->common.repeat != PIXEL_MANIPULATE_REPEAT_TYPE_NONE)
	{
	repeat (image->common.repeat, &x0, image->width);
	repeat (image->common.repeat, &y0, image->height);

	get_pixel (image, x0, y0, FALSE, out);
	}
	else
	{
	get_pixel (image, x0, y0, TRUE, out);
	}
}

static FORCE_INLINE void
bits_image_fetch_pixel_bilinear_32 (PIXEL_MANIPULATE_BITS_IMAGE   *image,
					PIXEL_MANIPULATE_FIXED  x,
					PIXEL_MANIPULATE_FIXED  y,
					get_pixel_t		get_pixel,
					void	   *out)
{
	ePIXEL_MANIPULATE_REPEAT_TYPE repeat_mode = image->common.repeat;
	int width = image->width;
	int height = image->height;
	int x1, y1, x2, y2;
	uint32 tl, tr, bl, br;
	int32 distx, disty;
	uint32 *ret = out;

	x1 = x - PIXEL_MANIPULATE_FIXED_1 / 2;
	y1 = y - PIXEL_MANIPULATE_FIXED_1 / 2;

	distx = PIXEL_MANIPULATE_FIXED_to_bilinear_weight (x1);
	disty = PIXEL_MANIPULATE_FIXED_to_bilinear_weight (y1);

	x1 = PIXEL_MANIPULATE_FIXED_TO_INTEGER (x1);
	y1 = PIXEL_MANIPULATE_FIXED_TO_INTEGER (y1);
	x2 = x1 + 1;
	y2 = y1 + 1;

	if (repeat_mode != PIXEL_MANIPULATE_REPEAT_TYPE_NONE)
	{
	repeat (repeat_mode, &x1, width);
	repeat (repeat_mode, &y1, height);
	repeat (repeat_mode, &x2, width);
	repeat (repeat_mode, &y2, height);

	get_pixel (image, x1, y1, FALSE, &tl);
	get_pixel (image, x2, y1, FALSE, &tr);
	get_pixel (image, x1, y2, FALSE, &bl);
	get_pixel (image, x2, y2, FALSE, &br);
	}
	else
	{
	get_pixel (image, x1, y1, TRUE, &tl);
	get_pixel (image, x2, y1, TRUE, &tr);
	get_pixel (image, x1, y2, TRUE, &bl);
	get_pixel (image, x2, y2, TRUE, &br);
	}

	*ret = bilinear_interpolation (tl, tr, bl, br, distx, disty);
}

static FORCE_INLINE void
bits_image_fetch_pixel_bilinear_float (PIXEL_MANIPULATE_BITS_IMAGE   *image,
					   PIXEL_MANIPULATE_FIXED  x,
					   PIXEL_MANIPULATE_FIXED  y,
					   get_pixel_t	 get_pixel,
					   void		  *out)
{
	ePIXEL_MANIPULATE_REPEAT_TYPE repeat_mode = image->common.repeat;
	int width = image->width;
	int height = image->height;
	int x1, y1, x2, y2;
	PIXEL_MANIPULATE_ARGB tl, tr, bl, br;
	float distx, disty;
	PIXEL_MANIPULATE_ARGB *ret = out;

	x1 = x - PIXEL_MANIPULATE_FIXED_1 / 2;
	y1 = y - PIXEL_MANIPULATE_FIXED_1 / 2;

	distx = ((float)PIXEL_MANIPULATE_FIXED_FRAC(x1)) / 65536.f;
	disty = ((float)PIXEL_MANIPULATE_FIXED_FRAC(y1)) / 65536.f;

	x1 = PIXEL_MANIPULATE_FIXED_TO_INTEGER (x1);
	y1 = PIXEL_MANIPULATE_FIXED_TO_INTEGER (y1);
	x2 = x1 + 1;
	y2 = y1 + 1;

	if (repeat_mode != PIXEL_MANIPULATE_REPEAT_TYPE_NONE)
	{
	repeat (repeat_mode, &x1, width);
	repeat (repeat_mode, &y1, height);
	repeat (repeat_mode, &x2, width);
	repeat (repeat_mode, &y2, height);

	get_pixel (image, x1, y1, FALSE, &tl);
	get_pixel (image, x2, y1, FALSE, &tr);
	get_pixel (image, x1, y2, FALSE, &bl);
	get_pixel (image, x2, y2, FALSE, &br);
	}
	else
	{
	get_pixel (image, x1, y1, TRUE, &tl);
	get_pixel (image, x2, y1, TRUE, &tr);
	get_pixel (image, x1, y2, TRUE, &bl);
	get_pixel (image, x2, y2, TRUE, &br);
	}

	*ret = bilinear_interpolation_float (tl, tr, bl, br, distx, disty);
}

static FORCE_INLINE void accum_32(unsigned int *satot, unsigned int *srtot,
				  unsigned int *sgtot, unsigned int *sbtot,
				  const void *p, PIXEL_MANIPULATE_FIXED f)
{
	uint32 pixel = *(uint32 *)p;

	*srtot += (int)RED_8 (pixel) * f;
	*sgtot += (int)GREEN_8 (pixel) * f;
	*sbtot += (int)BLUE_8 (pixel) * f;
	*satot += (int)ALPHA_8 (pixel) * f;
}

static FORCE_INLINE void reduce_32(unsigned int satot, unsigned int srtot,
				   unsigned int sgtot, unsigned int sbtot,
								   void *p)
{
	uint32 *ret = p;

	satot = (satot + 0x8000) >> 16;
	srtot = (srtot + 0x8000) >> 16;
	sgtot = (sgtot + 0x8000) >> 16;
	sbtot = (sbtot + 0x8000) >> 16;

	satot = CLIP (satot, 0, 0xff);
	srtot = CLIP (srtot, 0, 0xff);
	sgtot = CLIP (sgtot, 0, 0xff);
	sbtot = CLIP (sbtot, 0, 0xff);

	*ret = ((satot << 24) | (srtot << 16) | (sgtot <<  8) | (sbtot));
}

static FORCE_INLINE void accum_float(unsigned int *satot, unsigned int *srtot,
					 unsigned int *sgtot, unsigned int *sbtot,
					 const void *p, PIXEL_MANIPULATE_FIXED f)
{
	const PIXEL_MANIPULATE_ARGB *pixel = p;

	*satot += pixel->a * f;
	*srtot += pixel->r * f;
	*sgtot += pixel->g * f;
	*sbtot += pixel->b * f;
}

static FORCE_INLINE void reduce_float(unsigned int satot, unsigned int srtot,
					  unsigned int sgtot, unsigned int sbtot,
					  void *p)
{
	PIXEL_MANIPULATE_ARGB *ret = p;

	ret->a = CLIP (satot / 65536.f, 0.f, 1.f);
	ret->r = CLIP (srtot / 65536.f, 0.f, 1.f);
	ret->g = CLIP (sgtot / 65536.f, 0.f, 1.f);
	ret->b = CLIP (sbtot / 65536.f, 0.f, 1.f);
}

typedef void (* accumulate_pixel_t) (unsigned int *satot, unsigned int *srtot,
					 unsigned int *sgtot, unsigned int *sbtot,
					 const void *pixel, PIXEL_MANIPULATE_FIXED f);

typedef void (* reduce_pixel_t) (unsigned int satot, unsigned int srtot,
				 unsigned int sgtot, unsigned int sbtot,
								 void *out);

static FORCE_INLINE void
bits_image_fetch_pixel_convolution (PIXEL_MANIPULATE_BITS_IMAGE   *image,
					PIXEL_MANIPULATE_FIXED  x,
					PIXEL_MANIPULATE_FIXED  y,
					get_pixel_t	 get_pixel,
					void		  *out,
					accumulate_pixel_t accum,
					reduce_pixel_t reduce)
{
	PIXEL_MANIPULATE_FIXED *params = image->common.filter_parameters;
	int x_off = (params[0] - PIXEL_MANIPULATE_FIXED_1) >> 1;
	int y_off = (params[1] - PIXEL_MANIPULATE_FIXED_1) >> 1;
	int32 cwidth = PIXEL_MANIPULATE_FIXED_TO_INTEGER (params[0]);
	int32 cheight = PIXEL_MANIPULATE_FIXED_TO_INTEGER (params[1]);
	int32 i, j, x1, x2, y1, y2;
	ePIXEL_MANIPULATE_REPEAT_TYPE repeat_mode = image->common.repeat;
	int width = image->width;
	int height = image->height;
	unsigned int srtot, sgtot, sbtot, satot;

	params += 2;

	x1 = PIXEL_MANIPULATE_FIXED_TO_INTEGER (x - PIXEL_MANIPULATE_FIXED_E - x_off);
	y1 = PIXEL_MANIPULATE_FIXED_TO_INTEGER (y - PIXEL_MANIPULATE_FIXED_E - y_off);
	x2 = x1 + cwidth;
	y2 = y1 + cheight;

	srtot = sgtot = sbtot = satot = 0;

	for (i = y1; i < y2; ++i)
	{
	for (j = x1; j < x2; ++j)
	{
		int rx = j;
		int ry = i;

		PIXEL_MANIPULATE_FIXED f = *params;

		if (f)
		{
		/* Must be big enough to hold a PIXEL_MANIPULATE_ARGB */
		PIXEL_MANIPULATE_ARGB pixel;

		if (repeat_mode != PIXEL_MANIPULATE_REPEAT_TYPE_NONE)
		{
			repeat (repeat_mode, &rx, width);
			repeat (repeat_mode, &ry, height);

			get_pixel (image, rx, ry, FALSE, &pixel);
		}
		else
		{
			get_pixel (image, rx, ry, TRUE, &pixel);
		}

		accum (&satot, &srtot, &sgtot, &sbtot, &pixel, f);
		}

		params++;
	}
	}

	reduce (satot, srtot, sgtot, sbtot, out);
}

static void
bits_image_fetch_pixel_separable_convolution (PIXEL_MANIPULATE_BITS_IMAGE  *image,
						  PIXEL_MANIPULATE_FIXED x,
						  PIXEL_MANIPULATE_FIXED y,
						  get_pixel_t	get_pixel,
						  void		*out,
						  accumulate_pixel_t accum,
						  reduce_pixel_t	 reduce)
{
	PIXEL_MANIPULATE_FIXED *params = image->common.filter_parameters;
	ePIXEL_MANIPULATE_REPEAT_TYPE repeat_mode = image->common.repeat;
	int width = image->width;
	int height = image->height;
	int cwidth = PIXEL_MANIPULATE_FIXED_TO_INTEGER (params[0]);
	int cheight = PIXEL_MANIPULATE_FIXED_TO_INTEGER (params[1]);
	int x_phase_bits = PIXEL_MANIPULATE_FIXED_TO_INTEGER (params[2]);
	int y_phase_bits = PIXEL_MANIPULATE_FIXED_TO_INTEGER (params[3]);
	int x_phase_shift = 16 - x_phase_bits;
	int y_phase_shift = 16 - y_phase_bits;
	int x_off = ((cwidth << 16) - PIXEL_MANIPULATE_FIXED_1) >> 1;
	int y_off = ((cheight << 16) - PIXEL_MANIPULATE_FIXED_1) >> 1;
	PIXEL_MANIPULATE_FIXED *y_params;
	unsigned int srtot, sgtot, sbtot, satot;
	int32 x1, x2, y1, y2;
	int32 px, py;
	int i, j;

	/* Round x and y to the middle of the closest phase before continuing. This
	 * ensures that the convolution matrix is aligned right, since it was
	 * positioned relative to a particular phase (and not relative to whatever
	 * exact fraction we happen to get here).
	 */
	x = ((x >> x_phase_shift) << x_phase_shift) + ((1 << x_phase_shift) >> 1);
	y = ((y >> y_phase_shift) << y_phase_shift) + ((1 << y_phase_shift) >> 1);

	px = (x & 0xffff) >> x_phase_shift;
	py = (y & 0xffff) >> y_phase_shift;

	y_params = params + 4 + (1 << x_phase_bits) * cwidth + py * cheight;

	x1 = PIXEL_MANIPULATE_FIXED_TO_INTEGER (x - PIXEL_MANIPULATE_FIXED_E - x_off);
	y1 = PIXEL_MANIPULATE_FIXED_TO_INTEGER (y - PIXEL_MANIPULATE_FIXED_E - y_off);
	x2 = x1 + cwidth;
	y2 = y1 + cheight;

	srtot = sgtot = sbtot = satot = 0;

	for (i = y1; i < y2; ++i)
	{
		PIXEL_MANIPULATE_FIXED_48_16 fy = *y_params++;
		PIXEL_MANIPULATE_FIXED *x_params = params + 4 + px * cwidth;

		if (fy)
		{
			for (j = x1; j < x2; ++j)
			{
				PIXEL_MANIPULATE_FIXED fx = *x_params++;
		int rx = j;
		int ry = i;

				if (fx)
				{
					/* Must be big enough to hold a PIXEL_MANIPULATE_ARGB */
					PIXEL_MANIPULATE_ARGB pixel;
					PIXEL_MANIPULATE_FIXED f;

					if (repeat_mode != PIXEL_MANIPULATE_REPEAT_TYPE_NONE)
					{
						repeat (repeat_mode, &rx, width);
						repeat (repeat_mode, &ry, height);

						get_pixel (image, rx, ry, FALSE, &pixel);
					}
					else
					{
						get_pixel (image, rx, ry, TRUE, &pixel);
			}

					f = (PIXEL_MANIPULATE_FIXED)((fy * fx + 0x8000) >> 16);

			accum(&satot, &srtot, &sgtot, &sbtot, &pixel, f);
				}
			}
	}
	}


	reduce(satot, srtot, sgtot, sbtot, out);
}

static FORCE_INLINE void
bits_image_fetch_pixel_filtered (PIXEL_MANIPULATE_BITS_IMAGE  *image,
				 int  wide,
				 PIXEL_MANIPULATE_FIXED x,
				 PIXEL_MANIPULATE_FIXED y,
				 get_pixel_t	get_pixel,
				 void		  *out)
{
	switch (image->common.filter)
	{
	case PIXEL_MANIPULATE_FILTER_TYPE_NEAREST:
	case PIXEL_MANIPULATE_FILTER_TYPE_FAST:
	bits_image_fetch_pixel_nearest (image, x, y, get_pixel, out);
	break;

	case PIXEL_MANIPULATE_FILTER_TYPE_BILINEAR:
	case PIXEL_MANIPULATE_FILTER_TYPE_GOOD:
	case PIXEL_MANIPULATE_FILTER_TYPE_BEST:
	if (wide)
		bits_image_fetch_pixel_bilinear_float (image, x, y, get_pixel, out);
	else
		bits_image_fetch_pixel_bilinear_32 (image, x, y, get_pixel, out);
	break;

	case PIXEL_MANIPULATE_FILTER_TYPE_CONVOLUTION:
	if (wide)
	{
		bits_image_fetch_pixel_convolution (image, x, y,
						get_pixel, out,
						accum_float,
						reduce_float);
	}
	else
	{
		bits_image_fetch_pixel_convolution (image, x, y,
						get_pixel, out,
						accum_32, reduce_32);
	}
	break;

	case PIXEL_MANIPULATE_FILTER_TYPE_SEPARABLE_CONVOLUTION:
	if (wide)
	{
		bits_image_fetch_pixel_separable_convolution (image, x, y,
							  get_pixel, out,
							  accum_float,
							  reduce_float);
	}
	else
	{
		bits_image_fetch_pixel_separable_convolution (image, x, y,
							  get_pixel, out,
							  accum_32, reduce_32);
	}
		break;

	default:
	ASSERT (0);
		break;
	}
}

static uint32 *
__bits_image_fetch_affine_no_alpha (PIXEL_MANIPULATE_ITERATION*  iter,
					int	wide,
					const uint32 * mask)
{
	PIXEL_MANIPULATE_IMAGE *image  = iter->image;
	int			 offset = iter->x;
	int			 line   = iter->y++;
	int			 width  = iter->width;
	uint32 *	  buffer = iter->buffer;

	PIXEL_MANIPULATE_FIXED x, y;
	PIXEL_MANIPULATE_FIXED ux, uy;
	PIXEL_MANIPULATE_VECTOR v;
	int i;
	get_pixel_t get_pixel =
	wide ? fetch_pixel_no_alpha_float : fetch_pixel_no_alpha_32;

	/* reference point is the center of the pixel */
	v.vector[0] = PIXEL_MANIPULATE_INTEGER_TO_FIXED (offset) + PIXEL_MANIPULATE_FIXED_1 / 2;
	v.vector[1] = PIXEL_MANIPULATE_INTEGER_TO_FIXED (line) + PIXEL_MANIPULATE_FIXED_1 / 2;
	v.vector[2] = PIXEL_MANIPULATE_FIXED_1;

	if(image->common.transform)
	{
		if(FALSE == PixelManipulateTransformPoint3d(image->common.transform, &v))
			return iter->buffer;

		ux = image->common.transform->matrix[0][0];
		uy = image->common.transform->matrix[1][0];
	}
	else
	{
		ux = PIXEL_MANIPULATE_FIXED_1;
		uy = 0;
	}

	x = v.vector[0];
	y = v.vector[1];

	for (i = 0; i < width; ++i)
	{
		if (!mask || mask[i])
		{
			bits_image_fetch_pixel_filtered (
				&image->bits, wide, x, y, get_pixel, buffer);
		}

		x += ux;
		y += uy;
		buffer += wide ? 4 : 1;
	}

	return iter->buffer;
}

static uint32 *
bits_image_fetch_affine_no_alpha_32 (PIXEL_MANIPULATE_ITERATION *iter,
					 const uint32 *mask)
{
	return __bits_image_fetch_affine_no_alpha(iter, FALSE, mask);
}

static uint32 *
bits_image_fetch_affine_no_alpha_float (PIXEL_MANIPULATE_ITERATION *iter,
					const uint32 *mask)
{
	return __bits_image_fetch_affine_no_alpha(iter, TRUE, mask);
}

/* General fetcher */
static FORCE_INLINE void
fetch_pixel_general_32 (PIXEL_MANIPULATE_BITS_IMAGE *image,
			int x, int y, int check_bounds,
			void *out)
{
	uint32 pixel, *ret = out;

	if (check_bounds &&
	(x < 0 || x >= image->width || y < 0 || y >= image->height))
	{
	*ret = 0;
	return;
	}

	pixel = image->fetch_pixel_32 (image, x, y);

	if (image->common.alpha_map)
	{
	uint32 pixel_a;

	x -= image->common.alpha_origin_x;
	y -= image->common.alpha_origin_y;

	if (x < 0 || x >= image->common.alpha_map->width ||
		y < 0 || y >= image->common.alpha_map->height)
	{
		pixel_a = 0;
	}
	else
	{
		pixel_a = image->common.alpha_map->fetch_pixel_32 (
		image->common.alpha_map, x, y);

		pixel_a = ALPHA_8 (pixel_a);
	}

	pixel &= 0x00ffffff;
	pixel |= (pixel_a << 24);
	}

	*ret = pixel;
}

static FORCE_INLINE void
fetch_pixel_general_float (PIXEL_MANIPULATE_BITS_IMAGE *image,
			int x, int y, int check_bounds,
			void *out)
{
	PIXEL_MANIPULATE_ARGB *ret = out;

	if (check_bounds &&
	(x < 0 || x >= image->width || y < 0 || y >= image->height))
	{
	ret->a = ret->r = ret->g = ret->b = 0;
	return;
	}

	*ret = image->fetch_pixel_float (image, x, y);

	if (image->common.alpha_map)
	{
	x -= image->common.alpha_origin_x;
	y -= image->common.alpha_origin_y;

	if (x < 0 || x >= image->common.alpha_map->width ||
		y < 0 || y >= image->common.alpha_map->height)
	{
		ret->a = 0.f;
	}
	else
	{
		PIXEL_MANIPULATE_ARGB alpha;

		alpha = image->common.alpha_map->fetch_pixel_float (
			image->common.alpha_map, x, y);

		ret->a = alpha.a;
	}
	}
}

static uint32 *
__bits_image_fetch_general (PIXEL_MANIPULATE_ITERATION *iter,
				int wide,
				const uint32 *mask)
{
	PIXEL_MANIPULATE_IMAGE *image  = iter->image;
	int			 offset = iter->x;
	int			 line   = iter->y++;
	int			 width  = iter->width;
	uint32 *	  buffer = iter->buffer;
	get_pixel_t	 get_pixel =
	wide ? fetch_pixel_general_float : fetch_pixel_general_32;

	PIXEL_MANIPULATE_FIXED x, y, w;
	PIXEL_MANIPULATE_FIXED ux, uy, uw;
	PIXEL_MANIPULATE_VECTOR v;
	int i;

	/* reference point is the center of the pixel */
	v.vector[0] = PIXEL_MANIPULATE_INTEGER_TO_FIXED (offset) + PIXEL_MANIPULATE_FIXED_1 / 2;
	v.vector[1] = PIXEL_MANIPULATE_INTEGER_TO_FIXED (line) + PIXEL_MANIPULATE_FIXED_1 / 2;
	v.vector[2] = PIXEL_MANIPULATE_FIXED_1;

	if (image->common.transform)
	{
		if(FALSE == PixelManipulateTransformPoint3d(image->common.transform, &v))
			return buffer;

		ux = image->common.transform->matrix[0][0];
		uy = image->common.transform->matrix[1][0];
		uw = image->common.transform->matrix[2][0];
	}
	else
	{
		ux = PIXEL_MANIPULATE_FIXED_1;
		uy = 0;
		uw = 0;
	}

	x = v.vector[0];
	y = v.vector[1];
	w = v.vector[2];

	for (i = 0; i < width; ++i)
	{
		PIXEL_MANIPULATE_FIXED x0, y0;

		if (!mask || mask[i])
		{
			if (w != 0)
			{
				x0 = ((uint64)x << 16) / w;
				y0 = ((uint64)y << 16) / w;
			}
			else
			{
				x0 = 0;
				y0 = 0;
			}

			bits_image_fetch_pixel_filtered (
				&image->bits, wide, x0, y0, get_pixel, buffer);
		}

		x += ux;
		y += uy;
		w += uw;
		buffer += wide ? 4 : 1;
	}

	return iter->buffer;
}

static uint32 *
bits_image_fetch_general_32 (PIXEL_MANIPULATE_ITERATION *iter,
				 const uint32 *mask)
{
	return __bits_image_fetch_general(iter, FALSE, mask);
}

static uint32 *
bits_image_fetch_general_float (PIXEL_MANIPULATE_ITERATION *iter,
				const uint32 *mask)
{
	return __bits_image_fetch_general(iter, TRUE, mask);
}

static void
replicate_pixel_32 (PIXEL_MANIPULATE_BITS_IMAGE *   bits,
			int			  x,
			int			  y,
			int			  width,
			uint32 *	   buffer)
{
	uint32 color;
	uint32 *end;

	color = bits->fetch_pixel_32 (bits, x, y);

	end = buffer + width;
	while (buffer < end)
	*(buffer++) = color;
}

static void
replicate_pixel_float (PIXEL_MANIPULATE_BITS_IMAGE *   bits,
			   int			  x,
			   int			  y,
			   int			  width,
			   uint32 *	   b)
{
	PIXEL_MANIPULATE_ARGB color;
	PIXEL_MANIPULATE_ARGB *buffer = (PIXEL_MANIPULATE_ARGB *)b;
	PIXEL_MANIPULATE_ARGB *end;

	color = bits->fetch_pixel_float (bits, x, y);

	end = buffer + width;
	while (buffer < end)
	*(buffer++) = color;
}

static void
bits_image_fetch_untransformed_repeat_none (PIXEL_MANIPULATE_BITS_IMAGE *image,
											int wide,
											int		   x,
											int		   y,
											int		   width,
											uint32 *	buffer)
{
	uint32 w;

	if (y < 0 || y >= image->height)
	{
		(void)memset(buffer, 0, width * (wide? sizeof (PIXEL_MANIPULATE_ARGB) : 4));
		return;
	}

	if (x < 0)
	{
		w = MINIMUM(width, -x);

		(void)memset (buffer, 0, w * (wide ? sizeof (PIXEL_MANIPULATE_ARGB) : 4));

		width -= w;
		buffer += w * (wide? 4 : 1);
		x += w;
	}

	if (x < image->width)
	{
		w = MINIMUM(width, image->width - x);

		if (wide)
			image->fetch_scanline_float (image, x, y, w, buffer, NULL);
		else
			image->fetch_scanline_32 (image, x, y, w, buffer, NULL);

		width -= w;
		buffer += w * (wide? 4 : 1);
		x += w;
	}

	(void)memset(buffer, 0, width * (wide ? sizeof (PIXEL_MANIPULATE_ARGB) : 4));
}

static void
bits_image_fetch_untransformed_repeat_normal (PIXEL_MANIPULATE_BITS_IMAGE *image,
											  int wide,
											  int		   x,
											  int		   y,
											  int		   width,
											  uint32 *	buffer)
{
	uint32 w;

	while (y < 0)
	y += image->height;

	while (y >= image->height)
	y -= image->height;

	if (image->width == 1)
	{
	if (wide)
		replicate_pixel_float (image, 0, y, width, buffer);
	else
		replicate_pixel_32 (image, 0, y, width, buffer);

	return;
	}

	while (width)
	{
	while (x < 0)
		x += image->width;
	while (x >= image->width)
		x -= image->width;

	w = MINIMUM(width, image->width - x);

	if (wide)
		image->fetch_scanline_float (image, x, y, w, buffer, NULL);
	else
		image->fetch_scanline_32 (image, x, y, w, buffer, NULL);

	buffer += w * (wide? 4 : 1);
	x += w;
	width -= w;
	}
}

static uint32 *
bits_image_fetch_untransformed_32 (PIXEL_MANIPULATE_ITERATION* iter,
				   const uint32 *mask)
{
	PIXEL_MANIPULATE_IMAGE *image  = iter->image;
	int			 x	  = iter->x;
	int			 y	  = iter->y;
	int			 width  = iter->width;
	uint32 *	  buffer = iter->buffer;

	if (image->common.repeat == PIXEL_MANIPULATE_REPEAT_TYPE_NONE)
	{
	bits_image_fetch_untransformed_repeat_none (
		&image->bits, FALSE, x, y, width, buffer);
	}
	else
	{
	bits_image_fetch_untransformed_repeat_normal (
		&image->bits, FALSE, x, y, width, buffer);
	}

	iter->y++;
	return buffer;
}

static uint32 *
bits_image_fetch_untransformed_float (PIXEL_MANIPULATE_ITERATION* iter,
					  const uint32 *mask)
{
	PIXEL_MANIPULATE_IMAGE *image  = iter->image;
	int			 x	  = iter->x;
	int			 y	  = iter->y;
	int			 width  = iter->width;
	uint32 *	  buffer = iter->buffer;

	if (image->common.repeat == PIXEL_MANIPULATE_REPEAT_TYPE_NONE)
	{
	bits_image_fetch_untransformed_repeat_none (
		&image->bits, TRUE, x, y, width, buffer);
	}
	else
	{
	bits_image_fetch_untransformed_repeat_normal (
		&image->bits, TRUE, x, y, width, buffer);
	}

	iter->y++;
	return buffer;
}

typedef struct _FETCHER_INFO
{
	ePIXEL_MANIPULATE_FORMAT format;
	uint32 flags;
	pixel_manipulate_iteration_get_scanline get_scanline_32;
	pixel_manipulate_iteration_get_scanline get_scanline_float;
} FETCHER_INFO;

static const FETCHER_INFO fetcher_info[] =
{
	{ PIXEL_MANIPULATE_FORMAT_ANY,
	  (FAST_PATH_NO_ALPHA_MAP			|
	   FAST_PATH_ID_TRANSFORM			|
	   FAST_PATH_NO_CONVOLUTION_FILTER		|
	   FAST_PATH_NO_PAD_REPEAT			|
	   FAST_PATH_NO_REFLECT_REPEAT),
	  bits_image_fetch_untransformed_32,
	  bits_image_fetch_untransformed_float
	},

	/* Affine, no alpha */
	{ PIXEL_MANIPULATE_FORMAT_ANY,
	  (FAST_PATH_NO_ALPHA_MAP | FAST_PATH_HAS_TRANSFORM | FAST_PATH_AFFINE_TRANSFORM),
	  bits_image_fetch_affine_no_alpha_32,
	  bits_image_fetch_affine_no_alpha_float,
	},

	/* General */
	{ PIXEL_MANIPULATE_FORMAT_ANY,
	  0,
	  bits_image_fetch_general_32,
	  bits_image_fetch_general_float,
	},

	{ 0 },
};

void PixelManipulateBitsImageSourceIterationInitialize(
	union _PIXEL_MANIPULATE_IMAGE* image,
	PIXEL_MANIPULATE_ITERATION* iter
)
{
	ePIXEL_MANIPULATE_FORMAT format = image->common.extended_format_code;
	uint32 flags = image->common.flags;
	const FETCHER_INFO *info;

	for(info = fetcher_info; info->format != PIXEL_MANIPULATE_null; ++info)
	{
		if ((info->format == format || info->format == PIXEL_MANIPULATE_FORMAT_ANY)	&&
			(info->flags & flags) == info->flags)
		{
			if (iter->iterate_flags & PIXEL_MANIPULATE_ITERATION_NARROW)
			{
				iter->get_scanline = info->get_scanline_32;
			}
			else
			{
				iter->get_scanline = info->get_scanline_float;
			}
			return;
		}
	}

	iter->get_scanline = PixelManipulateIteratorGetScanlineNoop;
}

static uint32* DestGetScanlineNarrow(PIXEL_MANIPULATE_ITERATION* iter, const uint32* mask)
{
	PIXEL_MANIPULATE_IMAGE *image  = iter->image;
	int			 x	  = iter->x;
	int			 y	  = iter->y;
	int			 width  = iter->width;
	uint32			*buffer = iter->buffer;

	image->bits.fetch_scanline_32 (&image->bits, x, y, width, buffer, mask);
	if (image->common.alpha_map)
	{
		uint32 *alpha;

		if((alpha = MEM_ALLOC_FUNC(width * sizeof (uint32))))
		{
			int i;

			x -= image->common.alpha_origin_x;
			y -= image->common.alpha_origin_y;

			image->common.alpha_map->fetch_scanline_32 (
			image->common.alpha_map, x, y, width, alpha, mask);

			for (i = 0; i < width; ++i)
			{
				buffer[i] &= ~0xff000000;
				buffer[i] |= (alpha[i] & 0xff000000);
			}

			MEM_FREE_FUNC(alpha);
		}
	}

	return iter->buffer;
}

static uint32* DestGetScanlineWide(PIXEL_MANIPULATE_ITERATION* iter, const uint32* mask)
{
	PIXEL_MANIPULATE_BITS_IMAGE *image  = &iter->image->bits;
	int			 x	  = iter->x;
	int			 y	  = iter->y;
	int			 width  = iter->width;
	PIXEL_MANIPULATE_ARGB *buffer = (PIXEL_MANIPULATE_ARGB*)iter->buffer;

	image->fetch_scanline_float (
	image, x, y, width, (uint32*)buffer, mask);
	if (image->common.alpha_map)
	{
		PIXEL_MANIPULATE_ARGB *alpha;

		if ((alpha = MEM_ALLOC_FUNC(width * sizeof (PIXEL_MANIPULATE_ARGB))))
		{
			int i;

			x -= image->common.alpha_origin_x;
			y -= image->common.alpha_origin_y;

			image->common.alpha_map->fetch_scanline_float (
			image->common.alpha_map, x, y, width, (uint32 *)alpha, mask);

			for (i = 0; i < width; ++i)
				buffer[i].a = alpha[i].a;

			MEM_FREE_FUNC(alpha);
		}
	}

	return iter->buffer;
}

static void DestWriteBackNarrow(PIXEL_MANIPULATE_ITERATION* iter)
{
	PIXEL_MANIPULATE_BITS_IMAGE *image  = &iter->image->bits;
	int			 x	  = iter->x;
	int			 y	  = iter->y;
	int			 width  = iter->width;
	const uint32 *buffer = iter->buffer;

	image->store_scanline_32(image, x, y, width, buffer);

	if (image->common.alpha_map)
	{
		x -= image->common.alpha_origin_x;
		y -= image->common.alpha_origin_y;

		image->common.alpha_map->store_scanline_32 (
			image->common.alpha_map, x, y, width, buffer);
	}

	iter->y++;
}

static const float DitherFactorBlueNoise64(int x, int y)
{
	float m = dither_blue_noise_64x64[((y & 0x3f) << 6) | (x & 0x3f)];
	return (const float)(m * (1. / 4096.f) + (1. / 8192.f));
}

static const float DitherFactorBayer8(int x, int y)
{
	uint32 m;

	y ^= x;

	m = ((y & 0x1) << 5) | ((x & 0x1) << 4) |
	((y & 0x2) << 2) | ((x & 0x2) << 1) |
	((y & 0x4) >> 1) | ((x & 0x4) >> 2);

	return (float)(m) * (1 / 64.0f) + (1.0f / 128.0f);
}

typedef float (* dither_factor_t)(int x, int y);

static INLINE float DitherApplyChannel (float f, float d, float s)
{
	return f + (d - f) * s;
}

static INLINE float DitherComputeScale(int n_bits)
{
	if (n_bits == 0 || n_bits >= 32)
		return 0.f;

	return 1.f / (float)(1 << n_bits);
}

static const uint32* DitherApplyOrdered(PIXEL_MANIPULATE_ITERATION* iter, dither_factor_t factor)
{
	PIXEL_MANIPULATE_BITS_IMAGE *image = &iter->image->bits;
	int							x	= iter->x + image->dither_offset_x;
	int							y	= iter->y + image->dither_offset_y;
	int						width	= iter->width;
	PIXEL_MANIPULATE_ARGB *buffer	= (PIXEL_MANIPULATE_ARGB*)iter->buffer;

	ePIXEL_MANIPULATE_FORMAT format = image->format;
	int				  a_size = PIXEL_MANIPULATE_FORMAT_A(format);
	int				  r_size = PIXEL_MANIPULATE_FORMAT_R(format);
	int				  g_size = PIXEL_MANIPULATE_FORMAT_G(format);
	int				  b_size = PIXEL_MANIPULATE_FORMAT_B(format);

	float a_scale = DitherComputeScale(a_size);
	float r_scale = DitherComputeScale(r_size);
	float g_scale = DitherComputeScale(g_size);
	float b_scale = DitherComputeScale(b_size);

	int   i;
	float d;

	for (i = 0; i < width; ++i)
	{
		d = factor (x + i, y);

		buffer->a = DitherApplyChannel(buffer->a, d, a_scale);
		buffer->r = DitherApplyChannel(buffer->r, d, r_scale);
		buffer->g = DitherApplyChannel(buffer->g, d, g_scale);
		buffer->b = DitherApplyChannel(buffer->b, d, b_scale);

		buffer++;
	}

	return iter->buffer;
}

static void DestWriteBackWide(PIXEL_MANIPULATE_ITERATION* iter)
{
	PIXEL_MANIPULATE_BITS_IMAGE *image	= &iter->image->bits;
	int								x	= iter->x;
	int								y	= iter->y;
	int							width	= iter->width;
	const uint32				*buffer	= iter->buffer;

	switch (image->dither)
	{
	case PIXEL_MANIPULATE_DITHER_NONE:
		break;

	case PIXEL_MANIPULATE_DITHER_GOOD:
	case PIXEL_MANIPULATE_DITHER_BEST:
	case PIXEL_MANIPULATE_DITHER_ORDERED_BLUE_NOISE_64:
	buffer = DitherApplyOrdered (iter, DitherFactorBlueNoise64);
		break;

	case PIXEL_MANIPULATE_DITHER_FAST:
	case PIXEL_MANIPULATE_DITHER_ORDERED_BAYER_8:
	buffer = DitherApplyOrdered(iter, DitherFactorBayer8);
		break;
	}

	image->store_scanline_float(image, x, y, width, buffer);

	if(image->common.alpha_map)
	{
		x -= image->common.alpha_origin_x;
		y -= image->common.alpha_origin_y;

		image->common.alpha_map->store_scanline_float (
			image->common.alpha_map, x, y, width, buffer);
	}

	iter->y++;
}

void PixelManipulateBitsImageDestinationIterationInitialize(
	union _PIXEL_MANIPULATE_IMAGE* image,
	PIXEL_MANIPULATE_ITERATION* iter
)
{
	if((iter->iterate_flags & PIXEL_MANIPULATE_ITERATION_NARROW) != 0)
	{
		if((iter->iterate_flags & (PIXEL_MANIPULATE_ITERATION_IGNORE_RGB | PIXEL_MANIPULATE_ITERATION_IGNORE_ALPHA))
			== (PIXEL_MANIPULATE_ITERATION_IGNORE_RGB | PIXEL_MANIPULATE_ITERATION_IGNORE_ALPHA))
		{
			iter->get_scanline = PixelManipulateIteratorGetScanlineNoop;
		}
		else
		{
			iter->get_scanline = DestGetScanlineNarrow;
		}

		iter->write_back = DestWriteBackNarrow;
	}
	else
	{
		iter->get_scanline = DestGetScanlineWide;
		iter->write_back = DestWriteBackWide;
	}
}

static void
general_iter_init (PIXEL_MANIPULATE_ITERATION* iter, const PIXEL_MANIPULATE_ITERATION_INFO* info)
{
	PIXEL_MANIPULATE_IMAGE *image = iter->image;

	switch (image->type)
	{
	case PIXEL_MANIPULATE_IMAGE_TYPE_BITS:
		if ((iter->iterate_flags & PIXEL_MANIPULATE_ITERATION_SRC) == PIXEL_MANIPULATE_ITERATION_SRC)
		{
			PixelManipulateBitsImageSourceIterationInitialize(image, iter);
		}
		else
		{
			PixelManipulateBitsImageDestinationIterationInitialize(image, iter);
		}
		break;

	case PIXEL_MANIPULATE_IMAGE_TYPE_LINEAR:
		PixelManipulateLinearGradientIteratorInitialize(image, iter);
		break;

	case PIXEL_MANIPULATE_IMAGE_TYPE_RADIAL:
		PixelManipulateRadialGradientIterationInitialize(image, iter);
		break;

	case PIXEL_MANIPULATE_IMAGE_TYPE_CONICAL:
		PixelManipulateConicalGradientIterationInitialize(image, iter);
		break;

	case PIXEL_MANIPULATE_IMAGE_TYPE_SOLID:
		// _pixman_log_error (__func__, "Solid image not handled by noop");
		break;

	default:
	// _pixman_log_error (__func__, "Pixman bug: unknown image type\n");
		break;
	}
}

static const PIXEL_MANIPULATE_ITERATION_INFO general_iters[] =
{
	{ PIXEL_MANIPULATE_FORMAT_ANY, 0, 0, general_iter_init, NULL, NULL },
	{ 0 },
};

typedef struct op_info_t op_info_t;
struct op_info_t
{
	uint8 src, dst;
};

#define ITER_IGNORE_BOTH						\
	(PIXEL_MANIPULATE_ITERATION_IGNORE_ALPHA | PIXEL_MANIPULATE_ITERATION_IGNORE_RGB | PIXEL_MANIPULATE_ITERATION_LOCALIZED_ALPHA)

static const op_info_t op_flags[PIXEL_MANIPULATE_NUM_OPERATORS] =
{
	/* Src				   Dst				   */
	{ ITER_IGNORE_BOTH,	  ITER_IGNORE_BOTH	  }, /* CLEAR */
	{ PIXEL_MANIPULATE_ITERATION_LOCALIZED_ALPHA,  ITER_IGNORE_BOTH	  }, /* SRC */
	{ ITER_IGNORE_BOTH,	  PIXEL_MANIPULATE_ITERATION_LOCALIZED_ALPHA  }, /* DST */
	{ 0,					 PIXEL_MANIPULATE_ITERATION_LOCALIZED_ALPHA  }, /* OVER */
	{ PIXEL_MANIPULATE_ITERATION_LOCALIZED_ALPHA,  0					 }, /* OVER_REVERSE */
	{ PIXEL_MANIPULATE_ITERATION_LOCALIZED_ALPHA,  PIXEL_MANIPULATE_ITERATION_IGNORE_RGB	   }, /* IN */
	{ PIXEL_MANIPULATE_ITERATION_IGNORE_RGB,	   PIXEL_MANIPULATE_ITERATION_LOCALIZED_ALPHA  }, /* IN_REVERSE */
	{ PIXEL_MANIPULATE_ITERATION_LOCALIZED_ALPHA,  PIXEL_MANIPULATE_ITERATION_IGNORE_RGB	   }, /* OUT */
	{ PIXEL_MANIPULATE_ITERATION_IGNORE_RGB,	   PIXEL_MANIPULATE_ITERATION_LOCALIZED_ALPHA  }, /* OUT_REVERSE */
	{ 0,					 0					 }, /* ATOP */
	{ 0,					 0					 }, /* ATOP_REVERSE */
	{ 0,					 0					 }, /* XOR */
	{ PIXEL_MANIPULATE_ITERATION_LOCALIZED_ALPHA,  PIXEL_MANIPULATE_ITERATION_LOCALIZED_ALPHA  }, /* ADD */
	{ 0,					 0					 }, /* SATURATE */
};

#define SCANLINE_BUFFER_LENGTH 8192

static int
operator_needs_division (int op)
{
	static const uint8 needs_division[] =
	{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, /* SATURATE */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, /* DISJOINT */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, /* CONJOINT */
	0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 0, /* blend ops */
	};

	return needs_division[op];
}

static void general_composite_rect(PIXEL_MANIPULATE_IMPLEMENTATION *imp, PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint8 stack_scanline_buffer[3 * SCANLINE_BUFFER_LENGTH];
	uint8 *scanline_buffer = (uint8 *) stack_scanline_buffer;
	uint8 *src_buffer, *mask_buffer, *dest_buffer;
	PIXEL_MANIPULATE_ITERATION src_iter, mask_iter, dest_iter;
	pixel_manipulate_combine32_function compose;
	int component_alpha;
	ePIXEL_MANIPULATE_ITERATION_FLAGS width_flag, src_iter_flags;
	int Bpp;
	int i;

	if ((src_image->common.flags & FAST_PATH_NARROW_FORMAT)			 &&
		(!mask_image || mask_image->common.flags & FAST_PATH_NARROW_FORMAT)  &&
		(dest_image->common.flags & FAST_PATH_NARROW_FORMAT)			 &&
		!(operator_needs_division (op))									  &&
		(dest_image->bits.dither == PIXEL_MANIPULATE_DITHER_NONE))
	{
		width_flag = PIXEL_MANIPULATE_ITERATION_NARROW;
		Bpp = 4;
	}
	else
	{
		width_flag = PIXEL_MANIPULATE_ITERATION_WIDE;
		Bpp = 16;
	}

#define ALIGN(addr)							\
	((uint8*)((((uintptr_t)(addr)) + 15) & (~15)))

	if(width <= 0 || width >= INT32_MAX / (Bpp * 3))
		return;

	if(width * Bpp * 3 > sizeof (stack_scanline_buffer) - 15 * 3)
	{
		scanline_buffer = (uint8*)MEM_ALLOC_FUNC(width * Bpp * 3 + 15 * 3);

	if (!scanline_buffer)
		return;

		(void)memset(scanline_buffer, 0, width * Bpp * 3 + 15 * 3);
	}
	else
	{
		(void)memset(stack_scanline_buffer, 0, sizeof (stack_scanline_buffer));
	}

	src_buffer = ALIGN (scanline_buffer);
	mask_buffer = ALIGN (src_buffer + width * Bpp);
	dest_buffer = ALIGN (mask_buffer + width * Bpp);

	if (width_flag == PIXEL_MANIPULATE_ITERATION_WIDE)
	{
	/* To make sure there aren't any NANs in the buffers */
	memset (src_buffer, 0, width * Bpp);
	memset (mask_buffer, 0, width * Bpp);
	memset (dest_buffer, 0, width * Bpp);
	}

	/* src iter */
	src_iter_flags = width_flag | op_flags[op].src | PIXEL_MANIPULATE_ITERATION_SRC;

	PixelManipulateImplementationIterationInitialize(imp->top_level, &src_iter, src_image,
									  src_x, src_y, width, height,
									  src_buffer, src_iter_flags,
									  info->src_flags);

	/* mask iter */
	if ((src_iter_flags & (PIXEL_MANIPULATE_ITERATION_IGNORE_ALPHA | PIXEL_MANIPULATE_ITERATION_IGNORE_RGB)) ==
	(PIXEL_MANIPULATE_ITERATION_IGNORE_ALPHA | PIXEL_MANIPULATE_ITERATION_IGNORE_RGB))
	{
	/* If it doesn't matter what the source is, then it doesn't matter
	 * what the mask is
	 */
	mask_image = NULL;
	}

	component_alpha = mask_image && mask_image->common.component_alpha;

	PixelManipulateImplementationIterationInitialize(
	imp->top_level, &mask_iter,
	mask_image, mask_x, mask_y, width, height, mask_buffer,
	PIXEL_MANIPULATE_ITERATION_SRC | width_flag | (component_alpha ? 0 : PIXEL_MANIPULATE_ITERATION_IGNORE_RGB),
	info->mask_flags);

	/* dest iter */
	PixelManipulateImplementationIterationInitialize (
	imp->top_level, &dest_iter, dest_image, dest_x, dest_y, width, height,
	dest_buffer, PIXEL_MANIPULATE_ITERATION_DEST | width_flag | op_flags[op].dst, info->dest_flags);

	compose = PixelManipulateImplementationLookupCombiner(
	imp->top_level, op, component_alpha, width_flag != PIXEL_MANIPULATE_ITERATION_WIDE);

	for (i = 0; i < height; ++i)
	{
		uint32 *s, *m, *d;

		m = mask_iter.get_scanline (&mask_iter, NULL);
		s = src_iter.get_scanline (&src_iter, m);
		d = dest_iter.get_scanline (&dest_iter, NULL);

		compose(imp->top_level, op, d, s, m, width);

		dest_iter.write_back(&dest_iter);
	}

	if (src_iter.finish)
		src_iter.finish (&src_iter);
	if (mask_iter.finish)
		mask_iter.finish (&mask_iter);
	if (dest_iter.finish)
		dest_iter.finish (&dest_iter);

	if(scanline_buffer != (uint8 *) stack_scanline_buffer)
		free (scanline_buffer);
}

static const PIXEL_MANIPULATE_FAST_PATH general_fast_path[] =
{
	{ PIXEL_MANIPULATE_OPERATE_ANY, PIXEL_MANIPULATE_FORMAT_ANY, 0, PIXEL_MANIPULATE_FORMAT_ANY,	0, PIXEL_MANIPULATE_FORMAT_ANY, 0, general_composite_rect },
	{ PIXEL_MANIPULATE_OPERATE_NONE }
};

PIXEL_MANIPULATE_IMPLEMENTATION* PixelManipulateImplementationCreateGeneral(void)
{
	PIXEL_MANIPULATE_IMPLEMENTATION *imp = PixelManipulateImplementationCreate(NULL, general_fast_path);

	PixelManipulateSetupCombinerFunctions32(imp);
	_pixman_setup_combiner_functions_float(imp);

	imp->iteration_info = general_iters;

	return imp;
}

#ifdef __cplusplus
}
#endif
