#include "pixel_manipulate_matrix.h"
#include "pixel_manipulate_private.h"

#ifdef __cplusplus
extern "C" {
#endif

static INLINE void Fixed64_16_to_int128(
	int64 high,
	int64 low,
	int64* r_high,
	int64* r_low,
	int scale_bits
)
{
	high += low >> 16;
	low &= 0xFFFF;

	if(scale_bits <= 0)
	{
		*r_low = high >> (-scale_bits);
		*r_high = *r_low >> 63;
	}
	else
	{
		*r_high = high >> (64 - scale_bits);
		*r_low = (uint64)high << scale_bits;
		if(scale_bits < 16)
		{
			*r_low += low >> (16 - scale_bits);
		}
		else
		{
			*r_low += low << (scale_bits - 16);
		}
	}
}

static INLINE uint64 RoundedSdiv128_by_48(
	uint64 high,
	uint64 low,
	uint64 div,
	uint64 *result_high
)
{
	uint64 temp, remainder, result_low;

	remainder = high % div;
	*result_high = high / div;

	temp = (remainder << 16) + (low >> 48);
	result_low = temp / div;
	remainder = temp % div;

	temp = (remainder << 16) + ((low >> 32) & 0xFFFF);
	result_low = (result_low << 16) + (temp / div);
	remainder = temp % div;

	temp = (remainder << 16) + ((low >> 16) & 0xFFFF);
	result_low = (result_low << 16) + (temp / div);
	remainder = temp % div;

	temp = (remainder << 16) + (low & 0xFFFF);
	result_low = (result_low << 16) + (temp / div);
	remainder = temp % div;

	if(remainder * 2 >= div && ++result_low == 0)
	{
		*result_high += 1;
	}

	return result_low;
}

static INLINE int64 RoundedSdiv128_by_49(
	int64 high,
	uint64 low,
	int64 div,
	int64* signed_result_high
)
{
	uint64 result_low, result_high;
	int sign = 0;
	if(div < 0)
	{
		div = -div;
		sign ^= 1;
	}
	if(high < 0)
	{
		if(low != 0)
		{
			high++;
		}
		high = -high;
		low = - low;
		sign ^= 1;
	}
	result_low = RoundedSdiv128_by_48(high, low, div, &result_high);
	if(sign != 0)
	{
		if(result_low != 0)
		{
			result_high++;
		}
		result_high = -result_high;
		result_low = - result_low;
	}
	if(signed_result_high != NULL)
	{
		*signed_result_high = result_high;
	}
	return (int64)result_low;
}

static INLINE PIXEL_MANIPULATE_FIXED_48_16 Fixed112_16_to_Fixed48_16(int64 high, int64 low, int* clamp_flag)
{
	if((low >> 63) != high)
	{
		*clamp_flag = TRUE;
		return high >= 0 ? 0x7FFFFFFFFFFFFFFF : 0xFFFFFFFFFFFFFFFF;
	}
	return low;
}

#ifdef _MSC_VER
# include <intrin.h>
#endif

static INLINE int CountLeadingZeros(uint32 x)
{
#ifdef _MSC_VER
	unsigned long index;
	_BitScanReverse(&index, x);
	return (int)(31 - index);
#else
	return __builtin_clz (x);
#endif
}

int PixelManipulateTransform_31_16(
	const PIXEL_MANIPULATE_TRANSFORM* t,
	const PIXEL_MANIPULATE_VECTOR_48_16* v,
	PIXEL_MANIPULATE_VECTOR_48_16* result
)
{
	int clamp_flag = FALSE;
	int i;
	int64 temp[3][2], div_int;
	uint16 div_frac;

	ASSERT(v->v[0] < ((PIXEL_MANIPULATE_FIXED_48_16)1 << (30 + 16)));
	ASSERT(v->v[0] >= - ((PIXEL_MANIPULATE_FIXED_48_16)1 << (30 + 16)));
	ASSERT(v->v[1] < ((PIXEL_MANIPULATE_FIXED_48_16)1 << (30 + 16)));
	ASSERT(v->v[1] >= - ((PIXEL_MANIPULATE_FIXED_48_16)1 << (30 + 16)));
	ASSERT(v->v[2] < ((PIXEL_MANIPULATE_FIXED_48_16)1 << (30 + 16)));
	ASSERT(v->v[2] >= - ((PIXEL_MANIPULATE_FIXED_48_16)1 << (30 + 16)));

	for(i=0; i<3; i++)
	{
		temp[i][0] = (int64)t->matrix[i][0] * (v->v[0] >> 16);
		temp[i][1] = (int64)t->matrix[i][0] * (v->v[0] & 0xFFFF);
		temp[i][0] += (int64)t->matrix[i][1] * (v->v[1] >> 16);
		temp[i][1] += (int64)t->matrix[i][1] * (v->v[1] & 0xFFFF);
		temp[i][0] += (int64)t->matrix[i][2] * (v->v[2] >> 16);
		temp[i][1] += (int64)t->matrix[i][2] * (v->v[2] & 0xFFFF);
	}

	div_int = temp[2][0] + (temp[2][1] >> 16);
	div_frac = temp[2][1] & 0xFFFF;

	if(div_int == PIXEL_MANIPULATE_FIXED_1 && div_frac == 0)
	{
		result->v[0] = temp[0][0] + ((temp[0][1] + 0x8000) >> 16);
		result->v[1] = temp[1][0] + ((temp[1][1] + 0x8000) >> 16);
		result->v[2] = PIXEL_MANIPULATE_FIXED_1;
	}
	else if(div_int == 0 && div_frac == 0)
	{
		clamp_flag = TRUE;

		result->v[0] = temp[0][0] + ((temp[0][1] + 0x8000) >> 16);
		result->v[1] = temp[1][0] + ((temp[1][1] + 0x8000) >> 16);

		if(result->v[0] > 0)
		{
			result->v[0] = 0x7FFFFFFFFFFFFFFF;
		}
		else if(result->v[0] < 0)
		{
			result->v[0] = 0xFFFFFFFFFFFFFFFF;
		}

		if(result->v[1] > 0)
		{
			result->v[1] = 0x7FFFFFFFFFFFFFFF;
		}
		else if(result->v[1] < 0)
		{
			result->v[1] = 0xFFFFFFFFFFFFFFFF;
		}
	}
	else
	{
		int32 hi32div_bits = div_int >> 32;
		if(hi32div_bits < 0)
		{
			hi32div_bits = ~hi32div_bits;
		}

		if(hi32div_bits == 0)
		{
			int64 hi, rhi, lo, rlo;
			int64 div = ((uint64)div_int << 16) + div_frac;

			Fixed64_16_to_int128(temp[0][0], temp[0][1], &hi, &lo, 32);
			rlo = RoundedSdiv128_by_49(hi, lo, div, &rhi);
			result->v[0] = Fixed112_16_to_Fixed48_16(rhi, rlo, &clamp_flag);

			Fixed64_16_to_int128(temp[1][0], temp[1][1], &hi, &lo, 32);
			rlo = RoundedSdiv128_by_49(hi, lo, div, &rhi);
			result->v[1] = Fixed112_16_to_Fixed48_16(rhi, rlo, &clamp_flag);
		}
		else
		{
			int64 hi, rhi, lo, rlo, div;
			int shift = 32 - CountLeadingZeros(hi32div_bits);
			Fixed64_16_to_int128(div_int, div_frac, &hi, &div, 16 - shift);

			Fixed64_16_to_int128(temp[0][0], temp[0][1], &hi, &lo, 32- shift);
			rlo = RoundedSdiv128_by_49(hi, lo, div, &rhi);
			result->v[0] = Fixed112_16_to_Fixed48_16(rhi, rlo, &clamp_flag);

			Fixed64_16_to_int128(temp[1][0], temp[1][1], &hi, &lo, 32 - shift);
			rlo = RoundedSdiv128_by_49(hi, lo, div, &rhi);
			result->v[1] = Fixed112_16_to_Fixed48_16(rhi, rlo, &clamp_flag);
		}
	}
	result->v[2] = PIXEL_MANIPULATE_FIXED_1;
	return !clamp_flag;
}

int PixelManipulateTransformPoint(const PIXEL_MANIPULATE_TRANSFORM* transform, PIXEL_MANIPULATE_VECTOR* vector)
{
	PIXEL_MANIPULATE_VECTOR_48_16 temp;
	temp.v[0] = vector->vector[0];
	temp.v[1] = vector->vector[1];
	temp.v[2] = vector->vector[2];

	// pixman_transform_point_31_16
	if(FALSE == PixelManipulateTransform_31_16(transform, &temp, &temp))
	{
		return FALSE;
	}

	vector->vector[0] = temp.v[0];
	vector->vector[1] = temp.v[1];
	vector->vector[2] = temp.v[2];

	return vector->vector[0] == temp.v[0]
			&& vector->vector[1] == temp.v[1]
			&& vector->vector[2] == temp.v[2];
}

void PixelManipulateTransformPoint_31_16_3d(
	const PIXEL_MANIPULATE_TRANSFORM	*t,
	const PIXEL_MANIPULATE_VECTOR_48_16 *v,
	PIXEL_MANIPULATE_VECTOR_48_16	   *result
)
{
	int i;
	int64 tmp[3][2];

	/* input vector values must have no more than 31 bits (including sign)
	 * in the integer part */
	assert (v->v[0] <   ((PIXEL_MANIPULATE_FIXED_48_16)1 << (30 + 16)));
	assert (v->v[0] >= -((PIXEL_MANIPULATE_FIXED_48_16)1 << (30 + 16)));
	assert (v->v[1] <   ((PIXEL_MANIPULATE_FIXED_48_16)1 << (30 + 16)));
	assert (v->v[1] >= -((PIXEL_MANIPULATE_FIXED_48_16)1 << (30 + 16)));
	assert (v->v[2] <   ((PIXEL_MANIPULATE_FIXED_48_16)1 << (30 + 16)));
	assert (v->v[2] >= -((PIXEL_MANIPULATE_FIXED_48_16)1 << (30 + 16)));

	for (i = 0; i < 3; i++)
	{
		tmp[i][0] = (int64)t->matrix[i][0] * (v->v[0] >> 16);
		tmp[i][1] = (int64)t->matrix[i][0] * (v->v[0] & 0xFFFF);
		tmp[i][0] += (int64)t->matrix[i][1] * (v->v[1] >> 16);
		tmp[i][1] += (int64)t->matrix[i][1] * (v->v[1] & 0xFFFF);
		tmp[i][0] += (int64)t->matrix[i][2] * (v->v[2] >> 16);
		tmp[i][1] += (int64)t->matrix[i][2] * (v->v[2] & 0xFFFF);
	}

	result->v[0] = tmp[0][0] + ((tmp[0][1] + 0x8000) >> 16);
	result->v[1] = tmp[1][0] + ((tmp[1][1] + 0x8000) >> 16);
	result->v[2] = tmp[2][0] + ((tmp[2][1] + 0x8000) >> 16);
}

int PixelManipulateTransformPoint3d (
	const PIXEL_MANIPULATE_TRANSFORM* transform,
	PIXEL_MANIPULATE_VECTOR*		 vector
)
{
	PIXEL_MANIPULATE_VECTOR_48_16 tmp;
	tmp.v[0] = vector->vector[0];
	tmp.v[1] = vector->vector[1];
	tmp.v[2] = vector->vector[2];

	PixelManipulateTransformPoint_31_16_3d (transform, &tmp, &tmp);

	vector->vector[0] = tmp.v[0];
	vector->vector[1] = tmp.v[1];
	vector->vector[2] = tmp.v[2];

	return vector->vector[0] == tmp.v[0] &&
		   vector->vector[1] == tmp.v[1] &&
		   vector->vector[2] == tmp.v[2];
}

#ifdef __cplusplus
}
#endif
