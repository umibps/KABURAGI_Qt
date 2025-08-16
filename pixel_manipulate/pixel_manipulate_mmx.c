#include <string.h>
#include "pixel_manipulate_composite.h"
#include "pixel_manipulate.h"
#include "pixel_manipulate_inline.h"
#include "pixel_manipulate_private.h"

#if (defined(USE_X86_MMX) && USE_X86_MMX) || defined USE_ARM_IWMMXT || defined USE_LOONGSON_MMI

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_LOONGSON_MMI
#include <loongson-mmintrin.h>
#else
#include <mmintrin.h>
#endif

#ifdef VERBOSE
#define CHECKPOINT() error_f ("at %s %d\n", __FUNCTION__, __LINE__)
#else
#define CHECKPOINT()
#endif

#if defined USE_ARM_IWMMXT && __GNUC__ == 4 && __GNUC_MINOR__ < 8
/* Empty the multimedia state. For some reason, ARM's mmintrin.h doesn't provide this.  */
extern __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
_mm_empty (void)
{

}
#endif

#ifdef USE_X86_MMX
# if (defined(__SUNPRO_C) || defined(_MSC_VER) || defined(_WIN64))
#  include <xmmintrin.h>
# else
/* We have to compile with -msse to use xmmintrin.h, but that causes SSE
 * instructions to be generated that we don't want. Just duplicate the
 * functions we want to use.  */
extern __inline int __attribute__((__gnu_inline__, __always_inline__, __artificial__))
_mm_movemask_pi8 (__m64 __A)
{
	int ret;

	asm ("pmovmskb %1, %0\n\t"
	: "=r" (ret)
	: "y" (__A)
	);

	return ret;
}

extern __inline __m64 __attribute__((__gnu_inline__, __always_inline__, __artificial__))
_mm_mulhi_pu16 (__m64 __A, __m64 __B)
{
	asm ("pmulhuw %1, %0\n\t"
	: "+y" (__A)
	: "y" (__B)
	);
	return __A;
}

# define _mm_shuffle_pi16(A, N)						\
	({									\
	__m64 ret;							\
									\
	asm ("pshufw %2, %1, %0\n\t"					\
		 : "=y" (ret)						\
		 : "y" (A), "K" ((const int8)N)				\
	);								\
									\
	ret;								\
	})
# endif
#endif

#ifndef _MSC_VER
#define _MM_SHUFFLE(fp3,fp2,fp1,fp0) \
 (((fp3) << 6) | ((fp2) << 4) | ((fp1) << 2) | (fp0))
#endif

/* Notes about writing mmx code
 *
 * give memory operands as the second operand. If you give it as the
 * first, gcc will first load it into a register, then use that
 * register
 *
 *   ie. use
 *
 *		 _mm_mullo_pi16 (x, mmx_constant);
 *
 *   not
 *
 *		 _mm_mullo_pi16 (mmx_constant, x);
 *
 * Also try to minimize dependencies. i.e. when you need a value, try
 * to calculate it from a value that was calculated as early as
 * possible.
 */

/* --------------- MMX primitives ------------------------------------- */

/* If __m64 is defined as a struct or union, then define M64_MEMBER to be
 * the name of the member used to access the data.
 * If __m64 requires using mm_cvt* intrinsics functions to convert between
 * uint64 and __m64 values, then define USE_CVT_INTRINSICS.
 * If __m64 and uint64 values can just be cast to each other directly,
 * then define USE_M64_CASTS.
 * If __m64 is a double datatype, then define USE_M64_DOUBLE.
 */
#ifdef _MSC_VER
# define M64_MEMBER m64_u64
#elif defined(__ICC)
# define USE_CVT_INTRINSICS
#elif defined(USE_LOONGSON_MMI)
# define USE_M64_DOUBLE
#elif defined(__GNUC__)
# define USE_M64_CASTS
#elif defined(__SUNPRO_C)
# if (__SUNPRO_C >= 0x5120) && !defined(__NOVECTORSIZE__)
/* Solaris Studio 12.3 (Sun C 5.12) introduces __attribute__(__vector_size__)
 * support, and defaults to using it to define __m64, unless __NOVECTORSIZE__
 * is defined.   If it is used, then the mm_cvt* intrinsics must be used.
 */
#  define USE_CVT_INTRINSICS
# else
/* For Studio 12.2 or older, or when __attribute__(__vector_size__) is
 * disabled, __m64 is defined as a struct containing "unsigned long long l_".
 */
#  define M64_MEMBER l_
# endif
#endif

#if defined(USE_M64_CASTS) || defined(USE_CVT_INTRINSICS) || defined(USE_M64_DOUBLE)
typedef uint64 mmxdatafield;
#else
typedef __m64 mmxdatafield;
#endif

typedef struct
{
	mmxdatafield mmx_4x00ff;
	mmxdatafield mmx_4x0080;
	mmxdatafield mmx_565_rgb;
	mmxdatafield mmx_565_unpack_multiplier;
	mmxdatafield mmx_565_pack_multiplier;
	mmxdatafield mmx_565_r;
	mmxdatafield mmx_565_g;
	mmxdatafield mmx_565_b;
	mmxdatafield mmx_packed_565_rb;
	mmxdatafield mmx_packed_565_g;
	mmxdatafield mmx_expand_565_g;
	mmxdatafield mmx_expand_565_b;
	mmxdatafield mmx_expand_565_r;
#ifndef USE_LOONGSON_MMI
	mmxdatafield mmx_mask_0;
	mmxdatafield mmx_mask_1;
	mmxdatafield mmx_mask_2;
	mmxdatafield mmx_mask_3;
#endif
	mmxdatafield mmx_full_alpha;
	mmxdatafield mmx_4x0101;
	mmxdatafield mmx_ff000000;
} mmx_data_t;

#if defined(_MSC_VER)
# define MMXDATA_INIT(field, val) { val ## UI64 }
#elif defined(M64_MEMBER)	   /* __m64 is a struct, not an integral type */
# define MMXDATA_INIT(field, val) field =   { val ## ULL }
#else						   /* mmxdatafield is an integral type */
# define MMXDATA_INIT(field, val) field =   val ## ULL
#endif

static const mmx_data_t c =
{
	MMXDATA_INIT (.mmx_4x00ff,				   0x00ff00ff00ff00ff),
	MMXDATA_INIT (.mmx_4x0080,				   0x0080008000800080),
	MMXDATA_INIT (.mmx_565_rgb,				  0x000001f0003f001f),
	MMXDATA_INIT (.mmx_565_unpack_multiplier,	0x0000008404100840),
	MMXDATA_INIT (.mmx_565_pack_multiplier,	  0x2000000420000004),
	MMXDATA_INIT (.mmx_565_r,					0x000000f800000000),
	MMXDATA_INIT (.mmx_565_g,					0x0000000000fc0000),
	MMXDATA_INIT (.mmx_565_b,					0x00000000000000f8),
	MMXDATA_INIT (.mmx_packed_565_rb,			0x00f800f800f800f8),
	MMXDATA_INIT (.mmx_packed_565_g,			 0x0000fc000000fc00),
	MMXDATA_INIT (.mmx_expand_565_g,			 0x07e007e007e007e0),
	MMXDATA_INIT (.mmx_expand_565_b,			 0x001f001f001f001f),
	MMXDATA_INIT (.mmx_expand_565_r,			 0xf800f800f800f800),
#ifndef USE_LOONGSON_MMI
	MMXDATA_INIT (.mmx_mask_0,				   0xffffffffffff0000),
	MMXDATA_INIT (.mmx_mask_1,				   0xffffffff0000ffff),
	MMXDATA_INIT (.mmx_mask_2,				   0xffff0000ffffffff),
	MMXDATA_INIT (.mmx_mask_3,				   0x0000ffffffffffff),
#endif
	MMXDATA_INIT (.mmx_full_alpha,			   0x00ff000000000000),
	MMXDATA_INIT (.mmx_4x0101,				   0x0101010101010101),
	MMXDATA_INIT (.mmx_ff000000,				 0xff000000ff000000),
};

#ifdef USE_CVT_INTRINSICS
#	define MC(x) to_m64 (c.mmx_ ## x)
#elif defined(USE_M64_CASTS)
#	define MC(x) ((__m64)c.mmx_ ## x)
#elif defined(USE_M64_DOUBLE)
#	define MC(x) (*(__m64 *)&c.mmx_ ## x)
#else
#	define MC(x) c.mmx_ ## x
#endif

static FORCE_INLINE __m64
to_m64 (uint64 x)
{
#ifdef USE_CVT_INTRINSICS
	return _mm_cvtsi64_m64 (x);
#elif defined M64_MEMBER		/* __m64 is a struct, not an integral type */
	__m64 res;

	res.M64_MEMBER = x;
	return res;
#elif defined USE_M64_DOUBLE
	return *(__m64 *)&x;
#else /* USE_M64_CASTS */
	return (__m64)x;
#endif
}

static FORCE_INLINE uint64
to_uint64 (__m64 x)
{
#ifdef USE_CVT_INTRINSICS
	return _mm_cvtm64_si64 (x);
#elif defined M64_MEMBER		/* __m64 is a struct, not an integral type */
	uint64 res = x.M64_MEMBER;
	return res;
#elif defined USE_M64_DOUBLE
	return *(uint64 *)&x;
#else /* USE_M64_CASTS */
	return (uint64)x;
#endif
}

static FORCE_INLINE __m64
shift (__m64 v,
	   int   s)
{
	if (s > 0)
	return _mm_slli_si64 (v, s);
	else if (s < 0)
	return _mm_srli_si64 (v, -s);
	else
	return v;
}

static FORCE_INLINE __m64
negate (__m64 mask)
{
	return _mm_xor_si64 (mask, MC (4x00ff));
}

/* Computes the product of two unsigned fixed-point 8-bit values from 0 to 1
 * and maps its result to the same range.
 *
 * Jim Blinn gives multiple ways to compute this in "Jim Blinn's Corner:
 * Notation, Notation, Notation", the first of which is
 *
 *   prod(a, b) = (a * b + 128) / 255.
 *
 * By approximating the division by 255 as 257/65536 it can be replaced by a
 * multiply and a right shift. This is the implementation that we use in
 * pix_multiply(), but we _mm_mulhi_pu16() by 257 (part of SSE1 or Extended
 * 3DNow!, and unavailable at the time of the book's publication) to perform
 * the multiplication and right shift in a single operation.
 *
 *   prod(a, b) = ((a * b + 128) * 257) >> 16.
 *
 * A third way (how pix_multiply() was implemented prior to 14208344) exists
 * also that performs the multiplication by 257 with adds and shifts.
 *
 * Where temp = a * b + 128
 *
 *   prod(a, b) = (temp + (temp >> 8)) >> 8.
 */
static FORCE_INLINE __m64
pix_multiply (__m64 a, __m64 b)
{
	__m64 res;

	res = _mm_mullo_pi16 (a, b);
	res = _mm_adds_pu16 (res, MC (4x0080));
	res = _mm_mulhi_pu16 (res, MC (4x0101));

	return res;
}

static FORCE_INLINE __m64
pix_add (__m64 a, __m64 b)
{
	return _mm_adds_pu8 (a, b);
}

static FORCE_INLINE __m64
expand_alpha (__m64 pixel)
{
	return _mm_shuffle_pi16 (pixel, _MM_SHUFFLE (3, 3, 3, 3));
}

static FORCE_INLINE __m64
expand_alpha_rev (__m64 pixel)
{
	return _mm_shuffle_pi16 (pixel, _MM_SHUFFLE (0, 0, 0, 0));
}

static FORCE_INLINE __m64
invert_colors (__m64 pixel)
{
	return _mm_shuffle_pi16 (pixel, _MM_SHUFFLE (3, 0, 1, 2));
}

static FORCE_INLINE __m64
over (__m64 src,
	  __m64 srca,
	  __m64 dest)
{
	return _mm_adds_pu8 (src, pix_multiply (dest, negate (srca)));
}

static FORCE_INLINE __m64
over_rev_non_pre (__m64 src, __m64 dest)
{
	__m64 srca = expand_alpha (src);
	__m64 srcfaaa = _mm_or_si64 (srca, MC (full_alpha));

	return over (pix_multiply (invert_colors (src), srcfaaa), srca, dest);
}

static FORCE_INLINE __m64
in (__m64 src, __m64 mask)
{
	return pix_multiply (src, mask);
}

#ifndef _MSC_VER
static FORCE_INLINE __m64
in_over (__m64 src, __m64 srca, __m64 mask, __m64 dest)
{
	return over (in (src, mask), pix_multiply (srca, mask), dest);
}

#else

#define in_over(src, srca, mask, dest)					\
	over (in (src, mask), pix_multiply (srca, mask), dest)

#endif

/* Elemental unaligned loads */

static FORCE_INLINE __m64 ldq_u(__m64 *p)
{
#ifdef USE_X86_MMX
	/* x86's alignment restrictions are very relaxed, but that's no excuse */
	__m64 r;
	(void)memcpy(&r, p, sizeof(__m64));
	return r;
#elif defined USE_ARM_IWMMXT
	int align = (uintptr_t)p & 7;
	__m64 *aligned_p;
	if (align == 0)
	return *p;
	aligned_p = (__m64 *)((uintptr_t)p & ~7);
	return (__m64) _mm_align_si64 (aligned_p[0], aligned_p[1], align);
#else
	struct __una_u64 { __m64 x __attribute__((packed)); };
	const struct __una_u64 *ptr = (const struct __una_u64 *) p;
	return (__m64) ptr->x;
#endif
}

static FORCE_INLINE uint32 ldl_u(const uint32 *p)
{
#ifdef USE_X86_MMX
	/* x86's alignment restrictions are very relaxed. */
	uint32 r;
	memcpy(&r, p, sizeof(uint32));
	return r;
#else
	struct __una_u32 { uint32 x __attribute__((packed)); };
	const struct __una_u32 *ptr = (const struct __una_u32 *) p;
	return ptr->x;
#endif
}

static FORCE_INLINE __m64
load (const uint32 *v)
{
#ifdef USE_LOONGSON_MMI
	__m64 ret;
	asm ("lwc1 %0, %1\n\t"
	: "=f" (ret)
	: "m" (*v)
	);
	return ret;
#else
	return _mm_cvtsi32_si64 (*v);
#endif
}

static FORCE_INLINE __m64
load8888 (const uint32 *v)
{
#ifdef USE_LOONGSON_MMI
	return _mm_unpacklo_pi8_f (*(__m32 *)v, _mm_setzero_si64 ());
#else
	return _mm_unpacklo_pi8 (load (v), _mm_setzero_si64 ());
#endif
}

static FORCE_INLINE __m64
load8888u (const uint32 *v)
{
	uint32 l = ldl_u (v);
	return load8888 (&l);
}

static FORCE_INLINE __m64
pack8888 (__m64 lo, __m64 hi)
{
	return _mm_packs_pu16 (lo, hi);
}

static FORCE_INLINE void
store (uint32 *dest, __m64 v)
{
#ifdef USE_LOONGSON_MMI
	asm ("swc1 %1, %0\n\t"
	: "=m" (*dest)
	: "f" (v)
	: "memory"
	);
#else
	*dest = _mm_cvtsi64_si32 (v);
#endif
}

static FORCE_INLINE void
store8888 (uint32 *dest, __m64 v)
{
	v = pack8888 (v, _mm_setzero_si64 ());
	store (dest, v);
}

static FORCE_INLINE int
is_equal (__m64 a, __m64 b)
{
#ifdef USE_LOONGSON_MMI
	/* __m64 is double, we can compare directly. */
	return a == b;
#else
	return _mm_movemask_pi8 (_mm_cmpeq_pi8 (a, b)) == 0xff;
#endif
}

static FORCE_INLINE int
is_opaque (__m64 v)
{
#ifdef USE_LOONGSON_MMI
	return is_equal (_mm_and_si64 (v, MC (full_alpha)), MC (full_alpha));
#else
	__m64 ffs = _mm_cmpeq_pi8 (v, v);
	return (_mm_movemask_pi8 (_mm_cmpeq_pi8 (v, ffs)) & 0x40);
#endif
}

static FORCE_INLINE int
is_zero (__m64 v)
{
	return is_equal (v, _mm_setzero_si64 ());
}

/* Expand 16 bits positioned at @pos (0-3) of a mmx register into
 *
 *	00RR00GG00BB
 *
 * --- Expanding 565 in the low word ---
 *
 * m = (m << (32 - 3)) | (m << (16 - 5)) | m;
 * m = m & (01f0003f001f);
 * m = m * (008404100840);
 * m = m >> 8;
 *
 * Note the trick here - the top word is shifted by another nibble to
 * avoid it bumping into the middle word
 */
static FORCE_INLINE __m64
expand565 (__m64 pixel, int pos)
{
	__m64 p = pixel;
	__m64 t1, t2;

	/* move pixel to low 16 bit and zero the rest */
#ifdef USE_LOONGSON_MMI
	p = loongson_extract_pi16 (p, pos);
#else
	p = shift (shift (p, (3 - pos) * 16), -48);
#endif

	t1 = shift (p, 36 - 11);
	t2 = shift (p, 16 - 5);

	p = _mm_or_si64 (t1, p);
	p = _mm_or_si64 (t2, p);
	p = _mm_and_si64 (p, MC (565_rgb));

	pixel = _mm_mullo_pi16 (p, MC (565_unpack_multiplier));
	return _mm_srli_pi16 (pixel, 8);
}

/* Expand 4 16 bit pixels in an mmx register into two mmx registers of
 *
 *	AARRGGBBRRGGBB
 */
static FORCE_INLINE void
expand_4xpacked565 (__m64 vin, __m64 *vout0, __m64 *vout1, int full_alpha)
{
	__m64 t0, t1, alpha = _mm_setzero_si64 ();
	__m64 r = _mm_and_si64 (vin, MC (expand_565_r));
	__m64 g = _mm_and_si64 (vin, MC (expand_565_g));
	__m64 b = _mm_and_si64 (vin, MC (expand_565_b));
	if (full_alpha)
	alpha = _mm_cmpeq_pi32 (alpha, alpha);

	/* Replicate high bits into empty low bits. */
	r = _mm_or_si64 (_mm_srli_pi16 (r, 8), _mm_srli_pi16 (r, 13));
	g = _mm_or_si64 (_mm_srli_pi16 (g, 3), _mm_srli_pi16 (g, 9));
	b = _mm_or_si64 (_mm_slli_pi16 (b, 3), _mm_srli_pi16 (b, 2));

	r = _mm_packs_pu16 (r, _mm_setzero_si64 ());	/* 00 00 00 00 R3 R2 R1 R0 */
	g = _mm_packs_pu16 (g, _mm_setzero_si64 ());	/* 00 00 00 00 G3 G2 G1 G0 */
	b = _mm_packs_pu16 (b, _mm_setzero_si64 ());	/* 00 00 00 00 B3 B2 B1 B0 */

	t1 = _mm_unpacklo_pi8 (r, alpha);			/* A3 R3 A2 R2 A1 R1 A0 R0 */
	t0 = _mm_unpacklo_pi8 (b, g);			/* G3 B3 G2 B2 G1 B1 G0 B0 */

	*vout0 = _mm_unpacklo_pi16 (t0, t1);		/* A1 R1 G1 B1 A0 R0 G0 B0 */
	*vout1 = _mm_unpackhi_pi16 (t0, t1);		/* A3 R3 G3 B3 A2 R2 G2 B2 */
}

static FORCE_INLINE __m64
expand8888 (__m64 in, int pos)
{
	if (pos == 0)
	return _mm_unpacklo_pi8 (in, _mm_setzero_si64 ());
	else
	return _mm_unpackhi_pi8 (in, _mm_setzero_si64 ());
}

static FORCE_INLINE __m64
expandx888 (__m64 in, int pos)
{
	return _mm_or_si64 (expand8888 (in, pos), MC (full_alpha));
}

static FORCE_INLINE void
expand_4x565 (__m64 vin, __m64 *vout0, __m64 *vout1, __m64 *vout2, __m64 *vout3, int full_alpha)
{
	__m64 v0, v1;
	expand_4xpacked565 (vin, &v0, &v1, full_alpha);
	*vout0 = expand8888 (v0, 0);
	*vout1 = expand8888 (v0, 1);
	*vout2 = expand8888 (v1, 0);
	*vout3 = expand8888 (v1, 1);
}

static FORCE_INLINE __m64
pack_565 (__m64 pixel, __m64 target, int pos)
{
	__m64 p = pixel;
	__m64 t = target;
	__m64 r, g, b;

	r = _mm_and_si64 (p, MC (565_r));
	g = _mm_and_si64 (p, MC (565_g));
	b = _mm_and_si64 (p, MC (565_b));

#ifdef USE_LOONGSON_MMI
	r = shift (r, -(32 - 8));
	g = shift (g, -(16 - 3));
	b = shift (b, -(0  + 3));

	p = _mm_or_si64 (r, g);
	p = _mm_or_si64 (p, b);
	return loongson_insert_pi16 (t, p, pos);
#else
	r = shift (r, -(32 - 8) + pos * 16);
	g = shift (g, -(16 - 3) + pos * 16);
	b = shift (b, -(0  + 3) + pos * 16);

	if (pos == 0)
	t = _mm_and_si64 (t, MC (mask_0));
	else if (pos == 1)
	t = _mm_and_si64 (t, MC (mask_1));
	else if (pos == 2)
	t = _mm_and_si64 (t, MC (mask_2));
	else if (pos == 3)
	t = _mm_and_si64 (t, MC (mask_3));

	p = _mm_or_si64 (r, t);
	p = _mm_or_si64 (g, p);

	return _mm_or_si64 (b, p);
#endif
}

static FORCE_INLINE __m64
pack_4xpacked565 (__m64 a, __m64 b)
{
	__m64 rb0 = _mm_and_si64 (a, MC (packed_565_rb));
	__m64 rb1 = _mm_and_si64 (b, MC (packed_565_rb));

	__m64 t0 = _mm_madd_pi16 (rb0, MC (565_pack_multiplier));
	__m64 t1 = _mm_madd_pi16 (rb1, MC (565_pack_multiplier));

	__m64 g0 = _mm_and_si64 (a, MC (packed_565_g));
	__m64 g1 = _mm_and_si64 (b, MC (packed_565_g));

	t0 = _mm_or_si64 (t0, g0);
	t1 = _mm_or_si64 (t1, g1);

	t0 = shift(t0, -5);
#ifdef USE_ARM_IWMMXT
	t1 = shift(t1, -5);
	return _mm_packs_pu32 (t0, t1);
#else
	t1 = shift(t1, -5 + 16);
	return _mm_shuffle_pi16 (_mm_or_si64 (t0, t1), _MM_SHUFFLE (3, 1, 2, 0));
#endif
}

#ifndef _MSC_VER

static FORCE_INLINE __m64
pack_4x565 (__m64 v0, __m64 v1, __m64 v2, __m64 v3)
{
	return pack_4xpacked565 (pack8888 (v0, v1), pack8888 (v2, v3));
}

static FORCE_INLINE __m64
pix_add_mul (__m64 x, __m64 a, __m64 y, __m64 b)
{
	x = pix_multiply (x, a);
	y = pix_multiply (y, b);

	return pix_add (x, y);
}

#else

/* MSVC only handles a "pass by register" of up to three SSE intrinsics */

#define pack_4x565(v0, v1, v2, v3) \
	pack_4xpacked565 (pack8888 (v0, v1), pack8888 (v2, v3))

#define pix_add_mul(x, a, y, b)	 \
	( x = pix_multiply (x, a),	 \
	  y = pix_multiply (y, b),	 \
	  pix_add (x, y) )

#endif

/* --------------- MMX code patch for fbcompose.c --------------------- */

static FORCE_INLINE __m64
combine (const uint32 *src, const uint32 *mask)
{
	__m64 vsrc = load8888 (src);

	if (mask)
	{
	__m64 m = load8888 (mask);

	m = expand_alpha (m);
	vsrc = pix_multiply (vsrc, m);
	}

	return vsrc;
}

static FORCE_INLINE __m64
core_combine_over_u_pixel_mmx (__m64 vsrc, __m64 vdst)
{
	vsrc = _mm_unpacklo_pi8 (vsrc, _mm_setzero_si64 ());

	if (is_opaque (vsrc))
	{
	return vsrc;
	}
	else if (!is_zero (vsrc))
	{
	return over (vsrc, expand_alpha (vsrc),
			 _mm_unpacklo_pi8 (vdst, _mm_setzero_si64 ()));
	}

	return _mm_unpacklo_pi8 (vdst, _mm_setzero_si64 ());
}

static void
mmx_combine_over_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					ePIXEL_MANIPULATE_OPERATE			 op,
					uint32 *			   dest,
					const uint32 *		 src,
					const uint32 *		 mask,
					int					  width)
{
	const uint32 *end = dest + width;

	while (dest < end)
	{
	__m64 vsrc = combine (src, mask);

	if (is_opaque (vsrc))
	{
		store8888 (dest, vsrc);
	}
	else if (!is_zero (vsrc))
	{
		__m64 sa = expand_alpha (vsrc);
		store8888 (dest, over (vsrc, sa, load8888 (dest)));
	}

	++dest;
	++src;
	if (mask)
		++mask;
	}
	_mm_empty ();
}

static void
mmx_combine_over_reverse_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							ePIXEL_MANIPULATE_OPERATE			 op,
							uint32 *			   dest,
							const uint32 *		 src,
							const uint32 *		 mask,
							int					  width)
{
	const uint32 *end = dest + width;

	while (dest < end)
	{
	__m64 d, da;
	__m64 s = combine (src, mask);

	d = load8888 (dest);
	da = expand_alpha (d);
	store8888 (dest, over (d, da, s));

	++dest;
	++src;
	if (mask)
		mask++;
	}
	_mm_empty ();
}

static void
mmx_combine_in_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
				  ePIXEL_MANIPULATE_OPERATE			 op,
				  uint32 *			   dest,
				  const uint32 *		 src,
				  const uint32 *		 mask,
				  int					  width)
{
	const uint32 *end = dest + width;

	while (dest < end)
	{
	__m64 a;
	__m64 x = combine (src, mask);

	a = load8888 (dest);
	a = expand_alpha (a);
	x = pix_multiply (x, a);

	store8888 (dest, x);

	++dest;
	++src;
	if (mask)
		mask++;
	}
	_mm_empty ();
}

static void
mmx_combine_in_reverse_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
						  ePIXEL_MANIPULATE_OPERATE			 op,
						  uint32 *			   dest,
						  const uint32 *		 src,
						  const uint32 *		 mask,
						  int					  width)
{
	const uint32 *end = dest + width;

	while (dest < end)
	{
	__m64 a = combine (src, mask);
	__m64 x;

	x = load8888 (dest);
	a = expand_alpha (a);
	x = pix_multiply (x, a);
	store8888 (dest, x);

	++dest;
	++src;
	if (mask)
		mask++;
	}
	_mm_empty ();
}

static void
mmx_combine_out_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
				   ePIXEL_MANIPULATE_OPERATE			 op,
				   uint32 *			   dest,
				   const uint32 *		 src,
				   const uint32 *		 mask,
				   int					  width)
{
	const uint32 *end = dest + width;

	while (dest < end)
	{
	__m64 a;
	__m64 x = combine (src, mask);

	a = load8888 (dest);
	a = expand_alpha (a);
	a = negate (a);
	x = pix_multiply (x, a);
	store8888 (dest, x);

	++dest;
	++src;
	if (mask)
		mask++;
	}
	_mm_empty ();
}

static void
mmx_combine_out_reverse_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
						   ePIXEL_MANIPULATE_OPERATE			 op,
						   uint32 *			   dest,
						   const uint32 *		 src,
						   const uint32 *		 mask,
						   int					  width)
{
	const uint32 *end = dest + width;

	while (dest < end)
	{
	__m64 a = combine (src, mask);
	__m64 x;

	x = load8888 (dest);
	a = expand_alpha (a);
	a = negate (a);
	x = pix_multiply (x, a);

	store8888 (dest, x);

	++dest;
	++src;
	if (mask)
		mask++;
	}
	_mm_empty ();
}

static void
mmx_combine_atOPERATE_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					ePIXEL_MANIPULATE_OPERATE			 op,
					uint32 *			   dest,
					const uint32 *		 src,
					const uint32 *		 mask,
					int					  width)
{
	const uint32 *end = dest + width;

	while (dest < end)
	{
	__m64 da, d, sia;
	__m64 s = combine (src, mask);

	d = load8888 (dest);
	sia = expand_alpha (s);
	sia = negate (sia);
	da = expand_alpha (d);
	s = pix_add_mul (s, da, d, sia);
	store8888 (dest, s);

	++dest;
	++src;
	if (mask)
		mask++;
	}
	_mm_empty ();
}

static void
mmx_combine_atOPERATE_reverse_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							ePIXEL_MANIPULATE_OPERATE			 op,
							uint32 *			   dest,
							const uint32 *		 src,
							const uint32 *		 mask,
							int					  width)
{
	const uint32 *end;

	end = dest + width;

	while (dest < end)
	{
	__m64 dia, d, sa;
	__m64 s = combine (src, mask);

	d = load8888 (dest);
	sa = expand_alpha (s);
	dia = expand_alpha (d);
	dia = negate (dia);
	s = pix_add_mul (s, dia, d, sa);
	store8888 (dest, s);

	++dest;
	++src;
	if (mask)
		mask++;
	}
	_mm_empty ();
}

static void
mmx_combine_xor_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
				   ePIXEL_MANIPULATE_OPERATE			 op,
				   uint32 *			   dest,
				   const uint32 *		 src,
				   const uint32 *		 mask,
				   int					  width)
{
	const uint32 *end = dest + width;

	while (dest < end)
	{
	__m64 dia, d, sia;
	__m64 s = combine (src, mask);

	d = load8888 (dest);
	sia = expand_alpha (s);
	dia = expand_alpha (d);
	sia = negate (sia);
	dia = negate (dia);
	s = pix_add_mul (s, dia, d, sia);
	store8888 (dest, s);

	++dest;
	++src;
	if (mask)
		mask++;
	}
	_mm_empty ();
}

static void
mmx_combine_add_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
				   ePIXEL_MANIPULATE_OPERATE			 op,
				   uint32 *			   dest,
				   const uint32 *		 src,
				   const uint32 *		 mask,
				   int					  width)
{
	const uint32 *end = dest + width;

	while (dest < end)
	{
	__m64 d;
	__m64 s = combine (src, mask);

	d = load8888 (dest);
	s = pix_add (s, d);
	store8888 (dest, s);

	++dest;
	++src;
	if (mask)
		mask++;
	}
	_mm_empty ();
}

static void
mmx_combine_saturate_u (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
						ePIXEL_MANIPULATE_OPERATE			 op,
						uint32 *			   dest,
						const uint32 *		 src,
						const uint32 *		 mask,
						int					  width)
{
	const uint32 *end = dest + width;

	while (dest < end)
	{
	uint32 s, sa, da;
	uint32 d = *dest;
	__m64 ms = combine (src, mask);
	__m64 md = load8888 (dest);

	store8888(&s, ms);
	da = ~d >> 24;
	sa = s >> 24;

	if (sa > da)
	{
		uint32 quot = DIV_UN8 (da, sa) << 24;
		__m64 msa = load8888 (&quot);
		msa = expand_alpha (msa);
		ms = pix_multiply (ms, msa);
	}

	md = pix_add (md, ms);
	store8888 (dest, md);

	++src;
	++dest;
	if (mask)
		mask++;
	}
	_mm_empty ();
}

static void
mmx_combine_src_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					ePIXEL_MANIPULATE_OPERATE			 op,
					uint32 *			   dest,
					const uint32 *		 src,
					const uint32 *		 mask,
					int					  width)
{
	const uint32 *end = src + width;

	while (src < end)
	{
	__m64 a = load8888 (mask);
	__m64 s = load8888 (src);

	s = pix_multiply (s, a);
	store8888 (dest, s);

	++src;
	++mask;
	++dest;
	}
	_mm_empty ();
}

static void
mmx_combine_over_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					 ePIXEL_MANIPULATE_OPERATE			 op,
					 uint32 *			   dest,
					 const uint32 *		 src,
					 const uint32 *		 mask,
					 int					  width)
{
	const uint32 *end = src + width;

	while (src < end)
	{
	__m64 a = load8888 (mask);
	__m64 s = load8888 (src);
	__m64 d = load8888 (dest);
	__m64 sa = expand_alpha (s);

	store8888 (dest, in_over (s, sa, a, d));

	++src;
	++dest;
	++mask;
	}
	_mm_empty ();
}

static void
mmx_combine_over_reverse_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							 ePIXEL_MANIPULATE_OPERATE			 op,
							 uint32 *			   dest,
							 const uint32 *		 src,
							 const uint32 *		 mask,
							 int					  width)
{
	const uint32 *end = src + width;

	while (src < end)
	{
	__m64 a = load8888 (mask);
	__m64 s = load8888 (src);
	__m64 d = load8888 (dest);
	__m64 da = expand_alpha (d);

	store8888 (dest, over (d, da, in (s, a)));

	++src;
	++dest;
	++mask;
	}
	_mm_empty ();
}

static void
mmx_combine_in_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
				   ePIXEL_MANIPULATE_OPERATE			 op,
				   uint32 *			   dest,
				   const uint32 *		 src,
				   const uint32 *		 mask,
				   int					  width)
{
	const uint32 *end = src + width;

	while (src < end)
	{
	__m64 a = load8888 (mask);
	__m64 s = load8888 (src);
	__m64 d = load8888 (dest);
	__m64 da = expand_alpha (d);

	s = pix_multiply (s, a);
	s = pix_multiply (s, da);
	store8888 (dest, s);

	++src;
	++dest;
	++mask;
	}
	_mm_empty ();
}

static void
mmx_combine_in_reverse_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
						   ePIXEL_MANIPULATE_OPERATE			 op,
						   uint32 *			   dest,
						   const uint32 *		 src,
						   const uint32 *		 mask,
						   int					  width)
{
	const uint32 *end = src + width;

	while (src < end)
	{
	__m64 a = load8888 (mask);
	__m64 s = load8888 (src);
	__m64 d = load8888 (dest);
	__m64 sa = expand_alpha (s);

	a = pix_multiply (a, sa);
	d = pix_multiply (d, a);
	store8888 (dest, d);

	++src;
	++dest;
	++mask;
	}
	_mm_empty ();
}

static void
mmx_combine_out_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					ePIXEL_MANIPULATE_OPERATE			 op,
					uint32 *			   dest,
					const uint32 *		 src,
					const uint32 *		 mask,
					int					  width)
{
	const uint32 *end = src + width;

	while (src < end)
	{
	__m64 a = load8888 (mask);
	__m64 s = load8888 (src);
	__m64 d = load8888 (dest);
	__m64 da = expand_alpha (d);

	da = negate (da);
	s = pix_multiply (s, a);
	s = pix_multiply (s, da);
	store8888 (dest, s);

	++src;
	++dest;
	++mask;
	}
	_mm_empty ();
}

static void
mmx_combine_out_reverse_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							ePIXEL_MANIPULATE_OPERATE			 op,
							uint32 *			   dest,
							const uint32 *		 src,
							const uint32 *		 mask,
							int					  width)
{
	const uint32 *end = src + width;

	while (src < end)
	{
	__m64 a = load8888 (mask);
	__m64 s = load8888 (src);
	__m64 d = load8888 (dest);
	__m64 sa = expand_alpha (s);

	a = pix_multiply (a, sa);
	a = negate (a);
	d = pix_multiply (d, a);
	store8888 (dest, d);

	++src;
	++dest;
	++mask;
	}
	_mm_empty ();
}

static void
mmx_combine_atOPERATE_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					 ePIXEL_MANIPULATE_OPERATE			 op,
					 uint32 *			   dest,
					 const uint32 *		 src,
					 const uint32 *		 mask,
					 int					  width)
{
	const uint32 *end = src + width;

	while (src < end)
	{
	__m64 a = load8888 (mask);
	__m64 s = load8888 (src);
	__m64 d = load8888 (dest);
	__m64 da = expand_alpha (d);
	__m64 sa = expand_alpha (s);

	s = pix_multiply (s, a);
	a = pix_multiply (a, sa);
	a = negate (a);
	d = pix_add_mul (d, a, s, da);
	store8888 (dest, d);

	++src;
	++dest;
	++mask;
	}
	_mm_empty ();
}

static void
mmx_combine_atOPERATE_reverse_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							 ePIXEL_MANIPULATE_OPERATE			 op,
							 uint32 *			   dest,
							 const uint32 *		 src,
							 const uint32 *		 mask,
							 int					  width)
{
	const uint32 *end = src + width;

	while (src < end)
	{
	__m64 a = load8888 (mask);
	__m64 s = load8888 (src);
	__m64 d = load8888 (dest);
	__m64 da = expand_alpha (d);
	__m64 sa = expand_alpha (s);

	s = pix_multiply (s, a);
	a = pix_multiply (a, sa);
	da = negate (da);
	d = pix_add_mul (d, a, s, da);
	store8888 (dest, d);

	++src;
	++dest;
	++mask;
	}
	_mm_empty ();
}

static void
mmx_combine_xor_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					ePIXEL_MANIPULATE_OPERATE			 op,
					uint32 *			   dest,
					const uint32 *		 src,
					const uint32 *		 mask,
					int					  width)
{
	const uint32 *end = src + width;

	while (src < end)
	{
	__m64 a = load8888 (mask);
	__m64 s = load8888 (src);
	__m64 d = load8888 (dest);
	__m64 da = expand_alpha (d);
	__m64 sa = expand_alpha (s);

	s = pix_multiply (s, a);
	a = pix_multiply (a, sa);
	da = negate (da);
	a = negate (a);
	d = pix_add_mul (d, a, s, da);
	store8888 (dest, d);

	++src;
	++dest;
	++mask;
	}
	_mm_empty ();
}

static void
mmx_combine_add_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					ePIXEL_MANIPULATE_OPERATE			 op,
					uint32 *			   dest,
					const uint32 *		 src,
					const uint32 *		 mask,
					int					  width)
{
	const uint32 *end = src + width;

	while (src < end)
	{
	__m64 a = load8888 (mask);
	__m64 s = load8888 (src);
	__m64 d = load8888 (dest);

	s = pix_multiply (s, a);
	d = pix_add (s, d);
	store8888 (dest, d);

	++src;
	++dest;
	++mask;
	}
	_mm_empty ();
}

/* ------------- MMX code paths called from fbpict.c -------------------- */

static void
mmx_composite_over_n_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
						   PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32 src;
	uint32	*dst_line, *dst;
	int32 w;
	int dst_stride;
	__m64 vsrc, vsrca;

	CHECKPOINT ();

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	if (src == 0)
	return;

	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);

	vsrc = load8888 (&src);
	vsrca = expand_alpha (vsrc);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	w = width;

	CHECKPOINT ();

	while (w && (uintptr_t)dst & 7)
	{
		store8888 (dst, over (vsrc, vsrca, load8888 (dst)));

		w--;
		dst++;
	}

	while (w >= 2)
	{
		__m64 vdest;
		__m64 dest0, dest1;

		vdest = *(__m64 *)dst;

		dest0 = over (vsrc, vsrca, expand8888 (vdest, 0));
		dest1 = over (vsrc, vsrca, expand8888 (vdest, 1));

		*(__m64 *)dst = pack8888 (dest0, dest1);

		dst += 2;
		w -= 2;
	}

	CHECKPOINT ();

	if (w)
	{
		store8888 (dst, over (vsrc, vsrca, load8888 (dst)));
	}
	}

	_mm_empty ();
}

static void
mmx_composite_over_n_0565 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
						   PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32 src;
	uint16	*dst_line, *dst;
	int32 w;
	int dst_stride;
	__m64 vsrc, vsrca;

	CHECKPOINT ();

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	if (src == 0)
	return;

	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint16, dst_stride, dst_line, 1);

	vsrc = load8888 (&src);
	vsrca = expand_alpha (vsrc);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	w = width;

	CHECKPOINT ();

	while (w && (uintptr_t)dst & 7)
	{
		uint64 d = *dst;
		__m64 vdest = expand565 (to_m64 (d), 0);

		vdest = pack_565 (over (vsrc, vsrca, vdest), vdest, 0);
		*dst = to_uint64 (vdest);

		w--;
		dst++;
	}

	while (w >= 4)
	{
		__m64 vdest = *(__m64 *)dst;
		__m64 v0, v1, v2, v3;

		expand_4x565 (vdest, &v0, &v1, &v2, &v3, 0);

		v0 = over (vsrc, vsrca, v0);
		v1 = over (vsrc, vsrca, v1);
		v2 = over (vsrc, vsrca, v2);
		v3 = over (vsrc, vsrca, v3);

		*(__m64 *)dst = pack_4x565 (v0, v1, v2, v3);

		dst += 4;
		w -= 4;
	}

	CHECKPOINT ();

	while (w)
	{
		uint64 d = *dst;
		__m64 vdest = expand565 (to_m64 (d), 0);

		vdest = pack_565 (over (vsrc, vsrca, vdest), vdest, 0);
		*dst = to_uint64 (vdest);

		w--;
		dst++;
	}
	}

	_mm_empty ();
}

static void
mmx_composite_over_n_8888_8888_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
								   PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32 src;
	uint32	*dst_line;
	uint32	*mask_line;
	int dst_stride, mask_stride;
	__m64 vsrc, vsrca;

	CHECKPOINT ();

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	if (src == 0)
	return;

	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint32, mask_stride, mask_line, 1);

	vsrc = load8888 (&src);
	vsrca = expand_alpha (vsrc);

	while (height--)
	{
	int twidth = width;
	uint32 *p = (uint32 *)mask_line;
	uint32 *q = (uint32 *)dst_line;

	while (twidth && (uintptr_t)q & 7)
	{
		uint32 m = *(uint32 *)p;

		if (m)
		{
		__m64 vdest = load8888 (q);
		vdest = in_over (vsrc, vsrca, load8888 (&m), vdest);
		store8888 (q, vdest);
		}

		twidth--;
		p++;
		q++;
	}

	while (twidth >= 2)
	{
		uint32 m0, m1;
		m0 = *p;
		m1 = *(p + 1);

		if (m0 | m1)
		{
		__m64 dest0, dest1;
		__m64 vdest = *(__m64 *)q;

		dest0 = in_over (vsrc, vsrca, load8888 (&m0),
						 expand8888 (vdest, 0));
		dest1 = in_over (vsrc, vsrca, load8888 (&m1),
						 expand8888 (vdest, 1));

		*(__m64 *)q = pack8888 (dest0, dest1);
		}

		p += 2;
		q += 2;
		twidth -= 2;
	}

	if (twidth)
	{
		uint32 m = *(uint32 *)p;

		if (m)
		{
		__m64 vdest = load8888 (q);
		vdest = in_over (vsrc, vsrca, load8888 (&m), vdest);
		store8888 (q, vdest);
		}

		twidth--;
		p++;
		q++;
	}

	dst_line += dst_stride;
	mask_line += mask_stride;
	}

	_mm_empty ();
}

static void
mmx_composite_over_8888_n_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
								PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32	*dst_line, *dst;
	uint32	*src_line, *src;
	uint32 mask;
	__m64 vmask;
	int dst_stride, src_stride;
	int32 w;

	CHECKPOINT ();

	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (src_image, src_x, src_y, uint32, src_stride, src_line, 1);

	mask = PixelManipulateImageGetSolid (imp, mask_image, dest_image->bits.format);
	vmask = expand_alpha (load8888 (&mask));

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w && (uintptr_t)dst & 7)
	{
		__m64 s = load8888 (src);
		__m64 d = load8888 (dst);

		store8888 (dst, in_over (s, expand_alpha (s), vmask, d));

		w--;
		dst++;
		src++;
	}

	while (w >= 2)
	{
		__m64 vs = ldq_u ((__m64 *)src);
		__m64 vd = *(__m64 *)dst;
		__m64 vsrc0 = expand8888 (vs, 0);
		__m64 vsrc1 = expand8888 (vs, 1);

		*(__m64 *)dst = pack8888 (
			in_over (vsrc0, expand_alpha (vsrc0), vmask, expand8888 (vd, 0)),
			in_over (vsrc1, expand_alpha (vsrc1), vmask, expand8888 (vd, 1)));

		w -= 2;
		dst += 2;
		src += 2;
	}

	if (w)
	{
		__m64 s = load8888 (src);
		__m64 d = load8888 (dst);

		store8888 (dst, in_over (s, expand_alpha (s), vmask, d));
	}
	}

	_mm_empty ();
}

static void
mmx_composite_over_x888_n_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
								PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32 *dst_line, *dst;
	uint32 *src_line, *src;
	uint32 mask;
	__m64 vmask;
	int dst_stride, src_stride;
	int32 w;
	__m64 srca;

	CHECKPOINT ();

	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (src_image, src_x, src_y, uint32, src_stride, src_line, 1);
	mask = PixelManipulateImageGetSolid (imp, mask_image, dest_image->bits.format);

	vmask = expand_alpha (load8888 (&mask));
	srca = MC (4x00ff);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w && (uintptr_t)dst & 7)
	{
		uint32 ssrc = *src | 0xff000000;
		__m64 s = load8888 (&ssrc);
		__m64 d = load8888 (dst);

		store8888 (dst, in_over (s, srca, vmask, d));

		w--;
		dst++;
		src++;
	}

	while (w >= 16)
	{
		__m64 vd0 = *(__m64 *)(dst + 0);
		__m64 vd1 = *(__m64 *)(dst + 2);
		__m64 vd2 = *(__m64 *)(dst + 4);
		__m64 vd3 = *(__m64 *)(dst + 6);
		__m64 vd4 = *(__m64 *)(dst + 8);
		__m64 vd5 = *(__m64 *)(dst + 10);
		__m64 vd6 = *(__m64 *)(dst + 12);
		__m64 vd7 = *(__m64 *)(dst + 14);

		__m64 vs0 = ldq_u ((__m64 *)(src + 0));
		__m64 vs1 = ldq_u ((__m64 *)(src + 2));
		__m64 vs2 = ldq_u ((__m64 *)(src + 4));
		__m64 vs3 = ldq_u ((__m64 *)(src + 6));
		__m64 vs4 = ldq_u ((__m64 *)(src + 8));
		__m64 vs5 = ldq_u ((__m64 *)(src + 10));
		__m64 vs6 = ldq_u ((__m64 *)(src + 12));
		__m64 vs7 = ldq_u ((__m64 *)(src + 14));

		vd0 = pack8888 (
			in_over (expandx888 (vs0, 0), srca, vmask, expand8888 (vd0, 0)),
			in_over (expandx888 (vs0, 1), srca, vmask, expand8888 (vd0, 1)));

		vd1 = pack8888 (
			in_over (expandx888 (vs1, 0), srca, vmask, expand8888 (vd1, 0)),
			in_over (expandx888 (vs1, 1), srca, vmask, expand8888 (vd1, 1)));

		vd2 = pack8888 (
			in_over (expandx888 (vs2, 0), srca, vmask, expand8888 (vd2, 0)),
			in_over (expandx888 (vs2, 1), srca, vmask, expand8888 (vd2, 1)));

		vd3 = pack8888 (
			in_over (expandx888 (vs3, 0), srca, vmask, expand8888 (vd3, 0)),
			in_over (expandx888 (vs3, 1), srca, vmask, expand8888 (vd3, 1)));

		vd4 = pack8888 (
			in_over (expandx888 (vs4, 0), srca, vmask, expand8888 (vd4, 0)),
			in_over (expandx888 (vs4, 1), srca, vmask, expand8888 (vd4, 1)));

		vd5 = pack8888 (
			in_over (expandx888 (vs5, 0), srca, vmask, expand8888 (vd5, 0)),
			in_over (expandx888 (vs5, 1), srca, vmask, expand8888 (vd5, 1)));

		vd6 = pack8888 (
			in_over (expandx888 (vs6, 0), srca, vmask, expand8888 (vd6, 0)),
			in_over (expandx888 (vs6, 1), srca, vmask, expand8888 (vd6, 1)));

		vd7 = pack8888 (
			in_over (expandx888 (vs7, 0), srca, vmask, expand8888 (vd7, 0)),
			in_over (expandx888 (vs7, 1), srca, vmask, expand8888 (vd7, 1)));

		*(__m64 *)(dst + 0) = vd0;
		*(__m64 *)(dst + 2) = vd1;
		*(__m64 *)(dst + 4) = vd2;
		*(__m64 *)(dst + 6) = vd3;
		*(__m64 *)(dst + 8) = vd4;
		*(__m64 *)(dst + 10) = vd5;
		*(__m64 *)(dst + 12) = vd6;
		*(__m64 *)(dst + 14) = vd7;

		w -= 16;
		dst += 16;
		src += 16;
	}

	while (w)
	{
		uint32 ssrc = *src | 0xff000000;
		__m64 s = load8888 (&ssrc);
		__m64 d = load8888 (dst);

		store8888 (dst, in_over (s, srca, vmask, d));

		w--;
		dst++;
		src++;
	}
	}

	_mm_empty ();
}

static void
mmx_composite_over_8888_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							  PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32 *dst_line, *dst;
	uint32 *src_line, *src;
	uint32 s;
	int dst_stride, src_stride;
	uint8 a;
	int32 w;

	CHECKPOINT ();

	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (src_image, src_x, src_y, uint32, src_stride, src_line, 1);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w--)
	{
		s = *src++;
		a = s >> 24;

		if (a == 0xff)
		{
		*dst = s;
		}
		else if (s)
		{
		__m64 ms, sa;
		ms = load8888 (&s);
		sa = expand_alpha (ms);
		store8888 (dst, over (ms, sa, load8888 (dst)));
		}

		dst++;
	}
	}
	_mm_empty ();
}

static void
mmx_composite_over_8888_0565 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							  PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint16	*dst_line, *dst;
	uint32	*src_line, *src;
	int dst_stride, src_stride;
	int32 w;

	CHECKPOINT ();

	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint16, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (src_image, src_x, src_y, uint32, src_stride, src_line, 1);

#if 0
	/* FIXME */
	assert (src_image->drawable == mask_image->drawable);
#endif

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	CHECKPOINT ();

	while (w && (uintptr_t)dst & 7)
	{
		__m64 vsrc = load8888 (src);
		uint64 d = *dst;
		__m64 vdest = expand565 (to_m64 (d), 0);

		vdest = pack_565 (
		over (vsrc, expand_alpha (vsrc), vdest), vdest, 0);

		*dst = to_uint64 (vdest);

		w--;
		dst++;
		src++;
	}

	CHECKPOINT ();

	while (w >= 4)
	{
		__m64 vdest = *(__m64 *)dst;
		__m64 v0, v1, v2, v3;
		__m64 vsrc0, vsrc1, vsrc2, vsrc3;

		expand_4x565 (vdest, &v0, &v1, &v2, &v3, 0);

		vsrc0 = load8888 ((src + 0));
		vsrc1 = load8888 ((src + 1));
		vsrc2 = load8888 ((src + 2));
		vsrc3 = load8888 ((src + 3));

		v0 = over (vsrc0, expand_alpha (vsrc0), v0);
		v1 = over (vsrc1, expand_alpha (vsrc1), v1);
		v2 = over (vsrc2, expand_alpha (vsrc2), v2);
		v3 = over (vsrc3, expand_alpha (vsrc3), v3);

		*(__m64 *)dst = pack_4x565 (v0, v1, v2, v3);

		w -= 4;
		dst += 4;
		src += 4;
	}

	CHECKPOINT ();

	while (w)
	{
		__m64 vsrc = load8888 (src);
		uint64 d = *dst;
		__m64 vdest = expand565 (to_m64 (d), 0);

		vdest = pack_565 (over (vsrc, expand_alpha (vsrc), vdest), vdest, 0);

		*dst = to_uint64 (vdest);

		w--;
		dst++;
		src++;
	}
	}

	_mm_empty ();
}

static void
mmx_composite_over_n_8_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							 PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32 src, srca;
	uint32 *dst_line, *dst;
	uint8 *mask_line, *mask;
	int dst_stride, mask_stride;
	int32 w;
	__m64 vsrc, vsrca;
	uint64 srcsrc;

	CHECKPOINT ();

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	srca = src >> 24;
	if (src == 0)
	return;

	srcsrc = (uint64)src << 32 | src;

	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8, mask_stride, mask_line, 1);

	vsrc = load8888 (&src);
	vsrca = expand_alpha (vsrc);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	CHECKPOINT ();

	while (w && (uintptr_t)dst & 7)
	{
		uint64 m = *mask;

		if (m)
		{
		__m64 vdest = in_over (vsrc, vsrca,
					   expand_alpha_rev (to_m64 (m)),
					   load8888 (dst));

		store8888 (dst, vdest);
		}

		w--;
		mask++;
		dst++;
	}

	CHECKPOINT ();

	while (w >= 2)
	{
		uint64 m0, m1;

		m0 = *mask;
		m1 = *(mask + 1);

		if (srca == 0xff && (m0 & m1) == 0xff)
		{
		*(uint64 *)dst = srcsrc;
		}
		else if (m0 | m1)
		{
		__m64 vdest;
		__m64 dest0, dest1;

		vdest = *(__m64 *)dst;

		dest0 = in_over (vsrc, vsrca, expand_alpha_rev (to_m64 (m0)),
				 expand8888 (vdest, 0));
		dest1 = in_over (vsrc, vsrca, expand_alpha_rev (to_m64 (m1)),
				 expand8888 (vdest, 1));

		*(__m64 *)dst = pack8888 (dest0, dest1);
		}

		mask += 2;
		dst += 2;
		w -= 2;
	}

	CHECKPOINT ();

	if (w)
	{
		uint64 m = *mask;

		if (m)
		{
		__m64 vdest = load8888 (dst);

		vdest = in_over (
			vsrc, vsrca, expand_alpha_rev (to_m64 (m)), vdest);
		store8888 (dst, vdest);
		}
	}
	}

	_mm_empty ();
}

static int
mmx_fill (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
		  uint32 *			   bits,
		  int					  stride,
		  int					  bpp,
		  int					  x,
		  int					  y,
		  int					  width,
		  int					  height,
		  uint32		   filler)
{
	uint64 fill;
	__m64 vfill;
	uint32 byte_width;
	uint8	 *byte_line;

#if defined __GNUC__ && defined USE_X86_MMX
	__m64 v1, v2, v3, v4, v5, v6, v7;
#endif

	if (bpp != 16 && bpp != 32 && bpp != 8)
	return FALSE;

	if (bpp == 8)
	{
	stride = stride * (int) sizeof (uint32) / 1;
	byte_line = (uint8 *)(((uint8 *)bits) + stride * y + x);
	byte_width = width;
	stride *= 1;
		filler = (filler & 0xff) * 0x01010101;
	}
	else if (bpp == 16)
	{
	stride = stride * (int) sizeof (uint32) / 2;
	byte_line = (uint8 *)(((uint16 *)bits) + stride * y + x);
	byte_width = 2 * width;
	stride *= 2;
		filler = (filler & 0xffff) * 0x00010001;
	}
	else
	{
	stride = stride * (int) sizeof (uint32) / 4;
	byte_line = (uint8 *)(((uint32 *)bits) + stride * y + x);
	byte_width = 4 * width;
	stride *= 4;
	}

	fill = ((uint64)filler << 32) | filler;
	vfill = to_m64 (fill);

#if defined __GNUC__ && defined USE_X86_MMX
	__asm__ (
		"movq		%7,	%0\n"
		"movq		%7,	%1\n"
		"movq		%7,	%2\n"
		"movq		%7,	%3\n"
		"movq		%7,	%4\n"
		"movq		%7,	%5\n"
		"movq		%7,	%6\n"
	: "=&y" (v1), "=&y" (v2), "=&y" (v3),
	  "=&y" (v4), "=&y" (v5), "=&y" (v6), "=y" (v7)
	: "y" (vfill));
#endif

	while (height--)
	{
	int w;
	uint8 *d = byte_line;

	byte_line += stride;
	w = byte_width;

	if (w >= 1 && ((uintptr_t)d & 1))
	{
		*(uint8 *)d = (filler & 0xff);
		w--;
		d++;
	}

	if (w >= 2 && ((uintptr_t)d & 3))
	{
		*(uint16 *)d = filler;
		w -= 2;
		d += 2;
	}

	while (w >= 4 && ((uintptr_t)d & 7))
	{
		*(uint32 *)d = filler;

		w -= 4;
		d += 4;
	}

	while (w >= 64)
	{
#if defined __GNUC__ && defined USE_X86_MMX
		__asm__ (
			"movq	%1,	  (%0)\n"
			"movq	%2,	 8(%0)\n"
			"movq	%3,	16(%0)\n"
			"movq	%4,	24(%0)\n"
			"movq	%5,	32(%0)\n"
			"movq	%6,	40(%0)\n"
			"movq	%7,	48(%0)\n"
			"movq	%8,	56(%0)\n"
		:
		: "r" (d),
		  "y" (vfill), "y" (v1), "y" (v2), "y" (v3),
		  "y" (v4), "y" (v5), "y" (v6), "y" (v7)
		: "memory");
#else
		*(__m64*) (d +  0) = vfill;
		*(__m64*) (d +  8) = vfill;
		*(__m64*) (d + 16) = vfill;
		*(__m64*) (d + 24) = vfill;
		*(__m64*) (d + 32) = vfill;
		*(__m64*) (d + 40) = vfill;
		*(__m64*) (d + 48) = vfill;
		*(__m64*) (d + 56) = vfill;
#endif
		w -= 64;
		d += 64;
	}

	while (w >= 4)
	{
		*(uint32 *)d = filler;

		w -= 4;
		d += 4;
	}
	if (w >= 2)
	{
		*(uint16 *)d = filler;
		w -= 2;
		d += 2;
	}
	if (w >= 1)
	{
		*(uint8 *)d = (filler & 0xff);
		w--;
		d++;
	}

	}

	_mm_empty ();
	return TRUE;
}

static void
mmx_composite_src_x888_0565 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
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

	while (w && (uintptr_t)dst & 7)
	{
		s = *src++;
		*dst = convert_8888_to_0565 (s);
		dst++;
		w--;
	}

	while (w >= 4)
	{
		__m64 vdest;
		__m64 vsrc0 = ldq_u ((__m64 *)(src + 0));
		__m64 vsrc1 = ldq_u ((__m64 *)(src + 2));

		vdest = pack_4xpacked565 (vsrc0, vsrc1);

		*(__m64 *)dst = vdest;

		w -= 4;
		src += 4;
		dst += 4;
	}

	while (w)
	{
		s = *src++;
		*dst = convert_8888_to_0565 (s);
		dst++;
		w--;
	}
	}

	_mm_empty ();
}

static void
mmx_composite_src_n_8_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32 src, srca;
	uint32	*dst_line, *dst;
	uint8	 *mask_line, *mask;
	int dst_stride, mask_stride;
	int32 w;
	__m64 vsrc;
	uint64 srcsrc;

	CHECKPOINT ();

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	srca = src >> 24;
	if (src == 0)
	{
	mmx_fill (imp, dest_image->bits.bits, dest_image->bits.row_stride,
		  PIXEL_MANIPULATE_FORMAT_BPP (dest_image->bits.format),
		  dest_x, dest_y, width, height, 0);
	return;
	}

	srcsrc = (uint64)src << 32 | src;

	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8, mask_stride, mask_line, 1);

	vsrc = load8888 (&src);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	CHECKPOINT ();

	while (w && (uintptr_t)dst & 7)
	{
		uint64 m = *mask;

		if (m)
		{
		__m64 vdest = in (vsrc, expand_alpha_rev (to_m64 (m)));

		store8888 (dst, vdest);
		}
		else
		{
		*dst = 0;
		}

		w--;
		mask++;
		dst++;
	}

	CHECKPOINT ();

	while (w >= 2)
	{
		uint64 m0, m1;
		m0 = *mask;
		m1 = *(mask + 1);

		if (srca == 0xff && (m0 & m1) == 0xff)
		{
		*(uint64 *)dst = srcsrc;
		}
		else if (m0 | m1)
		{
		__m64 dest0, dest1;

		dest0 = in (vsrc, expand_alpha_rev (to_m64 (m0)));
		dest1 = in (vsrc, expand_alpha_rev (to_m64 (m1)));

		*(__m64 *)dst = pack8888 (dest0, dest1);
		}
		else
		{
		*(uint64 *)dst = 0;
		}

		mask += 2;
		dst += 2;
		w -= 2;
	}

	CHECKPOINT ();

	if (w)
	{
		uint64 m = *mask;

		if (m)
		{
		__m64 vdest = load8888 (dst);

		vdest = in (vsrc, expand_alpha_rev (to_m64 (m)));
		store8888 (dst, vdest);
		}
		else
		{
		*dst = 0;
		}
	}
	}

	_mm_empty ();
}

static void
mmx_composite_over_n_8_0565 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							 PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32 src, srca;
	uint16 *dst_line, *dst;
	uint8 *mask_line, *mask;
	int dst_stride, mask_stride;
	int32 w;
	__m64 vsrc, vsrca, tmp;
	__m64 srcsrcsrcsrc;

	CHECKPOINT ();

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	srca = src >> 24;
	if (src == 0)
	return;

	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint16, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8, mask_stride, mask_line, 1);

	vsrc = load8888 (&src);
	vsrca = expand_alpha (vsrc);

	tmp = pack_565 (vsrc, _mm_setzero_si64 (), 0);
	srcsrcsrcsrc = expand_alpha_rev (tmp);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	CHECKPOINT ();

	while (w && (uintptr_t)dst & 7)
	{
		uint64 m = *mask;

		if (m)
		{
		uint64 d = *dst;
		__m64 vd = to_m64 (d);
		__m64 vdest = in_over (
			vsrc, vsrca, expand_alpha_rev (to_m64 (m)), expand565 (vd, 0));

		vd = pack_565 (vdest, _mm_setzero_si64 (), 0);
		*dst = to_uint64 (vd);
		}

		w--;
		mask++;
		dst++;
	}

	CHECKPOINT ();

	while (w >= 4)
	{
		uint64 m0, m1, m2, m3;
		m0 = *mask;
		m1 = *(mask + 1);
		m2 = *(mask + 2);
		m3 = *(mask + 3);

		if (srca == 0xff && (m0 & m1 & m2 & m3) == 0xff)
		{
		*(__m64 *)dst = srcsrcsrcsrc;
		}
		else if (m0 | m1 | m2 | m3)
		{
		__m64 vdest = *(__m64 *)dst;
		__m64 v0, v1, v2, v3;
		__m64 vm0, vm1, vm2, vm3;

		expand_4x565 (vdest, &v0, &v1, &v2, &v3, 0);

		vm0 = to_m64 (m0);
		v0 = in_over (vsrc, vsrca, expand_alpha_rev (vm0), v0);

		vm1 = to_m64 (m1);
		v1 = in_over (vsrc, vsrca, expand_alpha_rev (vm1), v1);

		vm2 = to_m64 (m2);
		v2 = in_over (vsrc, vsrca, expand_alpha_rev (vm2), v2);

		vm3 = to_m64 (m3);
		v3 = in_over (vsrc, vsrca, expand_alpha_rev (vm3), v3);

		*(__m64 *)dst = pack_4x565 (v0, v1, v2, v3);;
		}

		w -= 4;
		mask += 4;
		dst += 4;
	}

	CHECKPOINT ();

	while (w)
	{
		uint64 m = *mask;

		if (m)
		{
		uint64 d = *dst;
		__m64 vd = to_m64 (d);
		__m64 vdest = in_over (vsrc, vsrca, expand_alpha_rev (to_m64 (m)),
					   expand565 (vd, 0));
		vd = pack_565 (vdest, _mm_setzero_si64 (), 0);
		*dst = to_uint64 (vd);
		}

		w--;
		mask++;
		dst++;
	}
	}

	_mm_empty ();
}

static void
mmx_composite_over_pixbuf_0565 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
								PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint16	*dst_line, *dst;
	uint32	*src_line, *src;
	int dst_stride, src_stride;
	int32 w;

	CHECKPOINT ();

	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint16, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (src_image, src_x, src_y, uint32, src_stride, src_line, 1);

#if 0
	/* FIXME */
	assert (src_image->drawable == mask_image->drawable);
#endif

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	CHECKPOINT ();

	while (w && (uintptr_t)dst & 7)
	{
		__m64 vsrc = load8888 (src);
		uint64 d = *dst;
		__m64 vdest = expand565 (to_m64 (d), 0);

		vdest = pack_565 (over_rev_non_pre (vsrc, vdest), vdest, 0);

		*dst = to_uint64 (vdest);

		w--;
		dst++;
		src++;
	}

	CHECKPOINT ();

	while (w >= 4)
	{
		uint32 s0, s1, s2, s3;
		unsigned char a0, a1, a2, a3;

		s0 = *src;
		s1 = *(src + 1);
		s2 = *(src + 2);
		s3 = *(src + 3);

		a0 = (s0 >> 24);
		a1 = (s1 >> 24);
		a2 = (s2 >> 24);
		a3 = (s3 >> 24);

		if ((a0 & a1 & a2 & a3) == 0xFF)
		{
		__m64 v0 = invert_colors (load8888 (&s0));
		__m64 v1 = invert_colors (load8888 (&s1));
		__m64 v2 = invert_colors (load8888 (&s2));
		__m64 v3 = invert_colors (load8888 (&s3));

		*(__m64 *)dst = pack_4x565 (v0, v1, v2, v3);
		}
		else if (s0 | s1 | s2 | s3)
		{
		__m64 vdest = *(__m64 *)dst;
		__m64 v0, v1, v2, v3;

		__m64 vsrc0 = load8888 (&s0);
		__m64 vsrc1 = load8888 (&s1);
		__m64 vsrc2 = load8888 (&s2);
		__m64 vsrc3 = load8888 (&s3);

		expand_4x565 (vdest, &v0, &v1, &v2, &v3, 0);

		v0 = over_rev_non_pre (vsrc0, v0);
		v1 = over_rev_non_pre (vsrc1, v1);
		v2 = over_rev_non_pre (vsrc2, v2);
		v3 = over_rev_non_pre (vsrc3, v3);

		*(__m64 *)dst = pack_4x565 (v0, v1, v2, v3);
		}

		w -= 4;
		dst += 4;
		src += 4;
	}

	CHECKPOINT ();

	while (w)
	{
		__m64 vsrc = load8888 (src);
		uint64 d = *dst;
		__m64 vdest = expand565 (to_m64 (d), 0);

		vdest = pack_565 (over_rev_non_pre (vsrc, vdest), vdest, 0);

		*dst = to_uint64 (vdest);

		w--;
		dst++;
		src++;
	}
	}

	_mm_empty ();
}

static void
mmx_composite_over_pixbuf_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
								PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32	*dst_line, *dst;
	uint32	*src_line, *src;
	int dst_stride, src_stride;
	int32 w;

	CHECKPOINT ();

	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (src_image, src_x, src_y, uint32, src_stride, src_line, 1);

#if 0
	/* FIXME */
	assert (src_image->drawable == mask_image->drawable);
#endif

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w && (uintptr_t)dst & 7)
	{
		__m64 s = load8888 (src);
		__m64 d = load8888 (dst);

		store8888 (dst, over_rev_non_pre (s, d));

		w--;
		dst++;
		src++;
	}

	while (w >= 2)
	{
		uint32 s0, s1;
		unsigned char a0, a1;
		__m64 d0, d1;

		s0 = *src;
		s1 = *(src + 1);

		a0 = (s0 >> 24);
		a1 = (s1 >> 24);

		if ((a0 & a1) == 0xFF)
		{
		d0 = invert_colors (load8888 (&s0));
		d1 = invert_colors (load8888 (&s1));

		*(__m64 *)dst = pack8888 (d0, d1);
		}
		else if (s0 | s1)
		{
		__m64 vdest = *(__m64 *)dst;

		d0 = over_rev_non_pre (load8888 (&s0), expand8888 (vdest, 0));
		d1 = over_rev_non_pre (load8888 (&s1), expand8888 (vdest, 1));

		*(__m64 *)dst = pack8888 (d0, d1);
		}

		w -= 2;
		dst += 2;
		src += 2;
	}

	if (w)
	{
		__m64 s = load8888 (src);
		__m64 d = load8888 (dst);

		store8888 (dst, over_rev_non_pre (s, d));
	}
	}

	_mm_empty ();
}

static void
mmx_composite_over_n_8888_0565_ca (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
								   PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32 src;
	uint16	*dst_line;
	uint32	*mask_line;
	int dst_stride, mask_stride;
	__m64 vsrc, vsrca;

	CHECKPOINT ();

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	if (src == 0)
	return;

	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint16, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint32, mask_stride, mask_line, 1);

	vsrc = load8888 (&src);
	vsrca = expand_alpha (vsrc);

	while (height--)
	{
	int twidth = width;
	uint32 *p = (uint32 *)mask_line;
	uint16 *q = (uint16 *)dst_line;

	while (twidth && ((uintptr_t)q & 7))
	{
		uint32 m = *(uint32 *)p;

		if (m)
		{
		uint64 d = *q;
		__m64 vdest = expand565 (to_m64 (d), 0);
		vdest = pack_565 (in_over (vsrc, vsrca, load8888 (&m), vdest), vdest, 0);
		*q = to_uint64 (vdest);
		}

		twidth--;
		p++;
		q++;
	}

	while (twidth >= 4)
	{
		uint32 m0, m1, m2, m3;

		m0 = *p;
		m1 = *(p + 1);
		m2 = *(p + 2);
		m3 = *(p + 3);

		if ((m0 | m1 | m2 | m3))
		{
		__m64 vdest = *(__m64 *)q;
		__m64 v0, v1, v2, v3;

		expand_4x565 (vdest, &v0, &v1, &v2, &v3, 0);

		v0 = in_over (vsrc, vsrca, load8888 (&m0), v0);
		v1 = in_over (vsrc, vsrca, load8888 (&m1), v1);
		v2 = in_over (vsrc, vsrca, load8888 (&m2), v2);
		v3 = in_over (vsrc, vsrca, load8888 (&m3), v3);

		*(__m64 *)q = pack_4x565 (v0, v1, v2, v3);
		}
		twidth -= 4;
		p += 4;
		q += 4;
	}

	while (twidth)
	{
		uint32 m;

		m = *(uint32 *)p;
		if (m)
		{
		uint64 d = *q;
		__m64 vdest = expand565 (to_m64 (d), 0);
		vdest = pack_565 (in_over (vsrc, vsrca, load8888 (&m), vdest), vdest, 0);
		*q = to_uint64 (vdest);
		}

		twidth--;
		p++;
		q++;
	}

	mask_line += mask_stride;
	dst_line += dst_stride;
	}

	_mm_empty ();
}

static void
mmx_composite_in_n_8_8 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
						PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint8 *dst_line, *dst;
	uint8 *mask_line, *mask;
	int dst_stride, mask_stride;
	int32 w;
	uint32 src;
	uint8 sa;
	__m64 vsrc, vsrca;

	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint8, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8, mask_stride, mask_line, 1);

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	sa = src >> 24;

	vsrc = load8888 (&src);
	vsrca = expand_alpha (vsrc);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w && (uintptr_t)dst & 7)
	{
		uint16 tmp;
		uint8 a;
		uint32 m, d;

		a = *mask++;
		d = *dst;

		m = MUL_UN8 (sa, a, tmp);
		d = MUL_UN8 (m, d, tmp);

		*dst++ = d;
		w--;
	}

	while (w >= 4)
	{
		__m64 vmask;
		__m64 vdest;

		vmask = load8888u ((uint32 *)mask);
		vdest = load8888 ((uint32 *)dst);

		store8888 ((uint32 *)dst, in (in (vsrca, vmask), vdest));

		dst += 4;
		mask += 4;
		w -= 4;
	}

	while (w--)
	{
		uint16 tmp;
		uint8 a;
		uint32 m, d;

		a = *mask++;
		d = *dst;

		m = MUL_UN8 (sa, a, tmp);
		d = MUL_UN8 (m, d, tmp);

		*dst++ = d;
	}
	}

	_mm_empty ();
}

static void
mmx_composite_in_8_8 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
					  PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint8	 *dst_line, *dst;
	uint8	 *src_line, *src;
	int src_stride, dst_stride;
	int32 w;

	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint8, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (src_image, src_x, src_y, uint8, src_stride, src_line, 1);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w && (uintptr_t)dst & 3)
	{
		uint8 s, d;
		uint16 tmp;

		s = *src;
		d = *dst;

		*dst = MUL_UN8 (s, d, tmp);

		src++;
		dst++;
		w--;
	}

	while (w >= 4)
	{
		uint32 *s = (uint32 *)src;
		uint32 *d = (uint32 *)dst;

		store8888 (d, in (load8888u (s), load8888 (d)));

		w -= 4;
		dst += 4;
		src += 4;
	}

	while (w--)
	{
		uint8 s, d;
		uint16 tmp;

		s = *src;
		d = *dst;

		*dst = MUL_UN8 (s, d, tmp);

		src++;
		dst++;
	}
	}

	_mm_empty ();
}

static void
mmx_composite_add_n_8_8 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
			 PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint8	 *dst_line, *dst;
	uint8	 *mask_line, *mask;
	int dst_stride, mask_stride;
	int32 w;
	uint32 src;
	uint8 sa;
	__m64 vsrc, vsrca;

	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint8, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8, mask_stride, mask_line, 1);

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	sa = src >> 24;

	if (src == 0)
	return;

	vsrc = load8888 (&src);
	vsrca = expand_alpha (vsrc);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w && (uintptr_t)dst & 3)
	{
		uint16 tmp;
		uint16 a;
		uint32 m, d;
		uint32 r;

		a = *mask++;
		d = *dst;

		m = MUL_UN8 (sa, a, tmp);
		r = ADD_UN8 (m, d, tmp);

		*dst++ = r;
		w--;
	}

	while (w >= 4)
	{
		__m64 vmask;
		__m64 vdest;

		vmask = load8888u ((uint32 *)mask);
		vdest = load8888 ((uint32 *)dst);

		store8888 ((uint32 *)dst, _mm_adds_pu8 (in (vsrca, vmask), vdest));

		dst += 4;
		mask += 4;
		w -= 4;
	}

	while (w--)
	{
		uint16 tmp;
		uint16 a;
		uint32 m, d;
		uint32 r;

		a = *mask++;
		d = *dst;

		m = MUL_UN8 (sa, a, tmp);
		r = ADD_UN8 (m, d, tmp);

		*dst++ = r;
	}
	}

	_mm_empty ();
}

static void
mmx_composite_add_8_8 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
			   PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint8 *dst_line, *dst;
	uint8 *src_line, *src;
	int dst_stride, src_stride;
	int32 w;
	uint8 s, d;
	uint16 t;

	CHECKPOINT ();

	IMAGE_GET_LINE (src_image, src_x, src_y, uint8, src_stride, src_line, 1);
	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint8, dst_stride, dst_line, 1);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w && (uintptr_t)dst & 7)
	{
		s = *src;
		d = *dst;
		t = d + s;
		s = t | (0 - (t >> 8));
		*dst = s;

		dst++;
		src++;
		w--;
	}

	while (w >= 8)
	{
		*(__m64*)dst = _mm_adds_pu8 (ldq_u ((__m64 *)src), *(__m64*)dst);
		dst += 8;
		src += 8;
		w -= 8;
	}

	while (w)
	{
		s = *src;
		d = *dst;
		t = d + s;
		s = t | (0 - (t >> 8));
		*dst = s;

		dst++;
		src++;
		w--;
	}
	}

	_mm_empty ();
}

static void
mmx_composite_add_0565_0565 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							 PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint16	*dst_line, *dst;
	uint32	d;
	uint16	*src_line, *src;
	uint32	s;
	int dst_stride, src_stride;
	int32 w;

	CHECKPOINT ();

	IMAGE_GET_LINE (src_image, src_x, src_y, uint16, src_stride, src_line, 1);
	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint16, dst_stride, dst_line, 1);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w && (uintptr_t)dst & 7)
	{
		s = *src++;
		if (s)
		{
		d = *dst;
		s = convert_0565_to_8888 (s);
		if (d)
		{
			d = convert_0565_to_8888 (d);
			UN8x4_ADD_UN8x4 (s, d);
		}
		*dst = convert_8888_to_0565 (s);
		}
		dst++;
		w--;
	}

	while (w >= 4)
	{
		__m64 vdest = *(__m64 *)dst;
		__m64 vsrc = ldq_u ((__m64 *)src);
		__m64 vd0, vd1;
		__m64 vs0, vs1;

		expand_4xpacked565 (vdest, &vd0, &vd1, 0);
		expand_4xpacked565 (vsrc, &vs0, &vs1, 0);

		vd0 = _mm_adds_pu8 (vd0, vs0);
		vd1 = _mm_adds_pu8 (vd1, vs1);

		*(__m64 *)dst = pack_4xpacked565 (vd0, vd1);

		dst += 4;
		src += 4;
		w -= 4;
	}

	while (w--)
	{
		s = *src++;
		if (s)
		{
		d = *dst;
		s = convert_0565_to_8888 (s);
		if (d)
		{
			d = convert_0565_to_8888 (d);
			UN8x4_ADD_UN8x4 (s, d);
		}
		*dst = convert_8888_to_0565 (s);
		}
		dst++;
	}
	}

	_mm_empty ();
}

static void
mmx_composite_add_8888_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
							 PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32	*dst_line, *dst;
	uint32	*src_line, *src;
	int dst_stride, src_stride;
	int32 w;

	CHECKPOINT ();

	IMAGE_GET_LINE (src_image, src_x, src_y, uint32, src_stride, src_line, 1);
	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w && (uintptr_t)dst & 7)
	{
		store (dst, _mm_adds_pu8 (load ((const uint32 *)src),
								  load ((const uint32 *)dst)));
		dst++;
		src++;
		w--;
	}

	while (w >= 2)
	{
		*(__m64 *)dst = _mm_adds_pu8 (ldq_u ((__m64 *)src), *(__m64*)dst);
		dst += 2;
		src += 2;
		w -= 2;
	}

	if (w)
	{
		store (dst, _mm_adds_pu8 (load ((const uint32 *)src),
								  load ((const uint32 *)dst)));

	}
	}

	_mm_empty ();
}

static int
mmx_blt (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
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
	src_bytes = (uint8 *)(((uint16 *)src_bits) + src_stride * (src_y) + (src_x));
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

	if (w >= 1 && ((uintptr_t)d & 1))
	{
		*(uint8 *)d = *(uint8 *)s;
		w -= 1;
		s += 1;
		d += 1;
	}

	if (w >= 2 && ((uintptr_t)d & 3))
	{
		*(uint16 *)d = *(uint16 *)s;
		w -= 2;
		s += 2;
		d += 2;
	}

	while (w >= 4 && ((uintptr_t)d & 7))
	{
		*(uint32 *)d = ldl_u ((uint32 *)s);

		w -= 4;
		s += 4;
		d += 4;
	}

	while (w >= 64)
	{
#if (defined (__GNUC__) || (defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590))) && defined USE_X86_MMX
		__asm__ (
			"movq	  (%1),	  %%mm0\n"
			"movq	 8(%1),	  %%mm1\n"
			"movq	16(%1),	  %%mm2\n"
			"movq	24(%1),	  %%mm3\n"
			"movq	32(%1),	  %%mm4\n"
			"movq	40(%1),	  %%mm5\n"
			"movq	48(%1),	  %%mm6\n"
			"movq	56(%1),	  %%mm7\n"

			"movq	%%mm0,	  (%0)\n"
			"movq	%%mm1,	 8(%0)\n"
			"movq	%%mm2,	16(%0)\n"
			"movq	%%mm3,	24(%0)\n"
			"movq	%%mm4,	32(%0)\n"
			"movq	%%mm5,	40(%0)\n"
			"movq	%%mm6,	48(%0)\n"
			"movq	%%mm7,	56(%0)\n"
		:
		: "r" (d), "r" (s)
		: "memory",
		  "%mm0", "%mm1", "%mm2", "%mm3",
		  "%mm4", "%mm5", "%mm6", "%mm7");
#else
		__m64 v0 = ldq_u ((__m64 *)(s + 0));
		__m64 v1 = ldq_u ((__m64 *)(s + 8));
		__m64 v2 = ldq_u ((__m64 *)(s + 16));
		__m64 v3 = ldq_u ((__m64 *)(s + 24));
		__m64 v4 = ldq_u ((__m64 *)(s + 32));
		__m64 v5 = ldq_u ((__m64 *)(s + 40));
		__m64 v6 = ldq_u ((__m64 *)(s + 48));
		__m64 v7 = ldq_u ((__m64 *)(s + 56));
		*(__m64 *)(d + 0)  = v0;
		*(__m64 *)(d + 8)  = v1;
		*(__m64 *)(d + 16) = v2;
		*(__m64 *)(d + 24) = v3;
		*(__m64 *)(d + 32) = v4;
		*(__m64 *)(d + 40) = v5;
		*(__m64 *)(d + 48) = v6;
		*(__m64 *)(d + 56) = v7;
#endif

		w -= 64;
		s += 64;
		d += 64;
	}
	while (w >= 4)
	{
		*(uint32 *)d = ldl_u ((uint32 *)s);

		w -= 4;
		s += 4;
		d += 4;
	}
	if (w >= 2)
	{
		*(uint16 *)d = *(uint16 *)s;
		w -= 2;
		s += 2;
		d += 2;
	}
	}

	_mm_empty ();

	return TRUE;
}

static void
mmx_composite_copy_area (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
						 PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);

	mmx_blt (imp, src_image->bits.bits,
		 dest_image->bits.bits,
		 src_image->bits.row_stride,
		 dest_image->bits.row_stride,
		 PIXEL_MANIPULATE_FORMAT_BPP (src_image->bits.format),
		 PIXEL_MANIPULATE_FORMAT_BPP (dest_image->bits.format),
		 src_x, src_y, dest_x, dest_y, width, height);
}

static void
mmx_composite_over_x888_8_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
								PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32  *src, *src_line;
	uint32  *dst, *dst_line;
	uint8  *mask, *mask_line;
	int src_stride, mask_stride, dst_stride;
	int32 w;

	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);
	IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8, mask_stride, mask_line, 1);
	IMAGE_GET_LINE (src_image, src_x, src_y, uint32, src_stride, src_line, 1);

	while (height--)
	{
	src = src_line;
	src_line += src_stride;
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;

	w = width;

	while (w--)
	{
		uint64 m = *mask;

		if (m)
		{
		uint32 ssrc = *src | 0xff000000;
		__m64 s = load8888 (&ssrc);

		if (m == 0xff)
		{
			store8888 (dst, s);
		}
		else
		{
			__m64 sa = expand_alpha (s);
			__m64 vm = expand_alpha_rev (to_m64 (m));
			__m64 vdest = in_over (s, sa, vm, load8888 (dst));

			store8888 (dst, vdest);
		}
		}

		mask++;
		dst++;
		src++;
	}
	}

	_mm_empty ();
}

static void
mmx_composite_over_reverse_n_8888 (PIXEL_MANIPULATE_IMPLEMENTATION *imp,
								   PIXEL_MANIPULATE_COMPOSITE_INFO *info)
{
	COMPOSITE_ARGS (info);
	uint32 src;
	uint32	*dst_line, *dst;
	int32 w;
	int dst_stride;
	__m64 vsrc;

	CHECKPOINT ();

	src = PixelManipulateImageGetSolid (imp, src_image, dest_image->bits.format);

	if (src == 0)
	return;

	IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32, dst_stride, dst_line, 1);

	vsrc = load8888 (&src);

	while (height--)
	{
	dst = dst_line;
	dst_line += dst_stride;
	w = width;

	CHECKPOINT ();

	while (w && (uintptr_t)dst & 7)
	{
		__m64 vdest = load8888 (dst);

		store8888 (dst, over (vdest, expand_alpha (vdest), vsrc));

		w--;
		dst++;
	}

	while (w >= 2)
	{
		__m64 vdest = *(__m64 *)dst;
		__m64 dest0 = expand8888 (vdest, 0);
		__m64 dest1 = expand8888 (vdest, 1);


		dest0 = over (dest0, expand_alpha (dest0), vsrc);
		dest1 = over (dest1, expand_alpha (dest1), vsrc);

		*(__m64 *)dst = pack8888 (dest0, dest1);

		dst += 2;
		w -= 2;
	}

	CHECKPOINT ();

	if (w)
	{
		__m64 vdest = load8888 (dst);

		store8888 (dst, over (vdest, expand_alpha (vdest), vsrc));
	}
	}

	_mm_empty ();
}

static FORCE_INLINE void
scaled_nearest_scanline_mmx_8888_8888_OVER (uint32*	   pd,
											const uint32* ps,
											int32		 w,
											PIXEL_MANIPULATE_FIXED  vx,
											PIXEL_MANIPULATE_FIXED  unit_x,
											PIXEL_MANIPULATE_FIXED  src_width_fixed,
											int   fully_transparent_src)
{
	if (fully_transparent_src)
	return;

	while (w)
	{
	__m64 d = load (pd);
	__m64 s = load (ps + PIXEL_MANIPULATE_FIXED_TO_INTEGER (vx));
	vx += unit_x;
	while (vx >= 0)
		vx -= src_width_fixed;

	store8888 (pd, core_combine_over_u_pixel_mmx (s, d));
	pd++;

	w--;
	}

	_mm_empty ();
}

FAST_NEAREST_MAINLOOP (mmx_8888_8888_cover_OVER,
			   scaled_nearest_scanline_mmx_8888_8888_OVER,
			   uint32, uint32, COVER)
FAST_NEAREST_MAINLOOP (mmx_8888_8888_none_OVER,
			   scaled_nearest_scanline_mmx_8888_8888_OVER,
			   uint32, uint32, NONE)
FAST_NEAREST_MAINLOOP (mmx_8888_8888_pad_OVER,
			   scaled_nearest_scanline_mmx_8888_8888_OVER,
			   uint32, uint32, PAD)
FAST_NEAREST_MAINLOOP (mmx_8888_8888_normal_OVER,
			   scaled_nearest_scanline_mmx_8888_8888_OVER,
			   uint32, uint32, NORMAL)

static FORCE_INLINE void
scaled_nearest_scanline_mmx_8888_n_8888_OVER (const uint32 * mask,
						  uint32 *	   dst,
						  const uint32 * src,
						  int32		  w,
						  PIXEL_MANIPULATE_FIXED   vx,
						  PIXEL_MANIPULATE_FIXED   unit_x,
						  PIXEL_MANIPULATE_FIXED   src_width_fixed,
						  int	zero_src)
{
	__m64 mm_mask;

	if (zero_src || (*mask >> 24) == 0)
	{
	/* A workaround for https://gcc.gnu.org/PR47759 */
	_mm_empty ();
	return;
	}

	mm_mask = expand_alpha (load8888 (mask));

	while (w)
	{
	uint32 s = *(src + PIXEL_MANIPULATE_FIXED_TO_INTEGER (vx));
	vx += unit_x;
	while (vx >= 0)
		vx -= src_width_fixed;

	if (s)
	{
		__m64 ms = load8888 (&s);
		__m64 alpha = expand_alpha (ms);
		__m64 dest  = load8888 (dst);

		store8888 (dst, (in_over (ms, alpha, mm_mask, dest)));
	}

	dst++;
	w--;
	}

	_mm_empty ();
}

#define BSHIFT ((1 << BILINEAR_INTERPOLATION_BITS))
#define BMSK (BSHIFT - 1)

#define BILINEAR_DECLARE_VARIABLES						\
	const __m64 mm_wt = _mm_set_pi16 (wt, wt, wt, wt);				\
	const __m64 mm_wb = _mm_set_pi16 (wb, wb, wb, wb);				\
	const __m64 mm_addc7 = _mm_set_pi16 (0, 1, 0, 1);				\
	const __m64 mm_xorc7 = _mm_set_pi16 (0, BMSK, 0, BMSK);			\
	const __m64 mm_ux = _mm_set_pi16 (unit_x, unit_x, unit_x, unit_x);		\
	const __m64 mm_zero = _mm_setzero_si64 ();					\
	__m64 mm_x = _mm_set_pi16 (vx, vx, vx, vx)

#define BILINEAR_INTERPOLATE_ONE_PIXEL(pix)					\
do {										\
	/* fetch 2x2 pixel block into 2 mmx registers */				\
	__m64 t = ldq_u ((__m64 *)&src_top [PIXEL_MANIPULATE_FIXED_TO_INTEGER (vx)]);		\
	__m64 b = ldq_u ((__m64 *)&src_bottom [PIXEL_MANIPULATE_FIXED_TO_INTEGER (vx)]);		\
	/* vertical interpolation */						\
	__m64 t_hi = _mm_mullo_pi16 (_mm_unpackhi_pi8 (t, mm_zero), mm_wt);		\
	__m64 t_lo = _mm_mullo_pi16 (_mm_unpacklo_pi8 (t, mm_zero), mm_wt);		\
	__m64 b_hi = _mm_mullo_pi16 (_mm_unpackhi_pi8 (b, mm_zero), mm_wb);		\
	__m64 b_lo = _mm_mullo_pi16 (_mm_unpacklo_pi8 (b, mm_zero), mm_wb);		\
	__m64 hi = _mm_add_pi16 (t_hi, b_hi);					\
	__m64 lo = _mm_add_pi16 (t_lo, b_lo);					\
	/* calculate horizontal weights */						\
	__m64 mm_wh = _mm_add_pi16 (mm_addc7, _mm_xor_si64 (mm_xorc7,		\
			  _mm_srli_pi16 (mm_x,					\
					 16 - BILINEAR_INTERPOLATION_BITS)));	\
	/* horizontal interpolation */						\
	__m64 p = _mm_unpacklo_pi16 (lo, hi);					\
	__m64 q = _mm_unpackhi_pi16 (lo, hi);					\
	vx += unit_x;								\
	lo = _mm_madd_pi16 (p, mm_wh);						\
	hi = _mm_madd_pi16 (q, mm_wh);						\
	mm_x = _mm_add_pi16 (mm_x, mm_ux);						\
	/* shift and pack the result */						\
	hi = _mm_srli_pi32 (hi, BILINEAR_INTERPOLATION_BITS * 2);			\
	lo = _mm_srli_pi32 (lo, BILINEAR_INTERPOLATION_BITS * 2);			\
	lo = _mm_packs_pi32 (lo, hi);						\
	lo = _mm_packs_pu16 (lo, lo);						\
	pix = lo;									\
} while (0)

#define BILINEAR_SKIP_ONE_PIXEL()						\
do {										\
	vx += unit_x;								\
	mm_x = _mm_add_pi16 (mm_x, mm_ux);						\
} while(0)

static FORCE_INLINE void
scaled_bilinear_scanline_mmx_8888_8888_SRC (uint32 *	   dst,
						const uint32 * mask,
						const uint32 * src_top,
						const uint32 * src_bottom,
						int32		  w,
						int			  wt,
						int			  wb,
						PIXEL_MANIPULATE_FIXED   vx,
						PIXEL_MANIPULATE_FIXED   unit_x,
						PIXEL_MANIPULATE_FIXED   max_vx,
						int	zero_src)
{
	BILINEAR_DECLARE_VARIABLES;
	__m64 pix;

	while (w--)
	{
	BILINEAR_INTERPOLATE_ONE_PIXEL (pix);
	store (dst, pix);
	dst++;
	}

	_mm_empty ();
}

FAST_NEAREST_MAINLOOP_COMMON(mmx_8888_n_8888_cover_OVER,
				  scaled_nearest_scanline_mmx_8888_n_8888_OVER,
				  uint32, uint32, uint32, COVER, TRUE, TRUE)
FAST_NEAREST_MAINLOOP_COMMON(mmx_8888_n_8888_pad_OVER,
				  scaled_nearest_scanline_mmx_8888_n_8888_OVER,
				  uint32, uint32, uint32, PAD, TRUE, TRUE)
FAST_NEAREST_MAINLOOP_COMMON(mmx_8888_n_8888_none_OVER,
				  scaled_nearest_scanline_mmx_8888_n_8888_OVER,
				  uint32, uint32, uint32, NONE, TRUE, TRUE)
FAST_NEAREST_MAINLOOP_COMMON(mmx_8888_n_8888_normal_OVER,
				  scaled_nearest_scanline_mmx_8888_n_8888_OVER,
				  uint32, uint32, uint32, NORMAL, TRUE, TRUE)

static FORCE_INLINE void
scaled_bilinear_scanline_mmx_8888_8888_OVER (uint32 *	   dst,
						 const uint32 * mask,
						 const uint32 * src_top,
						 const uint32 * src_bottom,
						 int32		  w,
						 int			  wt,
						 int			  wb,
						 PIXEL_MANIPULATE_FIXED   vx,
						 PIXEL_MANIPULATE_FIXED   unit_x,
						 PIXEL_MANIPULATE_FIXED   max_vx,
						 int	zero_src)
{
	BILINEAR_DECLARE_VARIABLES;
	__m64 pix1, pix2;

	while (w)
	{
	BILINEAR_INTERPOLATE_ONE_PIXEL (pix1);

	if (!is_zero (pix1))
	{
		pix2 = load (dst);
		store8888 (dst, core_combine_over_u_pixel_mmx (pix1, pix2));
	}

	w--;
	dst++;
	}

	_mm_empty ();
}

FAST_BILINEAR_MAINLOOP_COMMON(mmx_8888_8888_cover_SRC,
				   scaled_bilinear_scanline_mmx_8888_8888_SRC,
				   uint32, uint32, uint32,
				   COVER, FLAG_NONE)
FAST_BILINEAR_MAINLOOP_COMMON(mmx_8888_8888_pad_SRC,
				   scaled_bilinear_scanline_mmx_8888_8888_SRC,
				   uint32, uint32, uint32,
				   PAD, FLAG_NONE)
FAST_BILINEAR_MAINLOOP_COMMON(mmx_8888_8888_none_SRC,
				   scaled_bilinear_scanline_mmx_8888_8888_SRC,
				   uint32, uint32, uint32,
				   NONE, FLAG_NONE)
FAST_BILINEAR_MAINLOOP_COMMON(mmx_8888_8888_normal_SRC,
				   scaled_bilinear_scanline_mmx_8888_8888_SRC,
				   uint32, uint32, uint32,
				   NORMAL, FLAG_NONE)

static FORCE_INLINE void
scaled_bilinear_scanline_mmx_8888_8_8888_OVER (uint32 *	   dst,
						   const uint8  * mask,
						   const uint32 * src_top,
						   const uint32 * src_bottom,
						   int32		  w,
						   int			  wt,
						   int			  wb,
						   PIXEL_MANIPULATE_FIXED   vx,
						   PIXEL_MANIPULATE_FIXED   unit_x,
						   PIXEL_MANIPULATE_FIXED   max_vx,
						   int	zero_src)
{
	BILINEAR_DECLARE_VARIABLES;
	__m64 pix1, pix2;
	uint32 m;

	while (w)
	{
	m = (uint32) *mask++;

	if (m)
	{
		BILINEAR_INTERPOLATE_ONE_PIXEL (pix1);

		if (m == 0xff && is_opaque (pix1))
		{
		store (dst, pix1);
		}
		else
		{
		__m64 ms, md, ma, msa;

		pix2 = load (dst);
		ma = expand_alpha_rev (to_m64 (m));
		ms = _mm_unpacklo_pi8 (pix1, _mm_setzero_si64 ());
		md = _mm_unpacklo_pi8 (pix2, _mm_setzero_si64 ());

		msa = expand_alpha (ms);

		store8888 (dst, (in_over (ms, msa, ma, md)));
		}
	}
	else
	{
		BILINEAR_SKIP_ONE_PIXEL ();
	}

	w--;
	dst++;
	}

	_mm_empty ();
}

FAST_BILINEAR_MAINLOOP_COMMON (mmx_8888_8888_cover_OVER,
				   scaled_bilinear_scanline_mmx_8888_8888_OVER,
				   uint32, uint32, uint32,
				   COVER, FLAG_NONE)
FAST_BILINEAR_MAINLOOP_COMMON (mmx_8888_8888_pad_OVER,
				   scaled_bilinear_scanline_mmx_8888_8888_OVER,
				   uint32, uint32, uint32,
				   PAD, FLAG_NONE)
FAST_BILINEAR_MAINLOOP_COMMON (mmx_8888_8888_none_OVER,
				   scaled_bilinear_scanline_mmx_8888_8888_OVER,
				   uint32, uint32, uint32,
				   NONE, FLAG_NONE)
FAST_BILINEAR_MAINLOOP_COMMON (mmx_8888_8888_normal_OVER,
				   scaled_bilinear_scanline_mmx_8888_8888_OVER,
				   uint32, uint32, uint32,
				   NORMAL, FLAG_NONE)

static uint32* mmx_fetch_x8R8G8B8 (PIXEL_MANIPULATE_ITERATION* iter, const uint32* mask)
{
	int w = iter->width;
	uint32 *dst = iter->buffer;
	uint32 *src = (uint32 *)iter->bits;

	iter->bits += iter->stride;

	while (w && ((uintptr_t)dst) & 7)
	{
	*dst++ = (*src++) | 0xff000000;
	w--;
	}

	while (w >= 8)
	{
	__m64 vsrc1 = ldq_u ((__m64 *)(src + 0));
	__m64 vsrc2 = ldq_u ((__m64 *)(src + 2));
	__m64 vsrc3 = ldq_u ((__m64 *)(src + 4));
	__m64 vsrc4 = ldq_u ((__m64 *)(src + 6));

	*(__m64 *)(dst + 0) = _mm_or_si64 (vsrc1, MC (ff000000));
	*(__m64 *)(dst + 2) = _mm_or_si64 (vsrc2, MC (ff000000));
	*(__m64 *)(dst + 4) = _mm_or_si64 (vsrc3, MC (ff000000));
	*(__m64 *)(dst + 6) = _mm_or_si64 (vsrc4, MC (ff000000));

	dst += 8;
	src += 8;
	w -= 8;
	}

	while (w)
	{
	*dst++ = (*src++) | 0xff000000;
	w--;
	}

	_mm_empty ();
	return iter->buffer;
}

static uint32 *
mmx_fetch_R5G6B5 (PIXEL_MANIPULATE_ITERATION* iter, const uint32 *mask)
{
	int w = iter->width;
	uint32 *dst = iter->buffer;
	uint16 *src = (uint16 *)iter->bits;

	iter->bits += iter->stride;

	while (w && ((uintptr_t)dst) & 0x0f)
	{
	uint16 s = *src++;

	*dst++ = convert_0565_to_8888 (s);
	w--;
	}

	while (w >= 4)
	{
	__m64 vsrc = ldq_u ((__m64 *)src);
	__m64 mm0, mm1;

	expand_4xpacked565 (vsrc, &mm0, &mm1, 1);

	*(__m64 *)(dst + 0) = mm0;
	*(__m64 *)(dst + 2) = mm1;

	dst += 4;
	src += 4;
	w -= 4;
	}

	while (w)
	{
	uint16 s = *src++;

	*dst++ = convert_0565_to_8888 (s);
	w--;
	}

	_mm_empty ();
	return iter->buffer;
}

FAST_BILINEAR_MAINLOOP_COMMON (mmx_8888_8_8888_cover_OVER,
				   scaled_bilinear_scanline_mmx_8888_8_8888_OVER,
				   uint32, uint8, uint32,
				   COVER, FLAG_HAVE_NON_SOLID_MASK)
FAST_BILINEAR_MAINLOOP_COMMON (mmx_8888_8_8888_pad_OVER,
				   scaled_bilinear_scanline_mmx_8888_8_8888_OVER,
				   uint32, uint8, uint32,
				   PAD, FLAG_HAVE_NON_SOLID_MASK)
FAST_BILINEAR_MAINLOOP_COMMON (mmx_8888_8_8888_none_OVER,
				   scaled_bilinear_scanline_mmx_8888_8_8888_OVER,
				   uint32, uint8, uint32,
				   NONE, FLAG_HAVE_NON_SOLID_MASK)
FAST_BILINEAR_MAINLOOP_COMMON (mmx_8888_8_8888_normal_OVER,
				   scaled_bilinear_scanline_mmx_8888_8_8888_OVER,
				   uint32, uint8, uint32,
				   NORMAL, FLAG_HAVE_NON_SOLID_MASK)

static uint32 *
mmx_fetch_A8 (PIXEL_MANIPULATE_ITERATION* iter, const uint32* mask)
{
	int w = iter->width;
	uint32 *dst = iter->buffer;
	uint8 *src = iter->bits;

	iter->bits += iter->stride;

	while (w && (((uintptr_t)dst) & 15))
	{
		*dst++ = (uint32)*(src++) << 24;
		w--;
	}

	while (w >= 8)
	{
	__m64 mm0 = ldq_u ((__m64 *)src);

	__m64 mm1 = _mm_unpacklo_pi8  (_mm_setzero_si64(), mm0);
	__m64 mm2 = _mm_unpackhi_pi8  (_mm_setzero_si64(), mm0);
	__m64 mm3 = _mm_unpacklo_pi16 (_mm_setzero_si64(), mm1);
	__m64 mm4 = _mm_unpackhi_pi16 (_mm_setzero_si64(), mm1);
	__m64 mm5 = _mm_unpacklo_pi16 (_mm_setzero_si64(), mm2);
	__m64 mm6 = _mm_unpackhi_pi16 (_mm_setzero_si64(), mm2);

	*(__m64 *)(dst + 0) = mm3;
	*(__m64 *)(dst + 2) = mm4;
	*(__m64 *)(dst + 4) = mm5;
	*(__m64 *)(dst + 6) = mm6;

	dst += 8;
	src += 8;
	w -= 8;
	}

	while (w)
	{
	*dst++ = (uint32)*(src++) << 24;
	w--;
	}

	_mm_empty ();
	return iter->buffer;
}

#define IMAGE_FLAGS							\
	(FAST_PATH_STANDARD_FLAGS | FAST_PATH_ID_TRANSFORM |		\
	 FAST_PATH_BITS_IMAGE | FAST_PATH_SAMPLES_COVER_CLIP_NEAREST)

static const PIXEL_MANIPULATE_ITERATION_INFO mmx_iters[] =
{
	{ PIXEL_MANIPULATE_FORMAT_X8R8G8B8, IMAGE_FLAGS, PIXEL_MANIPULATE_ITERATION_NARROW,
	  PixelManipulateIteraterationInitializeBitsStride, mmx_fetch_x8R8G8B8, NULL
	},
	{ PIXEL_MANIPULATE_FORMAT_R5G6B5, IMAGE_FLAGS, PIXEL_MANIPULATE_ITERATION_NARROW,
	  PixelManipulateIteraterationInitializeBitsStride, mmx_fetch_R5G6B5, NULL
	},
	{ PIXEL_MANIPULATE_FORMAT_A8, IMAGE_FLAGS, PIXEL_MANIPULATE_ITERATION_NARROW,
	  PixelManipulateIteraterationInitializeBitsStride, mmx_fetch_A8, NULL
	},
	{ 0 },
};

static const PIXEL_MANIPULATE_FAST_PATH mmx_fast_paths[] =
{
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, SOLID,	FORMAT_A8,	   FORMAT_R5G6B5,   mmx_composite_over_n_8_0565	   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, SOLID,	FORMAT_A8,	   FORMAT_B5G6R5,   mmx_composite_over_n_8_0565	   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, SOLID,	FORMAT_A8,	   FORMAT_A8R8G8B8, mmx_composite_over_n_8_8888	   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, SOLID,	FORMAT_A8,	   FORMAT_X8R8G8B8, mmx_composite_over_n_8_8888	   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, SOLID,	FORMAT_A8,	   FORMAT_A8B8G8R8, mmx_composite_over_n_8_8888	   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, SOLID,	FORMAT_A8,	   FORMAT_X8B8G8R8, mmx_composite_over_n_8_8888	   ),
	PIXEL_MANIPULATE_STD_FAST_PATH_CA (OVER, SOLID,	FORMAT_A8R8G8B8, FORMAT_A8R8G8B8, mmx_composite_over_n_8888_8888_ca ),
	PIXEL_MANIPULATE_STD_FAST_PATH_CA (OVER, SOLID,	FORMAT_A8R8G8B8, FORMAT_X8R8G8B8, mmx_composite_over_n_8888_8888_ca ),
	PIXEL_MANIPULATE_STD_FAST_PATH_CA (OVER, SOLID,	FORMAT_A8R8G8B8, FORMAT_R5G6B5,   mmx_composite_over_n_8888_0565_ca ),
	PIXEL_MANIPULATE_STD_FAST_PATH_CA (OVER, SOLID,	FORMAT_A8B8G8R8, FORMAT_A8B8G8R8, mmx_composite_over_n_8888_8888_ca ),
	PIXEL_MANIPULATE_STD_FAST_PATH_CA (OVER, SOLID,	FORMAT_A8B8G8R8, FORMAT_X8B8G8R8, mmx_composite_over_n_8888_8888_ca ),
	PIXEL_MANIPULATE_STD_FAST_PATH_CA (OVER, SOLID,	FORMAT_A8B8G8R8, FORMAT_B5G6R5,   mmx_composite_over_n_8888_0565_ca ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, PIXBUF,   PIXBUF,   FORMAT_A8R8G8B8, mmx_composite_over_pixbuf_8888	),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, PIXBUF,   PIXBUF,   FORMAT_X8R8G8B8, mmx_composite_over_pixbuf_8888	),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, PIXBUF,   PIXBUF,   FORMAT_R5G6B5,   mmx_composite_over_pixbuf_0565	),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, RPIXBUF,  RPIXBUF,  FORMAT_A8B8G8R8, mmx_composite_over_pixbuf_8888	),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, RPIXBUF,  RPIXBUF,  FORMAT_X8B8G8R8, mmx_composite_over_pixbuf_8888	),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, RPIXBUF,  RPIXBUF,  FORMAT_B5G6R5,   mmx_composite_over_pixbuf_0565	),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, FORMAT_X8R8G8B8, SOLID,	FORMAT_A8R8G8B8, mmx_composite_over_x888_n_8888	),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, FORMAT_X8R8G8B8, SOLID,	FORMAT_X8R8G8B8, mmx_composite_over_x888_n_8888	),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, FORMAT_X8B8G8R8, SOLID,	FORMAT_A8B8G8R8, mmx_composite_over_x888_n_8888	),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, FORMAT_X8B8G8R8, SOLID,	FORMAT_X8B8G8R8, mmx_composite_over_x888_n_8888	),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, FORMAT_A8R8G8B8, SOLID,	FORMAT_A8R8G8B8, mmx_composite_over_8888_n_8888	),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, FORMAT_A8R8G8B8, SOLID,	FORMAT_X8R8G8B8, mmx_composite_over_8888_n_8888	),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, FORMAT_A8B8G8R8, SOLID,	FORMAT_A8B8G8R8, mmx_composite_over_8888_n_8888	),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, FORMAT_A8B8G8R8, SOLID,	FORMAT_X8B8G8R8, mmx_composite_over_8888_n_8888	),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, FORMAT_X8R8G8B8, FORMAT_A8,	   FORMAT_X8R8G8B8, mmx_composite_over_x888_8_8888	),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, FORMAT_X8R8G8B8, FORMAT_A8,	   FORMAT_A8R8G8B8, mmx_composite_over_x888_8_8888	),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, FORMAT_X8B8G8R8, FORMAT_A8,	   FORMAT_X8B8G8R8, mmx_composite_over_x888_8_8888	),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, FORMAT_X8B8G8R8, FORMAT_A8,	   FORMAT_A8B8G8R8, mmx_composite_over_x888_8_8888	),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, SOLID,	null,	 FORMAT_A8R8G8B8, mmx_composite_over_n_8888		 ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, SOLID,	null,	 FORMAT_X8R8G8B8, mmx_composite_over_n_8888		 ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, SOLID,	null,	 FORMAT_R5G6B5,   mmx_composite_over_n_0565		 ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, SOLID,	null,	 FORMAT_B5G6R5,   mmx_composite_over_n_0565		 ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, FORMAT_X8R8G8B8, null,	 FORMAT_X8R8G8B8, mmx_composite_copy_area		   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, FORMAT_X8B8G8R8, null,	 FORMAT_X8B8G8R8, mmx_composite_copy_area		   ),

	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, FORMAT_A8R8G8B8, null,	 FORMAT_A8R8G8B8, mmx_composite_over_8888_8888	  ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, FORMAT_A8R8G8B8, null,	 FORMAT_X8R8G8B8, mmx_composite_over_8888_8888	  ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, FORMAT_A8R8G8B8, null,	 FORMAT_R5G6B5,   mmx_composite_over_8888_0565	  ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, FORMAT_A8B8G8R8, null,	 FORMAT_A8B8G8R8, mmx_composite_over_8888_8888	  ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, FORMAT_A8B8G8R8, null,	 FORMAT_X8B8G8R8, mmx_composite_over_8888_8888	  ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER, FORMAT_A8B8G8R8, null,	 FORMAT_B5G6R5,   mmx_composite_over_8888_0565	  ),

	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER_REVERSE, SOLID, null, FORMAT_A8R8G8B8, mmx_composite_over_reverse_n_8888),
	PIXEL_MANIPULATE_STD_FAST_PATH	(OVER_REVERSE, SOLID, null, FORMAT_A8B8G8R8, mmx_composite_over_reverse_n_8888),

	PIXEL_MANIPULATE_STD_FAST_PATH	(ADD,  FORMAT_R5G6B5,   null,	 FORMAT_R5G6B5,   mmx_composite_add_0565_0565	   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(ADD,  FORMAT_B5G6R5,   null,	 FORMAT_B5G6R5,   mmx_composite_add_0565_0565	   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(ADD,  FORMAT_A8R8G8B8, null,	 FORMAT_A8R8G8B8, mmx_composite_add_8888_8888	   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(ADD,  FORMAT_A8B8G8R8, null,	 FORMAT_A8B8G8R8, mmx_composite_add_8888_8888	   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(ADD,  FORMAT_A8,	   null,	 FORMAT_A8,	   mmx_composite_add_8_8		   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(ADD,  SOLID,	FORMAT_A8,	   FORMAT_A8,	   mmx_composite_add_n_8_8		   ),

	PIXEL_MANIPULATE_STD_FAST_PATH	(SRC,  FORMAT_A8R8G8B8, null,	 FORMAT_R5G6B5,   mmx_composite_src_x888_0565	   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(SRC,  FORMAT_A8B8G8R8, null,	 FORMAT_B5G6R5,   mmx_composite_src_x888_0565	   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(SRC,  FORMAT_X8R8G8B8, null,	 FORMAT_R5G6B5,   mmx_composite_src_x888_0565	   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(SRC,  FORMAT_X8B8G8R8, null,	 FORMAT_B5G6R5,   mmx_composite_src_x888_0565	   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(SRC,  SOLID,	FORMAT_A8,	   FORMAT_A8R8G8B8, mmx_composite_src_n_8_8888		),
	PIXEL_MANIPULATE_STD_FAST_PATH	(SRC,  SOLID,	FORMAT_A8,	   FORMAT_X8R8G8B8, mmx_composite_src_n_8_8888		),
	PIXEL_MANIPULATE_STD_FAST_PATH	(SRC,  SOLID,	FORMAT_A8,	   FORMAT_A8B8G8R8, mmx_composite_src_n_8_8888		),
	PIXEL_MANIPULATE_STD_FAST_PATH	(SRC,  SOLID,	FORMAT_A8,	   FORMAT_X8B8G8R8, mmx_composite_src_n_8_8888		),
	PIXEL_MANIPULATE_STD_FAST_PATH	(SRC,  FORMAT_A8R8G8B8, null,	 FORMAT_A8R8G8B8, mmx_composite_copy_area		   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(SRC,  FORMAT_A8B8G8R8, null,	 FORMAT_A8B8G8R8, mmx_composite_copy_area		   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(SRC,  FORMAT_A8R8G8B8, null,	 FORMAT_X8R8G8B8, mmx_composite_copy_area		   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(SRC,  FORMAT_A8B8G8R8, null,	 FORMAT_X8B8G8R8, mmx_composite_copy_area		   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(SRC,  FORMAT_X8R8G8B8, null,	 FORMAT_X8R8G8B8, mmx_composite_copy_area		   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(SRC,  FORMAT_X8B8G8R8, null,	 FORMAT_X8B8G8R8, mmx_composite_copy_area		   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(SRC,  FORMAT_R5G6B5,   null,	 FORMAT_R5G6B5,   mmx_composite_copy_area		   ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(SRC,  FORMAT_B5G6R5,   null,	 FORMAT_B5G6R5,   mmx_composite_copy_area		   ),

	PIXEL_MANIPULATE_STD_FAST_PATH	(IN,   FORMAT_A8,	   null,	 FORMAT_A8,	   mmx_composite_in_8_8			  ),
	PIXEL_MANIPULATE_STD_FAST_PATH	(IN,   SOLID,	FORMAT_A8,	   FORMAT_A8,	   mmx_composite_in_n_8_8			),

	SIMPLE_NEAREST_FAST_PATH (OVER,   FORMAT_A8R8G8B8, FORMAT_X8R8G8B8, mmx_8888_8888							),
	SIMPLE_NEAREST_FAST_PATH (OVER,   FORMAT_A8B8G8R8, FORMAT_X8B8G8R8, mmx_8888_8888							),
	SIMPLE_NEAREST_FAST_PATH (OVER,   FORMAT_A8R8G8B8, FORMAT_A8R8G8B8, mmx_8888_8888							),
	SIMPLE_NEAREST_FAST_PATH (OVER,   FORMAT_A8B8G8R8, FORMAT_A8B8G8R8, mmx_8888_8888							),

	SIMPLE_NEAREST_SOLID_MASK_FAST_PATH (OVER, FORMAT_A8R8G8B8, FORMAT_A8R8G8B8, mmx_8888_n_8888				 ),
	SIMPLE_NEAREST_SOLID_MASK_FAST_PATH (OVER, FORMAT_A8B8G8R8, FORMAT_A8B8G8R8, mmx_8888_n_8888				 ),
	SIMPLE_NEAREST_SOLID_MASK_FAST_PATH (OVER, FORMAT_A8R8G8B8, FORMAT_X8R8G8B8, mmx_8888_n_8888				 ),
	SIMPLE_NEAREST_SOLID_MASK_FAST_PATH (OVER, FORMAT_A8B8G8R8, FORMAT_X8B8G8R8, mmx_8888_n_8888				 ),

	SIMPLE_BILINEAR_FAST_PATH (SRC, FORMAT_A8R8G8B8,		  FORMAT_A8R8G8B8, mmx_8888_8888					 ),
	SIMPLE_BILINEAR_FAST_PATH (SRC, FORMAT_A8R8G8B8,		  FORMAT_X8R8G8B8, mmx_8888_8888					 ),
	SIMPLE_BILINEAR_FAST_PATH (SRC, FORMAT_X8R8G8B8,		  FORMAT_X8R8G8B8, mmx_8888_8888					 ),
	SIMPLE_BILINEAR_FAST_PATH (SRC, FORMAT_A8B8G8R8,		  FORMAT_A8B8G8R8, mmx_8888_8888					 ),
	SIMPLE_BILINEAR_FAST_PATH (SRC, FORMAT_A8B8G8R8,		  FORMAT_X8B8G8R8, mmx_8888_8888					 ),
	SIMPLE_BILINEAR_FAST_PATH (SRC, FORMAT_X8B8G8R8,		  FORMAT_X8B8G8R8, mmx_8888_8888					 ),

	SIMPLE_BILINEAR_FAST_PATH (OVER, FORMAT_A8R8G8B8,		 FORMAT_X8R8G8B8, mmx_8888_8888					 ),
	SIMPLE_BILINEAR_FAST_PATH (OVER, FORMAT_A8B8G8R8,		 FORMAT_X8B8G8R8, mmx_8888_8888					 ),
	SIMPLE_BILINEAR_FAST_PATH (OVER, FORMAT_A8R8G8B8,		 FORMAT_A8R8G8B8, mmx_8888_8888					 ),
	SIMPLE_BILINEAR_FAST_PATH (OVER, FORMAT_A8B8G8R8,		 FORMAT_A8B8G8R8, mmx_8888_8888					 ),

	SIMPLE_BILINEAR_A8_MASK_FAST_PATH (OVER, FORMAT_A8R8G8B8, FORMAT_X8R8G8B8, mmx_8888_8_8888				   ),
	SIMPLE_BILINEAR_A8_MASK_FAST_PATH (OVER, FORMAT_A8B8G8R8, FORMAT_X8B8G8R8, mmx_8888_8_8888				   ),
	SIMPLE_BILINEAR_A8_MASK_FAST_PATH (OVER, FORMAT_A8R8G8B8, FORMAT_A8R8G8B8, mmx_8888_8_8888				   ),
	SIMPLE_BILINEAR_A8_MASK_FAST_PATH (OVER, FORMAT_A8B8G8R8, FORMAT_A8B8G8R8, mmx_8888_8_8888				   ),

	{ PIXEL_MANIPULATE_OPERATE_NONE },
};

PIXEL_MANIPULATE_IMPLEMENTATION*
	PixelManipulateImplementationCreateMMX(PIXEL_MANIPULATE_IMPLEMENTATION* fallback)
{
	PIXEL_MANIPULATE_IMPLEMENTATION *imp = PixelManipulateImplementationCreate(fallback, mmx_fast_paths);

	imp->combine32[PIXEL_MANIPULATE_OPERATE_OVER] = mmx_combine_over_u;
	imp->combine32[PIXEL_MANIPULATE_OPERATE_OVER_REVERSE] = mmx_combine_over_reverse_u;
	imp->combine32[PIXEL_MANIPULATE_OPERATE_IN] = mmx_combine_in_u;
	imp->combine32[PIXEL_MANIPULATE_OPERATE_IN_REVERSE] = mmx_combine_in_reverse_u;
	imp->combine32[PIXEL_MANIPULATE_OPERATE_OUT] = mmx_combine_out_u;
	imp->combine32[PIXEL_MANIPULATE_OPERATE_OUT_REVERSE] = mmx_combine_out_reverse_u;
	imp->combine32[PIXEL_MANIPULATE_OPERATE_ATOP] = mmx_combine_atOPERATE_u;
	imp->combine32[PIXEL_MANIPULATE_OPERATE_ATOP_REVERSE] = mmx_combine_atOPERATE_reverse_u;
	imp->combine32[PIXEL_MANIPULATE_OPERATE_XOR] = mmx_combine_xor_u;
	imp->combine32[PIXEL_MANIPULATE_OPERATE_ADD] = mmx_combine_add_u;
	imp->combine32[PIXEL_MANIPULATE_OPERATE_SATURATE] = mmx_combine_saturate_u;

	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_SRC] = mmx_combine_src_ca;
	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_OVER] = mmx_combine_over_ca;
	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_OVER_REVERSE] = mmx_combine_over_reverse_ca;
	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_IN] = mmx_combine_in_ca;
	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_IN_REVERSE] = mmx_combine_in_reverse_ca;
	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_OUT] = mmx_combine_out_ca;
	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_OUT_REVERSE] = mmx_combine_out_reverse_ca;
	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_ATOP] = mmx_combine_atOPERATE_ca;
	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_ATOP_REVERSE] = mmx_combine_atOPERATE_reverse_ca;
	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_XOR] = mmx_combine_xor_ca;
	imp->combine32_ca[PIXEL_MANIPULATE_OPERATE_ADD] = mmx_combine_add_ca;

	imp->block_transfer = mmx_blt;
	imp->fill = mmx_fill;

	imp->iteration_info = mmx_iters;

	return imp;
}

#ifdef __cplusplus
}
#endif

#endif // USE_X86_MMX || USE_ARM_IWMMXT || USE_LOONGSON_MMI
