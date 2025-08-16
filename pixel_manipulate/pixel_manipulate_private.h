#ifndef _INCLUDED_PIXEL_MANIPULATE_PRIVE_H_
#define _INCLUDED_PIXEL_MANIPULATE_PRIVE_H_

#include <float.h>
#include "pixel_manipulate_format.h"
#include "pixel_manipulate.h"
#include "pixel_manipulate_region.h"

#ifdef _MSC_VER
# define FORCE_INLINE __forceinline
#elif defined(__GNUC__)
# define FORCE_INLINE __attribute__ ((__always_inline__))
#else
# define FORCE_INLINE inline
#endif

#ifndef INT16_MIN
# define INT16_MIN			  (-32767-1)
#endif

#ifndef INT16_MAX
# define INT16_MAX			  (32767)
#endif

#ifndef INT32_MIN
# define INT32_MIN			  (-2147483647-1)
#endif

#ifndef INT32_MAX
# define INT32_MAX			  (2147483647)
#endif

#ifndef UINT32_MIN
# define UINT32_MIN			 (0)
#endif

#ifndef UINT32_MAX
# define UINT32_MAX			 (4294967295U)
#endif

#ifndef INT64_MIN
# define INT64_MIN			  (-9223372036854775807-1)
#endif

#ifndef INT64_MAX
# define INT64_MAX			  (9223372036854775807)
#endif

#ifndef SIZE_MAX
# define SIZE_MAX			   ((size_t)-1)
#endif

#define STEP_Y_SMALL(n) (PIXEL_MANIPULATE_FIXED_1 / N_Y_FRAC (n))
#define STEP_Y_BIG(n)   (PIXEL_MANIPULATE_FIXED_1 - (N_Y_FRAC (n) - 1) * STEP_Y_SMALL (n))

#define N_X_FRAC(n)	 ((n) == 1 ? 1 : (1 << ((n) / 2)) + 1)
#define N_Y_FRAC(n)	 ((n) == 1 ? 1 : (1 << ((n) / 2)) - 1)

#define STEP_X_SMALL(n) (PIXEL_MANIPULATE_FIXED_1 / N_X_FRAC (n))
#define STEP_X_BIG(n)   (PIXEL_MANIPULATE_FIXED_1 - (N_X_FRAC (n) - 1) * STEP_X_SMALL (n))

#define X_FRAC_FIRST(n) (STEP_X_BIG (n) / 2)
#define X_FRAC_LAST(n)  (X_FRAC_FIRST (n) + (N_X_FRAC (n) - 1) * STEP_X_SMALL (n))

#define Y_FRAC_FIRST(n) (STEP_Y_BIG(n) / 2)
#define Y_FRAC_LAST(n)  (Y_FRAC_FIRST(n) + (N_Y_FRAC(n) - 1) * STEP_Y_SMALL(n))

#define RENDER_SAMPLES_X(x, n)						\
	((n) == 1? 0 : (PIXEL_MANIPULATE_FIXED_FRAC(x) +				\
			X_FRAC_FIRST (n)) / STEP_X_SMALL (n))

#ifdef WORDS_BIGENDIAN
#   define SCREEN_SHIFT_LEFT(x,n)	((x) << (n))
#   define SCREEN_SHIFT_RIGHT(x,n)	((x) >> (n))
#else
#   define SCREEN_SHIFT_LEFT(x,n)	((x) >> (n))
#   define SCREEN_SHIFT_RIGHT(x,n)	((x) << (n))
#endif

#define BILINEAR_INTERPOLATION_BITS 7
#define BILINEAR_INTERPOLATION_RANGE (1 << BILINEAR_INTERPOLATION_BITS)

#define NUM_RECTS(region) ((region)->data ? (region)->data->num_rects : 1)

#define BOX_POINTER(region) (PIXEL_MANIPULATE_BOX32*)((PIXEL_MANIPULATE_REGION32*)((region)->data + 1))

#define EMPTY_ID 1
#define BROKEN_ID 2

#define PIXEL_MANIPULATE_EMPTY_DATA ((PIXEL_MANIPULATE_REGION32_DATA*)EMPTY_ID)
#define PIXEL_MANIPULATE_BROKEN_DATA ((PIXEL_MANIPULATE_REGION32_DATA*)BROKEN_ID)
#define PIXEL_MANIPULATE_REGION_NAR(region) ((region) == PIXEL_MANIPULATE_BROKEN_DATA)

#define REGION_BOX_POINTER(reg) ((PIXEL_MANIPULATE_BOX32*)((reg)->data + 1))
#define REGION_BOX(reg, i) (&REGION_BOX_POINTER(reg)[i])
#define REGION_END(reg) REGION_BOX(reg, (reg)->data->num_rects - 1)

#define PIXEL_MANIPULATE_EMPTY_BOX ((PIXEL_MANIPULATE_BOX32*)EMPTY_ID)

#define DIV(a, b)					   \
	((((a) < 0) == ((b) < 0)) ? (a) / (b) :				\
	 ((a) - (b) + 1 - (((b) < 0) << 1)) / (b))

#define MOD(a, b) ((a) < 0 ? ((b) - ((-(a) - 1) % (b))) - 1 : (a) % (b))

#define MUL_UN8(a, b, t)						\
	((t) = (a) * (uint16)(b) + ONE_HALF, ((((t) >> G_SHIFT ) + (t) ) >> G_SHIFT ))

#define DIV_UN8(a, b)							\
	(((uint16) (a) * MASK + ((b) / 2)) / (b))

#define CLIP(v, low, high) ((v) < (low) ? (low) : ((v) > (high) ? (high) : (v)))

typedef struct _PIXEL_MANIPULATE_VECTOR_48_16
{
	PIXEL_MANIPULATE_FIXED v[3];
} PIXEL_MANIPULATE_VECTOR_48_16;

#if defined (__GNUC__)
#  define MAYBE_UNUSED  __attribute__((unused))
#else
#  define MAYBE_UNUSED
#endif

#define COMPOSITE_ARGS(info)					\
	MAYBE_UNUSED ePIXEL_MANIPULATE_OPERATE		op = info->op;			\
	MAYBE_UNUSED PIXEL_MANIPULATE_IMAGE   *src_image = info->src_image;	\
	MAYBE_UNUSED PIXEL_MANIPULATE_IMAGE   *mask_image = info->mask_image;	\
	MAYBE_UNUSED PIXEL_MANIPULATE_IMAGE   *dest_image = info->dest_image;	\
	MAYBE_UNUSED int32			src_x = info->src_x;		\
	MAYBE_UNUSED int32			src_y = info->src_y;		\
	MAYBE_UNUSED int32			mask_x = info->mask_x;		\
	MAYBE_UNUSED int32			mask_y = info->mask_y;		\
	MAYBE_UNUSED int32			dest_x = info->dest_x;		\
	MAYBE_UNUSED int32			dest_y = info->dest_y;		\
	MAYBE_UNUSED int32			width = info->width;		\
	MAYBE_UNUSED int32			height = info->height

#define IMAGE_GET_LINE(image, x, y, type, out_stride, line, mul)	\
	do									\
	{									\
	uint32 *__bits__;						\
	int	   __stride__;						\
										\
	__bits__ = image->bits.bits;					\
	__stride__ = image->bits.row_stride;				\
	(out_stride) =							\
		__stride__ * (int) sizeof (uint32) / (int) sizeof (type);	\
	(line) =							\
		((type *) __bits__) + (out_stride) * (y) + (mul) * (x);	\
	} while (0)

#define FAST_PATH_ID_TRANSFORM			(1 <<  0)
#define FAST_PATH_NO_ALPHA_MAP			(1 <<  1)
#define FAST_PATH_NO_CONVOLUTION_FILTER		(1 <<  2)
#define FAST_PATH_NO_PAD_REPEAT			(1 <<  3)
#define FAST_PATH_NO_REFLECT_REPEAT		(1 <<  4)
#define FAST_PATH_NO_ACCESSORS			(1 <<  5)
#define FAST_PATH_NARROW_FORMAT			(1 <<  6)
#define FAST_PATH_COMPONENT_ALPHA		(1 <<  8)
#define FAST_PATH_SAMPLES_OPAQUE		(1 <<  7)
#define FAST_PATH_UNIFIED_ALPHA			(1 <<  9)
#define FAST_PATH_SCALE_TRANSFORM		(1 << 10)
#define FAST_PATH_NEAREST_FILTER		(1 << 11)
#define FAST_PATH_HAS_TRANSFORM			(1 << 12)
#define FAST_PATH_IS_OPAQUE			(1 << 13)
#define FAST_PATH_NO_NORMAL_REPEAT		(1 << 14)
#define FAST_PATH_NO_NONE_REPEAT		(1 << 15)
#define FAST_PATH_X_UNIT_POSITIVE		(1 << 16)
#define FAST_PATH_AFFINE_TRANSFORM		(1 << 17)
#define FAST_PATH_Y_UNIT_ZERO			(1 << 18)
#define FAST_PATH_BILINEAR_FILTER		(1 << 19)
#define FAST_PATH_ROTATE_90_TRANSFORM		(1 << 20)
#define FAST_PATH_ROTATE_180_TRANSFORM		(1 << 21)
#define FAST_PATH_ROTATE_270_TRANSFORM		(1 << 22)
#define FAST_PATH_SAMPLES_COVER_CLIP_NEAREST	(1 << 23)
#define FAST_PATH_SAMPLES_COVER_CLIP_BILINEAR	(1 << 24)
#define FAST_PATH_BITS_IMAGE			(1 << 25)
#define FAST_PATH_SEPARABLE_CONVOLUTION_FILTER  (1 << 26)

#define FAST_PATH_PAD_REPEAT						\
	(FAST_PATH_NO_NONE_REPEAT		|				\
	 FAST_PATH_NO_NORMAL_REPEAT		|				\
	 FAST_PATH_NO_REFLECT_REPEAT)

#define FAST_PATH_NORMAL_REPEAT						\
	(FAST_PATH_NO_NONE_REPEAT		|				\
	 FAST_PATH_NO_PAD_REPEAT		|				\
	 FAST_PATH_NO_REFLECT_REPEAT)

#define FAST_PATH_NONE_REPEAT						\
	(FAST_PATH_NO_NORMAL_REPEAT		|				\
	 FAST_PATH_NO_PAD_REPEAT		|				\
	 FAST_PATH_NO_REFLECT_REPEAT)

#define FAST_PATH_REFLECT_REPEAT					\
	(FAST_PATH_NO_NONE_REPEAT		|				\
	 FAST_PATH_NO_NORMAL_REPEAT		|				\
	 FAST_PATH_NO_PAD_REPEAT)

#define FAST_PATH_STANDARD_FLAGS					\
	(FAST_PATH_NO_CONVOLUTION_FILTER	|				\
	 FAST_PATH_NO_ACCESSORS		|				\
	 FAST_PATH_NO_ALPHA_MAP		|				\
	 FAST_PATH_NARROW_FORMAT)

#define FAST_PATH_STD_DEST_FLAGS					\
	(FAST_PATH_NO_ACCESSORS		|				\
	 FAST_PATH_NO_ALPHA_MAP		|				\
	 FAST_PATH_NARROW_FORMAT)

#define SOURCE_FLAGS(format)						\
	(FAST_PATH_STANDARD_FLAGS |						\
	 ((PIXEL_MANIPULATE_ ## format == PIXEL_MANIPULATE_SOLID) ?				\
	  0 : (FAST_PATH_SAMPLES_COVER_CLIP_NEAREST | FAST_PATH_NEAREST_FILTER | FAST_PATH_ID_TRANSFORM)))

#define MASK_FLAGS(format, extra)					\
	((PIXEL_MANIPULATE_ ## format == PIXEL_MANIPULATE_null) ? 0 : (SOURCE_FLAGS (format) | extra))

#define FAST_PATH(op, src, src_flags, mask, mask_flags, dest, dest_flags, func) \
	PIXEL_MANIPULATE_OPERATE_ ## op,							\
	PIXEL_MANIPULATE_ ## src,							\
	src_flags,									\
	PIXEL_MANIPULATE_ ## mask,								\
	mask_flags,									\
	PIXEL_MANIPULATE_ ## dest,													\
	dest_flags,									\
	func

#define PIXEL_MANIPULATE_STD_FAST_PATH(op, src, mask, dest, func)			\
	{ FAST_PATH (							\
		op,								\
		src,  SOURCE_FLAGS (src),					\
		mask, MASK_FLAGS (mask, FAST_PATH_UNIFIED_ALPHA),		\
		dest, FAST_PATH_STD_DEST_FLAGS,				\
		func) }

#define PIXEL_MANIPULATE_STD_FAST_PATH_CA(op, src, mask, dest, func)		\
	{ FAST_PATH (							\
		op,								\
		src,  SOURCE_FLAGS (src),					\
		mask, MASK_FLAGS (mask, FAST_PATH_COMPONENT_ALPHA),		\
		dest, FAST_PATH_STD_DEST_FLAGS,				\
		func) }

#define FLOAT_IS_ZERO(f)	 (-FLT_MIN < (f) && (f) < FLT_MIN)

#define COMPONENT_SIZE 8
#define MASK 0xff
#define ONE_HALF 0x80

#define A_SHIFT 8 * 3
#define R_SHIFT 8 * 2
#define G_SHIFT 8
#define A_MASK 0xff000000
#define R_MASK 0xff0000
#define G_MASK 0xff00

#define RB_MASK 0xff00ff
#define AG_MASK 0xff00ff00
#define RB_ONE_HALF 0x800080
#define RB_MASK_PLUS_ONE 0x1000100

#define ALPHA_8(x) ((x) >> A_SHIFT)
#define RED_8(x) (((x) >> R_SHIFT) & MASK)
#define GREEN_8(x) (((x) >> G_SHIFT) & MASK)
#define BLUE_8(x) ((x) & MASK)

#ifndef ADD_UN8
#define ADD_UN8(x, y, t)					 \
	((t) = (x) + (y),						 \
	 (uint32) (uint8) ((t) | (0 - ((t) >> G_SHIFT))))
#endif

#define DIV_ONE_UN8(x)							\
	(((x) + ONE_HALF + (((x) + ONE_HALF) >> G_SHIFT)) >> G_SHIFT)

#ifndef UN8_rb_ADD_UN8_rb
#define UN8_rb_ADD_UN8_rb(x, y, t)					\
	do									\
	{									\
	t = ((x) + (y));						\
	t |= RB_MASK_PLUS_ONE - ((t >> G_SHIFT) & RB_MASK);		\
	x = (t & RB_MASK);						\
	} while (0)
#endif

#ifndef UN8x4_ADD_UN8x4
#define UN8x4_ADD_UN8x4(x, y)						\
	do									\
	{									\
	uint32 r1__, r2__, r3__, t__;					\
									\
	r1__ = (x) & RB_MASK;						\
	r2__ = (y) & RB_MASK;						\
	UN8_rb_ADD_UN8_rb (r1__, r2__, t__);				\
									\
	r2__ = ((x) >> G_SHIFT) & RB_MASK;				\
	r3__ = ((y) >> G_SHIFT) & RB_MASK;				\
	UN8_rb_ADD_UN8_rb (r2__, r3__, t__);				\
									\
	x = r1__ | (r2__ << G_SHIFT);					\
	} while (0)
#endif

#define UN8_rb_MUL_UN8(x, a, t)						\
	do									\
	{									\
	t  = ((x) & RB_MASK) * (a);					\
	t += RB_ONE_HALF;						\
	x = (t + ((t >> G_SHIFT) & RB_MASK)) >> G_SHIFT;		\
	x &= RB_MASK;							\
	} while (0)

#define UN8_rb_MUL_UN8_rb(x, a, t)					\
	do									\
	{									\
	t  = (x & MASK) * (a & MASK);					\
	t |= (x & R_MASK) * ((a >> R_SHIFT) & MASK);			\
	t += RB_ONE_HALF;						\
	t = (t + ((t >> G_SHIFT) & RB_MASK)) >> G_SHIFT;		\
	x = t & RB_MASK;						\
	} while (0)

#define UN8x4_MUL_UN8x4(x, a)						\
	do									\
	{									\
	uint32 r1__, r2__, r3__, t__;					\
									\
	r1__ = (x);							\
	r2__ = (a);							\
	UN8_rb_MUL_UN8_rb (r1__, r2__, t__);				\
									\
	r2__ = (x) >> G_SHIFT;						\
	r3__ = (a) >> G_SHIFT;						\
	UN8_rb_MUL_UN8_rb (r2__, r3__, t__);				\
									\
	(x) = r1__ | (r2__ << G_SHIFT);					\
	} while (0)

#define UN8x4_MUL_UN8(x, a)						\
	do									\
	{									\
	uint32 r1__, r2__, t__;					\
									\
	r1__ = (x);							\
	UN8_rb_MUL_UN8 (r1__, (a), t__);				\
									\
	r2__ = (x) >> G_SHIFT;						\
	UN8_rb_MUL_UN8 (r2__, (a), t__);				\
									\
	(x) = r1__ | (r2__ << G_SHIFT);					\
	} while (0)

#define UN8x4_MUL_UN8_ADD_UN8x4(x, a, y)				\
	do									\
	{									\
	uint32 r1__, r2__, r3__, t__;					\
									\
	r1__ = (x);							\
	r2__ = (y) & RB_MASK;						\
	UN8_rb_MUL_UN8 (r1__, (a), t__);				\
	UN8_rb_ADD_UN8_rb (r1__, r2__, t__);				\
									\
	r2__ = (x) >> G_SHIFT;						\
	r3__ = ((y) >> G_SHIFT) & RB_MASK;				\
	UN8_rb_MUL_UN8 (r2__, (a), t__);				\
	UN8_rb_ADD_UN8_rb (r2__, r3__, t__);				\
									\
	(x) = r1__ | (r2__ << G_SHIFT);					\
	} while (0)

#define UN8x4_MUL_UN8x4_ADD_UN8x4(x, a, y)				\
	do									\
	{									\
	uint32 r1__, r2__, r3__, t__;					\
									\
	r1__ = (x);							\
	r2__ = (a);							\
	UN8_rb_MUL_UN8_rb (r1__, r2__, t__);				\
	r2__ = (y) & RB_MASK;						\
	UN8_rb_ADD_UN8_rb (r1__, r2__, t__);				\
									\
	r2__ = ((x) >> G_SHIFT);					\
	r3__ = ((a) >> G_SHIFT);					\
	UN8_rb_MUL_UN8_rb (r2__, r3__, t__);				\
	r3__ = ((y) >> G_SHIFT) & RB_MASK;				\
	UN8_rb_ADD_UN8_rb (r2__, r3__, t__);				\
									\
	(x) = r1__ | (r2__ << G_SHIFT);					\
	} while (0)

#define UN8x4_MUL_UN8x4_ADD_UN8x4_MUL_UN8(x, a, y, b)			\
	do									\
	{									\
	uint32 r1__, r2__, r3__, t__;					\
									\
	r1__ = (x);							\
	r2__ = (a);							\
	UN8_rb_MUL_UN8_rb (r1__, r2__, t__);				\
	r2__ = (y);							\
	UN8_rb_MUL_UN8 (r2__, (b), t__);				\
	UN8_rb_ADD_UN8_rb (r1__, r2__, t__);				\
									\
	r2__ = (x) >> G_SHIFT;						\
	r3__ = (a) >> G_SHIFT;						\
	UN8_rb_MUL_UN8_rb (r2__, r3__, t__);				\
	r3__ = (y) >> G_SHIFT;						\
	UN8_rb_MUL_UN8 (r3__, (b), t__);				\
	UN8_rb_ADD_UN8_rb (r2__, r3__, t__);				\
									\
	x = r1__ | (r2__ << G_SHIFT);					\
	} while (0)

#define UN8x4_MUL_UN8_ADD_UN8x4(x, a, y)				\
	do									\
	{									\
	uint32 r1__, r2__, r3__, t__;					\
									\
	r1__ = (x);							\
	r2__ = (y) & RB_MASK;						\
	UN8_rb_MUL_UN8 (r1__, (a), t__);				\
	UN8_rb_ADD_UN8_rb (r1__, r2__, t__);				\
									\
	r2__ = (x) >> G_SHIFT;						\
	r3__ = ((y) >> G_SHIFT) & RB_MASK;				\
	UN8_rb_MUL_UN8 (r2__, (a), t__);				\
	UN8_rb_ADD_UN8_rb (r2__, r3__, t__);				\
									\
	(x) = r1__ | (r2__ << G_SHIFT);					\
	} while (0)

#define UN8x4_MUL_UN8_ADD_UN8x4_MUL_UN8(x, a, y, b)			\
	do									\
	{									\
	uint32 r1__, r2__, r3__, t__;					\
									\
	r1__ = (x);							\
	r2__ = (y);							\
	UN8_rb_MUL_UN8 (r1__, (a), t__);				\
	UN8_rb_MUL_UN8 (r2__, (b), t__);				\
	UN8_rb_ADD_UN8_rb (r1__, r2__, t__);				\
									\
	r2__ = ((x) >> G_SHIFT);					\
	r3__ = ((y) >> G_SHIFT);					\
	UN8_rb_MUL_UN8 (r2__, (a), t__);				\
	UN8_rb_MUL_UN8 (r3__, (b), t__);				\
	UN8_rb_ADD_UN8_rb (r2__, r3__, t__);				\
									\
	(x) = r1__ | (r2__ << G_SHIFT);					\
	} while (0)

#define PIXEL_MANIPULATE_null PIXEL_MANIPULATE_FORMAT(0, 0, 0, 0, 0, 0)
#define PIXEL_MANIPULATE_SOLID PIXEL_MANIPULATE_FORMAT(0, 1, 0, 0, 0, 0)
#define PIXEL_MANIPULATE_PIXBUF PIXEL_MANIPULATE_FORMAT(0, 2, 0, 0, 0, 0)
#define PIXEL_MANIPULATE_RPIXBUF PIXEL_MANIPULATE_FORMAT(0, 3, 0, 0, 0, 0)

#define PIXEL_MANIPULATE_TRAPEZPOID_VALID(t)				   \
	((t)->left.point1.y != (t)->left.point2.y &&			   \
	 (t)->right.point1.y != (t)->right.point2.y &&			   \
	 ((t)->bottom > (t)->top))


typedef struct _PIXEL_MANIPULATE_GRADIENT_WALKER
{
	float	a_s, a_b;
	float	r_s, r_b;
	float	g_s, g_b;
	float	b_s, b_b;
	PIXEL_MANIPULATE_FIXED_48_16 left_x;
	PIXEL_MANIPULATE_FIXED_48_16 right_x;

	PIXEL_MANIPULATE_GRADIENT_STOP *stops;
	int	num_stops;
	ePIXEL_MANIPULATE_REPEAT_TYPE repeat;

	int need_reset;
} PIXEL_MANIPULATE_GRADIENT_WALKER;

typedef void (*PIXEL_MANIPULATE_GRADIENT_WALKER_WRITE)(
	PIXEL_MANIPULATE_GRADIENT_WALKER* walker,
	PIXEL_MANIPULATE_FIXED_48_16 x,
	uint32* buffer
);

typedef void (*PIXEL_MANIPULATE_GRADIENT_WALKER_FILL)(
	PIXEL_MANIPULATE_GRADIENT_WALKER* walker,
	PIXEL_MANIPULATE_FIXED_48_16 x,
	uint32* buffer,
	uint32* end
);

static INLINE int ClipGeneralImage(
	PIXEL_MANIPULATE_REGION32* region,
	PIXEL_MANIPULATE_REGION32* clip,
	int dx,
	int dy
)
{
	if(PIXEL_MANIPULATE_REGION32_NUM_RECTS(region) == 1 &&
		PIXEL_MANIPULATE_REGION32_NUM_RECTS(clip) == 1)
	{
		PIXEL_MANIPULATE_BOX32 *rbox = PixelManipulateRegion32Rectangles(region, NULL);
		PIXEL_MANIPULATE_BOX32 *cbox = PixelManipulateRegion32Rectangles(clip, NULL);
		int v;

		if(rbox->x1 < (v = cbox->x1 + dx))
			rbox->x1 = v;
		if(rbox->x2 > (v = cbox->x2 + dx))
			rbox->x2 = v;
		if(rbox->y1 < (v = cbox->y1 + dy))
			rbox->y1 = v;
		if(rbox->y2 > (v = cbox->y2 + dy))
			rbox->y2 = v;
		if(rbox->x1 >= rbox->x2 || rbox->y1 >= rbox->y2)
		{
			InitializePixelManipulateRegion32(region);
			return FALSE;
		}
	}
	else if(!PixelManipulateRegion32NotEmpty(clip))
	{
		return FALSE;
	}
	else
	{
		if(dx || dy)
		{
			PixelManipulateRegion32Translate(region, -dx, -dy);
		}
		if(!PixelManipulateRegion32Intersect(region, region, clip))
		{
			return FALSE;
		}
		if(dx || dy)
		{
			PixelManipulateRegion32Translate(region, dx, dy);
		}
	}

	return PixelManipulateRegion32NotEmpty(region);
}

static INLINE int ClipSourceImage(
	PIXEL_MANIPULATE_REGION32* region,
	PIXEL_MANIPULATE_IMAGE* image,
	int dx,
	int dy
)
{
	/* Source clips are ignored, unless they are explicitly turned on
	 * and the clip in question was set by an X client. (Because if
	 * the clip was not set by a client, then it is a hierarchy
	 * clip and those should always be ignored for sources).
	 */
	if (!image->common.clip_sources || !image->common.client_clip)
		return TRUE;

	return ClipGeneralImage (region, &image->common.clip_region, dx, dy);
}

static INLINE int PixelManipulateRegion32NotEmpty(PIXEL_MANIPULATE_REGION32* region)
{
	return TRUE; // (FALSE == (region->data && region->data != PIXEL_MANIPULATE_EMPTY_DATA && region->data->num_rects == 0));
}


#ifdef __cplusplus
extern "C" {
#endif

extern uint32 PixelManipulateImageGetSolid(
	struct _PIXEL_MANIPULATE_IMPLEMENTATION* imp,
	union _PIXEL_MANIPULATE_IMAGE*		 image,
	ePIXEL_MANIPULATE_FORMAT	 format
);

extern uint32* PixelManipulateIteratorGetScanlineNoop(struct _PIXEL_MANIPULATE_ITERATION* iter, const uint32* mask);

extern void PixelManipulateGradientWalkerWriteNarrow(
	PIXEL_MANIPULATE_GRADIENT_WALKER* walker,
	PIXEL_MANIPULATE_FIXED_48_16 x,
	uint32* buffer
);

extern void PixelManipulateGradientWalkerWriteWide(
	PIXEL_MANIPULATE_GRADIENT_WALKER* walker,
	PIXEL_MANIPULATE_FIXED_48_16 x,
	uint32* buffer
);

extern void PixelManipulateGradientWalkerFillNarrow(
	PIXEL_MANIPULATE_GRADIENT_WALKER* walker,
	PIXEL_MANIPULATE_FIXED_48_16 x,
	uint32* buffer,
	uint32* end
);

extern void PixelManipulateGradientWalkerFillWide(
	PIXEL_MANIPULATE_GRADIENT_WALKER* walker,
	PIXEL_MANIPULATE_FIXED_48_16 x,
	uint32* buffer,
	uint32* end
);

#ifdef __cplusplus
}
#endif

#endif // #ifndef _INCLUDED_PIXEL_MANIPULATE_PRIVE_H_
