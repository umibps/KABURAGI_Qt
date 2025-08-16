#include "pixel_manipulate_composite.h"
#include "pixel_manipulate.h"
#include "pixel_manipulate_private.h"
#include "pixel_manipulate_inline.h"

#if defined USE_SSSE3 || defined USE_ARM_IWMMXT || defined USE_LOONGSON_MMI

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <mmintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <tmmintrin.h>

typedef struct
{
	int		y;
	uint64 *	buffer;
} line_t;

typedef struct
{
	line_t		lines[2];
	PIXEL_MANIPULATE_FIXED	y;
	PIXEL_MANIPULATE_FIXED	x;
	uint64		data[1];
} bilinear_info_t;

static void
ssse3_fetch_horizontal (PIXEL_MANIPULATE_BITS_IMAGE *image, line_t *line,
			int y, PIXEL_MANIPULATE_FIXED x, PIXEL_MANIPULATE_FIXED ux, int n)
{
	uint32 *bits = image->bits + y * image->row_stride;
	__m128i vx = _mm_set_epi16 (
	- (x + 1), x, - (x + 1), x,
	- (x + ux + 1), x + ux,  - (x + ux + 1), x + ux);
	__m128i vux = _mm_set_epi16 (
	- 2 * ux, 2 * ux, - 2 * ux, 2 * ux,
	- 2 * ux, 2 * ux, - 2 * ux, 2 * ux);
	__m128i vaddc = _mm_set_epi16 (1, 0, 1, 0, 1, 0, 1, 0);
	__m128i *b = (__m128i *)line->buffer;
	__m128i vrl0, vrl1;

	while ((n -= 2) >= 0)
	{
	__m128i vw, vr, s;

	vrl1 = _mm_loadl_epi64 (
		(__m128i *)(bits + PIXEL_MANIPULATE_FIXED_TO_INTEGER (x + ux)));
	/* vrl1: R1, L1 */

	final_pixel:
	vrl0 = _mm_loadl_epi64 (
		(__m128i *)(bits + PIXEL_MANIPULATE_FIXED_TO_INTEGER (x)));
	/* vrl0: R0, L0 */

	/* The weights are based on vx which is a vector of
	 *
	 *	- (x + 1), x, - (x + 1), x,
	 *		  - (x + ux + 1), x + ux, - (x + ux + 1), x + ux
	 *
	 * so the 16 bit weights end up like this:
	 *
	 *	iw0, w0, iw0, w0, iw1, w1, iw1, w1
	 *
	 * and after shifting and packing, we get these bytes:
	 *
	 *	iw0, w0, iw0, w0, iw1, w1, iw1, w1,
	 *		iw0, w0, iw0, w0, iw1, w1, iw1, w1,
	 *
	 * which means the first and the second input pixel
	 * have to be interleaved like this:
	 *
	 *	la0, ra0, lr0, rr0, la1, ra1, lr1, rr1,
	 *		lg0, rg0, lb0, rb0, lg1, rg1, lb1, rb1
	 *
	 * before maddubsw can be used.
	 */

	vw = _mm_add_epi16 (
		vaddc, _mm_srli_epi16 (vx, 16 - BILINEAR_INTERPOLATION_BITS));
	/* vw: iw0, w0, iw0, w0, iw1, w1, iw1, w1
	 */

	vw = _mm_packus_epi16 (vw, vw);
	/* vw: iw0, w0, iw0, w0, iw1, w1, iw1, w1,
	 *		 iw0, w0, iw0, w0, iw1, w1, iw1, w1
	 */
	vx = _mm_add_epi16 (vx, vux);

	x += 2 * ux;

	vr = _mm_unpacklo_epi16 (vrl1, vrl0);
	/* vr: rar0, rar1, rgb0, rgb1, lar0, lar1, lgb0, lgb1 */

	s = _mm_shuffle_epi32 (vr, _MM_SHUFFLE (1, 0, 3, 2));
	/* s:  lar0, lar1, lgb0, lgb1, rar0, rar1, rgb0, rgb1 */

	vr = _mm_unpackhi_epi8 (vr, s);
	/* vr: la0, ra0, lr0, rr0, la1, ra1, lr1, rr1,
	 *		 lg0, rg0, lb0, rb0, lg1, rg1, lb1, rb1
	 */

	vr = _mm_maddubs_epi16 (vr, vw);

	/* When the weight is 0, the inverse weight is
	 * 128 which can't be represented in a signed byte.
	 * As a result maddubsw computes the following:
	 *
	 *	 r = l * -128 + r * 0
	 *
	 * rather than the desired
	 *
	 *	 r = l * 128 + r * 0
	 *
	 * We fix this by taking the absolute value of the
	 * result.
	 */
	vr = _mm_abs_epi16 (vr);

	/* vr: A0, R0, A1, R1, G0, B0, G1, B1 */
	_mm_store_si128 (b++, vr);
	}

	if (n == -1)
	{
	vrl1 = _mm_setzero_si128();
	goto final_pixel;
	}

	line->y = y;
}

static uint32 *
ssse3_fetch_bilinear_cover (PIXEL_MANIPULATE_ITERATION *iter, const uint32 *mask)
{
	PIXEL_MANIPULATE_FIXED fx, ux;
	bilinear_info_t *info = iter->data;
	line_t *line0, *line1;
	int y0, y1;
	int32 dist_y;
	__m128i vw;
	int i;

	fx = info->x;
	ux = iter->image->common.transform->matrix[0][0];

	y0 = PIXEL_MANIPULATE_FIXED_TO_INTEGER (info->y);
	y1 = y0 + 1;

	line0 = &info->lines[y0 & 0x01];
	line1 = &info->lines[y1 & 0x01];

	if (line0->y != y0)
	{
	ssse3_fetch_horizontal (
		&iter->image->bits, line0, y0, fx, ux, iter->width);
	}

	if (line1->y != y1)
	{
	ssse3_fetch_horizontal (
		&iter->image->bits, line1, y1, fx, ux, iter->width);
	}

	dist_y = PIXEL_MANIPULATE_FIXED_to_bilinear_weight (info->y);
	dist_y <<= (16 - BILINEAR_INTERPOLATION_BITS);

	vw = _mm_set_epi16 (
	dist_y, dist_y, dist_y, dist_y, dist_y, dist_y, dist_y, dist_y);

	for (i = 0; i + 3 < iter->width; i += 4)
	{
	__m128i top0 = _mm_load_si128 ((__m128i *)(line0->buffer + i));
	__m128i bot0 = _mm_load_si128 ((__m128i *)(line1->buffer + i));
	__m128i top1 = _mm_load_si128 ((__m128i *)(line0->buffer + i + 2));
	__m128i bot1 = _mm_load_si128 ((__m128i *)(line1->buffer + i + 2));
	__m128i r0, r1, tmp, p;

	r0 = _mm_mulhi_epu16 (
		_mm_sub_epi16 (bot0, top0), vw);
	tmp = _mm_cmplt_epi16 (bot0, top0);
	tmp = _mm_and_si128 (tmp, vw);
	r0 = _mm_sub_epi16 (r0, tmp);
	r0 = _mm_add_epi16 (r0, top0);
	r0 = _mm_srli_epi16 (r0, BILINEAR_INTERPOLATION_BITS);
	/* r0:  A0 R0 A1 R1 G0 B0 G1 B1 */
	r0 = _mm_shuffle_epi32 (r0, _MM_SHUFFLE (2, 0, 3, 1));
	/* r0:  A1 R1 G1 B1 A0 R0 G0 B0 */

	r1 = _mm_mulhi_epu16 (
		_mm_sub_epi16 (bot1, top1), vw);
	tmp = _mm_cmplt_epi16 (bot1, top1);
	tmp = _mm_and_si128 (tmp, vw);
	r1 = _mm_sub_epi16 (r1, tmp);
	r1 = _mm_add_epi16 (r1, top1);
	r1 = _mm_srli_epi16 (r1, BILINEAR_INTERPOLATION_BITS);
	r1 = _mm_shuffle_epi32 (r1, _MM_SHUFFLE (2, 0, 3, 1));
	/* r1: A3 R3 G3 B3 A2 R2 G2 B2 */

	p = _mm_packus_epi16 (r0, r1);

	_mm_storeu_si128 ((__m128i *)(iter->buffer + i), p);
	}

	while (i < iter->width)
	{
	__m128i top0 = _mm_load_si128 ((__m128i *)(line0->buffer + i));
	__m128i bot0 = _mm_load_si128 ((__m128i *)(line1->buffer + i));
	__m128i r0, tmp, p;

	r0 = _mm_mulhi_epu16 (
		_mm_sub_epi16 (bot0, top0), vw);
	tmp = _mm_cmplt_epi16 (bot0, top0);
	tmp = _mm_and_si128 (tmp, vw);
	r0 = _mm_sub_epi16 (r0, tmp);
	r0 = _mm_add_epi16 (r0, top0);
	r0 = _mm_srli_epi16 (r0, BILINEAR_INTERPOLATION_BITS);
	/* r0:  A0 R0 A1 R1 G0 B0 G1 B1 */
	r0 = _mm_shuffle_epi32 (r0, _MM_SHUFFLE (2, 0, 3, 1));
	/* r0:  A1 R1 G1 B1 A0 R0 G0 B0 */

	p = _mm_packus_epi16 (r0, r0);

	if (iter->width - i == 1)
	{
		*(uint32 *)(iter->buffer + i) = _mm_cvtsi128_si32 (p);
		i++;
	}
	else
	{
		_mm_storel_epi64 ((__m128i *)(iter->buffer + i), p);
		i += 2;
	}
	}

	info->y += iter->image->common.transform->matrix[1][1];

	return iter->buffer;
}

static void
ssse3_bilinear_cover_iter_fini (PIXEL_MANIPULATE_ITERATION *iter)
{
	free (iter->data);
}

static void
ssse3_bilinear_cover_iter_init (PIXEL_MANIPULATE_ITERATION *iter, const PIXEL_MANIPULATE_ITERATION_INFO *iter_info)
{
	int width = iter->width;
	bilinear_info_t *info;
	PIXEL_MANIPULATE_VECTOR v;

	/* Reference point is the center of the pixel */
	v.vector[0] = PIXEL_MANIPULATE_INTEGER_TO_FIXED( (iter->x) + PIXEL_MANIPULATE_FIXED_1 / 2);
	v.vector[1] = PIXEL_MANIPULATE_INTEGER_TO_FIXED( (iter->y) + PIXEL_MANIPULATE_FIXED_1 / 2);
	v.vector[2] = PIXEL_MANIPULATE_FIXED_1;

	if (!PixelManipulateTransformPoint3d (iter->image->common.transform, &v))
	goto fail;

	info = malloc (sizeof (*info) + (2 * width - 1) * sizeof (uint64) + 64);
	if (!info)
	goto fail;

	info->x = v.vector[0] - PIXEL_MANIPULATE_FIXED_1 / 2;
	info->y = v.vector[1] - PIXEL_MANIPULATE_FIXED_1 / 2;

#define ALIGN(addr)							\
	((void *)((((uintptr_t)(addr)) + 15) & (~15)))

	/* It is safe to set the y coordinates to -1 initially
	 * because COVER_CLIP_BILINEAR ensures that we will only
	 * be asked to fetch lines in the [0, height) interval
	 */
	info->lines[0].y = -1;
	info->lines[0].buffer = ALIGN (&(info->data[0]));
	info->lines[1].y = -1;
	info->lines[1].buffer = ALIGN (info->lines[0].buffer + width);

	iter->get_scanline = ssse3_fetch_bilinear_cover;
	iter->finish = ssse3_bilinear_cover_iter_fini;

	iter->data = info;
	return;

fail:

	iter->get_scanline = PixelManipulateIteratorGetScanlineNoop;
	iter->finish = NULL;
}

static const PIXEL_MANIPULATE_ITERATION_INFO ssse3_iters[] =
{
	{ PIXEL_MANIPULATE_FORMAT_A8R8G8B8,
	  (FAST_PATH_STANDARD_FLAGS			|
	   FAST_PATH_SCALE_TRANSFORM		|
	   FAST_PATH_BILINEAR_FILTER		|
	   FAST_PATH_SAMPLES_COVER_CLIP_BILINEAR),
	  PIXEL_MANIPULATE_ITERATION_NARROW | PIXEL_MANIPULATE_ITERATION_SRC,
	  ssse3_bilinear_cover_iter_init,
	  NULL, NULL
	},

	{ 0 },
};

static const PIXEL_MANIPULATE_FAST_PATH ssse3_fast_paths[] =
{
	{ PIXEL_MANIPULATE_OPERATE_NONE },
};

PIXEL_MANIPULATE_IMPLEMENTATION*
	PixelManipulateImplementationCreateSSSE3(PIXEL_MANIPULATE_IMPLEMENTATION* fallback)
{
	PIXEL_MANIPULATE_IMPLEMENTATION *imp =
		PixelManipulateImplementationCreate(fallback, ssse3_fast_paths);

	imp->iteration_info = ssse3_iters;

	return imp;
}

#ifdef __cplusplus
}
#endif

#endif
