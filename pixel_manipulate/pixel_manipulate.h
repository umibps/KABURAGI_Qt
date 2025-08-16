#ifndef _INCLUDED_PIXEL_MANIPULATE_H_
#define _INCLUDED_PIXEL_MANIPULATE_H_

#include "types.h"
#include "memory.h"
#include "pixel_manipulate_format.h"
#include "pixel_manipulate_operator.h"
#include "pixel_manipulate_composite.h"

typedef int32 PIXEL_MANIPULATE_FIXED_16_16;
typedef PIXEL_MANIPULATE_FIXED_16_16 PIXEL_MANIPULATE_FIXED;
typedef int64 PIXEL_MANIPULATE_FIXED_32_32;
typedef int64 PIXEL_MANIPULATE_FIXED_48_16;
#define PIXEL_MANIPULATE_FIXED_48_16MAX ((PIXEL_MANIPULATE_FIXED_48_16)0x7FFFFFFF)
#define PIXEL_MANIPULATE_FIXED_48_16MIN (-((PIXEL_MANIPULATE_FIXED_48_16) 1 << 31))

#define PIXEL_MANIPULATE_FIXED_E ((PIXEL_MANIPULATE_FIXED)1)
#define PIXEL_MANIPULATE_FIXED_1 (PIXEL_MANIPULATE_INTEGER_TO_FIXED(1))
#define PIXEL_MANIPULATE_FIXED_TO_INTEGER(F) ((int)((F) >> 16))
#define PIXEL_MANIPULATE_FIXED_TO_FLOAT(F) (double)((F) / (double)PIXEL_MANIPULATE_FIXED_1)
#define PIXEL_MANIPULATE_INTEGER_TO_FIXED(i) ((PIXEL_MANIPULATE_FIXED)((uint32)(i) << 16))
#define PIXEL_MANIPULATE_FLOAT_TO_FIXED(d)	((PIXEL_MANIPULATE_FIXED) ((d) * 65536.0))
#define PIXEL_MANIPULATE_FIXED_MINUS_1 (PIXEL_MANIPULATE_INTEGER_TO_FIXED(-1))
#define PIXEL_MANIPULATE_FIXED_1_MINUS_E (PIXEL_MANIPULATE_FIXED_1 - PIXEL_MANIPULATE_FIXED_E)
#define PIXEL_MANIPULATE_FIXED_FRAC(f) ((f) & PIXEL_MANIPULATE_FIXED_1_MINUS_E)
#define PIXEL_MANIPULATE_FIXED_FLOOR(f) ((f) & PIXEL_MANIPULATE_FIXED_1_MINUS_E)
#define PIXEL_MANIPULATE_FIXED_CEIL(f) PIXEL_MANIPULATE_FLOOR((f) + PIXEL_MANIPULATE_FIXED_1_MINUS_E)
#define PIXEL_MANIPULATE_FORMAT_RESHIFT(val, ofs, num) \
	(((val >> (ofs)) & ((1 << (num)) - 1)) << ((val >> 22) & 3))

#define PIXEL_MANIPULATE_FORMAT_BPP(f)	PIXEL_MANIPULATE_FORMAT_RESHIFT(f, 24, 8)

typedef enum _ePIXEL_MANIPULATE_IMAGE_TYPE
{
	PIXEL_MANIPULATE_IMAGE_TYPE_BITS,
	PIXEL_MANIPULATE_IMAGE_TYPE_LINEAR,
	PIXEL_MANIPULATE_IMAGE_TYPE_CONICAL,
	PIXEL_MANIPULATE_IMAGE_TYPE_RADIAL,
	PIXEL_MANIPULATE_IMAGE_TYPE_SOLID
} ePIXEL_MANIPULATE_IMAGE_TYPE;

typedef enum _ePIXEL_MANIPULATE_REPEAT_TYPE
{
	PIXEL_MANIPULATE_REPEAT_TYPE_NONE,
	PIXEL_MANIPULATE_REPEAT_TYPE_NORMAL,
	PIXEL_MANIPULATE_REPEAT_TYPE_PAD,
	PIXEL_MANIPULATE_REPEAT_TYPE_REFLECT,
	PIXEL_MANIPULATE_REPEAT_TYPE_COVER
} ePIXEL_MANIPULATE_REPEAT_TYPE;

typedef enum _ePIXEL_MANIPULATE_FILTER_TYPE
{
	PIXEL_MANIPULATE_FILTER_TYPE_FAST,
	PIXEL_MANIPULATE_FILTER_TYPE_GOOD,
	PIXEL_MANIPULATE_FILTER_TYPE_BEST,
	PIXEL_MANIPULATE_FILTER_TYPE_NEAREST,
	PIXEL_MANIPULATE_FILTER_TYPE_BILINEAR,
	PIXEL_MANIPULATE_FILTER_TYPE_CONVOLUTION,
	PIXEL_MANIPULATE_FILTER_TYPE_SEPARABLE_CONVOLUTION
} ePIXEL_MANIPULATE_FILTER_TYPE;

typedef enum _ePIXEL_MANIPULATE_DITHER
{
	PIXEL_MANIPULATE_DITHER_NONE,
	PIXEL_MANIPULATE_DITHER_FAST,
	PIXEL_MANIPULATE_DITHER_GOOD,
	PIXEL_MANIPULATE_DITHER_BEST,
	PIXEL_MANIPULATE_DITHER_ORDERED_BAYER_8,
	PIXEL_MANIPULATE_DITHER_ORDERED_BLUE_NOISE_64
} ePIXEL_MANIPULATE_DITHER;

typedef enum _ePIXEL_MANIPULATE_REGION_OVERLAP
{
	PIXEL_MANIPULATE_REGION_OVERLAP_OUT,
	PIXEL_MANIPULATE_REGION_OVERLAP_IN,
	PIXEL_MANIPULATE_REGION_OVERLAP_PART
} ePIXEL_MANIPULATE_REGION_OVERLAP;

typedef struct _PIXEL_MANIPULATE_BOX
{
	int x1, y1, x2, y2;
} PIXEL_MANIPULATE_BOX;

typedef struct _PIXEL_MANIPULATE_POINT_FIXED
{
	PIXEL_MANIPULATE_FIXED x, y;
} PIXEL_MANIPULATE_POINT_FIXED;

typedef struct _PIXEL_MANIPULATE_LINE_FIXED
{
	PIXEL_MANIPULATE_POINT_FIXED point1, point2;
} PIXEL_MANIPULATE_LINE_FIXED;

typedef struct _PIXEL_MANIPULATE_VECTOR
{
	PIXEL_MANIPULATE_FIXED vector[3];
} PIXEL_MANIPULATE_VECTOR;

typedef struct _PIXEL_MANIPULATE_TRANSFORM
{
	PIXEL_MANIPULATE_FIXED matrix[3][3];
} PIXEL_MANIPULATE_TRANSFORM;

typedef struct _PIXEL_MANIPULATE_REGION_DATA
{
	int size;
	int num_rects;
} PIXEL_MANIPULATE_REGION_DATA;

typedef struct _PIXEL_MANIPULATE_REGION
{
	PIXEL_MANIPULATE_BOX extents;
	PIXEL_MANIPULATE_REGION_DATA *data;
} PIXEL_MANIPULATE_REGION;

typedef struct _PIXEL_MANIPULATE_REGION32_DATA
{
	unsigned broken : 1;
	int32 size;
	int32 num_rects;
} PIXEL_MANIPULATE_REGION32_DATA;

typedef struct _PIXEL_MANIPULATE_RECTANGLE32
{
	int32 x, y;
	uint32 width, height;
} PIXEL_MANIPULATE_RECTANGLE32;

typedef struct _PIXEL_MANIPULATE_BOX32
{
	int32 x1, y1, x2, y2;
} PIXEL_MANIPULATE_BOX32;

typedef struct _PIXEL_MANIPULATE_REGION32
{
	PIXEL_MANIPULATE_BOX32 extents;
	PIXEL_MANIPULATE_REGION32_DATA *data;
	PIXEL_MANIPULATE_REGION32_DATA data_embedded[4];
} PIXEL_MANIPULATE_REGION32;

typedef struct _PIXEL_MANIPULATE_TRIANGLE
{
	PIXEL_MANIPULATE_FIXED point1, point2, point3;
} PIXEL_MANIPULATE_TRIANGLE;

typedef struct _PIXEL_MANIPULATE_TRAPEZOID
{
	PIXEL_MANIPULATE_FIXED top, bottom;
	PIXEL_MANIPULATE_LINE_FIXED left, right;
} PIXEL_MANIPULATE_TRAPEZOID;

typedef struct _PIXEL_MANIPULATE_BITS_IMAGE PIXEL_MANIPULATE_BITS_IMAGE;

typedef struct _PIXEL_MANIPULATE_IMAGE_COMMON
{
	ePIXEL_MANIPULATE_IMAGE_TYPE type;
	int reference_count;
	PIXEL_MANIPULATE_REGION32 clip_region;
	int alpha_count;
	uint32 *to_free;
	unsigned have_clip_region : 1;
	unsigned client_clip : 1;
	unsigned clip_sources : 1;
	unsigned multiple_of_4 : 1;
	unsigned self_allocated : 1;
	unsigned dirty : 1;
	PIXEL_MANIPULATE_TRANSFORM *transform;
	ePIXEL_MANIPULATE_REPEAT_TYPE repeat;
	ePIXEL_MANIPULATE_FILTER_TYPE filter;
	PIXEL_MANIPULATE_FIXED *filter_parameters;
	int num_filter_parameters;
	PIXEL_MANIPULATE_BITS_IMAGE *alpha_map;
	int alpha_origin_x;
	int alpha_origin_y;
	int component_alpha;
	void (*property_changed_function)(struct _PIXEL_MANIPULATE_IMAGE_COMMON*);
	void (*delete_function)(struct _PIXEL_MANIPULATE_IMAGE_COMMON*, void*);
	void *delete_data;
	unsigned int flags;
	ePIXEL_MANIPULATE_FORMAT extended_format_code;
	struct _PIXEL_MANIPULATE *pixel_manipulate;
} PIXEL_MANIPULATE_IMAGE_COMMON;

typedef struct _PIXEL_MANIPULATE_ARGB
{
	float a, r, g, b;
} PIXEL_MANIPULATE_ARGB;

typedef struct _PIXEL_MANIPULATE_COLOR
{
	uint16 red;
	uint16 green;
	uint16 blue;
	uint16 alpha;
} PIXEL_MANIPULATE_COLOR;

typedef struct _PIXEL_MANIPULATE_SOLID_FILL
{
	PIXEL_MANIPULATE_IMAGE_COMMON common;
	PIXEL_MANIPULATE_COLOR color;
	uint32 color_32;
	PIXEL_MANIPULATE_ARGB color_float;
} PIXEL_MANIPULATE_SOLID_FILL;

typedef struct _PIXEL_MANIPULATE_GRADIENT_STOP
{
	PIXEL_MANIPULATE_FIXED x;
	PIXEL_MANIPULATE_COLOR color;
} PIXEL_MANIPULATE_GRADIENT_STOP;

typedef struct _PIXEL_MANIPULATE_GRADIENT
{
	PIXEL_MANIPULATE_IMAGE_COMMON common;
	int num_stops;
	PIXEL_MANIPULATE_GRADIENT_STOP *stops;
} PIXEL_MANIPULATE_GRADIENT;

typedef struct _PIXEL_MANIPULATE_LINEAR_GRADIENT
{
	PIXEL_MANIPULATE_GRADIENT common;
	PIXEL_MANIPULATE_POINT_FIXED p1;
	PIXEL_MANIPULATE_POINT_FIXED p2;
} PIXEL_MANIPULATE_LINEAR_GRADIENT;

typedef struct _PIXEL_MANIPULATE_CIRCLE
{
	PIXEL_MANIPULATE_FIXED x;
	PIXEL_MANIPULATE_FIXED y;
	PIXEL_MANIPULATE_FIXED radius;
} PIXEL_MANIPULATE_CIRCLE;

typedef struct _PIXEL_MANIPULATE_RADIAL_GRADIENT
{
	PIXEL_MANIPULATE_GRADIENT common;
	PIXEL_MANIPULATE_CIRCLE c1;
	PIXEL_MANIPULATE_CIRCLE c2;
	PIXEL_MANIPULATE_CIRCLE delta;
	double a;
	double inverse_a;
	double minimum_dr;
} PIXEL_MANIPULATE_RADIAL_GRADIENT;

typedef struct _PIXEL_MANIPULATE_CONICAL_GRADIENT
{
	PIXEL_MANIPULATE_GRADIENT common;
	PIXEL_MANIPULATE_POINT_FIXED center;
	double angle;
} PIXEL_MANIPULATE_CONICAL_GRADIENT;

#define PIXEL_MANIPULATE_MAX_INDEXED 256
typedef uint8 PIXEL_MANIPULATE_INDEX_TYPE;

typedef struct _PIXEL_MANIPULATE_INDEXED
{
	int color;
	uint32 rgba[PIXEL_MANIPULATE_MAX_INDEXED];
	PIXEL_MANIPULATE_INDEX_TYPE entry[32768];
} PIXEL_MANIPULATE_INDEXED;

typedef void (*PIXEL_MANIPULATE_FETCH_SCANLINE)(PIXEL_MANIPULATE_BITS_IMAGE* image,
												int x, int y, int width, uint32* buffer, const uint32* mask);
typedef uint32 (*PIXEL_MANIPULATE_FETCH_PIXEL32)(PIXEL_MANIPULATE_BITS_IMAGE* image,
												 int x, int y);
typedef PIXEL_MANIPULATE_ARGB (*PIXEL_MANIPULATE_FETCH_PIXEL_FLOAT)(PIXEL_MANIPULATE_BITS_IMAGE* image,
																	int x, int y);
typedef void (*PIXEL_MANIPULATE_STORE_SCANLINE)(PIXEL_MANIPULATE_BITS_IMAGE* image,
												int x, int y, int width, const uint32* values);

typedef struct _PIXEL_MANIPULATE_EDGE
{
	PIXEL_MANIPULATE_FIXED x;
	PIXEL_MANIPULATE_FIXED e;
	PIXEL_MANIPULATE_FIXED stepx;
	PIXEL_MANIPULATE_FIXED signdx;
	PIXEL_MANIPULATE_FIXED dy;
	PIXEL_MANIPULATE_FIXED dx;


	PIXEL_MANIPULATE_FIXED stepx_small;
	PIXEL_MANIPULATE_FIXED stepx_big;
	PIXEL_MANIPULATE_FIXED dx_small;
	PIXEL_MANIPULATE_FIXED dx_big;
} PIXEL_MANIPULATE_EDGE;

struct _PIXEL_MANIPULATE_BITS_IMAGE
{
	PIXEL_MANIPULATE_IMAGE_COMMON common;
	ePIXEL_MANIPULATE_FORMAT format;
	const PIXEL_MANIPULATE_INDEXED *indexed;
	int width;
	int height;
	uint32 *bits;
	uint32 *to_free;
	int row_stride;
	ePIXEL_MANIPULATE_DITHER dither;
	uint32 dither_offset_x;
	uint32 dither_offset_y;

	PIXEL_MANIPULATE_FETCH_SCANLINE fetch_scanline_32;
	PIXEL_MANIPULATE_FETCH_PIXEL32 fetch_pixel_32;
	PIXEL_MANIPULATE_STORE_SCANLINE store_scanline_32;

	PIXEL_MANIPULATE_FETCH_SCANLINE fetch_scanline_float;
	PIXEL_MANIPULATE_FETCH_PIXEL_FLOAT fetch_pixel_float;
	PIXEL_MANIPULATE_STORE_SCANLINE store_scanline_float;

	// uint32 (*read_func)(const void* src, int size);
	// void (*write_func)(void* dst, uint32 value, int size);
};

typedef union _PIXEL_MANIPULATE_IMAGE
{
	ePIXEL_MANIPULATE_IMAGE_TYPE type;
	PIXEL_MANIPULATE_IMAGE_COMMON common;
	PIXEL_MANIPULATE_BITS_IMAGE bits;
	PIXEL_MANIPULATE_GRADIENT gradient;
	PIXEL_MANIPULATE_LINEAR_GRADIENT linear;
	PIXEL_MANIPULATE_CONICAL_GRADIENT conical;
	PIXEL_MANIPULATE_RADIAL_GRADIENT radial;
	PIXEL_MANIPULATE_SOLID_FILL solid;
} PIXEL_MANIPULATE_IMAGE;

typedef struct _PIXEL_MANIPULATE
{
	PIXEL_MANIPULATE_IMPLEMENTATION implementation;
} PIXEL_MANIPULATE;

#define PIXEL_MANIPULATE_FAST_PATH_ID_TRANSFORM (1 << 0)
#define PIXEL_MANIPULATE_FAST_PATH_NO_ALPHA_MAP (1 << 1)
#define PIXEL_MANIPULATE_FAST_PATH_SAMPLES_OPAQUE (1 << 7)
#define PIXEL_MANIPULATE_FAST_PATH_NEAREST_FILTER (1 << 11)
#define PIXEL_MANIPULATE_FAST_PATH_IS_OPAQUE	(1 << 13)
#define PIXEL_MANIPULATE_FAST_PATH_BILINEAR_FILTER (1 << 11)
#define PIXEL_MANIPULATE_FAST_PATH_SAMPLES_COVER_CLIP_NEAREST (1 << 23)
#define PIXEL_MANIPULATE_FAST_PATH_SAMPLES_COVER_CLIP_BILINEAR (1 << 24)

#define PIXEL_MANIPULATE_NEAREST_SAMPLE(d) ((d) - 0.5)

#ifdef __cplusplus
extern "C" {
#endif

extern int PixelManipulateImageFinish(PIXEL_MANIPULATE_IMAGE* image);
extern void PixelManipulateRegion32Translate(PIXEL_MANIPULATE_REGION32* region, int x, int y);

static INLINE void InitializePixelManipulateRegion32(PIXEL_MANIPULATE_REGION32* region)
{
	region->extents.x1 = region->extents.y1
		= region->extents.y1 = region->extents.y2 = 0;
	region->data = region->data_embedded;
	region->data->size = 0;
	region->data->num_rects = 0;
}

static INLINE PIXEL_MANIPULATE_BOX32* PixelManipulateRegion32Rectangles(PIXEL_MANIPULATE_REGION32* region, int* num_rects)
{
	if(num_rects != NULL)
	{
		*num_rects = ((region->data != NULL) ? region->data->num_rects : 1);
	}

	return (region->data != NULL) ? (PIXEL_MANIPULATE_BOX32*)(region->data + 1) : &region->extents;
}

static INLINE uint32* PixelManipulateImageGetData(PIXEL_MANIPULATE_IMAGE* image)
{
	if(image->type == PIXEL_MANIPULATE_IMAGE_TYPE_BITS)
	{
		return (uint32*)image->bits.bits;
	}
	return NULL;
}

static INLINE PIXEL_MANIPULATE_IMAGE* PixelManipulateImageReference(PIXEL_MANIPULATE_IMAGE* image)
{
	image->common.reference_count++;

	return image;
}

#ifndef FALSE
# define FALSE (0)
#endif
#ifndef TRUE
# define TRUE (1)
#endif

static int PixelManipulateImageUnreference(PIXEL_MANIPULATE_IMAGE* image)
{
	// TODO image_fini
	if(PixelManipulateImageFinish(image) != FALSE)
	{
		if(image->common.self_allocated)
		{
			MEM_FREE_FUNC(image);
		}
		return TRUE;
	}

	return FALSE;
}

extern void InitializePixelManipulate(PIXEL_MANIPULATE* pixel_manipulate);
extern void PixelManipulateSetupAccessors(PIXEL_MANIPULATE_BITS_IMAGE* image);
extern INLINE void PixelManipulateResetClipRegion(PIXEL_MANIPULATE_IMAGE* image);
extern int PixelManipulateImageInitialize(
	PIXEL_MANIPULATE_IMAGE* image,
	ePIXEL_MANIPULATE_FORMAT format,
	int width,
	int height,
	uint32* bits,
	int rowstride_bytes,
	int clear,
	PIXEL_MANIPULATE* manipulate
);
extern int InitializePixelManipulateBitsImage(
	PIXEL_MANIPULATE_IMAGE* image,
	ePIXEL_MANIPULATE_FORMAT format,
	int width,
	int height,
	uint32* bits,
	int row_stride,
	int clear
);
extern PIXEL_MANIPULATE_IMAGE* CreatePixelManipulateImageBits(
	ePIXEL_MANIPULATE_FORMAT format,
	int width,
	int height,
	uint32* bits,
	int row_stride,
	PIXEL_MANIPULATE* manipulate
);
extern int PixelManipulateImageInitializeWithBits(
	PIXEL_MANIPULATE_IMAGE* image,
	ePIXEL_MANIPULATE_FORMAT format,
	int width,
	int height,
	uint32* bits,
	int rowstride_bytes,
	int clear,
	PIXEL_MANIPULATE* manipulate
);
extern void PixelManipulateImageValidate(PIXEL_MANIPULATE_IMAGE* image);
extern int PixelManipulateImageSetFilter(
	PIXEL_MANIPULATE_IMAGE* image,
	ePIXEL_MANIPULATE_FILTER_TYPE filter,
	const PIXEL_MANIPULATE_FIXED* params,
	int n_params
);
extern void PixelManipulateImageSetComponentAlpha(PIXEL_MANIPULATE_IMAGE* image, int component_alpha);
extern void PixelManipulateImageValidate(PIXEL_MANIPULATE_IMAGE* image);
extern void PixelManipulateSetDestroyFunction(
	PIXEL_MANIPULATE_IMAGE* image,
	void (*destroy_function)(PIXEL_MANIPULATE_IMAGE*),
	void* data
);

extern int PixelManipulateImageFinish(PIXEL_MANIPULATE_IMAGE* image);

extern int PixelManipulateComputeCompositeRegion32(
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
);

extern int PixelManipulateFill(
	uint32* bits,
	int stride,
	int bpp,
	int x,
	int y,
	int width,
	int height,
	uint32 filler,
	PIXEL_MANIPULATE_IMPLEMENTATION* implementation
);

extern int PixelManipulateBlockTransfer(
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
);

extern void InitializePixelManipulateImage(PIXEL_MANIPULATE_IMAGE* image);

extern PIXEL_MANIPULATE_IMAGE* PixelManipulateAllocate(void);

extern int PixelManipulateSetClipRegion32(PIXEL_MANIPULATE_IMAGE* image, PIXEL_MANIPULATE_REGION32* region);

extern void PixelManipulateImageComposite32(
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
);

extern int PixelManipulateImageSetTransform(
	PIXEL_MANIPULATE_IMAGE* image,
	const PIXEL_MANIPULATE_TRANSFORM* transform
);

extern void PixelManipulateImageSetRepeat(
	PIXEL_MANIPULATE_IMAGE* image,
	ePIXEL_MANIPULATE_REPEAT_TYPE repeat
);

extern void PixelManipulateRasterizeEdges(
	PIXEL_MANIPULATE_IMAGE* image,
	PIXEL_MANIPULATE_EDGE* l,
	PIXEL_MANIPULATE_EDGE* r,
	PIXEL_MANIPULATE_FIXED  t,
	PIXEL_MANIPULATE_FIXED  b
);

extern void PixelManipulateRasterizeTrapezoid(
	PIXEL_MANIPULATE_IMAGE* image,
	const PIXEL_MANIPULATE_TRAPEZOID* trap,
	int x_offset,
	int y_offset
);

extern void PixelManipulateAddTriangles(
	PIXEL_MANIPULATE_IMAGE* image,
	int32 x_offset,
	int32 y_offset,
	int n_tris,
	const PIXEL_MANIPULATE_TRIANGLE* tris
);

extern int InitializePixelManipulateGradient(
	struct _PIXEL_MANIPULATE_GRADIENT* gradient,
	const PIXEL_MANIPULATE_GRADIENT_STOP* stops,
	int n_stops
);
extern void InitializePixelManipulateGradientWalker(
	struct _PIXEL_MANIPULATE_GRADIENT_WALKER* walker,
	struct _PIXEL_MANIPULATE_GRADIENT* gradient,
	int repeat
);

extern void InitializePixelManipulateImageSolidFill(PIXEL_MANIPULATE_IMAGE* image, const PIXEL_MANIPULATE_COLOR* color);
extern PIXEL_MANIPULATE_IMAGE* PixelManipulateImageCreateSolidFill(const PIXEL_MANIPULATE_COLOR* color);

extern void InitializePixelManipulateLinearGradient(
	PIXEL_MANIPULATE_IMAGE* image,
	const PIXEL_MANIPULATE_POINT_FIXED* p1,
	const PIXEL_MANIPULATE_POINT_FIXED* p2,
	const PIXEL_MANIPULATE_GRADIENT_STOP* stops,
	int n_stops
);
extern PIXEL_MANIPULATE_IMAGE* PixelManipulateCreateLinearGradient(
	const PIXEL_MANIPULATE_POINT_FIXED*  p1,
	const PIXEL_MANIPULATE_POINT_FIXED*  p2,
	const PIXEL_MANIPULATE_GRADIENT_STOP* stops,
	int n_stops
);

extern void InitializePixelManipulateImageRadialGradient(
	PIXEL_MANIPULATE_IMAGE* image,
	const PIXEL_MANIPULATE_POINT_FIXED* inner,
	const PIXEL_MANIPULATE_POINT_FIXED* outer,
	PIXEL_MANIPULATE_FIXED inner_radius,
	PIXEL_MANIPULATE_FIXED outer_radius,
	const PIXEL_MANIPULATE_GRADIENT_STOP* stops,
	int n_stops
);
extern PIXEL_MANIPULATE_IMAGE* PixelManipulateImageCreateRadialGradient(
	const PIXEL_MANIPULATE_POINT_FIXED* inner,
	const PIXEL_MANIPULATE_POINT_FIXED* outer,
	PIXEL_MANIPULATE_FIXED inner_radius,
	PIXEL_MANIPULATE_FIXED outer_radius,
	const PIXEL_MANIPULATE_GRADIENT_STOP* stops,
	int n_stops
);

extern void PixelManipulateImageSetDestroyFunction(
	PIXEL_MANIPULATE_IMAGE* image,
	void (*function)(struct _PIXEL_MANIPULATE_IMAGE_COMMON*, void*),
	void* data
);

extern void PixelManipulateChooseImplementation(PIXEL_MANIPULATE_IMPLEMENTATION** implementation);

#ifdef __cplusplus
}
#endif

#endif  // #ifndef _INCLUDED_PIXEL_MANIPULATE_H_
