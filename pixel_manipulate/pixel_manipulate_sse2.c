#include <string.h>
#include "pixel_manipulate_composite.h"
#include "pixel_manipulate.h"
#include "pixel_manipulate_format.h"
#include "pixel_manipulate_private.h"
#include "pixel_manipulate_inline.h"

#if defined USE_SSE2 || defined USE_ARM_IWMMXT || defined USE_LOONGSON_MMI

#ifdef __cplusplus
extern "C" {
#endif

/* PSHUFD is slow on a lot of old processors, and new processors have SSSE3 */
#define PSHUFD_IS_FAST 0

#include <xmmintrin.h> /* for _mm_shuffle_pi16 and _MM_SHUFFLE */
#include <emmintrin.h> /* for SSE2 intrinsics */

static __m128i mask_0080;
static __m128i mask_00ff;
static __m128i mask_0101;
static __m128i mask_ffff;
static __m128i mask_ff000000;
static __m128i mask_alpha;

static __m128i mask_565_r;
static __m128i mask_565_g1, mask_565_g2;
static __m128i mask_565_b;
static __m128i mask_red;
static __m128i mask_green;
static __m128i mask_blue;

static __m128i mask_565_fix_rb;
static __m128i mask_565_fix_g;

static __m128i mask_565_rb;
static __m128i mask_565_pack_multiplier;

static FORCE_INLINE __m128i
unpack_32_1x128 (uint32 data)
{
	return _mm_unpacklo_epi8 (_mm_cvtsi32_si128 (data), _mm_setzero_si128 ());
}

static FORCE_INLINE void
unpack_128_2x128 (__m128i data, __m128i* data_lo, __m128i* data_hi)
{
	*data_lo = _mm_unpacklo_epi8 (data, _mm_setzero_si128 ());
	*data_hi = _mm_unpackhi_epi8 (data, _mm_setzero_si128 ());
}

static FORCE_INLINE __m128i
unpack_565_to_8888 (__m128i lo)
{
	__m128i r, g, b, rb, t;

	r = _mm_and_si128 (_mm_slli_epi32 (lo, 8), mask_red);
	g = _mm_and_si128 (_mm_slli_epi32 (lo, 5), mask_green);
	b = _mm_and_si128 (_mm_slli_epi32 (lo, 3), mask_blue);

	rb = _mm_or_si128 (r, b);
	t  = _mm_and_si128 (rb, mask_565_fix_rb);
	t  = _mm_srli_epi32 (t, 5);
	rb = _mm_or_si128 (rb, t);

	t  = _mm_and_si128 (g, mask_565_fix_g);
	t  = _mm_srli_epi32 (t, 6);
	g  = _mm_or_si128 (g, t);

	return _mm_or_si128 (rb, g);
}

static FORCE_INLINE void
unpack_565_128_4x128 (__m128i  data,
					  __m128i* data0,
					  __m128i* data1,
					  __m128i* data2,
					  __m128i* data3)
{
	__m128i lo, hi;

	lo = _mm_unpacklo_epi16 (data, _mm_setzero_si128 ());
	hi = _mm_unpackhi_epi16 (data, _mm_setzero_si128 ());

	lo = unpack_565_to_8888 (lo);
	hi = unpack_565_to_8888 (hi);

	unpack_128_2x128 (lo, data0, data1);
	unpack_128_2x128 (hi, data2, data3);
}

static FORCE_INLINE uint16
pack_565_32_16 (uint32 pixel)
{
	return (uint16) (((pixel >> 8) & 0xf800) |
			   ((pixel >> 5) & 0x07e0) |
			   ((pixel >> 3) & 0x001f));
}

static FORCE_INLINE __m128i
pack_2x128_128 (__m128i lo, __m128i hi)
{
	return _mm_packus_epi16 (lo, hi);
}

static FORCE_INLINE __m128i
pack_565_2packedx128_128 (__m128i lo, __m128i hi)
{
	__m128i rb0 = _mm_and_si128 (lo, mask_565_rb);
	__m128i rb1 = _mm_and_si128 (hi, mask_565_rb);

	__m128i t0 = _mm_madd_epi16 (rb0, mask_565_pack_multiplier);
	__m128i t1 = _mm_madd_epi16 (rb1, mask_565_pack_multiplier);

	__m128i g0 = _mm_and_si128 (lo, mask_green);
	__m128i g1 = _mm_and_si128 (hi, mask_green);

	t0 = _mm_or_si128 (t0, g0);
	t1 = _mm_or_si128 (t1, g1);

	/* Simulates _mm_packus_epi32 */
	t0 = _mm_slli_epi32 (t0, 16 - 5);
	t1 = _mm_slli_epi32 (t1, 16 - 5);
	t0 = _mm_srai_epi32 (t0, 16);
	t1 = _mm_srai_epi32 (t1, 16);
	return _mm_packs_epi32 (t0, t1);
}

static FORCE_INLINE __m128i
pack_565_2x128_128 (__m128i lo, __m128i hi)
{
	__m128i data;
	__m128i r, g1, g2, b;

	data = pack_2x128_128 (lo, hi);

	r  = _mm_and_si128 (data, mask_565_r);
	g1 = _mm_and_si128 (_mm_slli_epi32 (data, 3), mask_565_g1);
	g2 = _mm_and_si128 (_mm_srli_epi32 (data, 5), mask_565_g2);
	b  = _mm_and_si128 (_mm_srli_epi32 (data, 3), mask_565_b);

	return _mm_or_si128 (_mm_or_si128 (_mm_or_si128 (r, g1), g2), b);
}

static FORCE_INLINE __m128i
pack_565_4x128_128 (__m128i* xmm0, __m128i* xmm1, __m128i* xmm2, __m128i* xmm3)
{
	return _mm_packus_epi16 (pack_565_2x128_128 (*xmm0, *xmm1),
				 pack_565_2x128_128 (*xmm2, *xmm3));
}

static FORCE_INLINE int
is_opaque (__m128i x)
{
	__m128i ffs = _mm_cmpeq_epi8 (x, x);

	return (_mm_movemask_epi8 (_mm_cmpeq_epi8 (x, ffs)) & 0X8888) == 0X8888;
}

static FORCE_INLINE int
is_zero (__m128i x)
{
	return _mm_movemask_epi8 (
	_mm_cmpeq_epi8 (x, _mm_setzero_si128 ())) == 0xffff;
}

static FORCE_INLINE int
is_transparent (__m128i x)
{
	return (_mm_movemask_epi8 (
		_mm_cmpeq_epi8 (x, _mm_setzero_si128 ())) & 0X8888) == 0X8888;
}

static FORCE_INLINE __m128i
expand_pixel_32_1x128 (uint32 data)
{
	return _mm_shuffle_epi32 (unpack_32_1x128 (data), _MM_SHUFFLE (1, 0, 1, 0));
}

static FORCE_INLINE __m128i
expand_alpha_1x128 (__m128i data)
{
	return _mm_shufflehi_epi16 (_mm_shufflelo_epi16 (data,
							 _MM_SHUFFLE (3, 3, 3, 3)),
				_MM_SHUFFLE (3, 3, 3, 3));
}

static FORCE_INLINE void
expand_alpha_2x128 (__m128i  data_lo,
					__m128i  data_hi,
					__m128i* alpha_lo,
					__m128i* alpha_hi)
{
	__m128i lo, hi;

	lo = _mm_shufflelo_epi16 (data_lo, _MM_SHUFFLE (3, 3, 3, 3));
	hi = _mm_shufflelo_epi16 (data_hi, _MM_SHUFFLE (3, 3, 3, 3));

	*alpha_lo = _mm_shufflehi_epi16 (lo, _MM_SHUFFLE (3, 3, 3, 3));
	*alpha_hi = _mm_shufflehi_epi16 (hi, _MM_SHUFFLE (3, 3, 3, 3));
}

static FORCE_INLINE void
expand_alpha_rev_2x128 (__m128i  data_lo,
						__m128i  data_hi,
						__m128i* alpha_lo,
						__m128i* alpha_hi)
{
	__m128i lo, hi;

	lo = _mm_shufflelo_epi16 (data_lo, _MM_SHUFFLE (0, 0, 0, 0));
	hi = _mm_shufflelo_epi16 (data_hi, _MM_SHUFFLE (0, 0, 0, 0));
	*alpha_lo = _mm_shufflehi_epi16 (lo, _MM_SHUFFLE (0, 0, 0, 0));
	*alpha_hi = _mm_shufflehi_epi16 (hi, _MM_SHUFFLE (0, 0, 0, 0));
}

static FORCE_INLINE void
pix_multiply_2x128 (__m128i* data_lo,
					__m128i* data_hi,
					__m128i* alpha_lo,
					__m128i* alpha_hi,
					__m128i* ret_lo,
					__m128i* ret_hi)
{
	__m128i lo, hi;

	lo = _mm_mullo_epi16 (*data_lo, *alpha_lo);
	hi = _mm_mullo_epi16 (*data_hi, *alpha_hi);
	lo = _mm_adds_epu16 (lo, mask_0080);
	hi = _mm_adds_epu16 (hi, mask_0080);
	*ret_lo = _mm_mulhi_epu16 (lo, mask_0101);
	*ret_hi = _mm_mulhi_epu16 (hi, mask_0101);
}

static FORCE_INLINE void
pix_add_multiply_2x128 (__m128i* src_lo,
						__m128i* src_hi,
						__m128i* alpha_dst_lo,
						__m128i* alpha_dst_hi,
						__m128i* dst_lo,
						__m128i* dst_hi,
						__m128i* alpha_src_lo,
						__m128i* alpha_src_hi,
						__m128i* ret_lo,
						__m128i* ret_hi)
{
	__m128i t1_lo, t1_hi;
	__m128i t2_lo, t2_hi;

	pix_multiply_2x128 (src_lo, src_hi, alpha_dst_lo, alpha_dst_hi, &t1_lo, &t1_hi);
	pix_multiply_2x128 (dst_lo, dst_hi, alpha_src_lo, alpha_src_hi, &t2_lo, &t2_hi);

	*ret_lo = _mm_adds_epu8 (t1_lo, t2_lo);
	*ret_hi = _mm_adds_epu8 (t1_hi, t2_hi);
}

static FORCE_INLINE void
negate_2x128 (__m128i  data_lo,
			  __m128i  data_hi,
			  __m128i* neg_lo,
			  __m128i* neg_hi)
{
	*neg_lo = _mm_xor_si128 (data_lo, mask_00ff);
	*neg_hi = _mm_xor_si128 (data_hi, mask_00ff);
}

static FORCE_INLINE void
invert_colors_2x128 (__m128i  data_lo,
					 __m128i  data_hi,
					 __m128i* inv_lo,
					 __m128i* inv_hi)
{
	__m128i lo, hi;

	lo = _mm_shufflelo_epi16 (data_lo, _MM_SHUFFLE (3, 0, 1, 2));
	hi = _mm_shufflelo_epi16 (data_hi, _MM_SHUFFLE (3, 0, 1, 2));
	*inv_lo = _mm_shufflehi_epi16 (lo, _MM_SHUFFLE (3, 0, 1, 2));
	*inv_hi = _mm_shufflehi_epi16 (hi, _MM_SHUFFLE (3, 0, 1, 2));
}

static FORCE_INLINE void
over_2x128 (__m128i* src_lo,
			__m128i* src_hi,
			__m128i* alpha_lo,
			__m128i* alpha_hi,
			__m128i* dst_lo,
			__m128i* dst_hi)
{
	__m128i t1, t2;

	negate_2x128 (*alpha_lo, *alpha_hi, &t1, &t2);

	pix_multiply_2x128 (dst_lo, dst_hi, &t1, &t2, dst_lo, dst_hi);

	*dst_lo = _mm_adds_epu8 (*src_lo, *dst_lo);
	*dst_hi = _mm_adds_epu8 (*src_hi, *dst_hi);
}

static FORCE_INLINE void
over_rev_non_pre_2x128 (__m128i  src_lo,
						__m128i  src_hi,
						__m128i* dst_lo,
						__m128i* dst_hi)
{
	__m128i lo, hi;
	__m128i alpha_lo, alpha_hi;

	expand_alpha_2x128 (src_lo, src_hi, &alpha_lo, &alpha_hi);

	lo = _mm_or_si128 (alpha_lo, mask_alpha);
	hi = _mm_or_si128 (alpha_hi, mask_alpha);

	invert_colors_2x128 (src_lo, src_hi, &src_lo, &src_hi);

	pix_multiply_2x128 (&src_lo, &src_hi, &lo, &hi, &lo, &hi);

	over_2x128 (&lo, &hi, &alpha_lo, &alpha_hi, dst_lo, dst_hi);
}

static FORCE_INLINE void
in_over_2x128 (__m128i* src_lo,
			   __m128i* src_hi,
			   __m128i* alpha_lo,
			   __m128i* alpha_hi,
			   __m128i* mask_lo,
			   __m128i* mask_hi,
			   __m128i* dst_lo,
			   __m128i* dst_hi)
{
	__m128i s_lo, s_hi;
	__m128i a_lo, a_hi;

	pix_multiply_2x128 (src_lo,   src_hi, mask_lo, mask_hi, &s_lo, &s_hi);
	pix_multiply_2x128 (alpha_lo, alpha_hi, mask_lo, mask_hi, &a_lo, &a_hi);

	over_2x128 (&s_lo, &s_hi, &a_lo, &a_hi, dst_lo, dst_hi);
}

/* load 4 pixels from a 16-byte boundary aligned address */
static FORCE_INLINE __m128i
load_128_aligned (__m128i* src)
{
	return _mm_load_si128 (src);
}

/* load 4 pixels from a unaligned address */
static FORCE_INLINE __m128i
load_128_unaligned (const __m128i* src)
{
	return _mm_loadu_si128 (src);
}

/* save 4 pixels using Write Combining memory on a 16-byte
 * boundary aligned address
 */
static FORCE_INLINE void
save_128_write_combining (__m128i* dst,
						  __m128i  data)
{
	_mm_stream_si128 (dst, data);
}

/* save 4 pixels on a 16-byte boundary aligned address */
static FORCE_INLINE void
save_128_aligned (__m128i* dst,
				  __m128i  data)
{
	_mm_store_si128 (dst, data);
}

/* save 4 pixels on a unaligned address */
static FORCE_INLINE void
save_128_unaligned (__m128i* dst,
					__m128i  data)
{
	_mm_storeu_si128 (dst, data);
}

static FORCE_INLINE __m128i
load_32_1x128 (uint32 data)
{
	return _mm_cvtsi32_si128 (data);
}

static FORCE_INLINE __m128i
expand_alpha_rev_1x128 (__m128i data)
{
	return _mm_shufflelo_epi16 (data, _MM_SHUFFLE (0, 0, 0, 0));
}

static FORCE_INLINE __m128i
expand_pixel_8_1x128 (uint8 data)
{
	return _mm_shufflelo_epi16 (
	unpack_32_1x128 ((uint32)data), _MM_SHUFFLE (0, 0, 0, 0));
}

static FORCE_INLINE __m128i
pix_multiply_1x128 (__m128i data,
			__m128i alpha)
{
	return _mm_mulhi_epu16 (_mm_adds_epu16 (_mm_mullo_epi16 (data, alpha),
						mask_0080),
				mask_0101);
}

static FORCE_INLINE __m128i
pix_add_multiply_1x128 (__m128i* src,
			__m128i* alpha_dst,
			__m128i* dst,
			__m128i* alpha_src)
{
	__m128i t1 = pix_multiply_1x128 (*src, *alpha_dst);
	__m128i t2 = pix_multiply_1x128 (*dst, *alpha_src);

	return _mm_adds_epu8 (t1, t2);
}

static FORCE_INLINE __m128i
negate_1x128 (__m128i data)
{
	return _mm_xor_si128 (data, mask_00ff);
}

static FORCE_INLINE __m128i
invert_colors_1x128 (__m128i data)
{
	return _mm_shufflelo_epi16 (data, _MM_SHUFFLE (3, 0, 1, 2));
}

static FORCE_INLINE __m128i
over_1x128 (__m128i src, __m128i alpha, __m128i dst)
{
	return _mm_adds_epu8 (src, pix_multiply_1x128 (dst, negate_1x128 (alpha)));
}

static FORCE_INLINE __m128i
in_over_1x128 (__m128i* src, __m128i* alpha, __m128i* mask, __m128i* dst)
{
	return over_1x128 (pix_multiply_1x128 (*src, *mask),
			   pix_multiply_1x128 (*alpha, *mask),
			   *dst);
}

static FORCE_INLINE __m128i
over_rev_non_pre_1x128 (__m128i src, __m128i dst)
{
	__m128i alpha = expand_alpha_1x128 (src);

	return over_1x128 (pix_multiply_1x128 (invert_colors_1x128 (src),
					   _mm_or_si128 (alpha, mask_alpha)),
			   alpha,
			   dst);
}

static FORCE_INLINE uint32
pack_1x128_32 (__m128i data)
{
	return _mm_cvtsi128_si32 (_mm_packus_epi16 (data, _mm_setzero_si128 ()));
}

static FORCE_INLINE __m128i
expand565_16_1x128 (uint16 pixel)
{
	__m128i m = _mm_cvtsi32_si128 (pixel);

	m = unpack_565_to_8888 (m);

	return _mm_unpacklo_epi8 (m, _mm_setzero_si128 ());
}

static FORCE_INLINE uint32
core_combine_over_u_pixel_sse2 (uint32 src, uint32 dst)
{
	uint8 a;
	__m128i xmms;

	a = src >> 24;

	if (a == 0xff)
	{
	return src;
	}
	else if (src)
	{
	xmms = unpack_32_1x128 (src);
	return pack_1x128_32 (
		over_1x128 (xmms, expand_alpha_1x128 (xmms),
			unpack_32_1x128 (dst)));
	}

	return dst;
}

static FORCE_INLINE uint32
combine1 (const uint32 *ps, const uint32 *pm)
{
	uint32 s;
	memcpy(&s, ps, sizeof(uint32));

	if (pm)
	{
	__m128i ms, mm;

	mm = unpack_32_1x128 (*pm);
	mm = expand_alpha_1x128 (mm);

	ms = unpack_32_1x128 (s);
	ms = pix_multiply_1x128 (ms, mm);

	s = pack_1x128_32 (ms);
	}

	return s;
}

static FORCE_INLINE __m128i
combine4 (const __m128i *ps, const __m128i *pm)
{
	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_msk_lo, xmm_msk_hi;
	__m128i s;

	if (pm)
	{
	xmm_msk_lo = load_128_unaligned (pm);

	if (is_transparent (xmm_msk_lo))
		return _mm_setzero_si128 ();
	}

	s = load_128_unaligned (ps);

	if (pm)
	{
	unpack_128_2x128 (s, &xmm_src_lo, &xmm_src_hi);
	unpack_128_2x128 (xmm_msk_lo, &xmm_msk_lo, &xmm_msk_hi);

	expand_alpha_2x128 (xmm_msk_lo, xmm_msk_hi, &xmm_msk_lo, &xmm_msk_hi);

	pix_multiply_2x128 (&xmm_src_lo, &xmm_src_hi,
				&xmm_msk_lo, &xmm_msk_hi,
				&xmm_src_lo, &xmm_src_hi);

	s = pack_2x128_128 (xmm_src_lo, xmm_src_hi);
	}

	return s;
}

static FORCE_INLINE void
core_combine_over_u_sse2_mask (uint32 *	  pd,
				   const uint32*	ps,
				   const uint32*	pm,
				   int				w)
{
	uint32 s, d;

	/* Align dst on a 16-byte boundary */
	while (w && ((uintptr_t)pd & 15))
	{
	d = *pd;
	s = combine1 (ps, pm);

	if (s)
		*pd = core_combine_over_u_pixel_sse2 (s, d);
	pd++;
	ps++;
	pm++;
	w--;
	}

	while (w >= 4)
	{
	__m128i mask = load_128_unaligned ((__m128i *)pm);

	if (!is_zero (mask))
	{
		__m128i src;
		__m128i src_hi, src_lo;
		__m128i mask_hi, mask_lo;
		__m128i alpha_hi, alpha_lo;

		src = load_128_unaligned ((__m128i *)ps);

		if (is_opaque (_mm_and_si128 (src, mask)))
		{
		save_128_aligned ((__m128i *)pd, src);
		}
		else
		{
		__m128i dst = load_128_aligned ((__m128i *)pd);
		__m128i dst_hi, dst_lo;

		unpack_128_2x128 (mask, &mask_lo, &mask_hi);
		unpack_128_2x128 (src, &src_lo, &src_hi);

		expand_alpha_2x128 (mask_lo, mask_hi, &mask_lo, &mask_hi);
		pix_multiply_2x128 (&src_lo, &src_hi,
					&mask_lo, &mask_hi,
					&src_lo, &src_hi);

		unpack_128_2x128 (dst, &dst_lo, &dst_hi);

		expand_alpha_2x128 (src_lo, src_hi,
					&alpha_lo, &alpha_hi);

		over_2x128 (&src_lo, &src_hi, &alpha_lo, &alpha_hi,
				&dst_lo, &dst_hi);

		save_128_aligned (
			(__m128i *)pd,
			pack_2x128_128 (dst_lo, dst_hi));
		}
	}

	pm += 4;
	ps += 4;
	pd += 4;
	w -= 4;
	}
	while (w)
	{
	d = *pd;
	s = combine1 (ps, pm);

	if (s)
		*pd = core_combine_over_u_pixel_sse2 (s, d);
	pd++;
	ps++;
	pm++;

	w--;
	}
}

static FORCE_INLINE void
core_combine_over_u_sse2_no_mask (uint32 *	  pd,
				  const uint32*	ps,
				  int				w)
{
	uint32 s, d;

	/* Align dst on a 16-byte boundary */
	while (w && ((uintptr_t)pd & 15))
	{
	d = *pd;
	s = *ps;

	if (s)
		*pd = core_combine_over_u_pixel_sse2 (s, d);
	pd++;
	ps++;
	w--;
	}

	while (w >= 4)
	{
	__m128i src;
	__m128i src_hi, src_lo, dst_hi, dst_lo;
	__m128i alpha_hi, alpha_lo;

	src = load_128_unaligned ((__m128i *)ps);

	if (!is_zero (src))
	{
		if (is_opaque (src))
		{
		save_128_aligned ((__m128i *)pd, src);
		}
		else
		{
		__m128i dst = load_128_aligned ((__m128i *)pd);

		unpack_128_2x128 (src, &src_lo, &src_hi);
		unpack_128_2x128 (dst, &dst_lo, &dst_hi);

		expand_alpha_2x128 (src_lo, src_hi,
					&alpha_lo, &alpha_hi);
		over_2x128 (&src_lo, &src_hi, &alpha_lo, &alpha_hi,
				&dst_lo, &dst_hi);

		save_128_aligned (
			(__m128i *)pd,
			pack_2x128_128 (dst_lo, dst_hi));
		}
	}

	ps += 4;
	pd += 4;
	w -= 4;
	}
	while (w)
	{
	d = *pd;
	s = *ps;

	if (s)
		*pd = core_combine_over_u_pixel_sse2 (s, d);
	pd++;
	ps++;

	w--;
	}
}

static FORCE_INLINE void
sse2_combine_over_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					 ePIXEL_MANIPULATE_OPERATE			 op,
					 uint32 *			   pd,
					 const uint32 *		 ps,
					 const uint32 *		 pm,
					 int					  w)
{
	if (pm)
	core_combine_over_u_sse2_mask (pd, ps, pm, w);
	else
	core_combine_over_u_sse2_no_mask (pd, ps, w);
}

static void
sse2_combine_over_reverse_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							 ePIXEL_MANIPULATE_OPERATE			 op,
							 uint32 *			   pd,
							 const uint32 *		 ps,
							 const uint32 *		 pm,
							 int					  w)
{
	uint32 s, d;

	__m128i xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_alpha_lo, xmm_alpha_hi;

	/* Align dst on a 16-byte boundary */
	while (w &&
		   ((uintptr_t)pd & 15))
	{
	d = *pd;
	s = combine1 (ps, pm);

	*pd++ = core_combine_over_u_pixel_sse2 (d, s);
	w--;
	ps++;
	if (pm)
		pm++;
	}

	while (w >= 4)
	{
	/* I'm loading unaligned because I'm not sure
	 * about the address alignment.
	 */
	xmm_src_hi = combine4 ((__m128i*)ps, (__m128i*)pm);
	xmm_dst_hi = load_128_aligned ((__m128i*) pd);

	unpack_128_2x128 (xmm_src_hi, &xmm_src_lo, &xmm_src_hi);
	unpack_128_2x128 (xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);

	expand_alpha_2x128 (xmm_dst_lo, xmm_dst_hi,
				&xmm_alpha_lo, &xmm_alpha_hi);

	over_2x128 (&xmm_dst_lo, &xmm_dst_hi,
			&xmm_alpha_lo, &xmm_alpha_hi,
			&xmm_src_lo, &xmm_src_hi);

	/* rebuid the 4 pixel data and save*/
	save_128_aligned ((__m128i*)pd,
			  pack_2x128_128 (xmm_src_lo, xmm_src_hi));

	w -= 4;
	ps += 4;
	pd += 4;

	if (pm)
		pm += 4;
	}

	while (w)
	{
	d = *pd;
	s = combine1 (ps, pm);

	*pd++ = core_combine_over_u_pixel_sse2 (d, s);
	ps++;
	w--;
	if (pm)
		pm++;
	}
}

static FORCE_INLINE uint32
core_combine_in_u_pixel_sse2 (uint32 src, uint32 dst)
{
	uint32 maska = src >> 24;

	if (maska == 0)
	{
	return 0;
	}
	else if (maska != 0xff)
	{
	return pack_1x128_32 (
		pix_multiply_1x128 (unpack_32_1x128 (dst),
				expand_alpha_1x128 (unpack_32_1x128 (src))));
	}

	return dst;
}

static void
sse2_combine_in_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
				   ePIXEL_MANIPULATE_OPERATE			 op,
				   uint32 *			   pd,
				   const uint32 *		 ps,
				   const uint32 *		 pm,
				   int					  w)
{
	uint32 s, d;

	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst_lo, xmm_dst_hi;

	while (w && ((uintptr_t)pd & 15))
	{
	s = combine1 (ps, pm);
	d = *pd;

	*pd++ = core_combine_in_u_pixel_sse2 (d, s);
	w--;
	ps++;
	if (pm)
		pm++;
	}

	while (w >= 4)
	{
	xmm_dst_hi = load_128_aligned ((__m128i*) pd);
	xmm_src_hi = combine4 ((__m128i*) ps, (__m128i*) pm);

	unpack_128_2x128 (xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);
	expand_alpha_2x128 (xmm_dst_lo, xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);

	unpack_128_2x128 (xmm_src_hi, &xmm_src_lo, &xmm_src_hi);
	pix_multiply_2x128 (&xmm_src_lo, &xmm_src_hi,
				&xmm_dst_lo, &xmm_dst_hi,
				&xmm_dst_lo, &xmm_dst_hi);

	save_128_aligned ((__m128i*)pd,
			  pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

	ps += 4;
	pd += 4;
	w -= 4;
	if (pm)
		pm += 4;
	}

	while (w)
	{
	s = combine1 (ps, pm);
	d = *pd;

	*pd++ = core_combine_in_u_pixel_sse2 (d, s);
	w--;
	ps++;
	if (pm)
		pm++;
	}
}

static void
sse2_combine_in_reverse_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
						   ePIXEL_MANIPULATE_OPERATE			 op,
						   uint32 *			   pd,
						   const uint32 *		 ps,
						   const uint32 *		 pm,
						   int					  w)
{
	uint32 s, d;

	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst_lo, xmm_dst_hi;

	while (w && ((uintptr_t)pd & 15))
	{
	s = combine1 (ps, pm);
	d = *pd;

	*pd++ = core_combine_in_u_pixel_sse2 (s, d);
	ps++;
	w--;
	if (pm)
		pm++;
	}

	while (w >= 4)
	{
	xmm_dst_hi = load_128_aligned ((__m128i*) pd);
	xmm_src_hi = combine4 ((__m128i*) ps, (__m128i*)pm);

	unpack_128_2x128 (xmm_src_hi, &xmm_src_lo, &xmm_src_hi);
	expand_alpha_2x128 (xmm_src_lo, xmm_src_hi, &xmm_src_lo, &xmm_src_hi);

	unpack_128_2x128 (xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);
	pix_multiply_2x128 (&xmm_dst_lo, &xmm_dst_hi,
				&xmm_src_lo, &xmm_src_hi,
				&xmm_dst_lo, &xmm_dst_hi);

	save_128_aligned (
		(__m128i*)pd, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

	ps += 4;
	pd += 4;
	w -= 4;
	if (pm)
		pm += 4;
	}

	while (w)
	{
	s = combine1 (ps, pm);
	d = *pd;

	*pd++ = core_combine_in_u_pixel_sse2 (s, d);
	w--;
	ps++;
	if (pm)
		pm++;
	}
}

static void
sse2_combine_out_reverse_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							ePIXEL_MANIPULATE_OPERATE			 op,
							uint32 *			   pd,
							const uint32 *		 ps,
							const uint32 *		 pm,
							int					  w)
{
	while (w && ((uintptr_t)pd & 15))
	{
	uint32 s = combine1 (ps, pm);
	uint32 d = *pd;

	*pd++ = pack_1x128_32 (
		pix_multiply_1x128 (
		unpack_32_1x128 (d), negate_1x128 (
			expand_alpha_1x128 (unpack_32_1x128 (s)))));

	if (pm)
		pm++;
	ps++;
	w--;
	}

	while (w >= 4)
	{
	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst_lo, xmm_dst_hi;

	xmm_src_hi = combine4 ((__m128i*)ps, (__m128i*)pm);
	xmm_dst_hi = load_128_aligned ((__m128i*) pd);

	unpack_128_2x128 (xmm_src_hi, &xmm_src_lo, &xmm_src_hi);
	unpack_128_2x128 (xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);

	expand_alpha_2x128 (xmm_src_lo, xmm_src_hi, &xmm_src_lo, &xmm_src_hi);
	negate_2x128	   (xmm_src_lo, xmm_src_hi, &xmm_src_lo, &xmm_src_hi);

	pix_multiply_2x128 (&xmm_dst_lo, &xmm_dst_hi,
				&xmm_src_lo, &xmm_src_hi,
				&xmm_dst_lo, &xmm_dst_hi);

	save_128_aligned (
		(__m128i*)pd, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

	ps += 4;
	pd += 4;
	if (pm)
		pm += 4;

	w -= 4;
	}

	while (w)
	{
	uint32 s = combine1 (ps, pm);
	uint32 d = *pd;

	*pd++ = pack_1x128_32 (
		pix_multiply_1x128 (
		unpack_32_1x128 (d), negate_1x128 (
			expand_alpha_1x128 (unpack_32_1x128 (s)))));
	ps++;
	if (pm)
		pm++;
	w--;
	}
}

static void
sse2_combine_out_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					ePIXEL_MANIPULATE_OPERATE			 op,
					uint32 *			   pd,
					const uint32 *		 ps,
					const uint32 *		 pm,
					int					  w)
{
	while (w && ((uintptr_t)pd & 15))
	{
	uint32 s = combine1 (ps, pm);
	uint32 d = *pd;

	*pd++ = pack_1x128_32 (
		pix_multiply_1x128 (
		unpack_32_1x128 (s), negate_1x128 (
			expand_alpha_1x128 (unpack_32_1x128 (d)))));
	w--;
	ps++;
	if (pm)
		pm++;
	}

	while (w >= 4)
	{
	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst_lo, xmm_dst_hi;

	xmm_src_hi = combine4 ((__m128i*) ps, (__m128i*)pm);
	xmm_dst_hi = load_128_aligned ((__m128i*) pd);

	unpack_128_2x128 (xmm_src_hi, &xmm_src_lo, &xmm_src_hi);
	unpack_128_2x128 (xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);

	expand_alpha_2x128 (xmm_dst_lo, xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);
	negate_2x128	   (xmm_dst_lo, xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);

	pix_multiply_2x128 (&xmm_src_lo, &xmm_src_hi,
				&xmm_dst_lo, &xmm_dst_hi,
				&xmm_dst_lo, &xmm_dst_hi);

	save_128_aligned (
		(__m128i*)pd, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

	ps += 4;
	pd += 4;
	w -= 4;
	if (pm)
		pm += 4;
	}

	while (w)
	{
	uint32 s = combine1 (ps, pm);
	uint32 d = *pd;

	*pd++ = pack_1x128_32 (
		pix_multiply_1x128 (
		unpack_32_1x128 (s), negate_1x128 (
			expand_alpha_1x128 (unpack_32_1x128 (d)))));
	w--;
	ps++;
	if (pm)
		pm++;
	}
}

static FORCE_INLINE uint32
core_combine_atop_u_pixel_sse2 (uint32 src,
								uint32 dst)
{
	__m128i s = unpack_32_1x128 (src);
	__m128i d = unpack_32_1x128 (dst);

	__m128i sa = negate_1x128 (expand_alpha_1x128 (s));
	__m128i da = expand_alpha_1x128 (d);

	return pack_1x128_32 (pix_add_multiply_1x128 (&s, &da, &d, &sa));
}

static void
sse2_combine_atop_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					 ePIXEL_MANIPULATE_OPERATE			 op,
					 uint32 *			   pd,
					 const uint32 *		 ps,
					 const uint32 *		 pm,
					 int					  w)
{
	uint32 s, d;

	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_alpha_src_lo, xmm_alpha_src_hi;
	__m128i xmm_alpha_dst_lo, xmm_alpha_dst_hi;

	while (w && ((uintptr_t)pd & 15))
	{
	s = combine1 (ps, pm);
	d = *pd;

	*pd++ = core_combine_atop_u_pixel_sse2 (s, d);
	w--;
	ps++;
	if (pm)
		pm++;
	}

	while (w >= 4)
	{
	xmm_src_hi = combine4 ((__m128i*)ps, (__m128i*)pm);
	xmm_dst_hi = load_128_aligned ((__m128i*) pd);

	unpack_128_2x128 (xmm_src_hi, &xmm_src_lo, &xmm_src_hi);
	unpack_128_2x128 (xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);

	expand_alpha_2x128 (xmm_src_lo, xmm_src_hi,
				&xmm_alpha_src_lo, &xmm_alpha_src_hi);
	expand_alpha_2x128 (xmm_dst_lo, xmm_dst_hi,
				&xmm_alpha_dst_lo, &xmm_alpha_dst_hi);

	negate_2x128 (xmm_alpha_src_lo, xmm_alpha_src_hi,
			  &xmm_alpha_src_lo, &xmm_alpha_src_hi);

	pix_add_multiply_2x128 (
		&xmm_src_lo, &xmm_src_hi, &xmm_alpha_dst_lo, &xmm_alpha_dst_hi,
		&xmm_dst_lo, &xmm_dst_hi, &xmm_alpha_src_lo, &xmm_alpha_src_hi,
		&xmm_dst_lo, &xmm_dst_hi);

	save_128_aligned (
		(__m128i*)pd, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

	ps += 4;
	pd += 4;
	w -= 4;
	if (pm)
		pm += 4;
	}

	while (w)
	{
	s = combine1 (ps, pm);
	d = *pd;

	*pd++ = core_combine_atop_u_pixel_sse2 (s, d);
	w--;
	ps++;
	if (pm)
		pm++;
	}
}

static FORCE_INLINE uint32
core_combine_reverse_atop_u_pixel_sse2 (uint32 src,
										uint32 dst)
{
	__m128i s = unpack_32_1x128 (src);
	__m128i d = unpack_32_1x128 (dst);

	__m128i sa = expand_alpha_1x128 (s);
	__m128i da = negate_1x128 (expand_alpha_1x128 (d));

	return pack_1x128_32 (pix_add_multiply_1x128 (&s, &da, &d, &sa));
}

static void
sse2_combine_atop_reverse_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							 ePIXEL_MANIPULATE_OPERATE			 op,
							 uint32 *			   pd,
							 const uint32 *		 ps,
							 const uint32 *		 pm,
							 int					  w)
{
	uint32 s, d;

	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_alpha_src_lo, xmm_alpha_src_hi;
	__m128i xmm_alpha_dst_lo, xmm_alpha_dst_hi;

	while (w && ((uintptr_t)pd & 15))
	{
	s = combine1 (ps, pm);
	d = *pd;

	*pd++ = core_combine_reverse_atop_u_pixel_sse2 (s, d);
	ps++;
	w--;
	if (pm)
		pm++;
	}

	while (w >= 4)
	{
	xmm_src_hi = combine4 ((__m128i*)ps, (__m128i*)pm);
	xmm_dst_hi = load_128_aligned ((__m128i*) pd);

	unpack_128_2x128 (xmm_src_hi, &xmm_src_lo, &xmm_src_hi);
	unpack_128_2x128 (xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);

	expand_alpha_2x128 (xmm_src_lo, xmm_src_hi,
				&xmm_alpha_src_lo, &xmm_alpha_src_hi);
	expand_alpha_2x128 (xmm_dst_lo, xmm_dst_hi,
				&xmm_alpha_dst_lo, &xmm_alpha_dst_hi);

	negate_2x128 (xmm_alpha_dst_lo, xmm_alpha_dst_hi,
			  &xmm_alpha_dst_lo, &xmm_alpha_dst_hi);

	pix_add_multiply_2x128 (
		&xmm_src_lo, &xmm_src_hi, &xmm_alpha_dst_lo, &xmm_alpha_dst_hi,
		&xmm_dst_lo, &xmm_dst_hi, &xmm_alpha_src_lo, &xmm_alpha_src_hi,
		&xmm_dst_lo, &xmm_dst_hi);

	save_128_aligned (
		(__m128i*)pd, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

	ps += 4;
	pd += 4;
	w -= 4;
	if (pm)
		pm += 4;
	}

	while (w)
	{
	s = combine1 (ps, pm);
	d = *pd;

	*pd++ = core_combine_reverse_atop_u_pixel_sse2 (s, d);
	ps++;
	w--;
	if (pm)
		pm++;
	}
}

static FORCE_INLINE uint32
core_combine_xor_u_pixel_sse2 (uint32 src,
							   uint32 dst)
{
	__m128i s = unpack_32_1x128 (src);
	__m128i d = unpack_32_1x128 (dst);

	__m128i neg_d = negate_1x128 (expand_alpha_1x128 (d));
	__m128i neg_s = negate_1x128 (expand_alpha_1x128 (s));

	return pack_1x128_32 (pix_add_multiply_1x128 (&s, &neg_d, &d, &neg_s));
}

static void
sse2_combine_xor_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					ePIXEL_MANIPULATE_OPERATE			 op,
					uint32 *			   dst,
					const uint32 *		 src,
					const uint32 *		 mask,
					int					  width)
{
	int w = width;
	uint32 s, d;
	uint32* pd = dst;
	const uint32* ps = src;
	const uint32* pm = mask;

	__m128i xmm_src, xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst, xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_alpha_src_lo, xmm_alpha_src_hi;
	__m128i xmm_alpha_dst_lo, xmm_alpha_dst_hi;

	while (w && ((uintptr_t)pd & 15))
	{
	s = combine1 (ps, pm);
	d = *pd;

	*pd++ = core_combine_xor_u_pixel_sse2 (s, d);
	w--;
	ps++;
	if (pm)
		pm++;
	}

	while (w >= 4)
	{
	xmm_src = combine4 ((__m128i*) ps, (__m128i*) pm);
	xmm_dst = load_128_aligned ((__m128i*) pd);

	unpack_128_2x128 (xmm_src, &xmm_src_lo, &xmm_src_hi);
	unpack_128_2x128 (xmm_dst, &xmm_dst_lo, &xmm_dst_hi);

	expand_alpha_2x128 (xmm_src_lo, xmm_src_hi,
				&xmm_alpha_src_lo, &xmm_alpha_src_hi);
	expand_alpha_2x128 (xmm_dst_lo, xmm_dst_hi,
				&xmm_alpha_dst_lo, &xmm_alpha_dst_hi);

	negate_2x128 (xmm_alpha_src_lo, xmm_alpha_src_hi,
			  &xmm_alpha_src_lo, &xmm_alpha_src_hi);
	negate_2x128 (xmm_alpha_dst_lo, xmm_alpha_dst_hi,
			  &xmm_alpha_dst_lo, &xmm_alpha_dst_hi);

	pix_add_multiply_2x128 (
		&xmm_src_lo, &xmm_src_hi, &xmm_alpha_dst_lo, &xmm_alpha_dst_hi,
		&xmm_dst_lo, &xmm_dst_hi, &xmm_alpha_src_lo, &xmm_alpha_src_hi,
		&xmm_dst_lo, &xmm_dst_hi);

	save_128_aligned (
		(__m128i*)pd, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

	ps += 4;
	pd += 4;
	w -= 4;
	if (pm)
		pm += 4;
	}

	while (w)
	{
	s = combine1 (ps, pm);
	d = *pd;

	*pd++ = core_combine_xor_u_pixel_sse2 (s, d);
	w--;
	ps++;
	if (pm)
		pm++;
	}
}

static FORCE_INLINE void
sse2_combine_add_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					ePIXEL_MANIPULATE_OPERATE			 op,
					uint32 *			   dst,
					const uint32 *		 src,
					const uint32 *		 mask,
					int					  width)
{
	int w = width;
	uint32 s, d;
	uint32* pd = dst;
	const uint32* ps = src;
	const uint32* pm = mask;

	while (w && (uintptr_t)pd & 15)
	{
	s = combine1 (ps, pm);
	d = *pd;

	ps++;
	if (pm)
		pm++;
	*pd++ = _mm_cvtsi128_si32 (
		_mm_adds_epu8 (_mm_cvtsi32_si128 (s), _mm_cvtsi32_si128 (d)));
	w--;
	}

	while (w >= 4)
	{
	__m128i s;

	s = combine4 ((__m128i*)ps, (__m128i*)pm);

	save_128_aligned (
		(__m128i*)pd, _mm_adds_epu8 (s, load_128_aligned  ((__m128i*)pd)));

	pd += 4;
	ps += 4;
	if (pm)
		pm += 4;
	w -= 4;
	}

	while (w--)
	{
	s = combine1 (ps, pm);
	d = *pd;

	ps++;
	*pd++ = _mm_cvtsi128_si32 (
		_mm_adds_epu8 (_mm_cvtsi32_si128 (s), _mm_cvtsi32_si128 (d)));
	if (pm)
		pm++;
	}
}

static FORCE_INLINE uint32
core_combine_saturate_u_pixel_sse2 (uint32 src,
									uint32 dst)
{
	__m128i ms = unpack_32_1x128 (src);
	__m128i md = unpack_32_1x128 (dst);
	uint32 sa = src >> 24;
	uint32 da = ~dst >> 24;

	if (sa > da)
	{
	ms = pix_multiply_1x128 (
		ms, expand_alpha_1x128 (unpack_32_1x128 (DIV_UN8 (da, sa) << 24)));
	}

	return pack_1x128_32 (_mm_adds_epu16 (md, ms));
}

static void
sse2_combine_saturate_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
						 ePIXEL_MANIPULATE_OPERATE			 op,
						 uint32 *			   pd,
						 const uint32 *		 ps,
						 const uint32 *		 pm,
						 int					  w)
{
	uint32 s, d;

	uint32 pack_cmp;
	__m128i xmm_src, xmm_dst;

	while (w && (uintptr_t)pd & 15)
	{
	s = combine1 (ps, pm);
	d = *pd;

	*pd++ = core_combine_saturate_u_pixel_sse2 (s, d);
	w--;
	ps++;
	if (pm)
		pm++;
	}

	while (w >= 4)
	{
	xmm_dst = load_128_aligned  ((__m128i*)pd);
	xmm_src = combine4 ((__m128i*)ps, (__m128i*)pm);

	pack_cmp = _mm_movemask_epi8 (
		_mm_cmpgt_epi32 (
		_mm_srli_epi32 (xmm_src, 24),
		_mm_srli_epi32 (_mm_xor_si128 (xmm_dst, mask_ff000000), 24)));

	/* if some alpha src is grater than respective ~alpha dst */
	if (pack_cmp)
	{
		s = combine1 (ps++, pm);
		d = *pd;
		*pd++ = core_combine_saturate_u_pixel_sse2 (s, d);
		if (pm)
		pm++;

		s = combine1 (ps++, pm);
		d = *pd;
		*pd++ = core_combine_saturate_u_pixel_sse2 (s, d);
		if (pm)
		pm++;

		s = combine1 (ps++, pm);
		d = *pd;
		*pd++ = core_combine_saturate_u_pixel_sse2 (s, d);
		if (pm)
		pm++;

		s = combine1 (ps++, pm);
		d = *pd;
		*pd++ = core_combine_saturate_u_pixel_sse2 (s, d);
		if (pm)
		pm++;
	}
	else
	{
		save_128_aligned ((__m128i*)pd, _mm_adds_epu8 (xmm_dst, xmm_src));

		pd += 4;
		ps += 4;
		if (pm)
		pm += 4;
	}

	w -= 4;
	}

	while (w--)
	{
	s = combine1 (ps, pm);
	d = *pd;

	*pd++ = core_combine_saturate_u_pixel_sse2 (s, d);
	ps++;
	if (pm)
		pm++;
	}
}

static void
sse2_combine_src_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					 ePIXEL_MANIPULATE_OPERATE			 op,
					 uint32 *			   pd,
					 const uint32 *		 ps,
					 const uint32 *		 pm,
					 int					  w)
{
	uint32 s, m;

	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_mask_lo, xmm_mask_hi;
	__m128i xmm_dst_lo, xmm_dst_hi;

	while (w && (uintptr_t)pd & 15)
	{
	s = *ps++;
	m = *pm++;
	*pd++ = pack_1x128_32 (
		pix_multiply_1x128 (unpack_32_1x128 (s), unpack_32_1x128 (m)));
	w--;
	}

	while (w >= 4)
	{
	xmm_src_hi = load_128_unaligned ((__m128i*)ps);
	xmm_mask_hi = load_128_unaligned ((__m128i*)pm);

	unpack_128_2x128 (xmm_src_hi, &xmm_src_lo, &xmm_src_hi);
	unpack_128_2x128 (xmm_mask_hi, &xmm_mask_lo, &xmm_mask_hi);

	pix_multiply_2x128 (&xmm_src_lo, &xmm_src_hi,
				&xmm_mask_lo, &xmm_mask_hi,
				&xmm_dst_lo, &xmm_dst_hi);

	save_128_aligned (
		(__m128i*)pd, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

	ps += 4;
	pd += 4;
	pm += 4;
	w -= 4;
	}

	while (w)
	{
	s = *ps++;
	m = *pm++;
	*pd++ = pack_1x128_32 (
		pix_multiply_1x128 (unpack_32_1x128 (s), unpack_32_1x128 (m)));
	w--;
	}
}

static FORCE_INLINE uint32
core_combine_over_ca_pixel_sse2 (uint32 src,
								 uint32 mask,
								 uint32 dst)
{
	__m128i s = unpack_32_1x128 (src);
	__m128i expAlpha = expand_alpha_1x128 (s);
	__m128i unpk_mask = unpack_32_1x128 (mask);
	__m128i unpk_dst  = unpack_32_1x128 (dst);

	return pack_1x128_32 (in_over_1x128 (&s, &expAlpha, &unpk_mask, &unpk_dst));
}

static void
sse2_combine_over_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					  ePIXEL_MANIPULATE_OPERATE			 op,
					  uint32 *			   pd,
					  const uint32 *		 ps,
					  const uint32 *		 pm,
					  int					  w)
{
	uint32 s, m, d;

	__m128i xmm_alpha_lo, xmm_alpha_hi;
	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_mask_lo, xmm_mask_hi;

	while (w && (uintptr_t)pd & 15)
	{
	s = *ps++;
	m = *pm++;
	d = *pd;

	*pd++ = core_combine_over_ca_pixel_sse2 (s, m, d);
	w--;
	}

	while (w >= 4)
	{
	xmm_dst_hi = load_128_aligned ((__m128i*)pd);
	xmm_src_hi = load_128_unaligned ((__m128i*)ps);
	xmm_mask_hi = load_128_unaligned ((__m128i*)pm);

	unpack_128_2x128 (xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);
	unpack_128_2x128 (xmm_src_hi, &xmm_src_lo, &xmm_src_hi);
	unpack_128_2x128 (xmm_mask_hi, &xmm_mask_lo, &xmm_mask_hi);

	expand_alpha_2x128 (xmm_src_lo, xmm_src_hi,
				&xmm_alpha_lo, &xmm_alpha_hi);

	in_over_2x128 (&xmm_src_lo, &xmm_src_hi,
			   &xmm_alpha_lo, &xmm_alpha_hi,
			   &xmm_mask_lo, &xmm_mask_hi,
			   &xmm_dst_lo, &xmm_dst_hi);

	save_128_aligned (
		(__m128i*)pd, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

	ps += 4;
	pd += 4;
	pm += 4;
	w -= 4;
	}

	while (w)
	{
	s = *ps++;
	m = *pm++;
	d = *pd;

	*pd++ = core_combine_over_ca_pixel_sse2 (s, m, d);
	w--;
	}
}

static FORCE_INLINE uint32
core_combine_over_reverse_ca_pixel_sse2 (uint32 src,
										 uint32 mask,
										 uint32 dst)
{
	__m128i d = unpack_32_1x128 (dst);

	return pack_1x128_32 (
	over_1x128 (d, expand_alpha_1x128 (d),
			pix_multiply_1x128 (unpack_32_1x128 (src),
					unpack_32_1x128 (mask))));
}

static void
sse2_combine_over_reverse_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							  ePIXEL_MANIPULATE_OPERATE			 op,
							  uint32 *			   pd,
							  const uint32 *		 ps,
							  const uint32 *		 pm,
							  int					  w)
{
	uint32 s, m, d;

	__m128i xmm_alpha_lo, xmm_alpha_hi;
	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_mask_lo, xmm_mask_hi;

	while (w && (uintptr_t)pd & 15)
	{
	s = *ps++;
	m = *pm++;
	d = *pd;

	*pd++ = core_combine_over_reverse_ca_pixel_sse2 (s, m, d);
	w--;
	}

	while (w >= 4)
	{
	xmm_dst_hi = load_128_aligned ((__m128i*)pd);
	xmm_src_hi = load_128_unaligned ((__m128i*)ps);
	xmm_mask_hi = load_128_unaligned ((__m128i*)pm);

	unpack_128_2x128 (xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);
	unpack_128_2x128 (xmm_src_hi, &xmm_src_lo, &xmm_src_hi);
	unpack_128_2x128 (xmm_mask_hi, &xmm_mask_lo, &xmm_mask_hi);

	expand_alpha_2x128 (xmm_dst_lo, xmm_dst_hi,
				&xmm_alpha_lo, &xmm_alpha_hi);
	pix_multiply_2x128 (&xmm_src_lo, &xmm_src_hi,
				&xmm_mask_lo, &xmm_mask_hi,
				&xmm_mask_lo, &xmm_mask_hi);

	over_2x128 (&xmm_dst_lo, &xmm_dst_hi,
			&xmm_alpha_lo, &xmm_alpha_hi,
			&xmm_mask_lo, &xmm_mask_hi);

	save_128_aligned (
		(__m128i*)pd, pack_2x128_128 (xmm_mask_lo, xmm_mask_hi));

	ps += 4;
	pd += 4;
	pm += 4;
	w -= 4;
	}

	while (w)
	{
	s = *ps++;
	m = *pm++;
	d = *pd;

	*pd++ = core_combine_over_reverse_ca_pixel_sse2 (s, m, d);
	w--;
	}
}

static void
sse2_combine_in_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					ePIXEL_MANIPULATE_OPERATE			 op,
					uint32 *			   pd,
					const uint32 *		 ps,
					const uint32 *		 pm,
					int					  w)
{
	uint32 s, m, d;

	__m128i xmm_alpha_lo, xmm_alpha_hi;
	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_mask_lo, xmm_mask_hi;

	while (w && (uintptr_t)pd & 15)
	{
	s = *ps++;
	m = *pm++;
	d = *pd;

	*pd++ = pack_1x128_32 (
		pix_multiply_1x128 (
		pix_multiply_1x128 (unpack_32_1x128 (s), unpack_32_1x128 (m)),
		expand_alpha_1x128 (unpack_32_1x128 (d))));

	w--;
	}

	while (w >= 4)
	{
	xmm_dst_hi = load_128_aligned ((__m128i*)pd);
	xmm_src_hi = load_128_unaligned ((__m128i*)ps);
	xmm_mask_hi = load_128_unaligned ((__m128i*)pm);

	unpack_128_2x128 (xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);
	unpack_128_2x128 (xmm_src_hi, &xmm_src_lo, &xmm_src_hi);
	unpack_128_2x128 (xmm_mask_hi, &xmm_mask_lo, &xmm_mask_hi);

	expand_alpha_2x128 (xmm_dst_lo, xmm_dst_hi,
				&xmm_alpha_lo, &xmm_alpha_hi);

	pix_multiply_2x128 (&xmm_src_lo, &xmm_src_hi,
				&xmm_mask_lo, &xmm_mask_hi,
				&xmm_dst_lo, &xmm_dst_hi);

	pix_multiply_2x128 (&xmm_dst_lo, &xmm_dst_hi,
				&xmm_alpha_lo, &xmm_alpha_hi,
				&xmm_dst_lo, &xmm_dst_hi);

	save_128_aligned (
		(__m128i*)pd, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

	ps += 4;
	pd += 4;
	pm += 4;
	w -= 4;
	}

	while (w)
	{
	s = *ps++;
	m = *pm++;
	d = *pd;

	*pd++ = pack_1x128_32 (
		pix_multiply_1x128 (
		pix_multiply_1x128 (
			unpack_32_1x128 (s), unpack_32_1x128 (m)),
		expand_alpha_1x128 (unpack_32_1x128 (d))));

	w--;
	}
}

static void
sse2_combine_in_reverse_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							ePIXEL_MANIPULATE_OPERATE			 op,
							uint32 *			   pd,
							const uint32 *		 ps,
							const uint32 *		 pm,
							int					  w)
{
	uint32 s, m, d;

	__m128i xmm_alpha_lo, xmm_alpha_hi;
	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_mask_lo, xmm_mask_hi;

	while (w && (uintptr_t)pd & 15)
	{
	s = *ps++;
	m = *pm++;
	d = *pd;

	*pd++ = pack_1x128_32 (
		pix_multiply_1x128 (
		unpack_32_1x128 (d),
		pix_multiply_1x128 (unpack_32_1x128 (m),
				   expand_alpha_1x128 (unpack_32_1x128 (s)))));
	w--;
	}

	while (w >= 4)
	{
	xmm_dst_hi = load_128_aligned ((__m128i*)pd);
	xmm_src_hi = load_128_unaligned ((__m128i*)ps);
	xmm_mask_hi = load_128_unaligned ((__m128i*)pm);

	unpack_128_2x128 (xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);
	unpack_128_2x128 (xmm_src_hi, &xmm_src_lo, &xmm_src_hi);
	unpack_128_2x128 (xmm_mask_hi, &xmm_mask_lo, &xmm_mask_hi);

	expand_alpha_2x128 (xmm_src_lo, xmm_src_hi,
				&xmm_alpha_lo, &xmm_alpha_hi);
	pix_multiply_2x128 (&xmm_mask_lo, &xmm_mask_hi,
				&xmm_alpha_lo, &xmm_alpha_hi,
				&xmm_alpha_lo, &xmm_alpha_hi);

	pix_multiply_2x128 (&xmm_dst_lo, &xmm_dst_hi,
				&xmm_alpha_lo, &xmm_alpha_hi,
				&xmm_dst_lo, &xmm_dst_hi);

	save_128_aligned (
		(__m128i*)pd, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

	ps += 4;
	pd += 4;
	pm += 4;
	w -= 4;
	}

	while (w)
	{
	s = *ps++;
	m = *pm++;
	d = *pd;

	*pd++ = pack_1x128_32 (
		pix_multiply_1x128 (
		unpack_32_1x128 (d),
		pix_multiply_1x128 (unpack_32_1x128 (m),
				   expand_alpha_1x128 (unpack_32_1x128 (s)))));
	w--;
	}
}

static void
sse2_combine_out_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					 ePIXEL_MANIPULATE_OPERATE			 op,
					 uint32 *			   pd,
					 const uint32 *		 ps,
					 const uint32 *		 pm,
					 int					  w)
{
	uint32 s, m, d;

	__m128i xmm_alpha_lo, xmm_alpha_hi;
	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_mask_lo, xmm_mask_hi;

	while (w && (uintptr_t)pd & 15)
	{
	s = *ps++;
	m = *pm++;
	d = *pd;

	*pd++ = pack_1x128_32 (
		pix_multiply_1x128 (
		pix_multiply_1x128 (
			unpack_32_1x128 (s), unpack_32_1x128 (m)),
		negate_1x128 (expand_alpha_1x128 (unpack_32_1x128 (d)))));
	w--;
	}

	while (w >= 4)
	{
	xmm_dst_hi = load_128_aligned ((__m128i*)pd);
	xmm_src_hi = load_128_unaligned ((__m128i*)ps);
	xmm_mask_hi = load_128_unaligned ((__m128i*)pm);

	unpack_128_2x128 (xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);
	unpack_128_2x128 (xmm_src_hi, &xmm_src_lo, &xmm_src_hi);
	unpack_128_2x128 (xmm_mask_hi, &xmm_mask_lo, &xmm_mask_hi);

	expand_alpha_2x128 (xmm_dst_lo, xmm_dst_hi,
				&xmm_alpha_lo, &xmm_alpha_hi);
	negate_2x128 (xmm_alpha_lo, xmm_alpha_hi,
			  &xmm_alpha_lo, &xmm_alpha_hi);

	pix_multiply_2x128 (&xmm_src_lo, &xmm_src_hi,
				&xmm_mask_lo, &xmm_mask_hi,
				&xmm_dst_lo, &xmm_dst_hi);
	pix_multiply_2x128 (&xmm_dst_lo, &xmm_dst_hi,
				&xmm_alpha_lo, &xmm_alpha_hi,
				&xmm_dst_lo, &xmm_dst_hi);

	save_128_aligned (
		(__m128i*)pd, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

	ps += 4;
	pd += 4;
	pm += 4;
	w -= 4;
	}

	while (w)
	{
	s = *ps++;
	m = *pm++;
	d = *pd;

	*pd++ = pack_1x128_32 (
		pix_multiply_1x128 (
		pix_multiply_1x128 (
			unpack_32_1x128 (s), unpack_32_1x128 (m)),
		negate_1x128 (expand_alpha_1x128 (unpack_32_1x128 (d)))));

	w--;
	}
}

static void
sse2_combine_out_reverse_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							 ePIXEL_MANIPULATE_OPERATE			 op,
							 uint32 *			   pd,
							 const uint32 *		 ps,
							 const uint32 *		 pm,
							 int					  w)
{
	uint32 s, m, d;

	__m128i xmm_alpha_lo, xmm_alpha_hi;
	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_mask_lo, xmm_mask_hi;

	while (w && (uintptr_t)pd & 15)
	{
	s = *ps++;
	m = *pm++;
	d = *pd;

	*pd++ = pack_1x128_32 (
		pix_multiply_1x128 (
		unpack_32_1x128 (d),
		negate_1x128 (pix_multiply_1x128 (
				 unpack_32_1x128 (m),
				 expand_alpha_1x128 (unpack_32_1x128 (s))))));
	w--;
	}

	while (w >= 4)
	{
	xmm_dst_hi = load_128_aligned ((__m128i*)pd);
	xmm_src_hi = load_128_unaligned ((__m128i*)ps);
	xmm_mask_hi = load_128_unaligned ((__m128i*)pm);

	unpack_128_2x128 (xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);
	unpack_128_2x128 (xmm_src_hi, &xmm_src_lo, &xmm_src_hi);
	unpack_128_2x128 (xmm_mask_hi, &xmm_mask_lo, &xmm_mask_hi);

	expand_alpha_2x128 (xmm_src_lo, xmm_src_hi,
				&xmm_alpha_lo, &xmm_alpha_hi);

	pix_multiply_2x128 (&xmm_mask_lo, &xmm_mask_hi,
				&xmm_alpha_lo, &xmm_alpha_hi,
				&xmm_mask_lo, &xmm_mask_hi);

	negate_2x128 (xmm_mask_lo, xmm_mask_hi,
			  &xmm_mask_lo, &xmm_mask_hi);

	pix_multiply_2x128 (&xmm_dst_lo, &xmm_dst_hi,
				&xmm_mask_lo, &xmm_mask_hi,
				&xmm_dst_lo, &xmm_dst_hi);

	save_128_aligned (
		(__m128i*)pd, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

	ps += 4;
	pd += 4;
	pm += 4;
	w -= 4;
	}

	while (w)
	{
	s = *ps++;
	m = *pm++;
	d = *pd;

	*pd++ = pack_1x128_32 (
		pix_multiply_1x128 (
		unpack_32_1x128 (d),
		negate_1x128 (pix_multiply_1x128 (
				 unpack_32_1x128 (m),
				 expand_alpha_1x128 (unpack_32_1x128 (s))))));
	w--;
	}
}

static FORCE_INLINE uint32
core_combine_atop_ca_pixel_sse2 (uint32 src,
								 uint32 mask,
								 uint32 dst)
{
	__m128i m = unpack_32_1x128 (mask);
	__m128i s = unpack_32_1x128 (src);
	__m128i d = unpack_32_1x128 (dst);
	__m128i sa = expand_alpha_1x128 (s);
	__m128i da = expand_alpha_1x128 (d);

	s = pix_multiply_1x128 (s, m);
	m = negate_1x128 (pix_multiply_1x128 (m, sa));

	return pack_1x128_32 (pix_add_multiply_1x128 (&d, &m, &s, &da));
}

static void
sse2_combine_atop_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					  ePIXEL_MANIPULATE_OPERATE			 op,
					  uint32 *			   pd,
					  const uint32 *		 ps,
					  const uint32 *		 pm,
					  int					  w)
{
	uint32 s, m, d;

	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_alpha_src_lo, xmm_alpha_src_hi;
	__m128i xmm_alpha_dst_lo, xmm_alpha_dst_hi;
	__m128i xmm_mask_lo, xmm_mask_hi;

	while (w && (uintptr_t)pd & 15)
	{
	s = *ps++;
	m = *pm++;
	d = *pd;

	*pd++ = core_combine_atop_ca_pixel_sse2 (s, m, d);
	w--;
	}

	while (w >= 4)
	{
	xmm_dst_hi = load_128_aligned ((__m128i*)pd);
	xmm_src_hi = load_128_unaligned ((__m128i*)ps);
	xmm_mask_hi = load_128_unaligned ((__m128i*)pm);

	unpack_128_2x128 (xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);
	unpack_128_2x128 (xmm_src_hi, &xmm_src_lo, &xmm_src_hi);
	unpack_128_2x128 (xmm_mask_hi, &xmm_mask_lo, &xmm_mask_hi);

	expand_alpha_2x128 (xmm_src_lo, xmm_src_hi,
				&xmm_alpha_src_lo, &xmm_alpha_src_hi);
	expand_alpha_2x128 (xmm_dst_lo, xmm_dst_hi,
				&xmm_alpha_dst_lo, &xmm_alpha_dst_hi);

	pix_multiply_2x128 (&xmm_src_lo, &xmm_src_hi,
				&xmm_mask_lo, &xmm_mask_hi,
				&xmm_src_lo, &xmm_src_hi);
	pix_multiply_2x128 (&xmm_mask_lo, &xmm_mask_hi,
				&xmm_alpha_src_lo, &xmm_alpha_src_hi,
				&xmm_mask_lo, &xmm_mask_hi);

	negate_2x128 (xmm_mask_lo, xmm_mask_hi, &xmm_mask_lo, &xmm_mask_hi);

	pix_add_multiply_2x128 (
		&xmm_dst_lo, &xmm_dst_hi, &xmm_mask_lo, &xmm_mask_hi,
		&xmm_src_lo, &xmm_src_hi, &xmm_alpha_dst_lo, &xmm_alpha_dst_hi,
		&xmm_dst_lo, &xmm_dst_hi);

	save_128_aligned (
		(__m128i*)pd, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

	ps += 4;
	pd += 4;
	pm += 4;
	w -= 4;
	}

	while (w)
	{
	s = *ps++;
	m = *pm++;
	d = *pd;

	*pd++ = core_combine_atop_ca_pixel_sse2 (s, m, d);
	w--;
	}
}

static FORCE_INLINE uint32
core_combine_reverse_atop_ca_pixel_sse2 (uint32 src,
										 uint32 mask,
										 uint32 dst)
{
	__m128i m = unpack_32_1x128 (mask);
	__m128i s = unpack_32_1x128 (src);
	__m128i d = unpack_32_1x128 (dst);

	__m128i da = negate_1x128 (expand_alpha_1x128 (d));
	__m128i sa = expand_alpha_1x128 (s);

	s = pix_multiply_1x128 (s, m);
	m = pix_multiply_1x128 (m, sa);

	return pack_1x128_32 (pix_add_multiply_1x128 (&d, &m, &s, &da));
}

static void
sse2_combine_atop_reverse_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							  ePIXEL_MANIPULATE_OPERATE			 op,
							  uint32 *			   pd,
							  const uint32 *		 ps,
							  const uint32 *		 pm,
							  int					  w)
{
	uint32 s, m, d;

	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_alpha_src_lo, xmm_alpha_src_hi;
	__m128i xmm_alpha_dst_lo, xmm_alpha_dst_hi;
	__m128i xmm_mask_lo, xmm_mask_hi;

	while (w && (uintptr_t)pd & 15)
	{
	s = *ps++;
	m = *pm++;
	d = *pd;

	*pd++ = core_combine_reverse_atop_ca_pixel_sse2 (s, m, d);
	w--;
	}

	while (w >= 4)
	{
	xmm_dst_hi = load_128_aligned ((__m128i*)pd);
	xmm_src_hi = load_128_unaligned ((__m128i*)ps);
	xmm_mask_hi = load_128_unaligned ((__m128i*)pm);

	unpack_128_2x128 (xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);
	unpack_128_2x128 (xmm_src_hi, &xmm_src_lo, &xmm_src_hi);
	unpack_128_2x128 (xmm_mask_hi, &xmm_mask_lo, &xmm_mask_hi);

	expand_alpha_2x128 (xmm_src_lo, xmm_src_hi,
				&xmm_alpha_src_lo, &xmm_alpha_src_hi);
	expand_alpha_2x128 (xmm_dst_lo, xmm_dst_hi,
				&xmm_alpha_dst_lo, &xmm_alpha_dst_hi);

	pix_multiply_2x128 (&xmm_src_lo, &xmm_src_hi,
				&xmm_mask_lo, &xmm_mask_hi,
				&xmm_src_lo, &xmm_src_hi);
	pix_multiply_2x128 (&xmm_mask_lo, &xmm_mask_hi,
				&xmm_alpha_src_lo, &xmm_alpha_src_hi,
				&xmm_mask_lo, &xmm_mask_hi);

	negate_2x128 (xmm_alpha_dst_lo, xmm_alpha_dst_hi,
			  &xmm_alpha_dst_lo, &xmm_alpha_dst_hi);

	pix_add_multiply_2x128 (
		&xmm_dst_lo, &xmm_dst_hi, &xmm_mask_lo, &xmm_mask_hi,
		&xmm_src_lo, &xmm_src_hi, &xmm_alpha_dst_lo, &xmm_alpha_dst_hi,
		&xmm_dst_lo, &xmm_dst_hi);

	save_128_aligned (
		(__m128i*)pd, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

	ps += 4;
	pd += 4;
	pm += 4;
	w -= 4;
	}

	while (w)
	{
	s = *ps++;
	m = *pm++;
	d = *pd;

	*pd++ = core_combine_reverse_atop_ca_pixel_sse2 (s, m, d);
	w--;
	}
}

static FORCE_INLINE uint32
core_combine_xor_ca_pixel_sse2 (uint32 src,
								uint32 mask,
								uint32 dst)
{
	__m128i a = unpack_32_1x128 (mask);
	__m128i s = unpack_32_1x128 (src);
	__m128i d = unpack_32_1x128 (dst);

	__m128i alpha_dst = negate_1x128 (pix_multiply_1x128 (
					   a, expand_alpha_1x128 (s)));
	__m128i dest	  = pix_multiply_1x128 (s, a);
	__m128i alpha_src = negate_1x128 (expand_alpha_1x128 (d));

	return pack_1x128_32 (pix_add_multiply_1x128 (&d,
												&alpha_dst,
												&dest,
												&alpha_src));
}

static void
sse2_combine_xor_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					 ePIXEL_MANIPULATE_OPERATE			 op,
					 uint32 *			   pd,
					 const uint32 *		 ps,
					 const uint32 *		 pm,
					 int					  w)
{
	uint32 s, m, d;

	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_alpha_src_lo, xmm_alpha_src_hi;
	__m128i xmm_alpha_dst_lo, xmm_alpha_dst_hi;
	__m128i xmm_mask_lo, xmm_mask_hi;

	while (w && (uintptr_t)pd & 15)
	{
	s = *ps++;
	m = *pm++;
	d = *pd;

	*pd++ = core_combine_xor_ca_pixel_sse2 (s, m, d);
	w--;
	}

	while (w >= 4)
	{
	xmm_dst_hi = load_128_aligned ((__m128i*)pd);
	xmm_src_hi = load_128_unaligned ((__m128i*)ps);
	xmm_mask_hi = load_128_unaligned ((__m128i*)pm);

	unpack_128_2x128 (xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);
	unpack_128_2x128 (xmm_src_hi, &xmm_src_lo, &xmm_src_hi);
	unpack_128_2x128 (xmm_mask_hi, &xmm_mask_lo, &xmm_mask_hi);

	expand_alpha_2x128 (xmm_src_lo, xmm_src_hi,
				&xmm_alpha_src_lo, &xmm_alpha_src_hi);
	expand_alpha_2x128 (xmm_dst_lo, xmm_dst_hi,
				&xmm_alpha_dst_lo, &xmm_alpha_dst_hi);

	pix_multiply_2x128 (&xmm_src_lo, &xmm_src_hi,
				&xmm_mask_lo, &xmm_mask_hi,
				&xmm_src_lo, &xmm_src_hi);
	pix_multiply_2x128 (&xmm_mask_lo, &xmm_mask_hi,
				&xmm_alpha_src_lo, &xmm_alpha_src_hi,
				&xmm_mask_lo, &xmm_mask_hi);

	negate_2x128 (xmm_alpha_dst_lo, xmm_alpha_dst_hi,
			  &xmm_alpha_dst_lo, &xmm_alpha_dst_hi);
	negate_2x128 (xmm_mask_lo, xmm_mask_hi,
			  &xmm_mask_lo, &xmm_mask_hi);

	pix_add_multiply_2x128 (
		&xmm_dst_lo, &xmm_dst_hi, &xmm_mask_lo, &xmm_mask_hi,
		&xmm_src_lo, &xmm_src_hi, &xmm_alpha_dst_lo, &xmm_alpha_dst_hi,
		&xmm_dst_lo, &xmm_dst_hi);

	save_128_aligned (
		(__m128i*)pd, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

	ps += 4;
	pd += 4;
	pm += 4;
	w -= 4;
	}

	while (w)
	{
	s = *ps++;
	m = *pm++;
	d = *pd;

	*pd++ = core_combine_xor_ca_pixel_sse2 (s, m, d);
	w--;
	}
}

static void
sse2_combine_add_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					 ePIXEL_MANIPULATE_OPERATE			 op,
					 uint32 *			   pd,
					 const uint32 *		 ps,
					 const uint32 *		 pm,
					 int					  w)
{
	uint32 s, m, d;

	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_mask_lo, xmm_mask_hi;

	while (w && (uintptr_t)pd & 15)
	{
	s = *ps++;
	m = *pm++;
	d = *pd;

	*pd++ = pack_1x128_32 (
		_mm_adds_epu8 (pix_multiply_1x128 (unpack_32_1x128 (s),
						   unpack_32_1x128 (m)),
			   unpack_32_1x128 (d)));
	w--;
	}

	while (w >= 4)
	{
	xmm_src_hi = load_128_unaligned ((__m128i*)ps);
	xmm_mask_hi = load_128_unaligned ((__m128i*)pm);
	xmm_dst_hi = load_128_aligned ((__m128i*)pd);

	unpack_128_2x128 (xmm_src_hi, &xmm_src_lo, &xmm_src_hi);
	unpack_128_2x128 (xmm_mask_hi, &xmm_mask_lo, &xmm_mask_hi);
	unpack_128_2x128 (xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);

	pix_multiply_2x128 (&xmm_src_lo, &xmm_src_hi,
				&xmm_mask_lo, &xmm_mask_hi,
				&xmm_src_lo, &xmm_src_hi);

	save_128_aligned (
		(__m128i*)pd, pack_2x128_128 (
		_mm_adds_epu8 (xmm_src_lo, xmm_dst_lo),
		_mm_adds_epu8 (xmm_src_hi, xmm_dst_hi)));

	ps += 4;
	pd += 4;
	pm += 4;
	w -= 4;
	}

	while (w)
	{
	s = *ps++;
	m = *pm++;
	d = *pd;

	*pd++ = pack_1x128_32 (
		_mm_adds_epu8 (pix_multiply_1x128 (unpack_32_1x128 (s),
						   unpack_32_1x128 (m)),
			   unpack_32_1x128 (d)));
	w--;
	}
}

static FORCE_INLINE __m128i
create_mask_16_128 (uint16 mask)
{
	return _mm_set1_epi16 (mask);
}

/* Work around a code generation bug in Sun Studio 12. */
#if defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590)
# define create_mask_2x32_128(mask0, mask1)				\
	(_mm_set_epi32 ((mask0), (mask1), (mask0), (mask1)))
#else
static FORCE_INLINE __m128i
create_mask_2x32_128 (uint32 mask0,
					  uint32 mask1)
{
	return _mm_set_epi32 (mask0, mask1, mask0, mask1);
}
#endif

static void
sse2_composite_over_n_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32 src;
	uint32	*dst_line, *dst, d;
	int32 w;
	int dst_stride;
	__m128i xmm_src, xmm_alpha;
	__m128i xmm_dst, xmm_dst_lo, xmm_dst_hi;

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	if (src == 0)
	return;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);

	xmm_src = expand_pixel_32_1x128 (src);
	xmm_alpha = expand_alpha_1x128 (xmm_src);

	while (height--)
	{
	dst = dst_line;

	dst_line += dst_stride;
	w = width;

	while (w && (uintptr_t)dst & 15)
	{
		d = *dst;
		*dst++ = pack_1x128_32 (over_1x128 (xmm_src,
						xmm_alpha,
						unpack_32_1x128 (d)));
		w--;
	}

	while (w >= 4)
	{
		xmm_dst = load_128_aligned ((__m128i*)dst);

		unpack_128_2x128 (xmm_dst, &xmm_dst_lo, &xmm_dst_hi);

		over_2x128 (&xmm_src, &xmm_src,
			&xmm_alpha, &xmm_alpha,
			&xmm_dst_lo, &xmm_dst_hi);

		/* rebuid the 4 pixel data and save*/
		save_128_aligned (
		(__m128i*)dst, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

		w -= 4;
		dst += 4;
	}

	while (w)
	{
		d = *dst;
		*dst++ = pack_1x128_32 (over_1x128 (xmm_src,
						xmm_alpha,
						unpack_32_1x128 (d)));
		w--;
	}

	}
}

static void
sse2_composite_over_n_0565 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32 src;
	uint16	*dst_line, *dst, d;
	int32 w;
	int dst_stride;
	__m128i xmm_src, xmm_alpha;
	__m128i xmm_dst, xmm_dst0, xmm_dst1, xmm_dst2, xmm_dst3;

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	if (src == 0)
	return;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint16, dst_stride, dst_line, 1);

	xmm_src = expand_pixel_32_1x128 (src);
	xmm_alpha = expand_alpha_1x128 (xmm_src);

	while (height--)
	{
	dst = dst_line;

	dst_line += dst_stride;
	w = width;

	while (w && (uintptr_t)dst & 15)
	{
		d = *dst;

		*dst++ = pack_565_32_16 (
		pack_1x128_32 (over_1x128 (xmm_src,
					   xmm_alpha,
					   expand565_16_1x128 (d))));
		w--;
	}

	while (w >= 8)
	{
		xmm_dst = load_128_aligned ((__m128i*)dst);

		unpack_565_128_4x128 (xmm_dst,
				  &xmm_dst0, &xmm_dst1, &xmm_dst2, &xmm_dst3);

		over_2x128 (&xmm_src, &xmm_src,
			&xmm_alpha, &xmm_alpha,
			&xmm_dst0, &xmm_dst1);
		over_2x128 (&xmm_src, &xmm_src,
			&xmm_alpha, &xmm_alpha,
			&xmm_dst2, &xmm_dst3);

		xmm_dst = pack_565_4x128_128 (
		&xmm_dst0, &xmm_dst1, &xmm_dst2, &xmm_dst3);

		save_128_aligned ((__m128i*)dst, xmm_dst);

		dst += 8;
		w -= 8;
	}

	while (w--)
	{
		d = *dst;
		*dst++ = pack_565_32_16 (
		pack_1x128_32 (over_1x128 (xmm_src, xmm_alpha,
					   expand565_16_1x128 (d))));
	}
	}

}

static void
sse2_composite_add_n_8888_8888_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
				   PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32 src;
	uint32	*dst_line, d;
	uint32	*mask_line, m;
	uint32 pack_cmp;
	int dst_stride, mask_stride;

	__m128i xmm_src;
	__m128i xmm_dst;
	__m128i xmm_mask, xmm_mask_lo, xmm_mask_hi;

	__m128i mmx_src, mmx_mask, mmx_dest;

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	if (src == 0)
	return;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (
	mask_image, mask_x, mask_y, uint32, mask_stride, mask_line, 1);

	xmm_src = _mm_unpacklo_epi8 (
	create_mask_2x32_128 (src, src), _mm_setzero_si128 ());
	mmx_src   = xmm_src;

	while (height--)
	{
	int w = width;
	const uint32 *pm = (uint32 *)mask_line;
	uint32 *pd = (uint32 *)dst_line;

	dst_line += dst_stride;
	mask_line += mask_stride;

	while (w && (uintptr_t)pd & 15)
	{
		m = *pm++;

		if (m)
		{
		d = *pd;

		mmx_mask = unpack_32_1x128 (m);
		mmx_dest = unpack_32_1x128 (d);

		*pd = pack_1x128_32 (
			_mm_adds_epu8 (pix_multiply_1x128 (mmx_mask, mmx_src),
				   mmx_dest));
		}

		pd++;
		w--;
	}

	while (w >= 4)
	{
		xmm_mask = load_128_unaligned ((__m128i*)pm);

		pack_cmp =
		_mm_movemask_epi8 (
			_mm_cmpeq_epi32 (xmm_mask, _mm_setzero_si128 ()));

		/* if all bits in mask are zero, pack_cmp are equal to 0xffff */
		if (pack_cmp != 0xffff)
		{
		xmm_dst = load_128_aligned ((__m128i*)pd);

		unpack_128_2x128 (xmm_mask, &xmm_mask_lo, &xmm_mask_hi);

		pix_multiply_2x128 (&xmm_src, &xmm_src,
					&xmm_mask_lo, &xmm_mask_hi,
					&xmm_mask_lo, &xmm_mask_hi);
		xmm_mask_hi = pack_2x128_128 (xmm_mask_lo, xmm_mask_hi);

		save_128_aligned (
			(__m128i*)pd, _mm_adds_epu8 (xmm_mask_hi, xmm_dst));
		}

		pd += 4;
		pm += 4;
		w -= 4;
	}

	while (w)
	{
		m = *pm++;

		if (m)
		{
		d = *pd;

		mmx_mask = unpack_32_1x128 (m);
		mmx_dest = unpack_32_1x128 (d);

		*pd = pack_1x128_32 (
			_mm_adds_epu8 (pix_multiply_1x128 (mmx_mask, mmx_src),
				   mmx_dest));
		}

		pd++;
		w--;
	}
	}

}

static void
sse2_composite_over_n_8888_8888_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
									PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32 src;
	uint32	*dst_line, d;
	uint32	*mask_line, m;
	uint32 pack_cmp;
	int dst_stride, mask_stride;

	__m128i xmm_src, xmm_alpha;
	__m128i xmm_dst, xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_mask, xmm_mask_lo, xmm_mask_hi;

	__m128i mmx_src, mmx_alpha, mmx_mask, mmx_dest;

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	if (src == 0)
	return;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (
	mask_image, mask_x, mask_y, uint32, mask_stride, mask_line, 1);

	xmm_src = _mm_unpacklo_epi8 (
	create_mask_2x32_128 (src, src), _mm_setzero_si128 ());
	xmm_alpha = expand_alpha_1x128 (xmm_src);
	mmx_src   = xmm_src;
	mmx_alpha = xmm_alpha;

	while (height--)
	{
	int w = width;
	const uint32 *pm = (uint32 *)mask_line;
	uint32 *pd = (uint32 *)dst_line;

	dst_line += dst_stride;
	mask_line += mask_stride;

	while (w && (uintptr_t)pd & 15)
	{
		m = *pm++;

		if (m)
		{
		d = *pd;
		mmx_mask = unpack_32_1x128 (m);
		mmx_dest = unpack_32_1x128 (d);

		*pd = pack_1x128_32 (in_over_1x128 (&mmx_src,
										  &mmx_alpha,
										  &mmx_mask,
										  &mmx_dest));
		}

		pd++;
		w--;
	}

	while (w >= 4)
	{
		xmm_mask = load_128_unaligned ((__m128i*)pm);

		pack_cmp =
		_mm_movemask_epi8 (
			_mm_cmpeq_epi32 (xmm_mask, _mm_setzero_si128 ()));

		/* if all bits in mask are zero, pack_cmp are equal to 0xffff */
		if (pack_cmp != 0xffff)
		{
		xmm_dst = load_128_aligned ((__m128i*)pd);

		unpack_128_2x128 (xmm_mask, &xmm_mask_lo, &xmm_mask_hi);
		unpack_128_2x128 (xmm_dst, &xmm_dst_lo, &xmm_dst_hi);

		in_over_2x128 (&xmm_src, &xmm_src,
				   &xmm_alpha, &xmm_alpha,
				   &xmm_mask_lo, &xmm_mask_hi,
				   &xmm_dst_lo, &xmm_dst_hi);

		save_128_aligned (
			(__m128i*)pd, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));
		}

		pd += 4;
		pm += 4;
		w -= 4;
	}

	while (w)
	{
		m = *pm++;

		if (m)
		{
		d = *pd;
		mmx_mask = unpack_32_1x128 (m);
		mmx_dest = unpack_32_1x128 (d);

		*pd = pack_1x128_32 (
			in_over_1x128 (&mmx_src, &mmx_alpha, &mmx_mask, &mmx_dest));
		}

		pd++;
		w--;
	}
	}

}

static void
sse2_composite_over_8888_n_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
								 PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32	*dst_line, *dst;
	uint32	*src_line, *src;
	uint32 mask;
	int32 w;
	int dst_stride, src_stride;

	__m128i xmm_mask;
	__m128i xmm_src, xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst, xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_alpha_lo, xmm_alpha_hi;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (
	src_image, src_x, src_y, uint32, src_stride, src_line, 1);

	mask = PixelManipulateImageGetSolid(imp, mask_image, PIXEL_MANIPULATE_FORMAT_A8R8G8B8);

	xmm_mask = create_mask_16_128 (mask >> 24);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w && (uintptr_t)dst & 15)
	{
		uint32 s = *src++;

		if (s)
		{
		uint32 d = *dst;

		__m128i ms = unpack_32_1x128 (s);
		__m128i alpha	= expand_alpha_1x128 (ms);
		__m128i dest	 = xmm_mask;
		__m128i alpha_dst = unpack_32_1x128 (d);

		*dst = pack_1x128_32 (
			in_over_1x128 (&ms, &alpha, &dest, &alpha_dst));
		}
		dst++;
		w--;
	}

	while (w >= 4)
	{
		xmm_src = load_128_unaligned ((__m128i*)src);

		if (!is_zero (xmm_src))
		{
		xmm_dst = load_128_aligned ((__m128i*)dst);

		unpack_128_2x128 (xmm_src, &xmm_src_lo, &xmm_src_hi);
		unpack_128_2x128 (xmm_dst, &xmm_dst_lo, &xmm_dst_hi);
		expand_alpha_2x128 (xmm_src_lo, xmm_src_hi,
					&xmm_alpha_lo, &xmm_alpha_hi);

		in_over_2x128 (&xmm_src_lo, &xmm_src_hi,
				   &xmm_alpha_lo, &xmm_alpha_hi,
				   &xmm_mask, &xmm_mask,
				   &xmm_dst_lo, &xmm_dst_hi);

		save_128_aligned (
			(__m128i*)dst, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));
		}

		dst += 4;
		src += 4;
		w -= 4;
	}

	while (w)
	{
		uint32 s = *src++;

		if (s)
		{
		uint32 d = *dst;

		__m128i ms = unpack_32_1x128 (s);
		__m128i alpha = expand_alpha_1x128 (ms);
		__m128i mask  = xmm_mask;
		__m128i dest  = unpack_32_1x128 (d);

		*dst = pack_1x128_32 (
			in_over_1x128 (&ms, &alpha, &mask, &dest));
		}

		dst++;
		w--;
	}
	}

}

static void
sse2_composite_src_X888_0565 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							  PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint16	*dst_line, *dst;
	uint32	*src_line, *src, s;
	int dst_stride, src_stride;
	int32 w;

	IMAGE_GET_LINE (src_image, src_x, src_y, uint32, src_stride, src_line, 1);
	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint16, dst_stride, dst_line, 1);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w && (uintptr_t)dst & 15)
	{
		s = *src++;
		*dst = convert_8888_to_0565 (s);
		dst++;
		w--;
	}

	while (w >= 8)
	{
		__m128i xmm_src0 = load_128_unaligned ((__m128i *)src + 0);
		__m128i xmm_src1 = load_128_unaligned ((__m128i *)src + 1);

		save_128_aligned ((__m128i*)dst, pack_565_2packedx128_128 (xmm_src0, xmm_src1));

		w -= 8;
		src += 8;
		dst += 8;
	}

	while (w)
	{
		s = *src++;
		*dst = convert_8888_to_0565 (s);
		dst++;
		w--;
	}
	}
}

static void
sse2_composite_src_X888_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
				  PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32	*dst_line, *dst;
	uint32	*src_line, *src;
	int32 w;
	int dst_stride, src_stride;


	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (
	src_image, src_x, src_y, uint32, src_stride, src_line, 1);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w && (uintptr_t)dst & 15)
	{
		*dst++ = *src++ | 0xff000000;
		w--;
	}

	while (w >= 16)
	{
		__m128i xmm_src1, xmm_src2, xmm_src3, xmm_src4;

		xmm_src1 = load_128_unaligned ((__m128i*)src + 0);
		xmm_src2 = load_128_unaligned ((__m128i*)src + 1);
		xmm_src3 = load_128_unaligned ((__m128i*)src + 2);
		xmm_src4 = load_128_unaligned ((__m128i*)src + 3);

		save_128_aligned ((__m128i*)dst + 0, _mm_or_si128 (xmm_src1, mask_ff000000));
		save_128_aligned ((__m128i*)dst + 1, _mm_or_si128 (xmm_src2, mask_ff000000));
		save_128_aligned ((__m128i*)dst + 2, _mm_or_si128 (xmm_src3, mask_ff000000));
		save_128_aligned ((__m128i*)dst + 3, _mm_or_si128 (xmm_src4, mask_ff000000));

		dst += 16;
		src += 16;
		w -= 16;
	}

	while (w)
	{
		*dst++ = *src++ | 0xff000000;
		w--;
	}
	}

}

static void
sse2_composite_over_X888_n_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
								 PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32	*dst_line, *dst;
	uint32	*src_line, *src;
	uint32 mask;
	int dst_stride, src_stride;
	int32 w;

	__m128i xmm_mask, xmm_alpha;
	__m128i xmm_src, xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst, xmm_dst_lo, xmm_dst_hi;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (
	src_image, src_x, src_y, uint32, src_stride, src_line, 1);

	mask = PixelManipulateImageGetSolid(imp, mask_image, PIXEL_MANIPULATE_FORMAT_A8R8G8B8);

	xmm_mask = create_mask_16_128 (mask >> 24);
	xmm_alpha = mask_00ff;

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w && (uintptr_t)dst & 15)
	{
		uint32 s = (*src++) | 0xff000000;
		uint32 d = *dst;

		__m128i src   = unpack_32_1x128 (s);
		__m128i alpha = xmm_alpha;
		__m128i mask  = xmm_mask;
		__m128i dest  = unpack_32_1x128 (d);

		*dst++ = pack_1x128_32 (
		in_over_1x128 (&src, &alpha, &mask, &dest));

		w--;
	}

	while (w >= 4)
	{
		xmm_src = _mm_or_si128 (
		load_128_unaligned ((__m128i*)src), mask_ff000000);
		xmm_dst = load_128_aligned ((__m128i*)dst);

		unpack_128_2x128 (xmm_src, &xmm_src_lo, &xmm_src_hi);
		unpack_128_2x128 (xmm_dst, &xmm_dst_lo, &xmm_dst_hi);

		in_over_2x128 (&xmm_src_lo, &xmm_src_hi,
			   &xmm_alpha, &xmm_alpha,
			   &xmm_mask, &xmm_mask,
			   &xmm_dst_lo, &xmm_dst_hi);

		save_128_aligned (
		(__m128i*)dst, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

		dst += 4;
		src += 4;
		w -= 4;

	}

	while (w)
	{
		uint32 s = (*src++) | 0xff000000;
		uint32 d = *dst;

		__m128i src  = unpack_32_1x128 (s);
		__m128i alpha = xmm_alpha;
		__m128i mask  = xmm_mask;
		__m128i dest  = unpack_32_1x128 (d);

		*dst++ = pack_1x128_32 (
		in_over_1x128 (&src, &alpha, &mask, &dest));

		w--;
	}
	}

}

static void
sse2_composite_over_8888_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							   PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	int dst_stride, src_stride;
	uint32	*dst_line, *dst;
	uint32	*src_line, *src;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (
	src_image, src_x, src_y, uint32, src_stride, src_line, 1);

	dst = dst_line;
	src = src_line;

	while (height--)
	{
	sse2_combine_over_u (imp, op, dst, src, NULL, width);

	dst += dst_stride;
	src += src_stride;
	}
}

static FORCE_INLINE uint16
composite_over_8888_0565pixel (uint32 src, uint16 dst)
{
	__m128i ms;

	ms = unpack_32_1x128 (src);
	return pack_565_32_16 (
	pack_1x128_32 (
		over_1x128 (
		ms, expand_alpha_1x128 (ms), expand565_16_1x128 (dst))));
}

static void
sse2_composite_over_8888_0565 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							   PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint16	*dst_line, *dst, d;
	uint32	*src_line, *src, s;
	int dst_stride, src_stride;
	int32 w;

	__m128i xmm_alpha_lo, xmm_alpha_hi;
	__m128i xmm_src, xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst, xmm_dst0, xmm_dst1, xmm_dst2, xmm_dst3;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint16, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (
	src_image, src_x, src_y, uint32, src_stride, src_line, 1);

	while (height--)
	{
	dst = dst_line;
	src = src_line;

	dst_line += dst_stride;
	src_line += src_stride;
	w = width;

	/* Align dst on a 16-byte boundary */
	while (w &&
		   ((uintptr_t)dst & 15))
	{
		s = *src++;
		d = *dst;

		*dst++ = composite_over_8888_0565pixel (s, d);
		w--;
	}

	/* It's a 8 pixel loop */
	while (w >= 8)
	{
		/* I'm loading unaligned because I'm not sure
		 * about the address alignment.
		 */
		xmm_src = load_128_unaligned ((__m128i*) src);
		xmm_dst = load_128_aligned ((__m128i*) dst);

		/* Unpacking */
		unpack_128_2x128 (xmm_src, &xmm_src_lo, &xmm_src_hi);
		unpack_565_128_4x128 (xmm_dst,
				  &xmm_dst0, &xmm_dst1, &xmm_dst2, &xmm_dst3);
		expand_alpha_2x128 (xmm_src_lo, xmm_src_hi,
				&xmm_alpha_lo, &xmm_alpha_hi);

		/* I'm loading next 4 pixels from memory
		 * before to optimze the memory read.
		 */
		xmm_src = load_128_unaligned ((__m128i*) (src + 4));

		over_2x128 (&xmm_src_lo, &xmm_src_hi,
			&xmm_alpha_lo, &xmm_alpha_hi,
			&xmm_dst0, &xmm_dst1);

		/* Unpacking */
		unpack_128_2x128 (xmm_src, &xmm_src_lo, &xmm_src_hi);
		expand_alpha_2x128 (xmm_src_lo, xmm_src_hi,
				&xmm_alpha_lo, &xmm_alpha_hi);

		over_2x128 (&xmm_src_lo, &xmm_src_hi,
			&xmm_alpha_lo, &xmm_alpha_hi,
			&xmm_dst2, &xmm_dst3);

		save_128_aligned (
		(__m128i*)dst, pack_565_4x128_128 (
			&xmm_dst0, &xmm_dst1, &xmm_dst2, &xmm_dst3));

		w -= 8;
		dst += 8;
		src += 8;
	}

	while (w--)
	{
		s = *src++;
		d = *dst;

		*dst++ = composite_over_8888_0565pixel (s, d);
	}
	}

}

static void
sse2_composite_over_n_8_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							  PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32 src, srca;
	uint32 *dst_line, *dst;
	uint8 *mask_line, *mask;
	int dst_stride, mask_stride;
	int32 w;
	uint32 m, d;

	__m128i xmm_src, xmm_alpha, xmm_def;
	__m128i xmm_dst, xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_mask, xmm_mask_lo, xmm_mask_hi;

	__m128i mmx_src, mmx_alpha, mmx_mask, mmx_dest;

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	srca = src >> 24;
	if (src == 0)
	return;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (
	mask_image, mask_x, mask_y, uint8, mask_stride, mask_line, 1);

	xmm_def = create_mask_2x32_128 (src, src);
	xmm_src = expand_pixel_32_1x128 (src);
	xmm_alpha = expand_alpha_1x128 (xmm_src);
	mmx_src   = xmm_src;
	mmx_alpha = xmm_alpha;

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w && (uintptr_t)dst & 15)
	{
		uint8 m = *mask++;

		if (m)
		{
		d = *dst;
		mmx_mask = expand_pixel_8_1x128 (m);
		mmx_dest = unpack_32_1x128 (d);

		*dst = pack_1x128_32 (in_over_1x128 (&mmx_src,
										   &mmx_alpha,
										   &mmx_mask,
										   &mmx_dest));
		}

		w--;
		dst++;
	}

	while (w >= 4)
	{
			memcpy(&m, mask, sizeof(uint32));

		if (srca == 0xff && m == 0xffffffff)
		{
		save_128_aligned ((__m128i*)dst, xmm_def);
		}
		else if (m)
		{
		xmm_dst = load_128_aligned ((__m128i*) dst);
		xmm_mask = unpack_32_1x128 (m);
		xmm_mask = _mm_unpacklo_epi8 (xmm_mask, _mm_setzero_si128 ());

		/* Unpacking */
		unpack_128_2x128 (xmm_dst, &xmm_dst_lo, &xmm_dst_hi);
		unpack_128_2x128 (xmm_mask, &xmm_mask_lo, &xmm_mask_hi);

		expand_alpha_rev_2x128 (xmm_mask_lo, xmm_mask_hi,
					&xmm_mask_lo, &xmm_mask_hi);

		in_over_2x128 (&xmm_src, &xmm_src,
				   &xmm_alpha, &xmm_alpha,
				   &xmm_mask_lo, &xmm_mask_hi,
				   &xmm_dst_lo, &xmm_dst_hi);

		save_128_aligned (
			(__m128i*)dst, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));
		}

		w -= 4;
		dst += 4;
		mask += 4;
	}

	while (w)
	{
		uint8 m = *mask++;

		if (m)
		{
		d = *dst;
		mmx_mask = expand_pixel_8_1x128 (m);
		mmx_dest = unpack_32_1x128 (d);

		*dst = pack_1x128_32 (in_over_1x128 (&mmx_src,
										   &mmx_alpha,
										   &mmx_mask,
										   &mmx_dest));
		}

		w--;
		dst++;
	}
	}

}

#if defined(__GNUC__) && !defined(__X86_64__) && !defined(__amd64__)
__attribute__((__force_align_arg_pointer__))
#endif
static int
sse2_fill (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
		   uint32 *			   bits,
		   int					  stride,
		   int					  bpp,
		   int					  x,
		   int					  y,
		   int					  width,
		   int					  height,
		   uint32			filler)
{
	uint32 byte_width;
	uint8 *byte_line;

	__m128i xmm_def;

	if (bpp == 8)
	{
	uint32 b;
	uint32 w;

	stride = stride * (int) sizeof (uint32) / 1;
	byte_line = (uint8 *)(((uint8 *)bits) + stride * y + x);
	byte_width = width;
	stride *= 1;

	b = filler & 0xff;
	w = (b << 8) | b;
	filler = (w << 16) | w;
	}
	else if (bpp == 16)
	{
	stride = stride * (int) sizeof (uint32) / 2;
	byte_line = (uint8 *)(((uint16 *)bits) + stride * y + x);
	byte_width = 2 * width;
	stride *= 2;

		filler = (filler & 0xffff) * 0x00010001;
	}
	else if (bpp == 32)
	{
	stride = stride * (int) sizeof (uint32) / 4;
	byte_line = (uint8 *)(((uint32 *)bits) + stride * y + x);
	byte_width = 4 * width;
	stride *= 4;
	}
	else
	{
	return FALSE;
	}

	xmm_def = create_mask_2x32_128 (filler, filler);

	while (height--)
	{
	int w;
	uint8 *d = byte_line;
	byte_line += stride;
	w = byte_width;

	if (w >= 1 && ((uintptr_t)d & 1))
	{
		*(uint8 *)d = (uint8)filler;
		w -= 1;
		d += 1;
	}

	while (w >= 2 && ((uintptr_t)d & 3))
	{
		*(uint16 *)d = (uint16)filler;
		w -= 2;
		d += 2;
	}

	while (w >= 4 && ((uintptr_t)d & 15))
	{
		*(uint32 *)d = filler;

		w -= 4;
		d += 4;
	}

	while (w >= 128)
	{
		save_128_aligned ((__m128i*)(d),	 xmm_def);
		save_128_aligned ((__m128i*)(d + 16),  xmm_def);
		save_128_aligned ((__m128i*)(d + 32),  xmm_def);
		save_128_aligned ((__m128i*)(d + 48),  xmm_def);
		save_128_aligned ((__m128i*)(d + 64),  xmm_def);
		save_128_aligned ((__m128i*)(d + 80),  xmm_def);
		save_128_aligned ((__m128i*)(d + 96),  xmm_def);
		save_128_aligned ((__m128i*)(d + 112), xmm_def);

		d += 128;
		w -= 128;
	}

	if (w >= 64)
	{
		save_128_aligned ((__m128i*)(d),	 xmm_def);
		save_128_aligned ((__m128i*)(d + 16),  xmm_def);
		save_128_aligned ((__m128i*)(d + 32),  xmm_def);
		save_128_aligned ((__m128i*)(d + 48),  xmm_def);

		d += 64;
		w -= 64;
	}

	if (w >= 32)
	{
		save_128_aligned ((__m128i*)(d),	 xmm_def);
		save_128_aligned ((__m128i*)(d + 16),  xmm_def);

		d += 32;
		w -= 32;
	}

	if (w >= 16)
	{
		save_128_aligned ((__m128i*)(d),	 xmm_def);

		d += 16;
		w -= 16;
	}

	while (w >= 4)
	{
		*(uint32 *)d = filler;

		w -= 4;
		d += 4;
	}

	if (w >= 2)
	{
		*(uint16 *)d = (uint16)filler;
		w -= 2;
		d += 2;
	}

	if (w >= 1)
	{
		*(uint8 *)d = (uint8)filler;
		w -= 1;
		d += 1;
	}
	}

	return TRUE;
}

static void
sse2_composite_src_n_8_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							 PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32 src, srca;
	uint32	*dst_line, *dst;
	uint8	 *mask_line, *mask;
	int dst_stride, mask_stride;
	int32 w;
	uint32 m;

	__m128i xmm_src, xmm_def;
	__m128i xmm_mask, xmm_mask_lo, xmm_mask_hi;

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	srca = src >> 24;
	if (src == 0)
	{
	sse2_fill (imp, dest_image->bits.bits, dest_image->bits.row_stride,
		   PIXEL_MANIPULATE_FORMAT_BPP (dest_image->bits.format),
		   dest_x, dest_y, width, height, 0);
	return;
	}

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (
	mask_image, mask_x, mask_y, uint8, mask_stride, mask_line, 1);

	xmm_def = create_mask_2x32_128 (src, src);
	xmm_src = expand_pixel_32_1x128 (src);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w && (uintptr_t)dst & 15)
	{
		uint8 m = *mask++;

		if (m)
		{
		*dst = pack_1x128_32 (
			pix_multiply_1x128 (xmm_src, expand_pixel_8_1x128 (m)));
		}
		else
		{
		*dst = 0;
		}

		w--;
		dst++;
	}

	while (w >= 4)
	{
			memcpy(&m, mask, sizeof(uint32));

		if (srca == 0xff && m == 0xffffffff)
		{
		save_128_aligned ((__m128i*)dst, xmm_def);
		}
		else if (m)
		{
		xmm_mask = unpack_32_1x128 (m);
		xmm_mask = _mm_unpacklo_epi8 (xmm_mask, _mm_setzero_si128 ());

		/* Unpacking */
		unpack_128_2x128 (xmm_mask, &xmm_mask_lo, &xmm_mask_hi);

		expand_alpha_rev_2x128 (xmm_mask_lo, xmm_mask_hi,
					&xmm_mask_lo, &xmm_mask_hi);

		pix_multiply_2x128 (&xmm_src, &xmm_src,
					&xmm_mask_lo, &xmm_mask_hi,
					&xmm_mask_lo, &xmm_mask_hi);

		save_128_aligned (
			(__m128i*)dst, pack_2x128_128 (xmm_mask_lo, xmm_mask_hi));
		}
		else
		{
		save_128_aligned ((__m128i*)dst, _mm_setzero_si128 ());
		}

		w -= 4;
		dst += 4;
		mask += 4;
	}

	while (w)
	{
		uint8 m = *mask++;

		if (m)
		{
		*dst = pack_1x128_32 (
			pix_multiply_1x128 (
			xmm_src, expand_pixel_8_1x128 (m)));
		}
		else
		{
		*dst = 0;
		}

		w--;
		dst++;
	}
	}

}

static void
sse2_composite_over_n_8_0565 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							  PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32 src;
	uint16	*dst_line, *dst, d;
	uint8	 *mask_line, *mask;
	int dst_stride, mask_stride;
	int32 w;
	uint32 m;
	__m128i mmx_src, mmx_alpha, mmx_mask, mmx_dest;

	__m128i xmm_src, xmm_alpha;
	__m128i xmm_mask, xmm_mask_lo, xmm_mask_hi;
	__m128i xmm_dst, xmm_dst0, xmm_dst1, xmm_dst2, xmm_dst3;

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	if (src == 0)
	return;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint16, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (
	mask_image, mask_x, mask_y, uint8, mask_stride, mask_line, 1);

	xmm_src = expand_pixel_32_1x128 (src);
	xmm_alpha = expand_alpha_1x128 (xmm_src);
	mmx_src = xmm_src;
	mmx_alpha = xmm_alpha;

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w && (uintptr_t)dst & 15)
	{
		m = *mask++;

		if (m)
		{
		d = *dst;
		mmx_mask = expand_alpha_rev_1x128 (unpack_32_1x128 (m));
		mmx_dest = expand565_16_1x128 (d);

		*dst = pack_565_32_16 (
			pack_1x128_32 (
			in_over_1x128 (
				&mmx_src, &mmx_alpha, &mmx_mask, &mmx_dest)));
		}

		w--;
		dst++;
	}

	while (w >= 8)
	{
		xmm_dst = load_128_aligned ((__m128i*) dst);
		unpack_565_128_4x128 (xmm_dst,
				  &xmm_dst0, &xmm_dst1, &xmm_dst2, &xmm_dst3);

			memcpy(&m, mask, sizeof(uint32));
		mask += 4;

		if (m)
		{
		xmm_mask = unpack_32_1x128 (m);
		xmm_mask = _mm_unpacklo_epi8 (xmm_mask, _mm_setzero_si128 ());

		/* Unpacking */
		unpack_128_2x128 (xmm_mask, &xmm_mask_lo, &xmm_mask_hi);

		expand_alpha_rev_2x128 (xmm_mask_lo, xmm_mask_hi,
					&xmm_mask_lo, &xmm_mask_hi);

		in_over_2x128 (&xmm_src, &xmm_src,
				   &xmm_alpha, &xmm_alpha,
				   &xmm_mask_lo, &xmm_mask_hi,
				   &xmm_dst0, &xmm_dst1);
		}

			memcpy(&m, mask, sizeof(uint32));
		mask += 4;

		if (m)
		{
		xmm_mask = unpack_32_1x128 (m);
		xmm_mask = _mm_unpacklo_epi8 (xmm_mask, _mm_setzero_si128 ());

		/* Unpacking */
		unpack_128_2x128 (xmm_mask, &xmm_mask_lo, &xmm_mask_hi);

		expand_alpha_rev_2x128 (xmm_mask_lo, xmm_mask_hi,
					&xmm_mask_lo, &xmm_mask_hi);
		in_over_2x128 (&xmm_src, &xmm_src,
				   &xmm_alpha, &xmm_alpha,
				   &xmm_mask_lo, &xmm_mask_hi,
				   &xmm_dst2, &xmm_dst3);
		}

		save_128_aligned (
		(__m128i*)dst, pack_565_4x128_128 (
			&xmm_dst0, &xmm_dst1, &xmm_dst2, &xmm_dst3));

		w -= 8;
		dst += 8;
	}

	while (w)
	{
		m = *mask++;

		if (m)
		{
		d = *dst;
		mmx_mask = expand_alpha_rev_1x128 (unpack_32_1x128 (m));
		mmx_dest = expand565_16_1x128 (d);

		*dst = pack_565_32_16 (
			pack_1x128_32 (
			in_over_1x128 (
				&mmx_src, &mmx_alpha, &mmx_mask, &mmx_dest)));
		}

		w--;
		dst++;
	}
	}

}

static void
sse2_composite_over_pixbuf_0565 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
								 PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint16	*dst_line, *dst, d;
	uint32	*src_line, *src, s;
	int dst_stride, src_stride;
	int32 w;
	uint32 opaque, zero;

	__m128i ms;
	__m128i xmm_src, xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst, xmm_dst0, xmm_dst1, xmm_dst2, xmm_dst3;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint16, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (
	src_image, src_x, src_y, uint32, src_stride, src_line, 1);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w && (uintptr_t)dst & 15)
	{
		s = *src++;
		d = *dst;

		ms = unpack_32_1x128 (s);

		*dst++ = pack_565_32_16 (
		pack_1x128_32 (
			over_rev_non_pre_1x128 (ms, expand565_16_1x128 (d))));
		w--;
	}

	while (w >= 8)
	{
		/* First round */
		xmm_src = load_128_unaligned ((__m128i*)src);
		xmm_dst = load_128_aligned  ((__m128i*)dst);

		opaque = is_opaque (xmm_src);
		zero = is_zero (xmm_src);

		unpack_565_128_4x128 (xmm_dst,
				  &xmm_dst0, &xmm_dst1, &xmm_dst2, &xmm_dst3);
		unpack_128_2x128 (xmm_src, &xmm_src_lo, &xmm_src_hi);

		/* preload next round*/
		xmm_src = load_128_unaligned ((__m128i*)(src + 4));

		if (opaque)
		{
		invert_colors_2x128 (xmm_src_lo, xmm_src_hi,
					 &xmm_dst0, &xmm_dst1);
		}
		else if (!zero)
		{
		over_rev_non_pre_2x128 (xmm_src_lo, xmm_src_hi,
					&xmm_dst0, &xmm_dst1);
		}

		/* Second round */
		opaque = is_opaque (xmm_src);
		zero = is_zero (xmm_src);

		unpack_128_2x128 (xmm_src, &xmm_src_lo, &xmm_src_hi);

		if (opaque)
		{
		invert_colors_2x128 (xmm_src_lo, xmm_src_hi,
					 &xmm_dst2, &xmm_dst3);
		}
		else if (!zero)
		{
		over_rev_non_pre_2x128 (xmm_src_lo, xmm_src_hi,
					&xmm_dst2, &xmm_dst3);
		}

		save_128_aligned (
		(__m128i*)dst, pack_565_4x128_128 (
			&xmm_dst0, &xmm_dst1, &xmm_dst2, &xmm_dst3));

		w -= 8;
		src += 8;
		dst += 8;
	}

	while (w)
	{
		s = *src++;
		d = *dst;

		ms = unpack_32_1x128 (s);

		*dst++ = pack_565_32_16 (
		pack_1x128_32 (
			over_rev_non_pre_1x128 (ms, expand565_16_1x128 (d))));
		w--;
	}
	}

}

static void
sse2_composite_over_pixbuf_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
								 PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32	*dst_line, *dst, d;
	uint32	*src_line, *src, s;
	int dst_stride, src_stride;
	int32 w;
	uint32 opaque, zero;

	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst_lo, xmm_dst_hi;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (
	src_image, src_x, src_y, uint32, src_stride, src_line, 1);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w && (uintptr_t)dst & 15)
	{
		s = *src++;
		d = *dst;

		*dst++ = pack_1x128_32 (
		over_rev_non_pre_1x128 (
			unpack_32_1x128 (s), unpack_32_1x128 (d)));

		w--;
	}

	while (w >= 4)
	{
		xmm_src_hi = load_128_unaligned ((__m128i*)src);

		opaque = is_opaque (xmm_src_hi);
		zero = is_zero (xmm_src_hi);

		unpack_128_2x128 (xmm_src_hi, &xmm_src_lo, &xmm_src_hi);

		if (opaque)
		{
		invert_colors_2x128 (xmm_src_lo, xmm_src_hi,
					 &xmm_dst_lo, &xmm_dst_hi);

		save_128_aligned (
			(__m128i*)dst, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));
		}
		else if (!zero)
		{
		xmm_dst_hi = load_128_aligned  ((__m128i*)dst);

		unpack_128_2x128 (xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);

		over_rev_non_pre_2x128 (xmm_src_lo, xmm_src_hi,
					&xmm_dst_lo, &xmm_dst_hi);

		save_128_aligned (
			(__m128i*)dst, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));
		}

		w -= 4;
		dst += 4;
		src += 4;
	}

	while (w)
	{
		s = *src++;
		d = *dst;

		*dst++ = pack_1x128_32 (
		over_rev_non_pre_1x128 (
			unpack_32_1x128 (s), unpack_32_1x128 (d)));

		w--;
	}
	}

}

static void
sse2_composite_over_n_8888_0565_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
									PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32 src;
	uint16	*dst_line, *dst, d;
	uint32	*mask_line, *mask, m;
	int dst_stride, mask_stride;
	int w;
	uint32 pack_cmp;

	__m128i xmm_src, xmm_alpha;
	__m128i xmm_mask, xmm_mask_lo, xmm_mask_hi;
	__m128i xmm_dst, xmm_dst0, xmm_dst1, xmm_dst2, xmm_dst3;

	__m128i mmx_src, mmx_alpha, mmx_mask, mmx_dest;

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	if (src == 0)
	return;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint16, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (
	mask_image, mask_x, mask_y, uint32, mask_stride, mask_line, 1);

	xmm_src = expand_pixel_32_1x128 (src);
	xmm_alpha = expand_alpha_1x128 (xmm_src);
	mmx_src = xmm_src;
	mmx_alpha = xmm_alpha;

	while (height--)
	{
	w = width;
	mask = mask_line;
	dst = dst_line;
	mask_line += mask_stride;
	dst_line += dst_stride;

	while (w && ((uintptr_t)dst & 15))
	{
		m = *(uint32 *) mask;

		if (m)
		{
		d = *dst;
		mmx_mask = unpack_32_1x128 (m);
		mmx_dest = expand565_16_1x128 (d);

		*dst = pack_565_32_16 (
			pack_1x128_32 (
			in_over_1x128 (
				&mmx_src, &mmx_alpha, &mmx_mask, &mmx_dest)));
		}

		w--;
		dst++;
		mask++;
	}

	while (w >= 8)
	{
		/* First round */
		xmm_mask = load_128_unaligned ((__m128i*)mask);
		xmm_dst = load_128_aligned ((__m128i*)dst);

		pack_cmp = _mm_movemask_epi8 (
		_mm_cmpeq_epi32 (xmm_mask, _mm_setzero_si128 ()));

		unpack_565_128_4x128 (xmm_dst,
				  &xmm_dst0, &xmm_dst1, &xmm_dst2, &xmm_dst3);
		unpack_128_2x128 (xmm_mask, &xmm_mask_lo, &xmm_mask_hi);

		/* preload next round */
		xmm_mask = load_128_unaligned ((__m128i*)(mask + 4));

		/* preload next round */
		if (pack_cmp != 0xffff)
		{
		in_over_2x128 (&xmm_src, &xmm_src,
				   &xmm_alpha, &xmm_alpha,
				   &xmm_mask_lo, &xmm_mask_hi,
				   &xmm_dst0, &xmm_dst1);
		}

		/* Second round */
		pack_cmp = _mm_movemask_epi8 (
		_mm_cmpeq_epi32 (xmm_mask, _mm_setzero_si128 ()));

		unpack_128_2x128 (xmm_mask, &xmm_mask_lo, &xmm_mask_hi);

		if (pack_cmp != 0xffff)
		{
		in_over_2x128 (&xmm_src, &xmm_src,
				   &xmm_alpha, &xmm_alpha,
				   &xmm_mask_lo, &xmm_mask_hi,
				   &xmm_dst2, &xmm_dst3);
		}

		save_128_aligned (
		(__m128i*)dst, pack_565_4x128_128 (
			&xmm_dst0, &xmm_dst1, &xmm_dst2, &xmm_dst3));

		w -= 8;
		dst += 8;
		mask += 8;
	}

	while (w)
	{
		m = *(uint32 *) mask;

		if (m)
		{
		d = *dst;
		mmx_mask = unpack_32_1x128 (m);
		mmx_dest = expand565_16_1x128 (d);

		*dst = pack_565_32_16 (
			pack_1x128_32 (
			in_over_1x128 (
				&mmx_src, &mmx_alpha, &mmx_mask, &mmx_dest)));
		}

		w--;
		dst++;
		mask++;
	}
	}

}

static void sse2_composite_in_n_8_8 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
						 PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint8	 *dst_line, *dst;
	uint8	 *mask_line, *mask;
	int dst_stride, mask_stride;
	uint32 d, m;
	uint32 src;
	int32 w;

	__m128i xmm_alpha;
	__m128i xmm_mask, xmm_mask_lo, xmm_mask_hi;
	__m128i xmm_dst, xmm_dst_lo, xmm_dst_hi;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint8, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (
	mask_image, mask_x, mask_y, uint8, mask_stride, mask_line, 1);

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	xmm_alpha = expand_alpha_1x128 (expand_pixel_32_1x128 (src));

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w && ((uintptr_t)dst & 15))
	{
		m = (uint32) *mask++;
		d = (uint32) *dst;

		*dst++ = (uint8) pack_1x128_32 (
		pix_multiply_1x128 (
			pix_multiply_1x128 (xmm_alpha,
					   unpack_32_1x128 (m)),
			unpack_32_1x128 (d)));
		w--;
	}

	while (w >= 16)
	{
		xmm_mask = load_128_unaligned ((__m128i*)mask);
		xmm_dst = load_128_aligned ((__m128i*)dst);

		unpack_128_2x128 (xmm_mask, &xmm_mask_lo, &xmm_mask_hi);
		unpack_128_2x128 (xmm_dst, &xmm_dst_lo, &xmm_dst_hi);

		pix_multiply_2x128 (&xmm_alpha, &xmm_alpha,
				&xmm_mask_lo, &xmm_mask_hi,
				&xmm_mask_lo, &xmm_mask_hi);

		pix_multiply_2x128 (&xmm_mask_lo, &xmm_mask_hi,
				&xmm_dst_lo, &xmm_dst_hi,
				&xmm_dst_lo, &xmm_dst_hi);

		save_128_aligned (
		(__m128i*)dst, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

		mask += 16;
		dst += 16;
		w -= 16;
	}

	while (w)
	{
		m = (uint32) *mask++;
		d = (uint32) *dst;

		*dst++ = (uint8) pack_1x128_32 (
		pix_multiply_1x128 (
			pix_multiply_1x128 (
			xmm_alpha, unpack_32_1x128 (m)),
			unpack_32_1x128 (d)));
		w--;
	}
	}

}

static void
sse2_composite_in_n_8 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
			   PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint8	 *dst_line, *dst;
	int dst_stride;
	uint32 d;
	uint32 src;
	int32 w;

	__m128i xmm_alpha;
	__m128i xmm_dst, xmm_dst_lo, xmm_dst_hi;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint8, dst_stride, dst_line, 1);

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	xmm_alpha = expand_alpha_1x128 (expand_pixel_32_1x128 (src));

	src = src >> 24;

	if (src == 0xff)
	return;

	if (src == 0x00)
	{
	PixelManipulateFill (dest_image->bits.bits, dest_image->bits.row_stride,
			 8, dest_x, dest_y, width, height, src, imp);

	return;
	}

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	w = width;

	while (w && ((uintptr_t)dst & 15))
	{
		d = (uint32) *dst;

		*dst++ = (uint8) pack_1x128_32 (
		pix_multiply_1x128 (
			xmm_alpha,
			unpack_32_1x128 (d)));
		w--;
	}

	while (w >= 16)
	{
		xmm_dst = load_128_aligned ((__m128i*)dst);

		unpack_128_2x128 (xmm_dst, &xmm_dst_lo, &xmm_dst_hi);

		pix_multiply_2x128 (&xmm_alpha, &xmm_alpha,
				&xmm_dst_lo, &xmm_dst_hi,
				&xmm_dst_lo, &xmm_dst_hi);

		save_128_aligned (
		(__m128i*)dst, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

		dst += 16;
		w -= 16;
	}

	while (w)
	{
		d = (uint32) *dst;

		*dst++ = (uint8) pack_1x128_32 (
		pix_multiply_1x128 (
			xmm_alpha,
			unpack_32_1x128 (d)));
		w--;
	}
	}

}

static void
sse2_composite_in_8_8 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					   PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint8	 *dst_line, *dst;
	uint8	 *src_line, *src;
	int src_stride, dst_stride;
	int32 w;
	uint32 s, d;

	__m128i xmm_src, xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst, xmm_dst_lo, xmm_dst_hi;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint8, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (
	src_image, src_x, src_y, uint8, src_stride, src_line, 1);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w && ((uintptr_t)dst & 15))
	{
		s = (uint32) *src++;
		d = (uint32) *dst;

		*dst++ = (uint8) pack_1x128_32 (
		pix_multiply_1x128 (
			unpack_32_1x128 (s), unpack_32_1x128 (d)));
		w--;
	}

	while (w >= 16)
	{
		xmm_src = load_128_unaligned ((__m128i*)src);
		xmm_dst = load_128_aligned ((__m128i*)dst);

		unpack_128_2x128 (xmm_src, &xmm_src_lo, &xmm_src_hi);
		unpack_128_2x128 (xmm_dst, &xmm_dst_lo, &xmm_dst_hi);

		pix_multiply_2x128 (&xmm_src_lo, &xmm_src_hi,
				&xmm_dst_lo, &xmm_dst_hi,
				&xmm_dst_lo, &xmm_dst_hi);

		save_128_aligned (
		(__m128i*)dst, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

		src += 16;
		dst += 16;
		w -= 16;
	}

	while (w)
	{
		s = (uint32) *src++;
		d = (uint32) *dst;

		*dst++ = (uint8) pack_1x128_32 (
		pix_multiply_1x128 (unpack_32_1x128 (s), unpack_32_1x128 (d)));
		w--;
	}
	}

}

static void
sse2_composite_add_n_8_8 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
			  PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint8	 *dst_line, *dst;
	uint8	 *mask_line, *mask;
	int dst_stride, mask_stride;
	int32 w;
	uint32 src;
	uint32 m, d;

	__m128i xmm_alpha;
	__m128i xmm_mask, xmm_mask_lo, xmm_mask_hi;
	__m128i xmm_dst, xmm_dst_lo, xmm_dst_hi;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint8, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (
	mask_image, mask_x, mask_y, uint8, mask_stride, mask_line, 1);

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	xmm_alpha = expand_alpha_1x128 (expand_pixel_32_1x128 (src));

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w && ((uintptr_t)dst & 15))
	{
		m = (uint32) *mask++;
		d = (uint32) *dst;

		*dst++ = (uint8) pack_1x128_32 (
		_mm_adds_epu16 (
			pix_multiply_1x128 (
			xmm_alpha, unpack_32_1x128 (m)),
			unpack_32_1x128 (d)));
		w--;
	}

	while (w >= 16)
	{
		xmm_mask = load_128_unaligned ((__m128i*)mask);
		xmm_dst = load_128_aligned ((__m128i*)dst);

		unpack_128_2x128 (xmm_mask, &xmm_mask_lo, &xmm_mask_hi);
		unpack_128_2x128 (xmm_dst, &xmm_dst_lo, &xmm_dst_hi);

		pix_multiply_2x128 (&xmm_alpha, &xmm_alpha,
				&xmm_mask_lo, &xmm_mask_hi,
				&xmm_mask_lo, &xmm_mask_hi);

		xmm_dst_lo = _mm_adds_epu16 (xmm_mask_lo, xmm_dst_lo);
		xmm_dst_hi = _mm_adds_epu16 (xmm_mask_hi, xmm_dst_hi);

		save_128_aligned (
		(__m128i*)dst, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));

		mask += 16;
		dst += 16;
		w -= 16;
	}

	while (w)
	{
		m = (uint32) *mask++;
		d = (uint32) *dst;

		*dst++ = (uint8) pack_1x128_32 (
		_mm_adds_epu16 (
			pix_multiply_1x128 (
			xmm_alpha, unpack_32_1x128 (m)),
			unpack_32_1x128 (d)));

		w--;
	}
	}

}

static void sse2_composite_add_n_8(PIXEL_MANIPULATE_IMPLEMENTATION *imp,
			PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint8	 *dst_line, *dst;
	int dst_stride;
	int32 w;
	uint32 src;

	__m128i xmm_src;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint8, dst_stride, dst_line, 1);

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	src >>= 24;

	if (src == 0x00)
	return;

	if (src == 0xff)
	{
	PixelManipulateFill (dest_image->bits.bits, dest_image->bits.row_stride,
			 8, dest_x, dest_y, width, height, 0xff, imp);

	return;
	}

	src = (src << 24) | (src << 16) | (src << 8) | src;
	xmm_src = _mm_set_epi32 (src, src, src, src);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	w = width;

	while (w && ((uintptr_t)dst & 15))
	{
		*dst = (uint8)_mm_cvtsi128_si32 (
		_mm_adds_epu8 (
			xmm_src,
			_mm_cvtsi32_si128 (*dst)));

		w--;
		dst++;
	}

	while (w >= 16)
	{
		save_128_aligned (
		(__m128i*)dst, _mm_adds_epu8 (xmm_src, load_128_aligned  ((__m128i*)dst)));

		dst += 16;
		w -= 16;
	}

	while (w)
	{
		*dst = (uint8)_mm_cvtsi128_si32 (
		_mm_adds_epu8 (
			xmm_src,
			_mm_cvtsi32_si128 (*dst)));

		w--;
		dst++;
	}
	}

}

static void
sse2_composite_add_8_8 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
			PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint8	 *dst_line, *dst;
	uint8	 *src_line, *src;
	int dst_stride, src_stride;
	int32 w;
	uint16 t;

	IMAGE_GET_LINE (
	src_image, src_x, src_y, uint8, src_stride, src_line, 1);
	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint8, dst_stride, dst_line, 1);

	while (height--)
	{
	dst = dst_line;
	src = src_line;

	dst_line += dst_stride;
	src_line += src_stride;
	w = width;

	/* Small head */
	while (w && (uintptr_t)dst & 3)
	{
		t = (*dst) + (*src++);
		*dst++ = t | (0 - (t >> 8));
		w--;
	}

	sse2_combine_add_u (imp, op,
				(uint32*)dst, (uint32*)src, NULL, w >> 2);

	/* Small tail */
	dst += w & 0xfffc;
	src += w & 0xfffc;

	w &= 3;

	while (w)
	{
		t = (*dst) + (*src++);
		*dst++ = t | (0 - (t >> 8));
		w--;
	}
	}

}

static void
sse2_composite_add_8888_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							  PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32	*dst_line, *dst;
	uint32	*src_line, *src;
	int dst_stride, src_stride;

	IMAGE_GET_LINE (
	src_image, src_x, src_y, uint32, src_stride, src_line, 1);
	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;

	sse2_combine_add_u (imp, op, dst, src, NULL, width);
	}
}

static void
sse2_composite_add_n_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
			   PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32 *dst_line, *dst, src;
	int dst_stride;

	__m128i xmm_src;

	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);
	if (src == 0)
	return;

	if (src == ~0)
	{
	PixelManipulateFill (dest_image->bits.bits, dest_image->bits.row_stride, 32,
			 dest_x, dest_y, width, height, ~0, imp);

	return;
	}

	xmm_src = _mm_set_epi32 (src, src, src, src);
	while (height--)
	{
	int w = width;
	uint32 d;

	dst = dst_line;
	dst_line += dst_stride;

	while (w && (uintptr_t)dst & 15)
	{
		d = *dst;
		*dst++ =
		_mm_cvtsi128_si32 ( _mm_adds_epu8 (xmm_src, _mm_cvtsi32_si128 (d)));
		w--;
	}

	while (w >= 4)
	{
		save_128_aligned
		((__m128i*)dst,
		 _mm_adds_epu8 (xmm_src, load_128_aligned ((__m128i*)dst)));

		dst += 4;
		w -= 4;
	}

	while (w--)
	{
		d = *dst;
		*dst++ =
		_mm_cvtsi128_si32 (_mm_adds_epu8 (xmm_src,
						  _mm_cvtsi32_si128 (d)));
	}
	}
}

static void
sse2_composite_add_n_8_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
				 PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32	 *dst_line, *dst;
	uint8	 *mask_line, *mask;
	int dst_stride, mask_stride;
	int32 w;
	uint32 src;

	__m128i xmm_src;

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);
	if (src == 0)
	return;
	xmm_src = expand_pixel_32_1x128 (src);

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (
	mask_image, mask_x, mask_y, uint8, mask_stride, mask_line, 1);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w && ((uintptr_t)dst & 15))
	{
		uint8 m = *mask++;
		if (m)
		{
		*dst = pack_1x128_32
			(_mm_adds_epu16
			 (pix_multiply_1x128 (xmm_src, expand_pixel_8_1x128 (m)),
			  unpack_32_1x128 (*dst)));
		}
		dst++;
		w--;
	}

	while (w >= 4)
	{
		uint32 m;
			memcpy(&m, mask, sizeof(uint32));

		if (m)
		{
		__m128i xmm_mask_lo, xmm_mask_hi;
		__m128i xmm_dst_lo, xmm_dst_hi;

		__m128i xmm_dst = load_128_aligned ((__m128i*)dst);
		__m128i xmm_mask =
			_mm_unpacklo_epi8 (unpack_32_1x128(m),
					   _mm_setzero_si128 ());

		unpack_128_2x128 (xmm_mask, &xmm_mask_lo, &xmm_mask_hi);
		unpack_128_2x128 (xmm_dst, &xmm_dst_lo, &xmm_dst_hi);

		expand_alpha_rev_2x128 (xmm_mask_lo, xmm_mask_hi,
					&xmm_mask_lo, &xmm_mask_hi);

		pix_multiply_2x128 (&xmm_src, &xmm_src,
					&xmm_mask_lo, &xmm_mask_hi,
					&xmm_mask_lo, &xmm_mask_hi);

		xmm_dst_lo = _mm_adds_epu16 (xmm_mask_lo, xmm_dst_lo);
		xmm_dst_hi = _mm_adds_epu16 (xmm_mask_hi, xmm_dst_hi);

		save_128_aligned (
			(__m128i*)dst, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));
		}

		w -= 4;
		dst += 4;
		mask += 4;
	}

	while (w)
	{
		uint8 m = *mask++;
		if (m)
		{
		*dst = pack_1x128_32
			(_mm_adds_epu16
			 (pix_multiply_1x128 (xmm_src, expand_pixel_8_1x128 (m)),
			  unpack_32_1x128 (*dst)));
		}
		dst++;
		w--;
	}
	}
}

static int
sse2_blt (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
		  uint32 *			   src_bits,
		  uint32 *			   dst_bits,
		  int					  src_stride,
		  int					  dst_stride,
		  int					  src_bpp,
		  int					  dst_bpp,
		  int					  src_x,
		  int					  src_y,
		  int					  dest_x,
		  int					  dest_y,
		  int					  width,
		  int					  height)
{
	uint8 *   src_bytes;
	uint8 *   dst_bytes;
	int byte_width;

	if (src_bpp != dst_bpp)
	return FALSE;

	if (src_bpp == 16)
	{
	src_stride = src_stride * (int) sizeof (uint32) / 2;
	dst_stride = dst_stride * (int) sizeof (uint32) / 2;
	src_bytes =(uint8 *)(((uint16 *)src_bits) + src_stride * (src_y) + (src_x));
	dst_bytes = (uint8 *)(((uint16 *)dst_bits) + dst_stride * (dest_y) + (dest_x));
	byte_width = 2 * width;
	src_stride *= 2;
	dst_stride *= 2;
	}
	else if (src_bpp == 32)
	{
	src_stride = src_stride * (int) sizeof (uint32) / 4;
	dst_stride = dst_stride * (int) sizeof (uint32) / 4;
	src_bytes = (uint8 *)(((uint32 *)src_bits) + src_stride * (src_y) + (src_x));
	dst_bytes = (uint8 *)(((uint32 *)dst_bits) + dst_stride * (dest_y) + (dest_x));
	byte_width = 4 * width;
	src_stride *= 4;
	dst_stride *= 4;
	}
	else
	{
	return FALSE;
	}

	while (height--)
	{
	int w;
	uint8 *s = src_bytes;
	uint8 *d = dst_bytes;
	src_bytes += src_stride;
	dst_bytes += dst_stride;
	w = byte_width;

	while (w >= 2 && ((uintptr_t)d & 3))
	{
			memmove(d, s, 2);
		w -= 2;
		s += 2;
		d += 2;
	}

	while (w >= 4 && ((uintptr_t)d & 15))
	{
			memmove(d, s, 4);

		w -= 4;
		s += 4;
		d += 4;
	}

	while (w >= 64)
	{
		__m128i xmm0, xmm1, xmm2, xmm3;

		xmm0 = load_128_unaligned ((__m128i*)(s));
		xmm1 = load_128_unaligned ((__m128i*)(s + 16));
		xmm2 = load_128_unaligned ((__m128i*)(s + 32));
		xmm3 = load_128_unaligned ((__m128i*)(s + 48));

		save_128_aligned ((__m128i*)(d),	xmm0);
		save_128_aligned ((__m128i*)(d + 16), xmm1);
		save_128_aligned ((__m128i*)(d + 32), xmm2);
		save_128_aligned ((__m128i*)(d + 48), xmm3);

		s += 64;
		d += 64;
		w -= 64;
	}

	while (w >= 16)
	{
		save_128_aligned ((__m128i*)d, load_128_unaligned ((__m128i*)s) );

		w -= 16;
		d += 16;
		s += 16;
	}

	while (w >= 4)
	{
			memmove(d, s, 4);

		w -= 4;
		s += 4;
		d += 4;
	}

	if (w >= 2)
	{
			memmove(d, s, 2);
		w -= 2;
		s += 2;
		d += 2;
	}
	}

	return TRUE;
}

static void
sse2_composite_copy_area (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
						  PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	sse2_blt (imp, src_image->bits.bits,
		  dest_image->bits.bits,
		  src_image->bits.row_stride,
		  dest_image->bits.row_stride,
		  PIXEL_MANIPULATE_FORMAT_BPP (src_image->bits.format),
		  PIXEL_MANIPULATE_FORMAT_BPP (dest_image->bits.format),
		  src_x, src_y, dest_x, dest_y, width, height);
}

static void
sse2_composite_over_X888_8_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
								 PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32	*src, *src_line, s;
	uint32	*dst, *dst_line, d;
	uint8		 *mask, *mask_line;
	uint32 m;
	int src_stride, mask_stride, dst_stride;
	int32 w;
	__m128i ms;

	__m128i xmm_src, xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst, xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_mask, xmm_mask_lo, xmm_mask_hi;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (
	mask_image, mask_x, mask_y, uint8, mask_stride, mask_line, 1);
	IMAGE_GET_LINE (
	src_image, src_x, src_y, uint32, src_stride, src_line, 1);

	while (height--)
	{
		src = src_line;
		src_line += src_stride;
		dst = dst_line;
		dst_line += dst_stride;
		mask = mask_line;
		mask_line += mask_stride;

		w = width;

		while (w && (uintptr_t)dst & 15)
		{
			s = 0xff000000 | *src++;
			memcpy(&m, mask++, sizeof(uint32));
			d = *dst;
			ms = unpack_32_1x128 (s);

			if (m != 0xff)
			{
		__m128i ma = expand_alpha_rev_1x128 (unpack_32_1x128 (m));
		__m128i md = unpack_32_1x128 (d);

				ms = in_over_1x128 (&ms, &mask_00ff, &ma, &md);
			}

			*dst++ = pack_1x128_32 (ms);
			w--;
		}

		while (w >= 4)
		{
			memcpy(&m, mask, sizeof(uint32));
			xmm_src = _mm_or_si128 (
		load_128_unaligned ((__m128i*)src), mask_ff000000);

			if (m == 0xffffffff)
			{
				save_128_aligned ((__m128i*)dst, xmm_src);
			}
			else
			{
				xmm_dst = load_128_aligned ((__m128i*)dst);

				xmm_mask = _mm_unpacklo_epi16 (unpack_32_1x128 (m), _mm_setzero_si128());

				unpack_128_2x128 (xmm_src, &xmm_src_lo, &xmm_src_hi);
				unpack_128_2x128 (xmm_mask, &xmm_mask_lo, &xmm_mask_hi);
				unpack_128_2x128 (xmm_dst, &xmm_dst_lo, &xmm_dst_hi);

				expand_alpha_rev_2x128 (
			xmm_mask_lo, xmm_mask_hi, &xmm_mask_lo, &xmm_mask_hi);

				in_over_2x128 (&xmm_src_lo, &xmm_src_hi,
				   &mask_00ff, &mask_00ff, &xmm_mask_lo, &xmm_mask_hi,
				   &xmm_dst_lo, &xmm_dst_hi);

				save_128_aligned ((__m128i*)dst, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));
			}

			src += 4;
			dst += 4;
			mask += 4;
			w -= 4;
		}

		while (w)
		{
			memcpy(&m, mask++, sizeof(uint32));

			if (m)
			{
				s = 0xff000000 | *src;

				if (m == 0xff)
				{
					*dst = s;
				}
				else
				{
			__m128i ma, md, ms;

					d = *dst;

			ma = expand_alpha_rev_1x128 (unpack_32_1x128 (m));
			md = unpack_32_1x128 (d);
			ms = unpack_32_1x128 (s);

					*dst = pack_1x128_32 (in_over_1x128 (&ms, &mask_00ff, &ma, &md));
				}

			}

			src++;
			dst++;
			w--;
		}
	}

}

static void
sse2_composite_over_8888_8_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
								 PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32	*src, *src_line, s;
	uint32	*dst, *dst_line, d;
	uint8		 *mask, *mask_line;
	uint32 m;
	int src_stride, mask_stride, dst_stride;
	int32 w;

	__m128i xmm_src, xmm_src_lo, xmm_src_hi, xmm_srca_lo, xmm_srca_hi;
	__m128i xmm_dst, xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_mask, xmm_mask_lo, xmm_mask_hi;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (
	mask_image, mask_x, mask_y, uint8, mask_stride, mask_line, 1);
	IMAGE_GET_LINE (
	src_image, src_x, src_y, uint32, src_stride, src_line, 1);

	while (height--)
	{
		src = src_line;
		src_line += src_stride;
		dst = dst_line;
		dst_line += dst_stride;
		mask = mask_line;
		mask_line += mask_stride;

		w = width;

		while (w && (uintptr_t)dst & 15)
		{
		uint32 sa;

			s = *src++;
			m = (uint32) *mask++;
			d = *dst;

		sa = s >> 24;

		if (m)
		{
		if (sa == 0xff && m == 0xff)
		{
			*dst = s;
		}
		else
		{
			__m128i ms, md, ma, msa;

			ma = expand_alpha_rev_1x128 (load_32_1x128 (m));
			ms = unpack_32_1x128 (s);
			md = unpack_32_1x128 (d);

			msa = expand_alpha_rev_1x128 (load_32_1x128 (sa));

			*dst = pack_1x128_32 (in_over_1x128 (&ms, &msa, &ma, &md));
		}
		}

		dst++;
			w--;
		}

		while (w >= 4)
		{
			memcpy(&m, mask, sizeof(uint32));

		if (m)
		{
		xmm_src = load_128_unaligned ((__m128i*)src);

		if (m == 0xffffffff && is_opaque (xmm_src))
		{
			save_128_aligned ((__m128i *)dst, xmm_src);
		}
		else
		{
			xmm_dst = load_128_aligned ((__m128i *)dst);

			xmm_mask = _mm_unpacklo_epi16 (unpack_32_1x128 (m), _mm_setzero_si128());

			unpack_128_2x128 (xmm_src, &xmm_src_lo, &xmm_src_hi);
			unpack_128_2x128 (xmm_mask, &xmm_mask_lo, &xmm_mask_hi);
			unpack_128_2x128 (xmm_dst, &xmm_dst_lo, &xmm_dst_hi);

			expand_alpha_2x128 (xmm_src_lo, xmm_src_hi, &xmm_srca_lo, &xmm_srca_hi);
			expand_alpha_rev_2x128 (xmm_mask_lo, xmm_mask_hi, &xmm_mask_lo, &xmm_mask_hi);

			in_over_2x128 (&xmm_src_lo, &xmm_src_hi, &xmm_srca_lo, &xmm_srca_hi,
				   &xmm_mask_lo, &xmm_mask_hi, &xmm_dst_lo, &xmm_dst_hi);

			save_128_aligned ((__m128i*)dst, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));
		}
		}

			src += 4;
			dst += 4;
			mask += 4;
			w -= 4;
		}

		while (w)
		{
		uint32 sa;

			s = *src++;
			m = (uint32) *mask++;
			d = *dst;

		sa = s >> 24;

		if (m)
		{
		if (sa == 0xff && m == 0xff)
		{
			*dst = s;
		}
		else
		{
			__m128i ms, md, ma, msa;

			ma = expand_alpha_rev_1x128 (load_32_1x128 (m));
			ms = unpack_32_1x128 (s);
			md = unpack_32_1x128 (d);

			msa = expand_alpha_rev_1x128 (load_32_1x128 (sa));

			*dst = pack_1x128_32 (in_over_1x128 (&ms, &msa, &ma, &md));
		}
		}

		dst++;
			w--;
		}
	}

}

static void
sse2_composite_over_reverse_n_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32 src;
	uint32	*dst_line, *dst;
	__m128i xmm_src;
	__m128i xmm_dst, xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_dsta_hi, xmm_dsta_lo;
	int dst_stride;
	int32 w;

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	if (src == 0)
	return;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);

	xmm_src = expand_pixel_32_1x128 (src);

	while (height--)
	{
	dst = dst_line;

	dst_line += dst_stride;
	w = width;

	while (w && (uintptr_t)dst & 15)
	{
		__m128i vd;

		vd = unpack_32_1x128 (*dst);

		*dst = pack_1x128_32 (over_1x128 (vd, expand_alpha_1x128 (vd),
						  xmm_src));
		w--;
		dst++;
	}

	while (w >= 4)
	{
		__m128i tmp_lo, tmp_hi;

		xmm_dst = load_128_aligned ((__m128i*)dst);

		unpack_128_2x128 (xmm_dst, &xmm_dst_lo, &xmm_dst_hi);
		expand_alpha_2x128 (xmm_dst_lo, xmm_dst_hi, &xmm_dsta_lo, &xmm_dsta_hi);

		tmp_lo = xmm_src;
		tmp_hi = xmm_src;

		over_2x128 (&xmm_dst_lo, &xmm_dst_hi,
			&xmm_dsta_lo, &xmm_dsta_hi,
			&tmp_lo, &tmp_hi);

		save_128_aligned (
		(__m128i*)dst, pack_2x128_128 (tmp_lo, tmp_hi));

		w -= 4;
		dst += 4;
	}

	while (w)
	{
		__m128i vd;

		vd = unpack_32_1x128 (*dst);

		*dst = pack_1x128_32 (over_1x128 (vd, expand_alpha_1x128 (vd),
						  xmm_src));
		w--;
		dst++;
	}

	}

}

static void
sse2_composite_over_8888_8888_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32	*src, *src_line, s;
	uint32	*dst, *dst_line, d;
	uint32	*mask, *mask_line;
	uint32	m;
	int src_stride, mask_stride, dst_stride;
	int32 w;

	__m128i xmm_src, xmm_src_lo, xmm_src_hi, xmm_srca_lo, xmm_srca_hi;
	__m128i xmm_dst, xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_mask, xmm_mask_lo, xmm_mask_hi;

	IMAGE_GET_LINE (
	dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (
	mask_image, mask_x, mask_y, uint32, mask_stride, mask_line, 1);
	IMAGE_GET_LINE (
	src_image, src_x, src_y, uint32, src_stride, src_line, 1);

	while (height--)
	{
		src = src_line;
		src_line += src_stride;
		dst = dst_line;
		dst_line += dst_stride;
		mask = mask_line;
		mask_line += mask_stride;

		w = width;

		while (w && (uintptr_t)dst & 15)
		{
		uint32 sa;

			s = *src++;
			m = (*mask++) >> 24;
			d = *dst;

		sa = s >> 24;

		if (m)
		{
		if (sa == 0xff && m == 0xff)
		{
			*dst = s;
		}
		else
		{
			__m128i ms, md, ma, msa;

			ma = expand_alpha_rev_1x128 (load_32_1x128 (m));
			ms = unpack_32_1x128 (s);
			md = unpack_32_1x128 (d);

			msa = expand_alpha_rev_1x128 (load_32_1x128 (sa));

			*dst = pack_1x128_32 (in_over_1x128 (&ms, &msa, &ma, &md));
		}
		}

		dst++;
			w--;
		}

		while (w >= 4)
		{
		xmm_mask = load_128_unaligned ((__m128i*)mask);

		if (!is_transparent (xmm_mask))
		{
		xmm_src = load_128_unaligned ((__m128i*)src);

		if (is_opaque (xmm_mask) && is_opaque (xmm_src))
		{
			save_128_aligned ((__m128i *)dst, xmm_src);
		}
		else
		{
			xmm_dst = load_128_aligned ((__m128i *)dst);

			unpack_128_2x128 (xmm_src, &xmm_src_lo, &xmm_src_hi);
			unpack_128_2x128 (xmm_mask, &xmm_mask_lo, &xmm_mask_hi);
			unpack_128_2x128 (xmm_dst, &xmm_dst_lo, &xmm_dst_hi);

			expand_alpha_2x128 (xmm_src_lo, xmm_src_hi, &xmm_srca_lo, &xmm_srca_hi);
			expand_alpha_2x128 (xmm_mask_lo, xmm_mask_hi, &xmm_mask_lo, &xmm_mask_hi);

			in_over_2x128 (&xmm_src_lo, &xmm_src_hi, &xmm_srca_lo, &xmm_srca_hi,
				   &xmm_mask_lo, &xmm_mask_hi, &xmm_dst_lo, &xmm_dst_hi);

			save_128_aligned ((__m128i*)dst, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));
		}
		}

			src += 4;
			dst += 4;
			mask += 4;
			w -= 4;
		}

		while (w)
		{
		uint32 sa;

			s = *src++;
			m = (*mask++) >> 24;
			d = *dst;

		sa = s >> 24;

		if (m)
		{
		if (sa == 0xff && m == 0xff)
		{
			*dst = s;
		}
		else
		{
			__m128i ms, md, ma, msa;

			ma = expand_alpha_rev_1x128 (load_32_1x128 (m));
			ms = unpack_32_1x128 (s);
			md = unpack_32_1x128 (d);

			msa = expand_alpha_rev_1x128 (load_32_1x128 (sa));

			*dst = pack_1x128_32 (in_over_1x128 (&ms, &msa, &ma, &md));
		}
		}

		dst++;
			w--;
		}
	}

}

/* A variant of 'sse2_combine_over_u' with minor tweaks */
static FORCE_INLINE void
scaled_nearest_scanline_sse2_8888_8888_OVER (uint32*	   pd,
											 const uint32* ps,
											 int32		 w,
											 PIXEL_MANIPULATE_FIXED  vx,
											 PIXEL_MANIPULATE_FIXED  unit_x,
											 PIXEL_MANIPULATE_FIXED  src_width_fixed,
											 int  fully_transparent_src)
{
	uint32 s, d;
	const uint32* pm = NULL;

	__m128i xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_src_lo, xmm_src_hi;
	__m128i xmm_alpha_lo, xmm_alpha_hi;

	if (fully_transparent_src)
	return;

	/* Align dst on a 16-byte boundary */
	while (w && ((uintptr_t)pd & 15))
	{
	d = *pd;
	s = combine1 (ps + PIXEL_MANIPULATE_FIXED_TO_INTEGER (vx), pm);
	vx += unit_x;
	while (vx >= 0)
		vx -= src_width_fixed;

	*pd++ = core_combine_over_u_pixel_sse2 (s, d);
	if (pm)
		pm++;
	w--;
	}

	while (w >= 4)
	{
	__m128i tmp;
	uint32 tmp1, tmp2, tmp3, tmp4;

	tmp1 = *(ps + PIXEL_MANIPULATE_FIXED_TO_INTEGER (vx));
	vx += unit_x;
	while (vx >= 0)
		vx -= src_width_fixed;
	tmp2 = *(ps + PIXEL_MANIPULATE_FIXED_TO_INTEGER (vx));
	vx += unit_x;
	while (vx >= 0)
		vx -= src_width_fixed;
	tmp3 = *(ps + PIXEL_MANIPULATE_FIXED_TO_INTEGER (vx));
	vx += unit_x;
	while (vx >= 0)
		vx -= src_width_fixed;
	tmp4 = *(ps + PIXEL_MANIPULATE_FIXED_TO_INTEGER (vx));
	vx += unit_x;
	while (vx >= 0)
		vx -= src_width_fixed;

	tmp = _mm_set_epi32 (tmp4, tmp3, tmp2, tmp1);

	xmm_src_hi = combine4 ((__m128i*)&tmp, (__m128i*)pm);

	if (is_opaque (xmm_src_hi))
	{
		save_128_aligned ((__m128i*)pd, xmm_src_hi);
	}
	else if (!is_zero (xmm_src_hi))
	{
		xmm_dst_hi = load_128_aligned ((__m128i*) pd);

		unpack_128_2x128 (xmm_src_hi, &xmm_src_lo, &xmm_src_hi);
		unpack_128_2x128 (xmm_dst_hi, &xmm_dst_lo, &xmm_dst_hi);

		expand_alpha_2x128 (
		xmm_src_lo, xmm_src_hi, &xmm_alpha_lo, &xmm_alpha_hi);

		over_2x128 (&xmm_src_lo, &xmm_src_hi,
			&xmm_alpha_lo, &xmm_alpha_hi,
			&xmm_dst_lo, &xmm_dst_hi);

		/* rebuid the 4 pixel data and save*/
		save_128_aligned ((__m128i*)pd,
				  pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));
	}

	w -= 4;
	pd += 4;
	if (pm)
		pm += 4;
	}

	while (w)
	{
	d = *pd;
	s = combine1 (ps + PIXEL_MANIPULATE_FIXED_TO_INTEGER (vx), pm);
	vx += unit_x;
	while (vx >= 0)
		vx -= src_width_fixed;

	*pd++ = core_combine_over_u_pixel_sse2 (s, d);
	if (pm)
		pm++;

	w--;
	}
}

FAST_NEAREST_MAINLOOP (sse2_8888_8888_cover_OVER,
			   scaled_nearest_scanline_sse2_8888_8888_OVER,
			   uint32, uint32, COVER)
FAST_NEAREST_MAINLOOP (sse2_8888_8888_none_OVER,
			   scaled_nearest_scanline_sse2_8888_8888_OVER,
			   uint32, uint32, NONE)
FAST_NEAREST_MAINLOOP (sse2_8888_8888_pad_OVER,
			   scaled_nearest_scanline_sse2_8888_8888_OVER,
			   uint32, uint32, PAD)
FAST_NEAREST_MAINLOOP (sse2_8888_8888_normal_OVER,
			   scaled_nearest_scanline_sse2_8888_8888_OVER,
			   uint32, uint32, NORMAL)

static FORCE_INLINE void
scaled_nearest_scanline_sse2_8888_n_8888_OVER (const uint32 * mask,
						   uint32 *	   dst,
						   const uint32 * src,
						   int32		  w,
						   PIXEL_MANIPULATE_FIXED   vx,
						   PIXEL_MANIPULATE_FIXED   unit_x,
						   PIXEL_MANIPULATE_FIXED   src_width_fixed,
						   int	zero_src)
{
	__m128i xmm_mask;
	__m128i xmm_src, xmm_src_lo, xmm_src_hi;
	__m128i xmm_dst, xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_alpha_lo, xmm_alpha_hi;

	if (zero_src || (*mask >> 24) == 0)
	return;

	xmm_mask = create_mask_16_128 (*mask >> 24);

	while (w && (uintptr_t)dst & 15)
	{
	uint32 s = *(src + PIXEL_MANIPULATE_FIXED_TO_INTEGER (vx));
	vx += unit_x;
	while (vx >= 0)
		vx -= src_width_fixed;

	if (s)
	{
		uint32 d = *dst;

		__m128i ms = unpack_32_1x128 (s);
		__m128i alpha	 = expand_alpha_1x128 (ms);
		__m128i dest	  = xmm_mask;
		__m128i alpha_dst = unpack_32_1x128 (d);

		*dst = pack_1x128_32 (
		in_over_1x128 (&ms, &alpha, &dest, &alpha_dst));
	}
	dst++;
	w--;
	}

	while (w >= 4)
	{
	uint32 tmp1, tmp2, tmp3, tmp4;

	tmp1 = *(src + PIXEL_MANIPULATE_FIXED_TO_INTEGER (vx));
	vx += unit_x;
	while (vx >= 0)
		vx -= src_width_fixed;
	tmp2 = *(src + PIXEL_MANIPULATE_FIXED_TO_INTEGER (vx));
	vx += unit_x;
	while (vx >= 0)
		vx -= src_width_fixed;
	tmp3 = *(src + PIXEL_MANIPULATE_FIXED_TO_INTEGER (vx));
	vx += unit_x;
	while (vx >= 0)
		vx -= src_width_fixed;
	tmp4 = *(src + PIXEL_MANIPULATE_FIXED_TO_INTEGER (vx));
	vx += unit_x;
	while (vx >= 0)
		vx -= src_width_fixed;

	xmm_src = _mm_set_epi32 (tmp4, tmp3, tmp2, tmp1);

	if (!is_zero (xmm_src))
	{
		xmm_dst = load_128_aligned ((__m128i*)dst);

		unpack_128_2x128 (xmm_src, &xmm_src_lo, &xmm_src_hi);
		unpack_128_2x128 (xmm_dst, &xmm_dst_lo, &xmm_dst_hi);
		expand_alpha_2x128 (xmm_src_lo, xmm_src_hi,
					&xmm_alpha_lo, &xmm_alpha_hi);

		in_over_2x128 (&xmm_src_lo, &xmm_src_hi,
			   &xmm_alpha_lo, &xmm_alpha_hi,
			   &xmm_mask, &xmm_mask,
			   &xmm_dst_lo, &xmm_dst_hi);

		save_128_aligned (
		(__m128i*)dst, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));
	}

	dst += 4;
	w -= 4;
	}

	while (w)
	{
	uint32 s = *(src + PIXEL_MANIPULATE_FIXED_TO_INTEGER (vx));
	vx += unit_x;
	while (vx >= 0)
		vx -= src_width_fixed;

	if (s)
	{
		uint32 d = *dst;

		__m128i ms = unpack_32_1x128 (s);
		__m128i alpha = expand_alpha_1x128 (ms);
		__m128i mask  = xmm_mask;
		__m128i dest  = unpack_32_1x128 (d);

		*dst = pack_1x128_32 (
		in_over_1x128 (&ms, &alpha, &mask, &dest));
	}

	dst++;
	w--;
	}

}

FAST_NEAREST_MAINLOOP_COMMON (sse2_8888_n_8888_cover_OVER,
				  scaled_nearest_scanline_sse2_8888_n_8888_OVER,
				  uint32, uint32, uint32, COVER, TRUE, TRUE)
FAST_NEAREST_MAINLOOP_COMMON (sse2_8888_n_8888_pad_OVER,
				  scaled_nearest_scanline_sse2_8888_n_8888_OVER,
				  uint32, uint32, uint32, PAD, TRUE, TRUE)
FAST_NEAREST_MAINLOOP_COMMON (sse2_8888_n_8888_none_OVER,
				  scaled_nearest_scanline_sse2_8888_n_8888_OVER,
				  uint32, uint32, uint32, NONE, TRUE, TRUE)
FAST_NEAREST_MAINLOOP_COMMON (sse2_8888_n_8888_normal_OVER,
				  scaled_nearest_scanline_sse2_8888_n_8888_OVER,
				  uint32, uint32, uint32, NORMAL, TRUE, TRUE)

#if PSHUFD_IS_FAST

/***********************************************************************************/

# define BILINEAR_DECLARE_VARIABLES						\
	const __m128i xmm_wt = _mm_set_epi16 (wt, wt, wt, wt, wt, wt, wt, wt);	\
	const __m128i xmm_wb = _mm_set_epi16 (wb, wb, wb, wb, wb, wb, wb, wb);	\
	const __m128i xmm_addc = _mm_set_epi16 (0, 1, 0, 1, 0, 1, 0, 1);		\
	const __m128i xmm_ux1 = _mm_set_epi16 (unit_x, -unit_x, unit_x, -unit_x,	\
					   unit_x, -unit_x, unit_x, -unit_x);	\
	const __m128i xmm_ux4 = _mm_set_epi16 (unit_x * 4, -unit_x * 4,		\
					   unit_x * 4, -unit_x * 4,		\
					   unit_x * 4, -unit_x * 4,		\
					   unit_x * 4, -unit_x * 4);		\
	const __m128i xmm_zero = _mm_setzero_si128 ();				\
	__m128i xmm_x = _mm_set_epi16 (vx + unit_x * 3, -(vx + 1) - unit_x * 3,	\
				   vx + unit_x * 2, -(vx + 1) - unit_x * 2,	\
				   vx + unit_x * 1, -(vx + 1) - unit_x * 1,	\
				   vx + unit_x * 0, -(vx + 1) - unit_x * 0);	\
	__m128i xmm_wh_state;

#define BILINEAR_INTERPOLATE_ONE_PIXEL_HELPER(pix, phase_)			\
do {										\
	int phase = phase_;								\
	__m128i xmm_wh, xmm_a, xmm_b;						\
	/* fetch 2x2 pixel block into sse2 registers */				\
	__m128i tltr = _mm_loadl_epi64 ((__m128i *)&src_top[vx >> 16]);		\
	__m128i blbr = _mm_loadl_epi64 ((__m128i *)&src_bottom[vx >> 16]);		\
	vx += unit_x;								\
	/* vertical interpolation */						\
	xmm_a = _mm_mullo_epi16 (_mm_unpacklo_epi8 (tltr, xmm_zero), xmm_wt);	\
	xmm_b = _mm_mullo_epi16 (_mm_unpacklo_epi8 (blbr, xmm_zero), xmm_wb);	\
	xmm_a = _mm_add_epi16 (xmm_a, xmm_b);						\
	/* calculate horizontal weights */						\
	if (phase <= 0)								\
	{										\
	xmm_wh_state = _mm_add_epi16 (xmm_addc, _mm_srli_epi16 (xmm_x,		\
					16 - BILINEAR_INTERPOLATION_BITS));	\
	xmm_x = _mm_add_epi16 (xmm_x, (phase < 0) ? xmm_ux1 : xmm_ux4);		\
	phase = 0;								\
	}										\
	xmm_wh = _mm_shuffle_epi32 (xmm_wh_state, _MM_SHUFFLE (phase, phase,	\
							   phase, phase));	\
	/* horizontal interpolation */						\
	xmm_a = _mm_madd_epi16 (_mm_unpackhi_epi16 (_mm_shuffle_epi32 (		\
		xmm_a, _MM_SHUFFLE (1, 0, 3, 2)), xmm_a), xmm_wh);		\
	/* shift the result */							\
	pix = _mm_srli_epi32 (xmm_a, BILINEAR_INTERPOLATION_BITS * 2);		\
} while (0)

#else /************************************************************************/

# define BILINEAR_DECLARE_VARIABLES						\
	const __m128i xmm_wt = _mm_set_epi16 (wt, wt, wt, wt, wt, wt, wt, wt);	\
	const __m128i xmm_wb = _mm_set_epi16 (wb, wb, wb, wb, wb, wb, wb, wb);	\
	const __m128i xmm_addc = _mm_set_epi16 (0, 1, 0, 1, 0, 1, 0, 1);		\
	const __m128i xmm_ux1 = _mm_set_epi16 (unit_x, -unit_x, unit_x, -unit_x,	\
					  unit_x, -unit_x, unit_x, -unit_x);	\
	const __m128i xmm_ux4 = _mm_set_epi16 (unit_x * 4, -unit_x * 4,		\
					   unit_x * 4, -unit_x * 4,		\
					   unit_x * 4, -unit_x * 4,		\
					   unit_x * 4, -unit_x * 4);		\
	const __m128i xmm_zero = _mm_setzero_si128 ();				\
	__m128i xmm_x = _mm_set_epi16 (vx, -(vx + 1), vx, -(vx + 1),		\
				   vx, -(vx + 1), vx, -(vx + 1))

#define BILINEAR_INTERPOLATE_ONE_PIXEL_HELPER(pix, phase)			\
do {										\
	__m128i xmm_wh, xmm_a, xmm_b;						\
	/* fetch 2x2 pixel block into sse2 registers */				\
	__m128i tltr = _mm_loadl_epi64 ((__m128i *)&src_top[vx >> 16]);		\
	__m128i blbr = _mm_loadl_epi64 ((__m128i *)&src_bottom[vx >> 16]);		\
	(void)xmm_ux4; /* suppress warning: unused variable 'xmm_ux4' */		\
	vx += unit_x;								\
	/* vertical interpolation */						\
	xmm_a = _mm_mullo_epi16 (_mm_unpacklo_epi8 (tltr, xmm_zero), xmm_wt);	\
	xmm_b = _mm_mullo_epi16 (_mm_unpacklo_epi8 (blbr, xmm_zero), xmm_wb);	\
	xmm_a = _mm_add_epi16 (xmm_a, xmm_b);					\
	/* calculate horizontal weights */						\
	xmm_wh = _mm_add_epi16 (xmm_addc, _mm_srli_epi16 (xmm_x,			\
					16 - BILINEAR_INTERPOLATION_BITS));	\
	xmm_x = _mm_add_epi16 (xmm_x, xmm_ux1);					\
	/* horizontal interpolation */						\
	xmm_b = _mm_unpacklo_epi64 (/* any value is fine here */ xmm_b, xmm_a);	\
	xmm_a = _mm_madd_epi16 (_mm_unpackhi_epi16 (xmm_b, xmm_a), xmm_wh);		\
	/* shift the result */							\
	pix = _mm_srli_epi32 (xmm_a, BILINEAR_INTERPOLATION_BITS * 2);		\
} while (0)

/***********************************************************************************/

#endif

#define BILINEAR_INTERPOLATE_ONE_PIXEL(pix);					\
do {										\
	__m128i xmm_pix;							\
	BILINEAR_INTERPOLATE_ONE_PIXEL_HELPER (xmm_pix, -1);			\
	xmm_pix = _mm_packs_epi32 (xmm_pix, xmm_pix);				\
	xmm_pix = _mm_packus_epi16 (xmm_pix, xmm_pix);				\
	pix = _mm_cvtsi128_si32 (xmm_pix);					\
} while(0)

#define BILINEAR_INTERPOLATE_FOUR_PIXELS(pix);					\
do {										\
	__m128i xmm_pix1, xmm_pix2, xmm_pix3, xmm_pix4;				\
	BILINEAR_INTERPOLATE_ONE_PIXEL_HELPER (xmm_pix1, 0);			\
	BILINEAR_INTERPOLATE_ONE_PIXEL_HELPER (xmm_pix2, 1);			\
	BILINEAR_INTERPOLATE_ONE_PIXEL_HELPER (xmm_pix3, 2);			\
	BILINEAR_INTERPOLATE_ONE_PIXEL_HELPER (xmm_pix4, 3);			\
	xmm_pix1 = _mm_packs_epi32 (xmm_pix1, xmm_pix2);			\
	xmm_pix3 = _mm_packs_epi32 (xmm_pix3, xmm_pix4);			\
	pix = _mm_packus_epi16 (xmm_pix1, xmm_pix3);				\
} while(0)

#define BILINEAR_SKIP_ONE_PIXEL()						\
do {										\
	vx += unit_x;								\
	xmm_x = _mm_add_epi16 (xmm_x, xmm_ux1);					\
} while(0)

#define BILINEAR_SKIP_FOUR_PIXELS()						\
do {										\
	vx += unit_x * 4;								\
	xmm_x = _mm_add_epi16 (xmm_x, xmm_ux4);					\
} while(0)

/***********************************************************************************/

static FORCE_INLINE void
scaled_bilinear_scanline_sse2_8888_8888_SRC (uint32 *	   dst,
						 const uint32 * mask,
						 const uint32 * src_top,
						 const uint32 * src_bottom,
						 int32		  w,
						 int			  wt,
						 int			  wb,
						 PIXEL_MANIPULATE_FIXED   vx_,
						 PIXEL_MANIPULATE_FIXED   unit_x_,
						 PIXEL_MANIPULATE_FIXED   max_vx,
						 int	zero_src)
{
	intptr_t vx = vx_;
	intptr_t unit_x = unit_x_;
	BILINEAR_DECLARE_VARIABLES;
	uint32 pix1, pix2;

	while (w && ((uintptr_t)dst & 15))
	{
	BILINEAR_INTERPOLATE_ONE_PIXEL (pix1);
	*dst++ = pix1;
	w--;
	}

	while ((w -= 4) >= 0) {
	__m128i xmm_src;
	BILINEAR_INTERPOLATE_FOUR_PIXELS (xmm_src);
	_mm_store_si128 ((__m128i *)dst, xmm_src);
	dst += 4;
	}

	if (w & 2)
	{
	BILINEAR_INTERPOLATE_ONE_PIXEL (pix1);
	BILINEAR_INTERPOLATE_ONE_PIXEL (pix2);
	*dst++ = pix1;
	*dst++ = pix2;
	}

	if (w & 1)
	{
	BILINEAR_INTERPOLATE_ONE_PIXEL (pix1);
	*dst = pix1;
	}

}

FAST_BILINEAR_MAINLOOP_COMMON (sse2_8888_8888_cover_SRC,
				   scaled_bilinear_scanline_sse2_8888_8888_SRC,
				   uint32, uint32, uint32,
				   COVER, FLAG_NONE)
FAST_BILINEAR_MAINLOOP_COMMON (sse2_8888_8888_pad_SRC,
				   scaled_bilinear_scanline_sse2_8888_8888_SRC,
				   uint32, uint32, uint32,
				   PAD, FLAG_NONE)
FAST_BILINEAR_MAINLOOP_COMMON (sse2_8888_8888_none_SRC,
				   scaled_bilinear_scanline_sse2_8888_8888_SRC,
				   uint32, uint32, uint32,
				   NONE, FLAG_NONE)
FAST_BILINEAR_MAINLOOP_COMMON (sse2_8888_8888_normal_SRC,
				   scaled_bilinear_scanline_sse2_8888_8888_SRC,
				   uint32, uint32, uint32,
				   NORMAL, FLAG_NONE)

static FORCE_INLINE void
scaled_bilinear_scanline_sse2_X888_8888_SRC (uint32 *	   dst,
						 const uint32 * mask,
						 const uint32 * src_top,
						 const uint32 * src_bottom,
						 int32		  w,
						 int			  wt,
						 int			  wb,
						 PIXEL_MANIPULATE_FIXED   vx_,
						 PIXEL_MANIPULATE_FIXED   unit_x_,
						 PIXEL_MANIPULATE_FIXED   max_vx,
						 int	zero_src)
{
	intptr_t vx = vx_;
	intptr_t unit_x = unit_x_;
	BILINEAR_DECLARE_VARIABLES;
	uint32 pix1, pix2;

	while (w && ((uintptr_t)dst & 15))
	{
	BILINEAR_INTERPOLATE_ONE_PIXEL (pix1);
	*dst++ = pix1 | 0xFF000000;
	w--;
	}

	while ((w -= 4) >= 0) {
	__m128i xmm_src;
	BILINEAR_INTERPOLATE_FOUR_PIXELS (xmm_src);
	_mm_store_si128 ((__m128i *)dst, _mm_or_si128 (xmm_src, mask_ff000000));
	dst += 4;
	}

	if (w & 2)
	{
	BILINEAR_INTERPOLATE_ONE_PIXEL (pix1);
	BILINEAR_INTERPOLATE_ONE_PIXEL (pix2);
	*dst++ = pix1 | 0xFF000000;
	*dst++ = pix2 | 0xFF000000;
	}

	if (w & 1)
	{
	BILINEAR_INTERPOLATE_ONE_PIXEL (pix1);
	*dst = pix1 | 0xFF000000;
	}
}

FAST_BILINEAR_MAINLOOP_COMMON (sse2_X888_8888_cover_SRC,
				   scaled_bilinear_scanline_sse2_X888_8888_SRC,
				   uint32, uint32, uint32,
				   COVER, FLAG_NONE)
FAST_BILINEAR_MAINLOOP_COMMON (sse2_X888_8888_pad_SRC,
				   scaled_bilinear_scanline_sse2_X888_8888_SRC,
				   uint32, uint32, uint32,
				   PAD, FLAG_NONE)
FAST_BILINEAR_MAINLOOP_COMMON (sse2_X888_8888_normal_SRC,
				   scaled_bilinear_scanline_sse2_X888_8888_SRC,
				   uint32, uint32, uint32,
				   NORMAL, FLAG_NONE)

static FORCE_INLINE void
scaled_bilinear_scanline_sse2_8888_8888_OVER (uint32 *	   dst,
						  const uint32 * mask,
						  const uint32 * src_top,
						  const uint32 * src_bottom,
						  int32		  w,
						  int			  wt,
						  int			  wb,
						  PIXEL_MANIPULATE_FIXED   vx_,
						  PIXEL_MANIPULATE_FIXED   unit_x_,
						  PIXEL_MANIPULATE_FIXED   max_vx,
						  int	zero_src)
{
	intptr_t vx = vx_;
	intptr_t unit_x = unit_x_;
	BILINEAR_DECLARE_VARIABLES;
	uint32 pix1, pix2;

	while (w && ((uintptr_t)dst & 15))
	{
	BILINEAR_INTERPOLATE_ONE_PIXEL (pix1);

	if (pix1)
	{
		pix2 = *dst;
		*dst = core_combine_over_u_pixel_sse2 (pix1, pix2);
	}

	w--;
	dst++;
	}

	while (w  >= 4)
	{
	__m128i xmm_src;
	__m128i xmm_src_hi, xmm_src_lo, xmm_dst_hi, xmm_dst_lo;
	__m128i xmm_alpha_hi, xmm_alpha_lo;

	BILINEAR_INTERPOLATE_FOUR_PIXELS (xmm_src);

	if (!is_zero (xmm_src))
	{
		if (is_opaque (xmm_src))
		{
		save_128_aligned ((__m128i *)dst, xmm_src);
		}
		else
		{
		__m128i xmm_dst = load_128_aligned ((__m128i *)dst);

		unpack_128_2x128 (xmm_src, &xmm_src_lo, &xmm_src_hi);
		unpack_128_2x128 (xmm_dst, &xmm_dst_lo, &xmm_dst_hi);

		expand_alpha_2x128 (xmm_src_lo, xmm_src_hi, &xmm_alpha_lo, &xmm_alpha_hi);
		over_2x128 (&xmm_src_lo, &xmm_src_hi, &xmm_alpha_lo, &xmm_alpha_hi,
				&xmm_dst_lo, &xmm_dst_hi);

		save_128_aligned ((__m128i *)dst, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));
		}
	}

	w -= 4;
	dst += 4;
	}

	while (w)
	{
	BILINEAR_INTERPOLATE_ONE_PIXEL (pix1);

	if (pix1)
	{
		pix2 = *dst;
		*dst = core_combine_over_u_pixel_sse2 (pix1, pix2);
	}

	w--;
	dst++;
	}
}

FAST_BILINEAR_MAINLOOP_COMMON (sse2_8888_8888_cover_OVER,
				   scaled_bilinear_scanline_sse2_8888_8888_OVER,
				   uint32, uint32, uint32,
				   COVER, FLAG_NONE)
FAST_BILINEAR_MAINLOOP_COMMON (sse2_8888_8888_pad_OVER,
				   scaled_bilinear_scanline_sse2_8888_8888_OVER,
				   uint32, uint32, uint32,
				   PAD, FLAG_NONE)
FAST_BILINEAR_MAINLOOP_COMMON (sse2_8888_8888_none_OVER,
				   scaled_bilinear_scanline_sse2_8888_8888_OVER,
				   uint32, uint32, uint32,
				   NONE, FLAG_NONE)
FAST_BILINEAR_MAINLOOP_COMMON (sse2_8888_8888_normal_OVER,
				   scaled_bilinear_scanline_sse2_8888_8888_OVER,
				   uint32, uint32, uint32,
				   NORMAL, FLAG_NONE)

static FORCE_INLINE void
scaled_bilinear_scanline_sse2_8888_8_8888_OVER (uint32 *	   dst,
						const uint8  * mask,
						const uint32 * src_top,
						const uint32 * src_bottom,
						int32		  w,
						int			  wt,
						int			  wb,
						PIXEL_MANIPULATE_FIXED   vx_,
						PIXEL_MANIPULATE_FIXED   unit_x_,
						PIXEL_MANIPULATE_FIXED   max_vx,
						int	zero_src)
{
	intptr_t vx = vx_;
	intptr_t unit_x = unit_x_;
	BILINEAR_DECLARE_VARIABLES;
	uint32 pix1, pix2;
	uint32 m;

	while (w && ((uintptr_t)dst & 15))
	{
	uint32 sa;

	m = (uint32) *mask++;

	if (m)
	{
		BILINEAR_INTERPOLATE_ONE_PIXEL (pix1);
		sa = pix1 >> 24;

		if (sa == 0xff && m == 0xff)
		{
		*dst = pix1;
		}
		else
		{
		__m128i ms, md, ma, msa;

		pix2 = *dst;
		ma = expand_alpha_rev_1x128 (load_32_1x128 (m));
		ms = unpack_32_1x128 (pix1);
		md = unpack_32_1x128 (pix2);

		msa = expand_alpha_rev_1x128 (load_32_1x128 (sa));

		*dst = pack_1x128_32 (in_over_1x128 (&ms, &msa, &ma, &md));
		}
	}
	else
	{
		BILINEAR_SKIP_ONE_PIXEL ();
	}

	w--;
	dst++;
	}

	while (w >= 4)
	{
	__m128i xmm_src, xmm_src_lo, xmm_src_hi, xmm_srca_lo, xmm_srca_hi;
	__m128i xmm_dst, xmm_dst_lo, xmm_dst_hi;
	__m128i xmm_mask, xmm_mask_lo, xmm_mask_hi;

		memcpy(&m, mask, sizeof(uint32));

	if (m)
	{
		BILINEAR_INTERPOLATE_FOUR_PIXELS (xmm_src);

		if (m == 0xffffffff && is_opaque (xmm_src))
		{
		save_128_aligned ((__m128i *)dst, xmm_src);
		}
		else
		{
		xmm_dst = load_128_aligned ((__m128i *)dst);

		xmm_mask = _mm_unpacklo_epi16 (unpack_32_1x128 (m), _mm_setzero_si128());

		unpack_128_2x128 (xmm_src, &xmm_src_lo, &xmm_src_hi);
		unpack_128_2x128 (xmm_mask, &xmm_mask_lo, &xmm_mask_hi);
		unpack_128_2x128 (xmm_dst, &xmm_dst_lo, &xmm_dst_hi);

		expand_alpha_2x128 (xmm_src_lo, xmm_src_hi, &xmm_srca_lo, &xmm_srca_hi);
		expand_alpha_rev_2x128 (xmm_mask_lo, xmm_mask_hi, &xmm_mask_lo, &xmm_mask_hi);

		in_over_2x128 (&xmm_src_lo, &xmm_src_hi, &xmm_srca_lo, &xmm_srca_hi,
				   &xmm_mask_lo, &xmm_mask_hi, &xmm_dst_lo, &xmm_dst_hi);

		save_128_aligned ((__m128i*)dst, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));
		}
	}
	else
	{
		BILINEAR_SKIP_FOUR_PIXELS ();
	}

	w -= 4;
	dst += 4;
	mask += 4;
	}

	while (w)
	{
	uint32 sa;

	m = (uint32) *mask++;

	if (m)
	{
		BILINEAR_INTERPOLATE_ONE_PIXEL (pix1);
		sa = pix1 >> 24;

		if (sa == 0xff && m == 0xff)
		{
		*dst = pix1;
		}
		else
		{
		__m128i ms, md, ma, msa;

		pix2 = *dst;
		ma = expand_alpha_rev_1x128 (load_32_1x128 (m));
		ms = unpack_32_1x128 (pix1);
		md = unpack_32_1x128 (pix2);

		msa = expand_alpha_rev_1x128 (load_32_1x128 (sa));

		*dst = pack_1x128_32 (in_over_1x128 (&ms, &msa, &ma, &md));
		}
	}
	else
	{
		BILINEAR_SKIP_ONE_PIXEL ();
	}

	w--;
	dst++;
	}
}

FAST_BILINEAR_MAINLOOP_COMMON (sse2_8888_8_8888_cover_OVER,
				   scaled_bilinear_scanline_sse2_8888_8_8888_OVER,
				   uint32, uint8, uint32,
				   COVER, FLAG_HAVE_NON_SOLID_MASK)
FAST_BILINEAR_MAINLOOP_COMMON (sse2_8888_8_8888_pad_OVER,
				   scaled_bilinear_scanline_sse2_8888_8_8888_OVER,
				   uint32, uint8, uint32,
				   PAD, FLAG_HAVE_NON_SOLID_MASK)
FAST_BILINEAR_MAINLOOP_COMMON (sse2_8888_8_8888_none_OVER,
				   scaled_bilinear_scanline_sse2_8888_8_8888_OVER,
				   uint32, uint8, uint32,
				   NONE, FLAG_HAVE_NON_SOLID_MASK)
FAST_BILINEAR_MAINLOOP_COMMON (sse2_8888_8_8888_normal_OVER,
				   scaled_bilinear_scanline_sse2_8888_8_8888_OVER,
				   uint32, uint8, uint32,
				   NORMAL, FLAG_HAVE_NON_SOLID_MASK)

static FORCE_INLINE void
scaled_bilinear_scanline_sse2_8888_n_8888_OVER (uint32 *	   dst,
						const uint32 * mask,
						const uint32 * src_top,
						const uint32 * src_bottom,
						int32		  w,
						int			  wt,
						int			  wb,
						PIXEL_MANIPULATE_FIXED   vx_,
						PIXEL_MANIPULATE_FIXED   unit_x_,
						PIXEL_MANIPULATE_FIXED   max_vx,
						int	zero_src)
{
	intptr_t vx = vx_;
	intptr_t unit_x = unit_x_;
	BILINEAR_DECLARE_VARIABLES;
	uint32 pix1;
	__m128i xmm_mask;

	if (zero_src || (*mask >> 24) == 0)
	return;

	xmm_mask = create_mask_16_128 (*mask >> 24);

	while (w && ((uintptr_t)dst & 15))
	{
	BILINEAR_INTERPOLATE_ONE_PIXEL (pix1);
	if (pix1)
	{
		uint32 d = *dst;

		__m128i ms = unpack_32_1x128 (pix1);
		__m128i alpha	 = expand_alpha_1x128 (ms);
		__m128i dest	  = xmm_mask;
		__m128i alpha_dst = unpack_32_1x128 (d);

		*dst = pack_1x128_32
			(in_over_1x128 (&ms, &alpha, &dest, &alpha_dst));
	}

	dst++;
	w--;
	}

	while (w >= 4)
	{
	__m128i xmm_src;
	BILINEAR_INTERPOLATE_FOUR_PIXELS (xmm_src);

	if (!is_zero (xmm_src))
	{
		__m128i xmm_src_lo, xmm_src_hi;
		__m128i xmm_dst, xmm_dst_lo, xmm_dst_hi;
		__m128i xmm_alpha_lo, xmm_alpha_hi;

		xmm_dst = load_128_aligned ((__m128i*)dst);

		unpack_128_2x128 (xmm_src, &xmm_src_lo, &xmm_src_hi);
		unpack_128_2x128 (xmm_dst, &xmm_dst_lo, &xmm_dst_hi);
		expand_alpha_2x128 (xmm_src_lo, xmm_src_hi,
				&xmm_alpha_lo, &xmm_alpha_hi);

		in_over_2x128 (&xmm_src_lo, &xmm_src_hi,
			   &xmm_alpha_lo, &xmm_alpha_hi,
			   &xmm_mask, &xmm_mask,
			   &xmm_dst_lo, &xmm_dst_hi);

		save_128_aligned
		((__m128i*)dst, pack_2x128_128 (xmm_dst_lo, xmm_dst_hi));
	}

	dst += 4;
	w -= 4;
	}

	while (w)
	{
	BILINEAR_INTERPOLATE_ONE_PIXEL (pix1);
	if (pix1)
	{
		uint32 d = *dst;

		__m128i ms = unpack_32_1x128 (pix1);
		__m128i alpha	 = expand_alpha_1x128 (ms);
		__m128i dest	  = xmm_mask;
		__m128i alpha_dst = unpack_32_1x128 (d);

		*dst = pack_1x128_32
			(in_over_1x128 (&ms, &alpha, &dest, &alpha_dst));
	}

	dst++;
	w--;
	}
}

FAST_BILINEAR_MAINLOOP_COMMON (sse2_8888_n_8888_cover_OVER,
				   scaled_bilinear_scanline_sse2_8888_n_8888_OVER,
				   uint32, uint32, uint32,
				   COVER, FLAG_HAVE_SOLID_MASK)
FAST_BILINEAR_MAINLOOP_COMMON (sse2_8888_n_8888_pad_OVER,
				   scaled_bilinear_scanline_sse2_8888_n_8888_OVER,
				   uint32, uint32, uint32,
				   PAD, FLAG_HAVE_SOLID_MASK)
FAST_BILINEAR_MAINLOOP_COMMON (sse2_8888_n_8888_none_OVER,
				   scaled_bilinear_scanline_sse2_8888_n_8888_OVER,
				   uint32, uint32, uint32,
				   NONE, FLAG_HAVE_SOLID_MASK)
FAST_BILINEAR_MAINLOOP_COMMON (sse2_8888_n_8888_normal_OVER,
				   scaled_bilinear_scanline_sse2_8888_n_8888_OVER,
				   uint32, uint32, uint32,
				   NORMAL, FLAG_HAVE_SOLID_MASK)

static const PIXEL_MANIPULATE_FAST_PATH sse2_fast_paths[] =
{
	/* PIXEL_MANIPULATE_OPERATE_OVER */
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, SOLID, FORMAT_A8, FORMAT_R5G6B5, sse2_composite_over_n_8_0565),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, SOLID, FORMAT_A8, FORMAT_B5G6R5, sse2_composite_over_n_8_0565),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, SOLID, null, FORMAT_A8R8G8B8, sse2_composite_over_n_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, SOLID, null, FORMAT_X8R8G8B8, sse2_composite_over_n_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, SOLID, null, FORMAT_R5G6B5, sse2_composite_over_n_0565),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, SOLID, null, FORMAT_B5G6R5, sse2_composite_over_n_0565),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_A8R8G8B8, null, FORMAT_A8R8G8B8, sse2_composite_over_8888_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_A8R8G8B8, null, FORMAT_X8R8G8B8, sse2_composite_over_8888_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_A8B8G8R8, null, FORMAT_A8B8G8R8, sse2_composite_over_8888_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_A8B8G8R8, null, FORMAT_X8B8G8R8, sse2_composite_over_8888_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_A8R8G8B8, null, FORMAT_R5G6B5, sse2_composite_over_8888_0565),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_A8B8G8R8, null, FORMAT_B5G6R5, sse2_composite_over_8888_0565),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, SOLID, FORMAT_A8, FORMAT_A8R8G8B8, sse2_composite_over_n_8_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, SOLID, FORMAT_A8, FORMAT_X8R8G8B8, sse2_composite_over_n_8_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, SOLID, FORMAT_A8, FORMAT_A8B8G8R8, sse2_composite_over_n_8_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, SOLID, FORMAT_A8, FORMAT_X8B8G8R8, sse2_composite_over_n_8_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_A8R8G8B8, FORMAT_A8R8G8B8, FORMAT_A8R8G8B8, sse2_composite_over_8888_8888_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_A8R8G8B8, FORMAT_A8, FORMAT_X8R8G8B8, sse2_composite_over_8888_8_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_A8R8G8B8, FORMAT_A8, FORMAT_A8R8G8B8, sse2_composite_over_8888_8_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_A8B8G8R8, FORMAT_A8, FORMAT_X8B8G8R8, sse2_composite_over_8888_8_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_A8B8G8R8, FORMAT_A8, FORMAT_A8B8G8R8, sse2_composite_over_8888_8_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_X8R8G8B8, FORMAT_A8, FORMAT_X8R8G8B8, sse2_composite_over_X888_8_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_X8R8G8B8, FORMAT_A8, FORMAT_A8R8G8B8, sse2_composite_over_X888_8_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_X8B8G8R8, FORMAT_A8, FORMAT_X8B8G8R8, sse2_composite_over_X888_8_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_X8B8G8R8, FORMAT_A8, FORMAT_A8B8G8R8, sse2_composite_over_X888_8_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_X8R8G8B8, SOLID, FORMAT_A8R8G8B8, sse2_composite_over_X888_n_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_X8R8G8B8, SOLID, FORMAT_X8R8G8B8, sse2_composite_over_X888_n_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_X8B8G8R8, SOLID, FORMAT_A8B8G8R8, sse2_composite_over_X888_n_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_X8B8G8R8, SOLID, FORMAT_X8B8G8R8, sse2_composite_over_X888_n_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_A8R8G8B8, SOLID, FORMAT_A8R8G8B8, sse2_composite_over_8888_n_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_A8R8G8B8, SOLID, FORMAT_X8R8G8B8, sse2_composite_over_8888_n_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_A8B8G8R8, SOLID, FORMAT_A8B8G8R8, sse2_composite_over_8888_n_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_A8B8G8R8, SOLID, FORMAT_X8B8G8R8, sse2_composite_over_8888_n_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH_CA (OVER, SOLID, FORMAT_A8R8G8B8, FORMAT_A8R8G8B8, sse2_composite_over_n_8888_8888_ca),
	PIXEL_MANIPULATE_STD_FAST_PATH_CA (OVER, SOLID, FORMAT_A8R8G8B8, FORMAT_X8R8G8B8, sse2_composite_over_n_8888_8888_ca),
	PIXEL_MANIPULATE_STD_FAST_PATH_CA (OVER, SOLID, FORMAT_A8B8G8R8, FORMAT_A8B8G8R8, sse2_composite_over_n_8888_8888_ca),
	PIXEL_MANIPULATE_STD_FAST_PATH_CA (OVER, SOLID, FORMAT_A8B8G8R8, FORMAT_X8B8G8R8, sse2_composite_over_n_8888_8888_ca),
	PIXEL_MANIPULATE_STD_FAST_PATH_CA (OVER, SOLID, FORMAT_A8R8G8B8, FORMAT_R5G6B5, sse2_composite_over_n_8888_0565_ca),
	PIXEL_MANIPULATE_STD_FAST_PATH_CA (OVER, SOLID, FORMAT_A8B8G8R8, FORMAT_B5G6R5, sse2_composite_over_n_8888_0565_ca),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, PIXBUF, PIXBUF, FORMAT_A8R8G8B8, sse2_composite_over_pixbuf_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, PIXBUF, PIXBUF, FORMAT_X8R8G8B8, sse2_composite_over_pixbuf_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, RPIXBUF, RPIXBUF, FORMAT_A8B8G8R8, sse2_composite_over_pixbuf_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, RPIXBUF, RPIXBUF, FORMAT_X8B8G8R8, sse2_composite_over_pixbuf_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, PIXBUF, PIXBUF, FORMAT_R5G6B5, sse2_composite_over_pixbuf_0565),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, RPIXBUF, RPIXBUF, FORMAT_B5G6R5, sse2_composite_over_pixbuf_0565),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_X8R8G8B8, null, FORMAT_X8R8G8B8, sse2_composite_copy_area),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER, FORMAT_X8B8G8R8, null, FORMAT_X8B8G8R8, sse2_composite_copy_area),

	/* PIXEL_MANIPULATE_OPERATE_OVER_REVERSE */
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER_REVERSE, SOLID, null, FORMAT_A8R8G8B8, sse2_composite_over_reverse_n_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (OVER_REVERSE, SOLID, null, FORMAT_A8B8G8R8, sse2_composite_over_reverse_n_8888),

	/* PIXEL_MANIPULATE_OPERATE_ADD */
	PIXEL_MANIPULATE_STD_FAST_PATH_CA (ADD, SOLID, FORMAT_A8R8G8B8, FORMAT_A8R8G8B8, sse2_composite_add_n_8888_8888_ca),
	PIXEL_MANIPULATE_STD_FAST_PATH (ADD, FORMAT_A8, null, FORMAT_A8, sse2_composite_add_8_8),
	PIXEL_MANIPULATE_STD_FAST_PATH (ADD, FORMAT_A8R8G8B8, null, FORMAT_A8R8G8B8, sse2_composite_add_8888_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (ADD, FORMAT_A8B8G8R8, null, FORMAT_A8B8G8R8, sse2_composite_add_8888_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (ADD, SOLID, FORMAT_A8, FORMAT_A8, sse2_composite_add_n_8_8),
	PIXEL_MANIPULATE_STD_FAST_PATH (ADD, SOLID, null, FORMAT_A8, sse2_composite_add_n_8),
	PIXEL_MANIPULATE_STD_FAST_PATH (ADD, SOLID, null, FORMAT_X8R8G8B8, sse2_composite_add_n_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (ADD, SOLID, null, FORMAT_A8R8G8B8, sse2_composite_add_n_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (ADD, SOLID, null, FORMAT_X8B8G8R8, sse2_composite_add_n_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (ADD, SOLID, null, FORMAT_A8B8G8R8, sse2_composite_add_n_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (ADD, SOLID, FORMAT_A8, FORMAT_X8R8G8B8, sse2_composite_add_n_8_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (ADD, SOLID, FORMAT_A8, FORMAT_A8R8G8B8, sse2_composite_add_n_8_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (ADD, SOLID, FORMAT_A8, FORMAT_X8B8G8R8, sse2_composite_add_n_8_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (ADD, SOLID, FORMAT_A8, FORMAT_A8B8G8R8, sse2_composite_add_n_8_8888),

	/* PIXEL_MANIPULATE_OPERATE_SRC */
	PIXEL_MANIPULATE_STD_FAST_PATH (SRC, SOLID, FORMAT_A8, FORMAT_A8R8G8B8, sse2_composite_src_n_8_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (SRC, SOLID, FORMAT_A8, FORMAT_X8R8G8B8, sse2_composite_src_n_8_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (SRC, SOLID, FORMAT_A8, FORMAT_A8B8G8R8, sse2_composite_src_n_8_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (SRC, SOLID, FORMAT_A8, FORMAT_X8B8G8R8, sse2_composite_src_n_8_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (SRC, FORMAT_A8R8G8B8, null, FORMAT_R5G6B5, sse2_composite_src_X888_0565),
	PIXEL_MANIPULATE_STD_FAST_PATH (SRC, FORMAT_A8B8G8R8, null, FORMAT_B5G6R5, sse2_composite_src_X888_0565),
	PIXEL_MANIPULATE_STD_FAST_PATH (SRC, FORMAT_X8R8G8B8, null, FORMAT_R5G6B5, sse2_composite_src_X888_0565),
	PIXEL_MANIPULATE_STD_FAST_PATH (SRC, FORMAT_X8B8G8R8, null, FORMAT_B5G6R5, sse2_composite_src_X888_0565),
	PIXEL_MANIPULATE_STD_FAST_PATH (SRC, FORMAT_X8R8G8B8, null, FORMAT_A8R8G8B8, sse2_composite_src_X888_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (SRC, FORMAT_X8B8G8R8, null, FORMAT_A8B8G8R8, sse2_composite_src_X888_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH (SRC, FORMAT_A8R8G8B8, null, FORMAT_A8R8G8B8, sse2_composite_copy_area),
	PIXEL_MANIPULATE_STD_FAST_PATH (SRC, FORMAT_A8B8G8R8, null, FORMAT_A8B8G8R8, sse2_composite_copy_area),
	PIXEL_MANIPULATE_STD_FAST_PATH (SRC, FORMAT_A8R8G8B8, null, FORMAT_X8R8G8B8, sse2_composite_copy_area),
	PIXEL_MANIPULATE_STD_FAST_PATH (SRC, FORMAT_A8B8G8R8, null, FORMAT_X8B8G8R8, sse2_composite_copy_area),
	PIXEL_MANIPULATE_STD_FAST_PATH (SRC, FORMAT_X8R8G8B8, null, FORMAT_X8R8G8B8, sse2_composite_copy_area),
	PIXEL_MANIPULATE_STD_FAST_PATH (SRC, FORMAT_X8B8G8R8, null, FORMAT_X8B8G8R8, sse2_composite_copy_area),
	PIXEL_MANIPULATE_STD_FAST_PATH (SRC, FORMAT_R5G6B5, null, FORMAT_R5G6B5, sse2_composite_copy_area),
	PIXEL_MANIPULATE_STD_FAST_PATH (SRC, FORMAT_B5G6R5, null, FORMAT_B5G6R5, sse2_composite_copy_area),

	/* PIXEL_MANIPULATE_OPERATE_IN */
	PIXEL_MANIPULATE_STD_FAST_PATH (IN, FORMAT_A8, null, FORMAT_A8, sse2_composite_in_8_8),
	PIXEL_MANIPULATE_STD_FAST_PATH (IN, SOLID, FORMAT_A8, FORMAT_A8, sse2_composite_in_n_8_8),
	PIXEL_MANIPULATE_STD_FAST_PATH (IN, SOLID, null, FORMAT_A8, sse2_composite_in_n_8),

	SIMPLE_NEAREST_FAST_PATH (OVER, FORMAT_A8R8G8B8, FORMAT_X8R8G8B8, sse2_8888_8888),
	SIMPLE_NEAREST_FAST_PATH (OVER, FORMAT_A8B8G8R8, FORMAT_X8B8G8R8, sse2_8888_8888),
	SIMPLE_NEAREST_FAST_PATH (OVER, FORMAT_A8R8G8B8, FORMAT_A8R8G8B8, sse2_8888_8888),
	SIMPLE_NEAREST_FAST_PATH (OVER, FORMAT_A8B8G8R8, FORMAT_A8B8G8R8, sse2_8888_8888),

	SIMPLE_NEAREST_SOLID_MASK_FAST_PATH (OVER, FORMAT_A8R8G8B8, FORMAT_A8R8G8B8, sse2_8888_n_8888),
	SIMPLE_NEAREST_SOLID_MASK_FAST_PATH (OVER, FORMAT_A8B8G8R8, FORMAT_A8B8G8R8, sse2_8888_n_8888),
	SIMPLE_NEAREST_SOLID_MASK_FAST_PATH (OVER, FORMAT_A8R8G8B8, FORMAT_X8R8G8B8, sse2_8888_n_8888),
	SIMPLE_NEAREST_SOLID_MASK_FAST_PATH (OVER, FORMAT_A8B8G8R8, FORMAT_X8B8G8R8, sse2_8888_n_8888),

	SIMPLE_BILINEAR_FAST_PATH (SRC, FORMAT_A8R8G8B8, FORMAT_A8R8G8B8, sse2_8888_8888),
	SIMPLE_BILINEAR_FAST_PATH (SRC, FORMAT_A8R8G8B8, FORMAT_X8R8G8B8, sse2_8888_8888),
	SIMPLE_BILINEAR_FAST_PATH (SRC, FORMAT_X8R8G8B8, FORMAT_X8R8G8B8, sse2_8888_8888),
	SIMPLE_BILINEAR_FAST_PATH (SRC, FORMAT_A8B8G8R8, FORMAT_A8B8G8R8, sse2_8888_8888),
	SIMPLE_BILINEAR_FAST_PATH (SRC, FORMAT_A8B8G8R8, FORMAT_X8B8G8R8, sse2_8888_8888),
	SIMPLE_BILINEAR_FAST_PATH (SRC, FORMAT_X8B8G8R8, FORMAT_X8B8G8R8, sse2_8888_8888),

	SIMPLE_BILINEAR_FAST_PATH_COVER  (SRC, FORMAT_X8R8G8B8, FORMAT_A8R8G8B8, sse2_X888_8888),
	SIMPLE_BILINEAR_FAST_PATH_COVER  (SRC, FORMAT_X8B8G8R8, FORMAT_A8B8G8R8, sse2_X888_8888),
	SIMPLE_BILINEAR_FAST_PATH_PAD	(SRC, FORMAT_X8R8G8B8, FORMAT_A8R8G8B8, sse2_X888_8888),
	SIMPLE_BILINEAR_FAST_PATH_PAD	(SRC, FORMAT_X8B8G8R8, FORMAT_A8B8G8R8, sse2_X888_8888),
	SIMPLE_BILINEAR_FAST_PATH_NORMAL (SRC, FORMAT_X8R8G8B8, FORMAT_A8R8G8B8, sse2_X888_8888),
	SIMPLE_BILINEAR_FAST_PATH_NORMAL (SRC, FORMAT_X8B8G8R8, FORMAT_A8B8G8R8, sse2_X888_8888),

	SIMPLE_BILINEAR_FAST_PATH (OVER, FORMAT_A8R8G8B8, FORMAT_X8R8G8B8, sse2_8888_8888),
	SIMPLE_BILINEAR_FAST_PATH (OVER, FORMAT_A8B8G8R8, FORMAT_X8B8G8R8, sse2_8888_8888),
	SIMPLE_BILINEAR_FAST_PATH (OVER, FORMAT_A8R8G8B8, FORMAT_A8R8G8B8, sse2_8888_8888),
	SIMPLE_BILINEAR_FAST_PATH (OVER, FORMAT_A8B8G8R8, FORMAT_A8B8G8R8, sse2_8888_8888),

	SIMPLE_BILINEAR_SOLID_MASK_FAST_PATH (OVER, FORMAT_A8R8G8B8, FORMAT_X8R8G8B8, sse2_8888_n_8888),
	SIMPLE_BILINEAR_SOLID_MASK_FAST_PATH (OVER, FORMAT_A8B8G8R8, FORMAT_X8B8G8R8, sse2_8888_n_8888),
	SIMPLE_BILINEAR_SOLID_MASK_FAST_PATH (OVER, FORMAT_A8R8G8B8, FORMAT_A8R8G8B8, sse2_8888_n_8888),
	SIMPLE_BILINEAR_SOLID_MASK_FAST_PATH (OVER, FORMAT_A8B8G8R8, FORMAT_A8B8G8R8, sse2_8888_n_8888),

	SIMPLE_BILINEAR_A8_MASK_FAST_PATH (OVER, FORMAT_A8R8G8B8, FORMAT_X8R8G8B8, sse2_8888_8_8888),
	SIMPLE_BILINEAR_A8_MASK_FAST_PATH (OVER, FORMAT_A8B8G8R8, FORMAT_X8B8G8R8, sse2_8888_8_8888),
	SIMPLE_BILINEAR_A8_MASK_FAST_PATH (OVER, FORMAT_A8R8G8B8, FORMAT_A8R8G8B8, sse2_8888_8_8888),
	SIMPLE_BILINEAR_A8_MASK_FAST_PATH (OVER, FORMAT_A8B8G8R8, FORMAT_A8B8G8R8, sse2_8888_8_8888),

	{ PIXEL_MANIPULATE_OPERATE_NONE },
};

static uint32 *
sse2_fetch_X8R8G8B8 (PIXEL_MANIPULATE_ITERATION *iter, const uint32 *mask)
{
	int w = iter->width;
	__m128i ff000000 = mask_ff000000;
	uint32 *dst = iter->buffer;
	uint32 *src = (uint32 *)iter->bits;

	iter->bits += iter->stride;

	while (w && ((uintptr_t)dst) & 0x0f)
	{
	*dst++ = (*src++) | 0xff000000;
	w--;
	}

	while (w >= 4)
	{
	save_128_aligned (
		(__m128i *)dst, _mm_or_si128 (
		load_128_unaligned ((__m128i *)src), ff000000));

	dst += 4;
	src += 4;
	w -= 4;
	}

	while (w)
	{
	*dst++ = (*src++) | 0xff000000;
	w--;
	}

	return iter->buffer;
}

static uint32 *
sse2_fetch_r5g6b5 (PIXEL_MANIPULATE_ITERATION *iter, const uint32 *mask)
{
	int w = iter->width;
	uint32 *dst = iter->buffer;
	uint16 *src = (uint16 *)iter->bits;
	__m128i ff000000 = mask_ff000000;

	iter->bits += iter->stride;

	while (w && ((uintptr_t)dst) & 0x0f)
	{
	uint16 s = *src++;

	*dst++ = convert_0565_to_8888 (s);
	w--;
	}

	while (w >= 8)
	{
	__m128i lo, hi, s;

	s = _mm_loadu_si128 ((__m128i *)src);

	lo = unpack_565_to_8888 (_mm_unpacklo_epi16 (s, _mm_setzero_si128 ()));
	hi = unpack_565_to_8888 (_mm_unpackhi_epi16 (s, _mm_setzero_si128 ()));

	save_128_aligned ((__m128i *)(dst + 0), _mm_or_si128 (lo, ff000000));
	save_128_aligned ((__m128i *)(dst + 4), _mm_or_si128 (hi, ff000000));

	dst += 8;
	src += 8;
	w -= 8;
	}

	while (w)
	{
	uint16 s = *src++;

	*dst++ = convert_0565_to_8888 (s);
	w--;
	}

	return iter->buffer;
}

static uint32 *
sse2_fetch_A8 (PIXEL_MANIPULATE_ITERATION *iter, const uint32 *mask)
{
	int w = iter->width;
	uint32 *dst = iter->buffer;
	uint8 *src = iter->bits;
	__m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6;

	iter->bits += iter->stride;

	while (w && (((uintptr_t)dst) & 15))
	{
		*dst++ = (uint32)(*(src++)) << 24;
		w--;
	}

	while (w >= 16)
	{
	xmm0 = _mm_loadu_si128((__m128i *)src);

	xmm1 = _mm_unpacklo_epi8  (_mm_setzero_si128(), xmm0);
	xmm2 = _mm_unpackhi_epi8  (_mm_setzero_si128(), xmm0);
	xmm3 = _mm_unpacklo_epi16 (_mm_setzero_si128(), xmm1);
	xmm4 = _mm_unpackhi_epi16 (_mm_setzero_si128(), xmm1);
	xmm5 = _mm_unpacklo_epi16 (_mm_setzero_si128(), xmm2);
	xmm6 = _mm_unpackhi_epi16 (_mm_setzero_si128(), xmm2);

	_mm_store_si128(((__m128i *)(dst +  0)), xmm3);
	_mm_store_si128(((__m128i *)(dst +  4)), xmm4);
	_mm_store_si128(((__m128i *)(dst +  8)), xmm5);
	_mm_store_si128(((__m128i *)(dst + 12)), xmm6);

	dst += 16;
	src += 16;
	w -= 16;
	}

	while (w)
	{
	*dst++ = (uint32)(*(src++)) << 24;
	w--;
	}

	return iter->buffer;
}

#define IMAGE_FLAGS							\
	(FAST_PATH_STANDARD_FLAGS | FAST_PATH_ID_TRANSFORM |		\
	 FAST_PATH_BITS_IMAGE | FAST_PATH_SAMPLES_COVER_CLIP_NEAREST)

static const PIXEL_MANIPULATE_ITERATION_INFO sse2_iters[] =
{
	{ PIXEL_MANIPULATE_FORMAT_X8R8G8B8, IMAGE_FLAGS, PIXEL_MANIPULATE_ITERATION_NARROW,
	  PixelManipulateIteraterationInitializeBitsStride, sse2_fetch_X8R8G8B8, NULL
	},
	{ PIXEL_MANIPULATE_FORMAT_R5G6B5, IMAGE_FLAGS, PIXEL_MANIPULATE_ITERATION_NARROW,
	  PixelManipulateIteraterationInitializeBitsStride, sse2_fetch_r5g6b5, NULL
	},
	{ PIXEL_MANIPULATE_FORMAT_A8, IMAGE_FLAGS, PIXEL_MANIPULATE_ITERATION_NARROW,
	  PixelManipulateIteraterationInitializeBitsStride, sse2_fetch_A8, NULL
	},
	{ 0},
};

#if defined(__GNUC__) && !defined(__X86_64__) && !defined(__amd64__)
__attribute__((__force_align_arg_pointer__))
#endif
PIXEL_MANIPULATE_IMPLEMENTATION *
	PixelManipulateImplementationCreateSSE2 (PIXEL_MANIPULATE_IMPLEMENTATION *fallback)
{
	PIXEL_MANIPULATE_IMPLEMENTATION *imp = PixelManipulateImplementationCreate(fallback, sse2_fast_paths);

	/* SSE2 constants */
	mask_565_r  = create_mask_2x32_128 (0x00f80000, 0x00f80000);
	mask_565_g1 = create_mask_2x32_128 (0x00070000, 0x00070000);
	mask_565_g2 = create_mask_2x32_128 (0x000000e0, 0x000000e0);
	mask_565_b  = create_mask_2x32_128 (0x0000001f, 0x0000001f);
	mask_red   = create_mask_2x32_128 (0x00f80000, 0x00f80000);
	mask_green = create_mask_2x32_128 (0x0000fc00, 0x0000fc00);
	mask_blue  = create_mask_2x32_128 (0x000000f8, 0x000000f8);
	mask_565_fix_rb = create_mask_2x32_128 (0x00e000e0, 0x00e000e0);
	mask_565_fix_g = create_mask_2x32_128  (0x0000c000, 0x0000c000);
	mask_0080 = create_mask_16_128 (0x0080);
	mask_00ff = create_mask_16_128 (0x00ff);
	mask_0101 = create_mask_16_128 (0x0101);
	mask_ffff = create_mask_16_128 (0xffff);
	mask_ff000000 = create_mask_2x32_128 (0xff000000, 0xff000000);
	mask_alpha = create_mask_2x32_128 (0x00ff0000, 0x00000000);
	mask_565_rb = create_mask_2x32_128 (0x00f800f8, 0x00f800f8);
	mask_565_pack_multiplier = create_mask_2x32_128 (0x20000004, 0x20000004);

	/* Set up function pointers */
	imp->combine32[PIXEL_MANIPULATE_OPERATE_OVER] = sse2_combine_over_u;
	imp->combine32[PIXEL_MANIPULATE_OPERATE_OVER_REVERSE] = sse2_combine_over_reverse_u;
	imp->combine32[PIXEL_MANIPULATE_OPERATE_IN] = sse2_combine_in_u;
	imp->combine32[PIXEL_MANIPULATE_OPERATE_IN_REVERSE] = sse2_combine_in_reverse_u;
	imp->combine32[PIXEL_MANIPULATE_OPERATE_OUT] = sse2_combine_out_u;
	imp->combine32[PIXEL_MANIPULATE_OPERATE_OUT_REVERSE] = sse2_combine_out_reverse_u;
	imp->combine32[PIXEL_MANIPULATE_OPERATE_ATOP] = sse2_combine_atop_u;
	imp->combine32[PIXEL_MANIPULATE_OPERATE_ATOP_REVERSE] = sse2_combine_atop_reverse_u;
	imp->combine32[PIXEL_MANIPULATE_OPERATE_XOR] = sse2_combine_xor_u;
	imp->combine32[PIXEL_MANIPULATE_OPERATE_ADD] = sse2_combine_add_u;

	imp->combine32[PIXEL_MANIPULATE_OPERATE_SATURATE] = sse2_combine_saturate_u;

	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_SRC] = sse2_combine_src_ca;
	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_OVER] = sse2_combine_over_ca;
	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_OVER_REVERSE] = sse2_combine_over_reverse_ca;
	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_IN] = sse2_combine_in_ca;
	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_IN_REVERSE] = sse2_combine_in_reverse_ca;
	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_OUT] = sse2_combine_out_ca;
	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_OUT_REVERSE] = sse2_combine_out_reverse_ca;
	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_ATOP] = sse2_combine_atop_ca;
	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_ATOP_REVERSE] = sse2_combine_atop_reverse_ca;
	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_XOR] = sse2_combine_xor_ca;
	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_ADD] = sse2_combine_add_ca;

	imp->block_transfer = sse2_blt;
	imp->fill = sse2_fill;

	imp->iteration_info = sse2_iters;

	return imp;
}

#ifdef __cplusplus
}
#endif

#endif // USE_X86_MMX || USE_ARM_IWMMXT || USE_LOONGSON_MMI
