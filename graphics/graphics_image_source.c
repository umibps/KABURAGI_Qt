#include <string.h>
#include <math.h>
#include "graphics_surface.h"
#include "graphics_compositor.h"
#include "graphics_types.h"
#include "graphics_region.h"
#include "graphics_private.h"
#include "graphics_inline.h"
#include "graphics_matrix.h"
#include "../pixel_manipulate/pixel_manipulate.h"
#include "memory.h"

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

#define PIXEL_MANIPULATE_MAXIMUM_INTEGER ((PIXEL_MANIPULATE_FIXED_1 >> 1) - PIXEL_MANIPULATE_FIXED_E) /* need to ensure deltas also fit */

#if 0 // CAIRO_NO_MUTEX
#define PIXMAN_HAS_ATOMIC_OPS 1
#endif

#if 0 // PIXMAN_HAS_ATOMIC_OPS
static PIXEL_MANIPULATE_IMAGE *_PixelManipulateTransparentImage;
static PIXEL_MANIPULATE_IMAGE *_PixelManipulateBlackImage;
static PIXEL_MANIPULATE_IMAGE *_PixelManipulateWhiteImage;

static PIXEL_MANIPULATE_IMAGE *
PixelManipulateTransparentImage (void)
{
	PIXEL_MANIPULATE_IMAGE *image;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	image = _PixelManipulateTransparentImage;
	if(UNLIKELY(image == NULL))
	{
		PIXEL_MANIPULATE_COLOR color;

		color.red   = 0x00;
		color.green = 0x00;
		color.blue  = 0x00;
		color.alpha = 0x00;

		image = PixelManipulateImageCreateSolidFill(&color);
		if (UNLIKELY(image == NULL))
			return NULL;

		if (_cairo_atomic_ptr_cmpxchg (&_PixelManipulateTransparentImage,
			NULL, image))
		{
			PixelManipulateImageReference(image);
		}
	}
	else
	{
		PixelManipulateImageReference(image);
	}

	return image;
}

static PIXEL_MANIPULATE_IMAGE *
PixelManipulateBlackImage (void)
{
	PIXEL_MANIPULATE_IMAGE *image;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	image = _PixelManipulateBlackImage;
	if (UNLIKELY(image == NULL)) {
		PIXEL_MANIPULATE_COLOR color;

		color.red   = 0x00;
		color.green = 0x00;
		color.blue  = 0x00;
		color.alpha = 0xffff;

		image = PixelManipulateImageCreateSolidFill(&color);
		if (UNLIKELY(image == NULL))
			return NULL;

		if (_cairo_atomic_ptr_cmpxchg (&_PixelManipulateBlackImage,
			NULL, image))
		{
			PixelManipulateImageReference(image);
		}
	} else {
		PixelManipulateImageReference(image);
	}

	return image;
}

static PIXEL_MANIPULATE_IMAGE *
PixelManipulateWhiteImage (void)
{
	PIXEL_MANIPULATE_IMAGE *image;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	image = _PixelManipulateWhiteImage;
	if (UNLIKELY(image == NULL)) {
		PIXEL_MANIPULATE_COLOR color;

		color.red   = 0xffff;
		color.green = 0xffff;
		color.blue  = 0xffff;
		color.alpha = 0xffff;

		image = PixelManipulateImageCreateSolidFill(&color);
		if (UNLIKELY(image == NULL))
			return NULL;

		if (_cairo_atomic_ptr_cmpxchg (&_PixelManipulateWhiteImage,
			NULL, image))
		{
			PixelManipulateImageReference(image);
		}
	} else {
		PixelManipulateImageReference(image);
	}

	return image;
}

static uint32
hars_petruska_f54_1_random (void)
{
#define rol(x,k) ((x << k) | (x >> (32-k)))
	static uint32 x;
	return x = (x ^ rol (x, 5) ^ rol (x, 24)) + 0x37798849;
#undef rol
}

static struct {
	GRAPHICS_COLOR color;
	PIXEL_MANIPULATE_IMAGE *image;
} cache[16];
static int n_cached;

#else  /* !PIXMAN_HAS_ATOMIC_OPS */

PIXEL_MANIPULATE_IMAGE* PixelManipulateImageForColor(const GRAPHICS_COLOR* cairo_color, PIXEL_MANIPULATE_IMAGE* image, void* graphics);

static INLINE PIXEL_MANIPULATE_IMAGE* PixelManipulateTransparentImage(PIXEL_MANIPULATE_IMAGE* image, GRAPHICS* graphics)
{
	TRACE ((stderr, "%s\n", __FUNCTION__));
	return PixelManipulateImageForColor(&graphics->color_transparent, image, graphics);
}

static INLINE PIXEL_MANIPULATE_IMAGE* PixelManipulateBlackImage(PIXEL_MANIPULATE_IMAGE* image, GRAPHICS* graphics)
{
	TRACE ((stderr, "%s\n", __FUNCTION__));
	return PixelManipulateImageForColor(&graphics->color_black, image, graphics);
}

static INLINE PIXEL_MANIPULATE_IMAGE* PixelManipulateWhiteImage(PIXEL_MANIPULATE_IMAGE* image, GRAPHICS* graphics)
{
	TRACE ((stderr, "%s\n", __FUNCTION__));
	return PixelManipulateImageForColor(&graphics->color_white, image, graphics);
}
#endif /* !PIXMAN_HAS_ATOMIC_OPS */


PIXEL_MANIPULATE_IMAGE* PixelManipulateImageForColor(const GRAPHICS_COLOR* cairo_color, PIXEL_MANIPULATE_IMAGE* image, void* graphics)
{
	
	PIXEL_MANIPULATE_COLOR color;

#if 0 //PIXMAN_HAS_ATOMIC_OPS
	int i;

	if (CAIRO_COLOR_IS_CLEAR (cairo_color))
	{
		if(image == NULL)
		{
			return PixelManipulateTransparentImage();
		}
		else
		{
			*image = PixelManipulateTransparentImage();
			return image;
		}
	}

	if(CAIRO_COLOR_IS_OPAQUE (cairo_color))
	{
		if(cairo_color->red_short <= 0x00ff &&
			cairo_color->green_short <= 0x00ff &&
			cairo_color->blue_short <= 0x00ff)
		{
			if(image == NULL)
			{
				return PixelManipulateBlackImage();
			}
			else
			{
				*image = PixelManipulateBlackImage();
				return image;
			}
		}

		if (cairo_color->red_short >= 0xff00 &&
			cairo_color->green_short >= 0xff00 &&
			cairo_color->blue_short >= 0xff00)
		{
			if(image == NULL)
			{
				return PixelManipulateWhiteImage();
			}
			else
			{
				*image = PixelManipulateWhiteImage();
				return image;
			}
		}
	}

	CAIRO_MUTEX_LOCK (_cairo_image_solid_cache_mutex);
	for (i = 0; i < n_cached; i++)
	{
		if(_cairo_color_equal (&cache[i].color, cairo_color)) {
			image = PixelManipulateImageReference(cache[i].image);
			goto UNLOCK;
		}
	}
#endif

	color.red   = cairo_color->red_short;
	color.green = cairo_color->green_short;
	color.blue  = cairo_color->blue_short;
	color.alpha = cairo_color->alpha_short;

	if(image == NULL)
	{
		image = PixelManipulateImageCreateSolidFill(&color);
	}
	else
	{
		InitializePixelManipulateImageSolidFill(image, &color);
	}
#if 0 // PIXMAN_HAS_ATOMIC_OPS
	if (image == NULL)
		goto UNLOCK;

	if(n_cached < (sizeof(cache) / sizeof(cache[0])))
	{
		i = n_cached++;
	}
	else
	{
		i = hars_petruska_f54_1_random () % (sizeof(cache) / sizeof(cache[0]));
		PixelManipulateImageUnreference(cache[i].image);
	}
	cache[i].image = PixelManipulateImageReference(image);
	cache[i].color = *cairo_color;

UNLOCK:
	CAIRO_MUTEX_UNLOCK (_cairo_image_solid_cache_mutex);
#endif
	return image;
}

void GraphicsImageResetStaticData(void)
{
#if 0 // PIXMAN_HAS_ATOMIC_OPS
	while (n_cached)
		PixelManipulateImageUnreference(cache[--n_cached].image);

	if (_PixelManipulateTransparentImage) {
		PixelManipulateImageUnreference(_PixelManipulateTransparentImage);
		_PixelManipulateTransparentImage = NULL;
	}

	if (_PixelManipulateBlackImage) {
		PixelManipulateImageUnreference(_PixelManipulateBlackImage);
		_PixelManipulateBlackImage = NULL;
	}

	if (_PixelManipulateWhiteImage) {
		PixelManipulateImageUnreference(_PixelManipulateWhiteImage);
		_PixelManipulateWhiteImage = NULL;
	}
#endif
}

static PIXEL_MANIPULATE_IMAGE* PixelManipulateImageForGradient(
	const GRAPHICS_GRADIENT_PATTERN *pattern,
	const GRAPHICS_RECTANGLE_INT *extents,
	int *ix, int *iy,
	PIXEL_MANIPULATE_IMAGE* image
)
{
	PIXEL_MANIPULATE_IMAGE	  *pixman_image;
	PIXEL_MANIPULATE_GRADIENT_STOP pixman_stops_static[4];
	PIXEL_MANIPULATE_GRADIENT_STOP *pixman_stops = pixman_stops_static;
	PIXEL_MANIPULATE_TRANSFORM	  pixman_transform;
	GRAPHICS_MATRIX matrix;
	GRAPHICS_CIRCLE_FLOAT extremes[2];
	PIXEL_MANIPULATE_POINT_FIXED p1, p2;
	unsigned int i;
	eGRAPHICS_INTEGER_STATUS status;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	if (pattern->num_stops > (sizeof(pixman_stops_static) / sizeof(pixman_stops_static[0])))
	{
		pixman_stops = MEM_ALLOC_FUNC(pattern->num_stops * sizeof(PIXEL_MANIPULATE_GRADIENT_STOP));
		if (UNLIKELY(pixman_stops == NULL))
			return NULL;
	}

	for (i = 0; i < pattern->num_stops; i++)
	{
		pixman_stops[i].x = GraphicsFixed16_16FromFloat(pattern->stops[i].offset);
		pixman_stops[i].color.red   = pattern->stops[i].color.red_short;
		pixman_stops[i].color.green = pattern->stops[i].color.green_short;
		pixman_stops[i].color.blue  = pattern->stops[i].color.blue_short;
		pixman_stops[i].color.alpha = pattern->stops[i].color.alpha_short;
	}
	
	GraphicsGradientPatternFitToRange(pattern, PIXEL_MANIPULATE_MAXIMUM_INTEGER >> 1, &matrix, extremes);

	p1.x = GraphicsFixed16_16FromFloat(extremes[0].center.x);
	p1.y = GraphicsFixed16_16FromFloat(extremes[0].center.y);
	p2.x = GraphicsFixed16_16FromFloat(extremes[1].center.x);
	p2.y = GraphicsFixed16_16FromFloat(extremes[1].center.y);

	if (pattern->base.type == GRAPHICS_PATTERN_TYPE_LINEAR)
	{
		if(image == NULL)
		{
			pixman_image = PixelManipulateCreateLinearGradient(&p1, &p2,
				pixman_stops, pattern->num_stops);
		}
		else
		{
			InitializePixelManipulateLinearGradient(image, &p1, &p2,
				pixman_stops, pattern->num_stops);
			pixman_image = image;
		}
	}
	else
	{
		PIXEL_MANIPULATE_FIXED r1, r2;

		r1   = GraphicsFixed16_16FromFloat(extremes[0].radius);
		r2   = GraphicsFixed16_16FromFloat(extremes[1].radius);

		if(image == NULL)
		{
			pixman_image = PixelManipulateImageCreateRadialGradient(&p1, &p2, r1, r2,
				pixman_stops,
				pattern->num_stops);
		}
		else
		{
			InitializePixelManipulateImageRadialGradient(image, &p1, &p2, r1, r2,
				pixman_stops,
				pattern->num_stops);
			pixman_image = image;
		}
	}

	if (pixman_stops != pixman_stops_static)
		MEM_FREE_FUNC(pixman_stops);

	if (UNLIKELY(pixman_image == NULL))
		return NULL;

	*ix = *iy = 0;
	status = GraphicsMatrixToPixelManipulateMatrixOffset(&matrix, pattern->base.filter,
		extents->x + extents->width/2.,
		extents->y + extents->height/2.,
		&pixman_transform, ix, iy);
	if (status != GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO) {
		if (UNLIKELY(status != GRAPHICS_INTEGER_STATUS_SUCCESS) ||
			! PixelManipulateImageSetTransform(pixman_image, &pixman_transform))
		{
			PixelManipulateImageUnreference(pixman_image);
			return NULL;
		}
	}

	{
		ePIXEL_MANIPULATE_REPEAT_TYPE pixman_repeat;

		switch (pattern->base.extend) {
		default:
		case GRAPHICS_EXTEND_NONE:
			pixman_repeat = PIXEL_MANIPULATE_REPEAT_TYPE_NONE;
			break;
		case GRAPHICS_EXTEND_REPEAT:
			pixman_repeat = PIXEL_MANIPULATE_REPEAT_TYPE_NORMAL;
			break;
		case GRAPHICS_EXTEND_REFLECT:
			pixman_repeat = PIXEL_MANIPULATE_REPEAT_TYPE_REFLECT;
			break;
		case GRAPHICS_EXTEND_PAD:
			pixman_repeat = PIXEL_MANIPULATE_REPEAT_TYPE_PAD;
			break;
		}

		PixelManipulateImageSetRepeat(pixman_image, pixman_repeat);
	}

	return pixman_image;
}

static PIXEL_MANIPULATE_IMAGE* PixelManipulateImageForMesh(
	const GRAPHICS_MESH_PATTERN *pattern,
	const GRAPHICS_RECTANGLE_INT *extents,
	int *tx, int *ty,
	PIXEL_MANIPULATE_IMAGE* im
)
{
	PIXEL_MANIPULATE_IMAGE *image;
	int width, height;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	*tx = -extents->x;
	*ty = -extents->y;
	width = extents->width;
	height = extents->height;

	if(im == NULL)
	{
		image = CreatePixelManipulateImageBits(PIXEL_MANIPULATE_FORMAT_A8R8G8B8, width, height, NULL, 0,
					&(((GRAPHICS*)pattern->base.graphics)->pixel_manipulate));
	}
	else
	{
		if(PixelManipulateImageInitializeWithBits(im, PIXEL_MANIPULATE_FORMAT_A8R8G8B8, width, height, NULL, 0, FALSE,
			&(((GRAPHICS*)pattern->base.graphics)->pixel_manipulate)) == FALSE)
		{
			return NULL;
		}
		image = im;
	}
	if (UNLIKELY(image == NULL))
		return NULL;

	GraphicsMeshPatternRasterize(pattern,
		PixelManipulateImageGetData(image),
		width, height,
		image->bits.row_stride * sizeof(uint32), // pixman_image_get_stride (image),
		*tx, *ty);
	return image;
}

struct acquire_source_cleanup
{
	GRAPHICS_SURFACE *surface;
	GRAPHICS_IMAGE_SURFACE *image;
	void *image_extra;
};

static void _acquire_source_cleanup(PIXEL_MANIPULATE_IMAGE* pixman_image, void* closure)
{
	struct acquire_source_cleanup *data = closure;

	GraphicsSurfaceReleaseSourceImage(data->surface,
		data->image,
		data->image_extra);
	MEM_FREE_FUNC(data);
}

static void _defer_free_cleanup(PIXEL_MANIPULATE_IMAGE* pixman_image, void* closure)
{
	DestroyGraphicsSurface(closure);
}

static uint16 expand_channel(uint16 v, uint32 bits)
{
	int offset = 16 - bits;
	while (offset > 0) {
		v |= v >> bits;
		offset -= bits;
		bits += bits;
	}
	return v;
}

static PIXEL_MANIPULATE_IMAGE* PixelManipulateToSolid(GRAPHICS_IMAGE_SURFACE* image,  PIXEL_MANIPULATE_IMAGE* im, int x, int y)
{
	uint32 pixel;
	PIXEL_MANIPULATE_COLOR color;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	switch (image->format) {
	default:
	case GRAPHICS_FORMAT_INVALID:
		return NULL;

	case GRAPHICS_FORMAT_A1:
		pixel = *(uint8*) (image->data + y * image->stride + x/8);
		return pixel & (1 << (x&7))
			? PixelManipulateBlackImage(im, image->compositor->graphics) : PixelManipulateTransparentImage(im, image->compositor->graphics);

	case GRAPHICS_FORMAT_A8:
		color.alpha = *(uint8*)(image->data + y * image->stride + x);
		color.alpha |= color.alpha << 8;
		if (color.alpha == 0)
			return PixelManipulateTransparentImage(im, image->compositor->graphics);
		if (color.alpha == 0xffff)
			return PixelManipulateBlackImage(im, image->compositor->graphics);

		color.red = color.green = color.blue = 0;
		return PixelManipulateImageCreateSolidFill(&color);

	/*
	case CAIRO_FORMAT_RGB16_565:
		pixel = *(uint16 *) (image->data + y * image->stride + 2 * x);
		if (pixel == 0)
			return PixelManipulateBlackImage ();
		if (pixel == 0xffff)
			return PixelManipulateWhiteImage ();

		color.alpha = 0xffff;
		color.red = expand_channel ((pixel >> 11 & 0x1f) << 11, 5);
		color.green = expand_channel ((pixel >> 5 & 0x3f) << 10, 6);
		color.blue = expand_channel ((pixel & 0x1f) << 11, 5);
		return PixelManipulateImageCreateSolidFill(&color);

	case CAIRO_FORMAT_RGB30:
		pixel = *(uint32 *) (image->data + y * image->stride + 4 * x);
		pixel &= 0x3fffffff; /* ignore alpha bits */
		/*
		if (pixel == 0)
			return PixelManipulateBlackImage ();
		if (pixel == 0x3fffffff)
			return PixelManipulateWhiteImage ();
		*/
		/* convert 10bpc to 16bpc */
		/*
		color.alpha = 0xffff;
		color.red = expand_channel((pixel >> 20) & 0x3fff, 10);
		color.green = expand_channel((pixel >> 10) & 0x3fff, 10);
		color.blue = expand_channel(pixel & 0x3fff, 10);
		return PixelManipulateImageCreateSolidFill(&color);
	*/
	case GRAPHICS_FORMAT_ARGB32:
	case GRAPHICS_FORMAT_RGB24:
		pixel = *(uint32 *) (image->data + y * image->stride + 4 * x);
		color.alpha = image->format == GRAPHICS_FORMAT_ARGB32 ? (pixel >> 24) | (pixel >> 16 & 0xff00) : 0xffff;
		if (color.alpha == 0)
			return PixelManipulateTransparentImage(im, image->compositor->graphics);
		if (pixel == 0xffffffff)
			return PixelManipulateWhiteImage(im, image->compositor->graphics);
		if (color.alpha == 0xffff && (pixel & 0xffffff) == 0)
			return PixelManipulateBlackImage(im, image->compositor->graphics);

		color.red = (pixel >> 16 & 0xff) | (pixel >> 8 & 0xff00);
		color.green = (pixel >> 8 & 0xff) | (pixel & 0xff00);
		color.blue = (pixel & 0xff) | (pixel << 8 & 0xff00);
		if(im == NULL)
		{
			return PixelManipulateImageCreateSolidFill(&color);
		}
		else
		{
			InitializePixelManipulateImageSolidFill(im, &color);
		}
	}
	return im;
}

/* ========================================================================== */

/* Index into filter table */
typedef enum
{
	KERNEL_IMPULSE,
	KERNEL_BOX,
	KERNEL_LINEAR,
	KERNEL_MITCHELL,
	KERNEL_NOTCH,
	KERNEL_CATMULL_ROM,
	KERNEL_LANCZOS3,
	KERNEL_LANCZOS3_STRETCHED,
	KERNEL_TENT
} kernel_t;

/* Produce contribution of a filter of size r for pixel centered on x.
For a typical low-pass function this evaluates the function at x/r.
If the frequency is higher than 1/2, such as when r is less than 1,
this may need to integrate several samples, see cubic for examples.
*/
typedef double (* kernel_func_t) (double x, double r);

/* Return maximum number of pixels that will be non-zero. Except for
impluse this is the maximum of 2 and the width of the non-zero part
of the filter rounded up to the next integer.
*/
typedef int (* kernel_width_func_t) (double r);

/* Table of filters */
typedef struct
{
	kernel_t		kernel;
	kernel_func_t	func;
	kernel_width_func_t	width;
} filter_info_t;

/* PIXMAN_KERNEL_IMPULSE: Returns pixel nearest the center.  This
matches PIXEL_MANIPULATE_FILTER_TYPE_NEAREST. This is useful if you wish to
combine the result of nearest in one direction with another filter
in the other.
*/

static double impulse_kernel (double x, double r)
{
	return 1;
}

static int impulse_width (double r)
{
	return 1;
}

/* PIXMAN_KERNEL_BOX: Intersection of a box of width r with square
pixels. This is the smallest possible filter such that the output
image contains an equal contribution from all the input
pixels. Lots of software uses this. The function is a trapazoid of
width r+1, not a box.

When r == 1.0, PIXMAN_KERNEL_BOX, PIXMAN_KERNEL_LINEAR, and
PIXMAN_KERNEL_TENT all produce the same filter, allowing
them to be exchanged at this point.
*/

static double box_kernel(double x, double r)
{
	return MAXIMUM(0.0, MINIMUM(MINIMUM(r, 1.0),
		MINIMUM((r + 1) / 2 - x, (r + 1) / 2 + x)));
}

static int box_width(double r)
{
	return r < 1.0 ? 2 : ceil(r + 1);
}

/* PIXMAN_KERNEL_LINEAR: Weighted sum of the two pixels nearest the
center, or a triangle of width 2. This matches
PIXEL_MANIPULATE_FILTER_TYPE_BILINEAR. This is useful if you wish to combine the
result of bilinear in one direction with another filter in the
other.  This is not a good filter if r > 1. You may actually want
PIXEL_MANIPULATE_FILTER_TYPE_TENT.

When r == 1.0, PIXMAN_KERNEL_BOX, PIXMAN_KERNEL_LINEAR, and
PIXMAN_KERNEL_TENT all produce the same filter, allowing
them to be exchanged at this point.
*/

static double linear_kernel(double x, double r)
{
	return MAXIMUM(1.0 - fabs(x), 0.0);
}

static INLINE int linear_width(double r)
{
	return 2;
}

/* Cubic functions described in the Mitchell-Netravali paper.
http://mentallandscape.com/Papers_siggraph88.pdf. This describes
all possible cubic functions that can be used for sampling.
*/

static double general_cubic(double x, double r, double B, double C)
{
	double ax;
	if (r < 1.0)
		return
		general_cubic(x * 2 - .5, r * 2, B, C) +
		general_cubic(x * 2 + .5, r * 2, B, C);

	ax = fabs (x / r);

	if (ax < 1)
	{
		return (((12 - 9 * B - 6 * C) * ax +
			(-18 + 12 * B + 6 * C)) * ax * ax +
			(6 - 2 * B)) / 6;
	}
	else if (ax < 2)
	{
		return ((((-B - 6 * C) * ax +
			(6 * B + 30 * C)) * ax +
			(-12 * B - 48 * C)) * ax +
			(8 * B + 24 * C)) / 6;
	}
	else
	{
		return 0.0;
	}
}

static int cubic_width(double r)
{
	return MAXIMUM(2, ceil (r * 4));
}

/* PIXMAN_KERNEL_CATMULL_ROM: Catmull-Rom interpolation. Often called
"cubic interpolation", "b-spline", or just "cubic" by other
software. This filter has negative values so it can produce ringing
and output pixels outside the range of input pixels. This is very
close to lanczos2 so there is no reason to supply that as well.
*/

static double cubic_kernel(double x, double r)
{
	return general_cubic(x, r, 0.0, 0.5);
}

/* PIXMAN_KERNEL_MITCHELL: Cubic recommended by the Mitchell-Netravali
paper.  This has negative values and because the values at +/-1 are
not zero it does not interpolate the pixels, meaning it will change
an image even if there is no translation.
*/

static double mitchell_kernel(double x, double r)
{
	return general_cubic(x, r, 1/3.0, 1/3.0);
}

/* PIXMAN_KERNEL_NOTCH: Cubic recommended by the Mitchell-Netravali
paper to remove postaliasing artifacts. This does not remove
aliasing already present in the source image, though it may appear
to due to it's excessive blurriness. In any case this is more
useful than gaussian for image reconstruction.
*/

static double notch_kernel(double x, double r)
{
	return general_cubic(x, r, 1.5, -0.25);
}

/* PIXMAN_KERNEL_LANCZOS3: lanczos windowed sinc function from -3 to
+3. Very popular with high-end software though I think any
advantage over cubics is hidden by quantization and programming
mistakes. You will see LANCZOS5 or even 7 sometimes.
*/

static double sinc(double x)
{
	return x ? sin(M_PI * x) / (M_PI * x) : 1.0;
}

static double lanczos(double x, double n)
{
	return fabs (x) < n ? sinc (x) * sinc (x * (1.0 / n)) : 0.0;
}

static double
lanczos3_kernel (double x, double r)
{
	if (r < 1.0)
		return
		lanczos3_kernel (x * 2 - .5, r * 2) +
		lanczos3_kernel (x * 2 + .5, r * 2);
	else
		return lanczos(x / r, 3.0);
}

static int
lanczos3_width (double r)
{
	return MAXIMUM(2, ceil (r * 6));
}

/* PIXMAN_KERNEL_LANCZOS3_STRETCHED - The LANCZOS3 kernel widened by
4/3.  Recommended by Jim Blinn
http://graphics.cs.cmu.edu/nsp/course/15-462/Fall07/462/papers/jaggy.pdf
*/

static double nice_kernel(double x, double r)
{
	return lanczos3_kernel(x, r * (4.0/3));
}

static int nice_width(double r)
{
	return MAXIMUM(2.0, ceil (r * 8));
}

/* PIXMAN_KERNEL_TENT: Triangle of width 2r. Lots of software uses
this as a "better" filter, twice the size of a box but smaller than
a cubic.

When r == 1.0, PIXMAN_KERNEL_BOX, PIXMAN_KERNEL_LINEAR, and
PIXMAN_KERNEL_TENT all produce the same filter, allowing
them to be exchanged at this point.
*/

static double tent_kernel(double x, double r)
{
	if(r < 1.0)
	{
		return box_kernel(x, r);
	}
	return MAXIMUM(1.0 - fabs(x / r), 0.0);
}

static int tent_width(double r)
{
	return r < 1.0 ? 2 : ceil(2 * r);
}


static const filter_info_t filters[] =
{
	{ KERNEL_IMPULSE,		impulse_kernel,   impulse_width },
	{ KERNEL_BOX,		box_kernel,	   box_width },
	{ KERNEL_LINEAR,		linear_kernel,	linear_width },
	{ KERNEL_MITCHELL,		mitchell_kernel,  cubic_width },
	{ KERNEL_NOTCH,		notch_kernel,	 cubic_width },
	{ KERNEL_CATMULL_ROM,	cubic_kernel,	 cubic_width },
	{ KERNEL_LANCZOS3,		lanczos3_kernel,  lanczos3_width },
	{ KERNEL_LANCZOS3_STRETCHED,nice_kernel,	  nice_width },
	{ KERNEL_TENT,		tent_kernel,	  tent_width }
};

/* Fills in one dimension of the filter array */
static void get_filter(
	kernel_t filter,
	double r,
	int width,
	int subsample,
	PIXEL_MANIPULATE_FIXED* out
)
{
	int i;
	PIXEL_MANIPULATE_FIXED *p = out;
	int n_phases = 1 << subsample;
	double step = 1.0 / n_phases;
	kernel_func_t func = filters[filter].func;

	/* special-case the impulse filter: */
	if (width <= 1)
	{
		for (i = 0; i < n_phases; ++i)
			*p++ = PIXEL_MANIPULATE_FIXED_1;
		return;
	}

	for (i = 0; i < n_phases; ++i)
	{
		double frac = (i + .5) * step;
		/* Center of left-most pixel: */
		double x1 = ceil (frac - width / 2.0 - 0.5) - frac + 0.5;
		double total = 0;
		PIXEL_MANIPULATE_FIXED new_total = 0;
		int j;
		
		for (j = 0; j < width; ++j)
		{
			double v = func(x1 + j, r);
			total += v;
			p[j] = PIXEL_MANIPULATE_FLOAT_TO_FIXED(v);
		}

		/* Normalize */
		total = 1 / total;
		for (j = 0; j < width; ++j)
			new_total += (p[j] *= total);

		/* Put any error on center pixel */
		p[width / 2] += (PIXEL_MANIPULATE_FIXED_1 - new_total);

		p += width;
	}
}


/* Create the parameter list for a SEPARABLE_CONVOLUTION filter
* with the given kernels and scale parameters. 
*/
static PIXEL_MANIPULATE_FIXED* create_separable_convolution(
	int* n_values,
	kernel_t xfilter,
	double sx,
	kernel_t yfilter,
	double sy
)
{
	int xwidth, xsubsample, ywidth, ysubsample, size_x, size_y;
	PIXEL_MANIPULATE_FIXED *params;

	xwidth = filters[xfilter].width(sx);
	xsubsample = 0;
	if (xwidth > 1)
		while (sx * (1 << xsubsample) <= 128.0) xsubsample++;
	size_x = (1 << xsubsample) * xwidth;

	ywidth = filters[yfilter].width(sy);
	ysubsample = 0;
	if (ywidth > 1)
		while (sy * (1 << ysubsample) <= 128.0) ysubsample++;
	size_y = (1 << ysubsample) * ywidth;

	*n_values = 4 + size_x + size_y;
	params = MEM_ALLOC_FUNC(*n_values * sizeof (PIXEL_MANIPULATE_FIXED));
	if (!params) return 0;

	params[0] = PIXEL_MANIPULATE_INTEGER_TO_FIXED(xwidth);
	params[1] = PIXEL_MANIPULATE_INTEGER_TO_FIXED(ywidth);
	params[2] = PIXEL_MANIPULATE_INTEGER_TO_FIXED(xsubsample);
	params[3] = PIXEL_MANIPULATE_INTEGER_TO_FIXED(ysubsample);

	get_filter(xfilter, sx, xwidth, xsubsample, params + 4);
	get_filter(yfilter, sy, ywidth, ysubsample, params + 4 + size_x);

	return params;
}

/* ========================================================================== */

static int PixelManipulateImageSetProperties(
	PIXEL_MANIPULATE_IMAGE* pixman_image,
	const GRAPHICS_PATTERN* pattern,
	const GRAPHICS_RECTANGLE_INT* extents,
	int* ix,int* iy
)
{
	PIXEL_MANIPULATE_TRANSFORM pixman_transform;
	eGRAPHICS_INTEGER_STATUS status;

	status = GraphicsMatrixToPixelManipulateMatrixOffset(&pattern->matrix,
		pattern->filter,
		extents->x + extents->width/2.,
		extents->y + extents->height/2.,
		&pixman_transform, ix, iy);
	if (status == GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO)
	{
		/* If the transform is an identity, we don't need to set it
		* and we can use any filtering, so choose the fastest one. */
		PixelManipulateImageSetFilter(pixman_image, PIXEL_MANIPULATE_FILTER_TYPE_NEAREST, NULL, 0);
	}
	else if (UNLIKELY(status != GRAPHICS_INTEGER_STATUS_SUCCESS ||
		! PixelManipulateImageSetTransform(pixman_image,
			&pixman_transform)))
	{
		return FALSE;
	}
	else
	{
		ePIXEL_MANIPULATE_FILTER_TYPE graphics_filter;
		kernel_t kernel;
		double dx, dy;

		/* Compute scale factors from the pattern matrix. These scale
		* factors are from user to pattern space, and as such they
		* are greater than 1.0 for downscaling and less than 1.0 for
		* upscaling. The factors are the size of an axis-aligned
		* rectangle with the same area as the parallelgram a 1x1
		* square transforms to.
		*/
		dx = hypot (pattern->matrix.xx, pattern->matrix.xy);
		dy = hypot (pattern->matrix.yx, pattern->matrix.yy);

		/* Clip at maximum pixman_fixed number. Besides making it
		* passable to pixman, this avoids errors from inf and nan.
		*/
		if (! (dx < 0x7FFF)) dx = 0x7FFF;
		if (! (dy < 0x7FFF)) dy = 0x7FFF;

		switch (pattern->filter) {
		case GRAPHICS_FILTER_FAST:
			graphics_filter = PIXEL_MANIPULATE_FILTER_TYPE_FAST;
			break;
		case GRAPHICS_FILTER_GOOD:
			graphics_filter = PIXEL_MANIPULATE_FILTER_TYPE_SEPARABLE_CONVOLUTION;
			kernel = KERNEL_BOX;
			/* Clip the filter size to prevent extreme slowness. This
			value could be raised if 2-pass filtering is done */
			if (dx > 16.0) dx = 16.0;
			if (dy > 16.0) dy = 16.0;
			/* Match the bilinear filter for scales > .75: */
			if (dx < 1.0/0.75) dx = 1.0;
			if (dy < 1.0/0.75) dy = 1.0;
			break;
		case GRAPHICS_FILTER_BEST:
			graphics_filter = PIXEL_MANIPULATE_FILTER_TYPE_SEPARABLE_CONVOLUTION;
			kernel = KERNEL_CATMULL_ROM; /* LANCZOS3 is better but not much */
										 /* Clip the filter size to prevent extreme slowness. This
										 value could be raised if 2-pass filtering is done */
			if (dx > 16.0) { dx = 16.0; kernel = KERNEL_BOX; }
			/* blur up to 2x scale, then blend to square pixels for larger: */
			else if (dx < 1.0) {
				if (dx < 1.0/128) dx = 1.0/127;
				else if (dx < 0.5) dx = 1.0 / (1.0 / dx - 1.0);
				else dx = 1.0;
			}
			if (dy > 16.0) { dy = 16.0; kernel = KERNEL_BOX; }
			else if (dy < 1.0) {
				if (dy < 1.0/128) dy = 1.0/127;
				else if (dy < 0.5) dy = 1.0 / (1.0 / dy - 1.0);
				else dy = 1.0;
			}
			break;
		case GRAPHICS_FILTER_NEAREST:
			graphics_filter = PIXEL_MANIPULATE_FILTER_TYPE_NEAREST;
			break;
		case GRAPHICS_FILTER_BILINEAR:
			graphics_filter = PIXEL_MANIPULATE_FILTER_TYPE_BILINEAR;
			break;
		case GRAPHICS_FILTER_GAUSSIAN:
			/* XXX: The GAUSSIAN value has no implementation in cairo
			* whatsoever, so it was really a mistake to have it in the
			* API. We could fix this by officially deprecating it, or
			* else inventing semantics and providing an actual
			* implementation for it. */
		default:
			graphics_filter = PIXEL_MANIPULATE_FILTER_TYPE_BEST;
		}

		if (graphics_filter == PIXEL_MANIPULATE_FILTER_TYPE_SEPARABLE_CONVOLUTION) {
			int n_params;
			PIXEL_MANIPULATE_FIXED *params;
			params = create_separable_convolution
			(&n_params, kernel, dx, kernel, dy);
			PixelManipulateImageSetFilter(pixman_image, graphics_filter,
				params, n_params);
			MEM_FREE_FUNC(params);
		} else {
			PixelManipulateImageSetFilter(pixman_image, graphics_filter, NULL, 0);
		}
	}

	{
		ePIXEL_MANIPULATE_REPEAT_TYPE pixman_repeat;

		switch (pattern->extend) {
		default:
		case GRAPHICS_EXTEND_NONE:
			pixman_repeat = PIXEL_MANIPULATE_REPEAT_TYPE_NONE;
			break;
		case GRAPHICS_EXTEND_REPEAT:
			pixman_repeat = PIXEL_MANIPULATE_REPEAT_TYPE_NORMAL;
			break;
		case GRAPHICS_EXTEND_REFLECT:
			pixman_repeat = PIXEL_MANIPULATE_REPEAT_TYPE_REFLECT;
			break;
		case GRAPHICS_EXTEND_PAD:
			pixman_repeat = PIXEL_MANIPULATE_REPEAT_TYPE_PAD;
			break;
		}
		
		PixelManipulateImageSetRepeat(pixman_image, pixman_repeat);
	}

	if(pattern->has_component_alpha)
	{
		PixelManipulateImageSetComponentAlpha(pixman_image, TRUE);
	}

	return TRUE;
}

struct proxy {
	GRAPHICS_SURFACE base;
	GRAPHICS_SURFACE *image;
};

static eGRAPHICS_STATUS
proxy_acquire_source_image (void			 *abstract_surface,
	GRAPHICS_IMAGE_SURFACE	**image_out,
	void			**image_extra)
{
	struct proxy *proxy = abstract_surface;
	return GraphicsSurfaceAcquireSourceImage(proxy->image, image_out, image_extra);
}

static void
proxy_release_source_image (void			*abstract_surface,
	GRAPHICS_IMAGE_SURFACE	*image,
	void			*image_extra)
{
	struct proxy *proxy = abstract_surface;
	GraphicsSurfaceReleaseSourceImage(proxy->image, image, image_extra);
}

static eGRAPHICS_STATUS
proxy_finish (void *abstract_surface)
{
	return GRAPHICS_STATUS_SUCCESS;
}

static const GRAPHICS_SURFACE_BACKEND proxy_backend  = {
	GRAPHICS_INTERNAL_SURFACE_TYPE_NULL,
	proxy_finish,
	NULL,

	NULL, /* create similar */
	NULL, /* create similar image */
	NULL, /* map to image */
	NULL, /* unmap image */

	GraphicsSurfaceDefaultSource,
	proxy_acquire_source_image,
	proxy_release_source_image,
};

static GRAPHICS_SURFACE* attach_proxy(
	GRAPHICS_SURFACE* source,
	GRAPHICS_SURFACE* image
)
{
	struct proxy *proxy;

	proxy = MEM_ALLOC_FUNC(sizeof (*proxy));
	if (UNLIKELY(proxy == NULL))
		return NULL;
	
	InitializeGraphicsSurface(&proxy->base,
		&proxy_backend, image->content, FALSE, source->graphics);

	proxy->image = image;
	GraphicsSurfaceAttachSnapshot(source, &proxy->base, NULL);

	return &proxy->base;
}

static void detach_proxy(
	GRAPHICS_SURFACE* source,
	GRAPHICS_SURFACE* proxy
)
{
	GraphicsSurfaceFinish(proxy);
	//DestroyGraphicsSurface(proxy);
}

static GRAPHICS_SURFACE* get_proxy(GRAPHICS_SURFACE *proxy)
{
	return ((struct proxy *)proxy)->image;
}

static PIXEL_MANIPULATE_IMAGE* PixelManipulateImageForRecording(
	GRAPHICS_IMAGE_SURFACE* dst,
	const GRAPHICS_SURFACE_PATTERN* pattern,
	int is_mask,
	const GRAPHICS_RECTANGLE_INT* extents,
	const GRAPHICS_RECTANGLE_INT* sample,
	int* ix, int* iy,
	PIXEL_MANIPULATE_IMAGE* im,
	void* graphics
)
{
	GRAPHICS_SURFACE *source, *clone, *proxy;
	GRAPHICS_RECTANGLE_INT limit;
	GRAPHICS_RECTANGLE_INT src_limit;
	PIXEL_MANIPULATE_IMAGE *pixman_image;
	eGRAPHICS_STATUS status;
	eGRAPHICS_EXTENED extend;
	GRAPHICS_MATRIX *m, matrix;
	double sx = 1.0, sy = 1.0;
	int tx = 0, ty = 0;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	*ix = *iy = 0;
	
	source = GraphicsPatternGetSource(pattern, &limit);
	src_limit = limit;
	
	extend = pattern->base.extend;
	if (GraphicsRectangleContainsRectangle(&limit, sample))
		extend = GRAPHICS_EXTEND_NONE;

	if (extend == GRAPHICS_EXTEND_NONE) {
		if (! GraphicsRectangleIntersect(&limit, sample))
			return PixelManipulateTransparentImage(im, dst->compositor->graphics);
	}
	
	if (! GRAPHICS_MATRIX_IS_IDENTITY(&pattern->base.matrix)) {
		double x1, y1, x2, y2;

		matrix = pattern->base.matrix;
		status = GraphicsMatrixInvert(&matrix);
		assert (status == GRAPHICS_STATUS_SUCCESS);

		x1 = limit.x;
		y1 = limit.y;
		x2 = limit.x + limit.width;
		y2 = limit.y + limit.height;

		GraphicsMatrixTransformBoundingBox(&matrix,
			&x1, &y1, &x2, &y2, NULL);

		limit.x = floor (x1);
		limit.y = floor (y1);
		limit.width  = ceil (x2) - limit.x;
		limit.height = ceil (y2) - limit.y;
		sx = (double)src_limit.width / limit.width;
		sy = (double)src_limit.height / limit.height;
	}
	tx = limit.x;
	ty = limit.y;
	
	/* XXX transformations! */
	proxy = GraphicsSurfaceHasSnapshot(source, &proxy_backend);
	if(proxy != NULL) {
		clone = GraphicsSurfaceReference(get_proxy (proxy));
		goto done;
	}

	if(is_mask)
	{
		clone = GraphicsImageSurfaceCreate(GRAPHICS_FORMAT_A8,
			limit.width, limit.height, graphics);
	}
	else
	{
		if (dst->base.content == source->content)
			clone = GraphicsImageSurfaceCreate(dst->format,
				limit.width, limit.height, graphics);
		else
			clone = GraphicsImageSurfaceCreateWithContent(source->content,
				limit.width, limit.height, graphics);
	}

	m = NULL;
	if(extend == GRAPHICS_EXTEND_NONE)
	{
		matrix = pattern->base.matrix;
		if(tx | ty)
		{
			GraphicsMatrixTranslate(&matrix, tx, ty);
		}
		m = &matrix;
	}
	else
	{
		InitializeGraphicsMatrixScale(&matrix, sx, sy);
		GraphicsMatrixTranslate(&matrix, src_limit.x/sx, src_limit.y/sy);
		m = &matrix;
	}

	/* Handle recursion by returning future reads from the current image */
	proxy = attach_proxy (source, clone);
	// status = _cairo_recording_surface_replay_with_clip (source, m, clone, NULL);
	detach_proxy (source, proxy);
	if(UNLIKELY(status))
	{
		DestroyGraphicsSurface(clone);
		return NULL;
	}
	
done:
	if(im == NULL)
	{
		pixman_image = PixelManipulateImageReference(&((GRAPHICS_IMAGE_SURFACE *)clone)->image);
	}
	else
	{
		pixman_image = im;
	}
	DestroyGraphicsSurface(clone);

	if(extend == GRAPHICS_EXTEND_NONE) {
		*ix = -limit.x;
		*iy = -limit.y;
	}
	else
	{
		GRAPHICS_PATTERN_UNION tmp_pattern;
		InitializeGraphicsPatternStaticCopy(&tmp_pattern.base, &pattern->base);
		matrix = pattern->base.matrix;
		status = GraphicsMatrixInvert(&matrix);
		assert (status == GRAPHICS_STATUS_SUCCESS);
		GraphicsMatrixTranslate(&matrix, src_limit.x, src_limit.y);
		GraphicsMatrixScale(&matrix, sx, sy);
		status = GraphicsMatrixInvert(&matrix);
		assert (status == GRAPHICS_STATUS_SUCCESS);
		GraphicsPatternSetMatrix(&tmp_pattern.base, &matrix);
		if (! PixelManipulateImageSetProperties (pixman_image,
			&tmp_pattern.base, extents,
			ix, iy))
		{
			PixelManipulateImageUnreference(pixman_image);
			pixman_image= NULL;
		}
	}
	
	return pixman_image;
}

static PIXEL_MANIPULATE_IMAGE* PixelManipulateImageForSurface(
	GRAPHICS_IMAGE_SURFACE *dst,
	const GRAPHICS_SURFACE_PATTERN *pattern,
	int is_mask,
	const GRAPHICS_RECTANGLE_INT *extents,
	const GRAPHICS_RECTANGLE_INT *sample,
	int *ix, int *iy,
	PIXEL_MANIPULATE_IMAGE* im,
	void* graphics
)
{
	eGRAPHICS_EXTENED extend = pattern->base.extend;
	PIXEL_MANIPULATE_IMAGE *pixman_image = NULL;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	*ix = *iy = 0;
	if (pattern->surface->type == GRAPHICS_SURFACE_TYPE_RECORDING)
		return PixelManipulateImageForRecording(dst, pattern,
			is_mask, extents, sample,
			ix, iy, im, graphics);

	if (pattern->surface->type == GRAPHICS_SURFACE_TYPE_IMAGE &&
		(! is_mask || ! pattern->base.has_component_alpha ||
		(pattern->surface->content & GRAPHICS_CONTENT_COLOR) == 0))
	{
		GRAPHICS_SURFACE *defer_free = NULL;
		GRAPHICS_IMAGE_SURFACE *source = (GRAPHICS_IMAGE_SURFACE *) pattern->surface;
		eGRAPHICS_SURFACE_TYPE type;

		if (GraphicsSurfaceIsSnapshot(&source->base)) {
			defer_free = GraphicsSurfaceSnapshotGetTarget(&source->base);
			source = (GRAPHICS_IMAGE_SURFACE *) defer_free;
		}

		type = source->base.backend->type;
		if (type == GRAPHICS_SURFACE_TYPE_IMAGE) {
			if (extend != GRAPHICS_EXTEND_NONE &&
				sample->x >= 0 &&
				sample->y >= 0 &&
				sample->x + sample->width  <= source->width &&
				sample->y + sample->height <= source->height)
			{
				extend = GRAPHICS_EXTEND_NONE;
			}

			if (sample->width == 1 && sample->height == 1)
			{
				if (sample->x < 0 ||
					sample->y < 0 ||
					sample->x >= source->width ||
					sample->y >= source->height)
				{
					if (extend == GRAPHICS_EXTEND_NONE)
					{
						DestroyGraphicsSurface(defer_free);
						return PixelManipulateTransparentImage(im, dst->compositor->graphics);
					}
				}
				else
				{
					pixman_image = PixelManipulateToSolid (source,
						im, sample->x, sample->y);
					if(pixman_image)
					{
						DestroyGraphicsSurface(defer_free);
						return pixman_image;
					}
				}
			}

#if PIXMAN_HAS_ATOMIC_OPS
			/* avoid allocating a 'pattern' image if we can reuse the original */
			if (extend == GRAPHICS_EXTEND_NONE &&
				_cairo_matrix_is_pixman_translation (&pattern->base.matrix,
					pattern->base.filter,
					ix, iy))
			{
				DestroyGraphicsSurface(defer_free);
				return PixelManipulateImageReference(source->pixman_image);
			}
#endif

			if(im == NULL)
			{
				pixman_image = CreatePixelManipulateImageBits(source->pixel_format,
					source->width,
					source->height,
					(uint32 *) source->data,
					source->stride, &(((GRAPHICS*)graphics)->pixel_manipulate)
				);
			}
			else
			{
				(void)PixelManipulateImageInitializeWithBits(im, source->pixel_format,
					source->width,
					source->height,
					(uint32*)source->data,
					source->stride, FALSE, &(((GRAPHICS*)graphics)->pixel_manipulate)
				);
				pixman_image = im;
			}
			if(UNLIKELY(pixman_image == NULL))
			{
				DestroyGraphicsSurface(defer_free);
				return NULL;
			}

			if(defer_free)
			{
				PixelManipulateSetDestroyFunction(pixman_image,
					_defer_free_cleanup,
					defer_free);
			}
		}
		else if(type == GRAPHICS_SURFACE_TYPE_SUBSURFACE)
		{
			GRAPHICS_SUBSURFACE *sub;
			int is_contained = FALSE;

			sub = (GRAPHICS_SUBSURFACE*)source;
			source = (GRAPHICS_IMAGE_SURFACE*)sub->target;

			if (sample->x >= 0 &&
				sample->y >= 0 &&
				sample->x + sample->width  <= sub->extents.width &&
				sample->y + sample->height <= sub->extents.height)
			{
				is_contained = TRUE;
			}

			if (sample->width == 1 && sample->height == 1)
			{
				if (is_contained)
				{
					pixman_image = PixelManipulateToSolid (source, im,
						sub->extents.x + sample->x,
						sub->extents.y + sample->y);
					if (pixman_image)
						return pixman_image;
				}
				else
				{
					if (extend == GRAPHICS_EXTEND_NONE)
						return PixelManipulateTransparentImage(im, dst->compositor->graphics);
				}
			}

#if PIXMAN_HAS_ATOMIC_OPS
			*ix = sub->extents.x;
			*iy = sub->extents.y;
			if (is_contained &&
				_cairo_matrix_is_pixman_translation (&pattern->base.matrix,
					pattern->base.filter,
					ix, iy))
			{
				return PixelManipulateImageReference(source->pixman_image);
			}
#endif
			
			/* Avoid sub-byte offsets, force a copy in that case. */
			if (PIXEL_MANIPULATE_BIT_PER_PIXEL(source->pixel_format) >= 8) {
				if (is_contained) {
					void *data = source->data
						+ sub->extents.x * PIXEL_MANIPULATE_BIT_PER_PIXEL(source->pixel_format)/8
						+ sub->extents.y * source->stride;
					pixman_image = CreatePixelManipulateImageBits(source->pixel_format,
						sub->extents.width,
						sub->extents.height,
						data,
						source->stride, &(((GRAPHICS*)graphics)->pixel_manipulate)
					);
					if (UNLIKELY(pixman_image == NULL))
						return NULL;
				} else {
					/* XXX for a simple translation and EXTEND_NONE we can
					* fix up the pattern matrix instead.
					*/
				}
			}
		}
	}

	if (pixman_image == NULL) {
		struct acquire_source_cleanup *cleanup;
		GRAPHICS_IMAGE_SURFACE *image;
		void *extra;
		eGRAPHICS_STATUS status;

		status = GraphicsSurfaceAcquireSourceImage(pattern->surface, &image, &extra);
		if (UNLIKELY(status))
			return NULL;

		if(im == NULL)
		{
			pixman_image = CreatePixelManipulateImageBits(image->pixel_format,
				image->width,
				image->height,
				(uint32 *) image->data,
				image->stride,
				&(((GRAPHICS*)graphics)->pixel_manipulate)
			);
		}
		else
		{
			PixelManipulateImageInitializeWithBits(im, image->pixel_format,
				image->width,
				image->height,
				(uint32*)image->data,
				image->stride,
				FALSE,
				&(((GRAPHICS*)graphics)->pixel_manipulate)
			);
			pixman_image = im;
		}
		if (UNLIKELY(pixman_image == NULL)) {
			GraphicsSurfaceReleaseSourceImage(pattern->surface, image, extra);
			return NULL;
		}

		cleanup = MEM_ALLOC_FUNC(sizeof (*cleanup));
		if (UNLIKELY(cleanup == NULL)) {
			GraphicsSurfaceReleaseSourceImage(pattern->surface, image, extra);
			PixelManipulateImageUnreference(pixman_image);
			return NULL;
		}

		cleanup->surface = pattern->surface;
		cleanup->image = image;
		cleanup->image_extra = extra;
		PixelManipulateSetDestroyFunction(pixman_image,
			_acquire_source_cleanup, cleanup);
	}

	if (! PixelManipulateImageSetProperties (pixman_image,
		&pattern->base, extents,
		ix, iy)) {
		PixelManipulateImageUnreference(pixman_image);
		pixman_image= NULL;
	}

	return pixman_image;
}

struct raster_source_cleanup {
	const GRAPHICS_PATTERN *pattern;
	GRAPHICS_SURFACE *surface;
	GRAPHICS_IMAGE_SURFACE *image;
	void *image_extra;
};

static void _raster_source_cleanup(PIXEL_MANIPULATE_IMAGE *pixman_image,
	void *closure)
{
	struct raster_source_cleanup *data = closure;

	GraphicsSurfaceReleaseSourceImage(data->surface,
		data->image,
		data->image_extra);

	GraphicsRasterSourcePatternRelease(data->pattern,
		data->surface);

	MEM_FREE_FUNC(data);
}

static PIXEL_MANIPULATE_IMAGE* PixelManipulateImageForRaster(
	GRAPHICS_IMAGE_SURFACE *dst,
	const GRAPHICS_RASTER_SOURCE_PATTERN* pattern,
	int is_mask,
	const GRAPHICS_RECTANGLE_INT* extents,
	const GRAPHICS_RECTANGLE_INT* sample,
	int *ix, int *iy
)
{
	PIXEL_MANIPULATE_IMAGE *pixman_image;
	struct raster_source_cleanup *cleanup;
	GRAPHICS_IMAGE_SURFACE *image;
	void *extra;
	eGRAPHICS_STATUS status;
	GRAPHICS_SURFACE *surface;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	*ix = *iy = 0;

	surface = GraphicsRasterSourcePatternAcquire(&pattern->base,
		&dst->base, NULL);
	if (UNLIKELY(surface == NULL || surface->status))
		return NULL;

	status = GraphicsSurfaceAcquireSourceImage(surface, &image, &extra);
	if (UNLIKELY(status)) {
		GraphicsRasterSourcePatternRelease(&pattern->base, surface);
		return NULL;
	}

	assert (image->width == pattern->extents.width);
	assert (image->height == pattern->extents.height);

	pixman_image = CreatePixelManipulateImageBits(image->pixel_format,
		image->width,
		image->height,
		(uint32 *) image->data,
		image->stride,
		&(((GRAPHICS*)dst->base.graphics)->pixel_manipulate)
	);
	if (UNLIKELY(pixman_image == NULL)) {
		GraphicsSurfaceReleaseSourceImage(surface, image, extra);
		GraphicsRasterSourcePatternRelease(&pattern->base, surface);
		return NULL;
	}

	cleanup = MEM_ALLOC_FUNC(sizeof (*cleanup));
	if (UNLIKELY(cleanup == NULL)) {
		PixelManipulateImageUnreference(pixman_image);
		GraphicsSurfaceReleaseSourceImage(surface, image, extra);
		GraphicsRasterSourcePatternRelease(&pattern->base, surface);
		return NULL;
	}

	cleanup->pattern = &pattern->base;
	cleanup->surface = surface;
	cleanup->image = image;
	cleanup->image_extra = extra;
	PixelManipulateSetDestroyFunction(pixman_image,
		_raster_source_cleanup, cleanup);

	if (! PixelManipulateImageSetProperties (pixman_image,
		&pattern->base, extents,
		ix, iy)) {
		PixelManipulateImageUnreference(pixman_image);
		pixman_image= NULL;
	}

	return pixman_image;
}

PIXEL_MANIPULATE_IMAGE* PixelManipulateImageForPattern(
	GRAPHICS_IMAGE_SURFACE* dst,
	const GRAPHICS_PATTERN* pattern,
	int is_mask,
	const GRAPHICS_RECTANGLE_INT* extents,
	const GRAPHICS_RECTANGLE_INT* sample,
	int* tx, int* ty,
	PIXEL_MANIPULATE_IMAGE* image,
	void* graphics
)
{
	*tx = *ty = 0;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	if (pattern == NULL)
		return PixelManipulateWhiteImage(image, pattern->graphics);

	switch (pattern->type) {
	default:
		break;
	case GRAPHICS_PATTERN_TYPE_SOLID:
		return PixelManipulateImageForColor(&((const GRAPHICS_SOLID_PATTERN*)pattern)->color, image, pattern->graphics);

	case GRAPHICS_PATTERN_TYPE_RADIAL:
	case GRAPHICS_PATTERN_TYPE_LINEAR:
		return PixelManipulateImageForGradient ((const GRAPHICS_GRADIENT_PATTERN *)pattern,
			extents, tx, ty, image);

	case GRAPHICS_PATTERN_TYPE_MESH:
		return PixelManipulateImageForMesh ((const GRAPHICS_MESH_PATTERN*)pattern,
			extents, tx, ty, image);

	case GRAPHICS_PATTERN_TYPE_SURFACE:
		return PixelManipulateImageForSurface (dst,
			(const GRAPHICS_SURFACE_PATTERN *) pattern,
			is_mask, extents, sample,
			tx, ty, image, graphics);

	case GRAPHICS_PATTERN_TYPE_RASTER_SOURCE:
		return PixelManipulateImageForRaster (dst,
			(const GRAPHICS_RASTER_SOURCE_PATTERN*) pattern,
			is_mask, extents, sample,
			tx, ty);
	}
	return NULL;
}

static eGRAPHICS_STATUS  GraphicsImageSourceFinish(void *abstract_surface)
{
	GRAPHICS_IMAGE_SOURCE *source = abstract_surface;

	PixelManipulateImageUnreference(&source->image);
	return GRAPHICS_STATUS_SUCCESS;
}

void InitializeGraphicsImageSourceBackend(GRAPHICS_SURFACE_BACKEND* backend)
{
	const GRAPHICS_SURFACE_BACKEND image_source_backend =
	{
		GRAPHICS_SURFACE_TYPE_IMAGE,
		GraphicsImageSourceFinish,
		NULL
	};

	*backend = image_source_backend;
}

GRAPHICS_SURFACE* GraphicsImageSourceCreateForPattern(
	GRAPHICS_SURFACE* dst,
	const GRAPHICS_PATTERN* pattern,
	int is_mask,
	const GRAPHICS_RECTANGLE_INT* extents,
	const GRAPHICS_RECTANGLE_INT* sample,
	int* src_x,
	int* src_y,
	void* graphics
)
{
	GRAPHICS_IMAGE_SOURCE *source;

	TRACE ((stderr, "%s\n", __FUNCTION__));

	source = MEM_ALLOC_FUNC(sizeof(GRAPHICS_IMAGE_SOURCE));
	if (UNLIKELY(source == NULL))
		return NULL;
	(void)memset(source, 0, sizeof(*source));

	{
		PIXEL_MANIPULATE_IMAGE *valid_image;
		valid_image =
		PixelManipulateImageForPattern ((GRAPHICS_IMAGE_SURFACE *)dst,
			pattern, is_mask,
			extents, sample,
			src_x, src_y, &source->image, graphics);
		if(UNLIKELY(valid_image == NULL))
		{
			MEM_FREE_FUNC(source);
			return NULL;
		}
	}

	InitializeGraphicsSurface(&source->base,
		&((GRAPHICS*)graphics)->image_source_backend,
		GRAPHICS_CONTENT_COLOR_ALPHA,
		FALSE, /* is_vector */
		graphics
	);

	source->opaque_solid =
		pattern == NULL || GraphicsPatternIsOpaqueSolid(pattern);

	return &source->base;
}
