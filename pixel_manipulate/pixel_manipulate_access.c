#include "pixel_manipulate.h"
#include "pixel_manipulate_format.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DIV_PIXEL ((FLOAT_T)0.00392157)

static uint32 to_linear[256] =
{
	0x00000000, 0x399f22b4, 0x3a1f22b4, 0x3a6eb40e, 0x3a9f22b4, 0x3ac6eb61,
	0x3aeeb40e, 0x3b0b3e5d, 0x3b1f22b4, 0x3b33070b, 0x3b46eb61, 0x3b5b518a,
	0x3b70f18a, 0x3b83e1c5, 0x3b8fe614, 0x3b9c87fb, 0x3ba9c9b5, 0x3bb7ad6d,
	0x3bc63547, 0x3bd5635f, 0x3be539bd, 0x3bf5ba70, 0x3c0373b5, 0x3c0c6152,
	0x3c15a703, 0x3c1f45bc, 0x3c293e68, 0x3c3391f4, 0x3c3e4149, 0x3c494d43,
	0x3c54b6c7, 0x3c607eb1, 0x3c6ca5df, 0x3c792d22, 0x3c830aa8, 0x3c89af9e,
	0x3c9085db, 0x3c978dc5, 0x3c9ec7c0, 0x3ca63432, 0x3cadd37d, 0x3cb5a601,
	0x3cbdac20, 0x3cc5e639, 0x3cce54ab, 0x3cd6f7d2, 0x3cdfd00e, 0x3ce8ddb9,
	0x3cf2212c, 0x3cfb9ac1, 0x3d02a569, 0x3d0798dc, 0x3d0ca7e4, 0x3d11d2ae,
	0x3d171963, 0x3d1c7c2e, 0x3d21fb3a, 0x3d2796af, 0x3d2d4ebb, 0x3d332380,
	0x3d39152b, 0x3d3f23e3, 0x3d454fd0, 0x3d4b991c, 0x3d51ffeb, 0x3d588466,
	0x3d5f26b7, 0x3d65e6fe, 0x3d6cc564, 0x3d73c210, 0x3d7add25, 0x3d810b65,
	0x3d84b793, 0x3d88732e, 0x3d8c3e48, 0x3d9018f4, 0x3d940343, 0x3d97fd48,
	0x3d9c0714, 0x3da020b9, 0x3da44a48, 0x3da883d6, 0x3daccd70, 0x3db12728,
	0x3db59110, 0x3dba0b38, 0x3dbe95b2, 0x3dc3308f, 0x3dc7dbe0, 0x3dcc97b4,
	0x3dd1641c, 0x3dd6412a, 0x3ddb2eec, 0x3de02d75, 0x3de53cd3, 0x3dea5d16,
	0x3def8e52, 0x3df4d091, 0x3dfa23e5, 0x3dff885e, 0x3e027f06, 0x3e05427f,
	0x3e080ea2, 0x3e0ae376, 0x3e0dc104, 0x3e10a752, 0x3e139669, 0x3e168e50,
	0x3e198f0e, 0x3e1c98ab, 0x3e1fab2e, 0x3e22c6a0, 0x3e25eb08, 0x3e29186a,
	0x3e2c4ed0, 0x3e2f8e42, 0x3e32d6c4, 0x3e362861, 0x3e39831e, 0x3e3ce702,
	0x3e405416, 0x3e43ca5e, 0x3e4749e4, 0x3e4ad2ae, 0x3e4e64c2, 0x3e520027,
	0x3e55a4e6, 0x3e595303, 0x3e5d0a8a, 0x3e60cb7c, 0x3e6495e0, 0x3e6869bf,
	0x3e6c4720, 0x3e702e08, 0x3e741e7f, 0x3e78188c, 0x3e7c1c34, 0x3e8014c0,
	0x3e822039, 0x3e84308b, 0x3e8645b8, 0x3e885fc3, 0x3e8a7eb0, 0x3e8ca281,
	0x3e8ecb3a, 0x3e90f8df, 0x3e932b72, 0x3e9562f6, 0x3e979f6f, 0x3e99e0e0,
	0x3e9c274e, 0x3e9e72b8, 0x3ea0c322, 0x3ea31892, 0x3ea57308, 0x3ea7d28a,
	0x3eaa3718, 0x3eaca0b7, 0x3eaf0f69, 0x3eb18332, 0x3eb3fc16, 0x3eb67a15,
	0x3eb8fd34, 0x3ebb8576, 0x3ebe12de, 0x3ec0a56e, 0x3ec33d2a, 0x3ec5da14,
	0x3ec87c30, 0x3ecb2380, 0x3ecdd008, 0x3ed081ca, 0x3ed338c9, 0x3ed5f508,
	0x3ed8b68a, 0x3edb7d52, 0x3ede4962, 0x3ee11abe, 0x3ee3f168, 0x3ee6cd64,
	0x3ee9aeb6, 0x3eec955d, 0x3eef815d, 0x3ef272ba, 0x3ef56976, 0x3ef86594,
	0x3efb6717, 0x3efe6e02, 0x3f00bd2b, 0x3f02460c, 0x3f03d1a5, 0x3f055ff8,
	0x3f06f105, 0x3f0884ce, 0x3f0a1b54, 0x3f0bb499, 0x3f0d509f, 0x3f0eef65,
	0x3f1090ef, 0x3f12353c, 0x3f13dc50, 0x3f15862a, 0x3f1732cc, 0x3f18e237,
	0x3f1a946d, 0x3f1c4970, 0x3f1e013f, 0x3f1fbbde, 0x3f21794c, 0x3f23398c,
	0x3f24fca0, 0x3f26c286, 0x3f288b42, 0x3f2a56d3, 0x3f2c253d, 0x3f2df680,
	0x3f2fca9d, 0x3f31a195, 0x3f337b6a, 0x3f35581e, 0x3f3737b1, 0x3f391a24,
	0x3f3aff7a, 0x3f3ce7b2, 0x3f3ed2d0, 0x3f40c0d2, 0x3f42b1bc, 0x3f44a58e,
	0x3f469c49, 0x3f4895ee, 0x3f4a9280, 0x3f4c91ff, 0x3f4e946c, 0x3f5099c8,
	0x3f52a216, 0x3f54ad55, 0x3f56bb88, 0x3f58ccae, 0x3f5ae0cb, 0x3f5cf7de,
	0x3f5f11ec, 0x3f612ef0, 0x3f634eef, 0x3f6571ea, 0x3f6797e1, 0x3f69c0d6,
	0x3f6beccb, 0x3f6e1bc0, 0x3f704db6, 0x3f7282af, 0x3f74baac, 0x3f76f5ae,
	0x3f7933b6, 0x3f7b74c6, 0x3f7db8de, 0x3f800000
};

static INLINE uint8 ToSRGB(float f)
{
	uint8 low = 0;
	uint8 high = 256;

	while(high - low > 1)
	{
		uint8 mid = (low + high) / 2;
		if(to_linear[mid] > f)
		{
			high = mid;
		}
		else
		{
			low = mid;
		}
	}

	if(to_linear[high] - f < f - to_linear[low])
	{
		return high;
	}
	return low;
}

static INLINE uint16 FloatToUnorm(float f, int n_bits)
{
	uint32 u;

	if(f > 1.0f)
	{
		f = 1.0f;
	}
	if(f < 0.0f)
	{
		f = 0.0f;
	}

	u = f * (1 << n_bits);
	u -= (u >> n_bits);

	return u;
}

static INLINE uint16 UnormToFloat(uint16 u, int n_bits)
{
	uint32 m = ((1 << n_bits) - 1);
	return (u & m) * (1.0f / (float)m);
}

static INLINE uint16 PixelManipulateFloatToUnorm(uint16 u, int n_bits)
{
	return FloatToUnorm(u, n_bits);
}

static INLINE float PixelManipulateUnormToFloat(float f, int n_bits)
{
	return UnormToFloat(f, n_bits);
}

static void FetchScanlineA8R8G8B8Float(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int x,
	int y,
	int width,
	uint32* b,
	const uint32* mask
)
{
	const uint32 *bits = image->bits + y * image->row_stride;
	const uint32 *pixel = bits + x;
	const uint32 *end = pixel + width;
	PIXEL_MANIPULATE_ARGB *buffer = (PIXEL_MANIPULATE_ARGB*)b;

	while(pixel < end)
	{
		uint32 p = (uint32)(*(pixel++));
		PIXEL_MANIPULATE_ARGB *argb = buffer;

		argb->a = PixelManipulateUnormToFloat((p >> 24) & 0xff, 8);
		argb->r = to_linear[(p >> 16) & 0xFF];
		argb->g = to_linear[(p >> 8) & 0xFF];
		argb->b = to_linear[(p >> 0) & 0xFF];

		buffer++;
	}
}

static PIXEL_MANIPULATE_ARGB FetchPixelA8R8G8B8Float(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int offset,
	int line
)
{
	uint32 *bits = image->bits + line * image->row_stride;
	uint32 p = (*(bits + offset));
	PIXEL_MANIPULATE_ARGB argb;

	argb.a = PixelManipulateUnormToFloat((p >> 24) & 0xff, 8);
	argb.r = to_linear[(p >> 16) & 0xFF];
	argb.g = to_linear[(p >> 8) & 0xFF];
	argb.b = to_linear[(p >> 0) & 0xFF];

	return argb;
}

static void StoreScanlineA8R8G8B8Float(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int x,
	int y,
	int width,
	const uint32* v
)
{
	uint32 *bits = image->bits + image->row_stride * y;
	uint32 *pixel = bits + x;
	PIXEL_MANIPULATE_ARGB *values = (PIXEL_MANIPULATE_ARGB*)v;
	int i;

	for(i = 0; i< width; i++)
	{
		uint32 a, r, g, b;

		a = PixelManipulateFloatToUnorm(values[i].a, 8);
		r = ToSRGB(values[i].r);
		g = ToSRGB(values[i].g);
		b = ToSRGB(values[i].b);

		*pixel = (a << 24) | (r << 16) | (g << 8) | (b << 0);
		pixel++;
	}
}

static void FetchScanlineA8R8G8B8_32(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int x,
	int y,
	int width,
	uint32* buffer,
	const uint32* mask
)
{
	const uint32 *bits = image->bits + y * image->row_stride;
	const uint32 *pixel = (uint32*)bits + x;
	const uint32 *end = pixel + width;
	uint32 temp;

	while(pixel < end)
	{
		uint32 a, r, g, b;

		temp = (uint32)(*(pixel++));
		a = (temp >> 24) & 0xFF;
		r = (temp >> 16) & 0xFF;
		g = (temp >> 8) & 0xFF;
		b = (temp >> 0) & 0xFF;

		*buffer++ = (a << 24) | (r << 16) | (g << 8) | (b << 0);
	}
}

static uint32 FetchPixelA8R8G8B8_32(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int offset,
	int line
)
{
	uint32 *bits = image->bits + line * image->row_stride;
	uint32 temp = *(bits + offset);
	uint32 a, r, g, b;

	a = (temp >> 24) & 0xFF;
	r = (temp >> 16) & 0xFF;
	g = (temp >> 8) & 0xFF;
	b = (temp >> 0) & 0xFF;

	r = to_linear[r] * 255.0f + 0.5f;
	g = to_linear[g] * 255.0f + 0.5f;
	b = to_linear[b] * 255.0f + 0.5f;

	return (a << 24) | (r << 16) | (g << 8) | (b << 0);
}

static void StoreScanlineA8R8G8B8_32(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int x,
	int y,
	int width,
	const uint32* v
)
{
	uint32 *bits = image->bits + image->row_stride * y;
	uint64 *values = (uint64*)v;
	uint32 *pixel = bits + x;
	uint64 temp;
	int i;

	for(i = 0; i < width; i++)
	{
		uint32 a, r, g, b;

		temp = values[i];

		a = (temp >> 24) & 0xFF;
		r = (temp >> 16) & 0xFF;
		g = (temp >> 8) & 0xFF;
		b = (temp >> 0) & 0xFF;

		r = ToSRGB(r * (1/255.0f));
		g = ToSRGB(g * (1/255.0f));
		b = ToSRGB(b * (1/255.0f));

		*pixel = a | (r << 16) | (g << 8) | (b << 0);
		pixel++;
	}
}

static void FetchScanlineRGBA(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int x,
	int y,
	int width,
	uint32* buffer,
	const uint32* mask
)
{
	uint8 *bits = (uint8*)(image->bits + y * image->row_stride);
	uint32 *src;
	int i;

	src = (uint32*)(bits + x * 4);
	if(width % 4 == 0)
	{
		for(i = 0; i< width / 4; buffer += 4, src += 4, i++)
		{
			buffer[0] = src[0];
			buffer[1] = src[1];
			buffer[2] = src[2];
			buffer[3] = src[3];
		}
	}
	else
	{
		for(i = 0; i< width; buffer++, src++, i++)
		{
			*buffer = *src;
		}
	}
}

static void FetchScanlineRGBA_Float(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int x,
	int y,
	int width,
	uint32* buffer,
	const uint32* mask
)
{
	PIXEL_MANIPULATE_ARGB *argb = (PIXEL_MANIPULATE_ARGB*)buffer;
	const uint8 *bits = (uint8*)(image->bits + y * image->row_stride);
	const uint8 *src = bits;
	int i;

	if(width % 4 == 0)
	{
		for(i = 0; i< width / 4; argb += 4, src += 4 * 4, i++)
		{
			argb[0].r = src[0] * DIV_PIXEL;
			argb[0].g = src[1] * DIV_PIXEL;
			argb[0].b = src[2] * DIV_PIXEL;
			argb[0].a = src[3] * DIV_PIXEL;
			argb[1].r = src[4] * DIV_PIXEL;
			argb[1].g = src[5] * DIV_PIXEL;
			argb[1].b = src[6] * DIV_PIXEL;
			argb[1].a = src[7] * DIV_PIXEL;
			argb[2].r = src[8] * DIV_PIXEL;
			argb[2].g = src[9] * DIV_PIXEL;
			argb[2].b = src[10] * DIV_PIXEL;
			argb[2].a = src[11] * DIV_PIXEL;
			argb[3].r = src[12] * DIV_PIXEL;
			argb[3].g = src[13] * DIV_PIXEL;
			argb[3].b = src[14] * DIV_PIXEL;
			argb[3].a = src[15] * DIV_PIXEL;
		}
	}
}

static void StoreScanlineRGBA(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int x,
	int y,
	int width,
	const uint32* values
)
{
	const uint32 *src = values;
	uint32 *dst = image->bits + y * image->row_stride;
	int i;

	if(width % 4 == 0)
	{
		for(i = 0; i < width / 4; src += 4, dst += 4, i++)
		{
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = src[3];
		}
	}
	else
	{
		for(i = 0; i < width; src++, dst++, i++)
		{
			*dst = *src;
		}
	}
}

static void StoreScanlineRGBA_Float(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int x,
	int y,
	int width,
	const uint32* values
)
{
	const uint8 *src = (const uint8*)values;
	PIXEL_MANIPULATE_ARGB *dst = (PIXEL_MANIPULATE_ARGB*)(image->bits + y * image->row_stride + x);
	int i;

	if(width % 4 == 0)
	{
		for(i = 0; i < width / 4; src += 4 * 4, dst += 4, i++)
		{
			dst[0].r = src[0] * DIV_PIXEL;
			dst[0].g = src[1] * DIV_PIXEL;
			dst[0].b = src[2] * DIV_PIXEL;
			dst[0].a = src[3] * DIV_PIXEL;
			dst[1].r = src[4] * DIV_PIXEL;
			dst[1].g = src[5] * DIV_PIXEL;
			dst[1].b = src[6] * DIV_PIXEL;
			dst[1].a = src[7] * DIV_PIXEL;
			dst[2].r = src[8] * DIV_PIXEL;
			dst[2].g = src[9] * DIV_PIXEL;
			dst[2].b = src[10] * DIV_PIXEL;
			dst[2].a = src[11] * DIV_PIXEL;
			dst[3].r = src[12] * DIV_PIXEL;
			dst[3].g = src[13] * DIV_PIXEL;
			dst[3].b = src[14] * DIV_PIXEL;
			dst[3].a = src[15] * DIV_PIXEL;
		}
	}
	else
	{
		for(i = 0; i < width; src += 4, dst++, i++)
		{
			dst->r = src[0] * DIV_PIXEL;
			dst->g = src[1] * DIV_PIXEL;
			dst->b = src[2] * DIV_PIXEL;
			dst->a = src[3] * DIV_PIXEL;
		}
	}
}

static uint32 FetchRGBA(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int offset,
	int line
)
{
	uint32 *pixel = image->bits + line * image->row_stride;
	pixel += offset;

	return *pixel;
}

static PIXEL_MANIPULATE_ARGB FetchRGBA_Float(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int offset,
	int line
)
{
	PIXEL_MANIPULATE_ARGB argb;
	uint8 *pixel = (uint8*)(image->bits + line * image->row_stride + offset);

	argb.r = pixel[0] * DIV_PIXEL;
	argb.g = pixel[1] * DIV_PIXEL;
	argb.b = pixel[2] * DIV_PIXEL;
	argb.a = pixel[3] * DIV_PIXEL;

	return argb;
}

static void FetchScanlineRGB(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int x,
	int y,
	int width,
	uint32* buffer,
	const uint32* mask
)
{
	uint8 *bits = (uint8*)(image->bits + y * image->row_stride);
	uint8 *src, *dst;
	int i;

	src = bits + x * 3;
	if(width % 4 == 0)
	{
		for(i = 0; i< width / 4; buffer += 4, src += 4 * 3, i++)
		{
			dst = (uint8*)(&buffer[0]);
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst = (uint8*)(&buffer[1]);
			dst[0] = src[3];
			dst[1] = src[4];
			dst[2] = src[5];
			dst = (uint8*)(&buffer[2]);
			dst[0] = src[6];
			dst[1] = src[7];
			dst[2] = src[8];
			dst = (uint8*)(&buffer[3]);
			dst[0] = src[9];
			dst[1] = src[10];
			dst[2] = src[11];
		}
	}
	else
	{
		for(i = 0; i< width; buffer++, src += 3, i++)
		{
			dst = (uint8*)buffer;
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
		}
	}
}

static void FetchScanlineRGB_Float(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int x,
	int y,
	int width,
	uint32* buffer,
	const uint32* mask
)
{
	uint8 *bits = (uint8*)(image->bits + y * image->row_stride);
	uint8 *src;
	PIXEL_MANIPULATE_ARGB *dst = (PIXEL_MANIPULATE_ARGB*)buffer;
	int i;

	src = bits + x * 3;
	if(width % 4 == 0)
	{
		for(i = 0; i< width / 4; dst += 4, src += 4 * 3, i++)
		{
			dst[0].r = src[0] * DIV_PIXEL;
			dst[0].g = src[1] * DIV_PIXEL;
			dst[0].b = src[2] * DIV_PIXEL;
			dst[0].a = 1.0f;
			dst[1].r = src[3] * DIV_PIXEL;
			dst[1].g = src[4] * DIV_PIXEL;
			dst[1].b = src[5] * DIV_PIXEL;
			dst[1].a = 1.0f;
			dst[2].r = src[6] * DIV_PIXEL;
			dst[2].g = src[7] * DIV_PIXEL;
			dst[2].b = src[8] * DIV_PIXEL;
			dst[2].a = 1.0f;
			dst[3].r = src[9] * DIV_PIXEL;
			dst[3].g = src[10] * DIV_PIXEL;
			dst[3].b = src[11] * DIV_PIXEL;
			dst[3].a = 1.0f;
		}
	}
	else
	{
		for(i = 0; i< width; dst++, src += 3, i++)
		{
			dst->r = src[0] * DIV_PIXEL;
			dst->g = src[1] * DIV_PIXEL;
			dst->b = src[2] * DIV_PIXEL;
			dst->a = 1.0f;
		}
	}
}

static void StoreScanlineRGB(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int x,
	int y,
	int width,
	const uint32* values
)
{
	const uint8 *src;
	uint8 *dst = (uint8*)(image->bits + y * image->row_stride);
	int i;

	src = (const uint8*)values;
	if(width % 4 == 0)
	{
		for(i = 0; i < width / 4; src += 4 * 3, dst += 4 * 3, i++)
		{
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = src[3];
			dst[4] = src[4];
			dst[5] = src[5];
			dst[6] = src[6];
			dst[7] = src[7];
			dst[8] = src[8];
			dst[9] = src[9];
			dst[10] = src[10];
			dst[11] = src[11];
		}
	}
	else
	{
		for(i = 0; i < width; src += 3, dst += 3, i++)
		{
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
		}
	}
}

static void StoreScanlineRGB_Float(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int x,
	int y,
	int width,
	const uint32* values
)
{
	const uint8 *src = (const uint8*)values;
	PIXEL_MANIPULATE_ARGB *dst = (PIXEL_MANIPULATE_ARGB*)((uint8*)(image->bits + y * image->row_stride) + x * 3);
	int i;

	if(width % 4 == 0)
	{
		for(i = 0; i < width / 4; src += 4 * 3, dst += 4, i++)
		{
			dst[0].r= src[0] * DIV_PIXEL;
			dst[0].g= src[1] * DIV_PIXEL;
			dst[0].b= src[2] * DIV_PIXEL;
			dst[0].a= 1.0f;
			dst[1].r= src[3] * DIV_PIXEL;
			dst[1].g= src[4] * DIV_PIXEL;
			dst[1].b= src[5] * DIV_PIXEL;
			dst[1].a= 1.0f;
			dst[2].r= src[6] * DIV_PIXEL;
			dst[2].g= src[7] * DIV_PIXEL;
			dst[2].b= src[8] * DIV_PIXEL;
			dst[2].a= 1.0f;
			dst[3].r= src[9] * DIV_PIXEL;
			dst[3].g= src[10] * DIV_PIXEL;
			dst[3].b= src[11] * DIV_PIXEL;
			dst[3].a= 1.0f;
		}
	}
	else
	{
		for(i = 0; i < width; src += 3, dst++, i++)
		{
			dst->r = src[0] * DIV_PIXEL;
			dst->g = src[1] * DIV_PIXEL;
			dst->b = src[2] * DIV_PIXEL;
			dst->a = 1.0f;
		}
	}
}

static uint32 FetchRGB(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int offset,
	int line
)
{
	uint8 *pixel = (uint8*)(image->bits + line * image->row_stride);
	uint32 data = (pixel[0] << 16) | (pixel[1] << 8) | pixel[2];
	return data;
}

static PIXEL_MANIPULATE_ARGB FetchRGB_Float(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int offset,
	int line
)
{
	PIXEL_MANIPULATE_ARGB argb;
	uint8 *pixel = (uint8*)(image->bits + line * image->row_stride);

	pixel += offset * 3;
	argb.r = pixel[0] * DIV_PIXEL;
	argb.g = pixel[1] * DIV_PIXEL;
	argb.b = pixel[2] * DIV_PIXEL;
	argb.a = 1.0f;

	return argb;
}

static void FetchScanlineAlpha(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int x,
	int y,
	int width,
	uint32* buffer,
	const uint32* mask
)
{
	uint8 *bits = (uint8*)(image->bits + y * image->row_stride);
	uint8 *src;
	int i;

	src = bits + x;
	if(width % 4 == 0)
	{
		for(i = 0; i< width / 4; buffer += 4, src += 4, i++)
		{
			buffer[0] = src[0];
			buffer[1] = src[1];
			buffer[2] = src[2];
			buffer[3] = src[3];
		}
	}
	else
	{
		for(i = 0; i< width; buffer++, src++, i++)
		{
			*buffer = *src;
		}
	}
}

static void FetchScanlineAlphaFloat(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int x,
	int y,
	int width,
	uint32* buffer,
	const uint32* mask
)
{
	uint8 *bits = (uint8*)(image->bits + y * image->row_stride);
	PIXEL_MANIPULATE_ARGB *argb = (PIXEL_MANIPULATE_ARGB*)buffer;
	uint8 *src;
	int i;

	src = bits + x;
	if(width % 4 == 0)
	{
		for(i = 0; i< width / 4; argb += 4, src += 4, i++)
		{
			argb[0].r = 0.0f;
			argb[0].g = 0.0f;
			argb[0].b = 0.0f;
			argb[0].a = src[0] * DIV_PIXEL;
			argb[1].r = 0.0f;
			argb[1].g = 0.0f;
			argb[1].b = 0.0f;
			argb[1].a = src[1] * DIV_PIXEL;
			argb[2].r = 0.0f;
			argb[2].g = 0.0f;
			argb[2].b = 0.0f;
			argb[2].a = src[2] * DIV_PIXEL;
			argb[3].r = 0.0f;
			argb[3].g = 0.0f;
			argb[3].b = 0.0f;
			argb[3].a = src[3] * DIV_PIXEL;
		}
	}
	else
	{
		for(i = 0; i< width; argb++, src++, i++)
		{
			argb->r = 0.0f;
			argb->g = 0.0f;
			argb->b = 0.0f;
			argb->a = src[0] * DIV_PIXEL;
		}
	}
}

static void StoreScanlineAlpha(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int x,
	int y,
	int width,
	const uint32* values
)
{
	const uint8 *src;
	uint8 *dst = (uint8*)(image->bits + y * image->row_stride);
	int i;

	src = (const uint8*)values;
	if(width % 4 == 0)
	{
		for(i = 0; i < width / 4; src += 4 * 3, dst += 4 * 3, i++)
		{
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = src[3];
		}
	}
	else
	{
		for(i = 0; i < width; src ++, dst ++, i++)
		{
			dst[0] = src[0];
		}
	}
}

static uint32 FetchAlpha(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int offset,
	int line
)
{
	uint8 *pixel = (uint8*)(image->bits + line * image->row_stride) + offset;
	uint32 data = pixel[0];
	return data;
}

static PIXEL_MANIPULATE_ARGB FetchPixelAlphaFloat(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int offset,
	int line
)
{
	PIXEL_MANIPULATE_ARGB argb;
	uint8 *pixel = (uint8*)(image->bits + line * image->row_stride + offset);

	argb.r = 0.0f;
	argb.g = 0.0f;
	argb.b = 0.0f;
	argb.a = pixel[0] * DIV_PIXEL;

	return argb;
}

static void StoreScanlineAlphaFloat(
	PIXEL_MANIPULATE_BITS_IMAGE* image,
	int x,
	int y,
	int width,
	const uint32* values
)
{
	const uint8 *src = (const uint8*)values;
	PIXEL_MANIPULATE_ARGB *dst = (PIXEL_MANIPULATE_ARGB*)((uint8*)(image->bits + y * image->row_stride) + x);
	int i;

	if(width % 4 == 0)
	{
		for(i = 0; i < width / 4; src += 4 * 3, dst += 4, i++)
		{
			dst[0].r= 0.0f;
			dst[0].g= 0.0f;
			dst[0].b= 0.0f;
			dst[0].a= src[0] * DIV_PIXEL;
			dst[1].r= 0.0f;
			dst[1].g= 0.0f;
			dst[1].b= 0.0f;
			dst[1].a= src[1] * DIV_PIXEL;
			dst[2].r= 0.0f;
			dst[2].g= 0.0f;
			dst[2].b= 0.0f;
			dst[2].a= src[2] * DIV_PIXEL;
			dst[3].r= 0.0f;
			dst[3].g= 0.0f;
			dst[3].b= 0.0f;
			dst[3].a= src[3] * DIV_PIXEL;
		}
	}
	else
	{
		for(i = 0; i < width; src += 3, dst++, i++)
		{
			dst->r = 0.0f;
			dst->g = 0.0f;
			dst->b = 0.0f;
			dst->a = src[0] * (float)DIV_PIXEL;
		}
	}
}

typedef struct _ACCESSOR
{
	void (*fetch_scanline)(PIXEL_MANIPULATE_BITS_IMAGE* image,
		int x, int y, int width, uint32* buffer, const uint32* mask);
	void (*fetch_scanline_float)(PIXEL_MANIPULATE_BITS_IMAGE* image,
		int x, int y, int width, uint32* buffer, const uint32* mask);
	uint32 (*fetch_pixel32)(PIXEL_MANIPULATE_BITS_IMAGE* image,
		int offset, int line);
	PIXEL_MANIPULATE_ARGB (*fetch_pixel_float)(PIXEL_MANIPULATE_BITS_IMAGE* image,
		int offset, int line);
	void (*store_scan_line)(PIXEL_MANIPULATE_BITS_IMAGE* image,
		int x, int y, int width, const uint32* values);
	void (*store_scan_line_float)(PIXEL_MANIPULATE_BITS_IMAGE* image,
		int x, int y, int width, const uint32* values);
} ACCESSOR;

void PixelManipulateSetupAccessors(PIXEL_MANIPULATE_BITS_IMAGE* image)
{
	const ACCESSOR accessor[] =
	{
		{FetchScanlineAlpha, FetchScanlineAlphaFloat,
			FetchAlpha, FetchPixelAlphaFloat,
			StoreScanlineAlpha, StoreScanlineAlphaFloat},
		{FetchScanlineRGB, FetchScanlineRGB_Float,
			FetchRGB, FetchRGB_Float,
			StoreScanlineRGB, StoreScanlineRGB_Float},
		{FetchScanlineRGBA, FetchScanlineRGBA_Float,
			FetchRGBA, FetchRGBA_Float,
			StoreScanlineRGBA, StoreScanlineRGBA_Float}
	};
	int index = PIXEL_MANIPULATE_BIT_PER_PIXEL(image->format) / 8 / 2;

	image->fetch_scanline_32 = accessor[index].fetch_scanline;
	image->fetch_pixel_32 = accessor[index].fetch_pixel32;
	image->store_scanline_32 = accessor[index].store_scan_line;
	image->fetch_scanline_float = accessor[index].fetch_scanline_float;
	image->fetch_pixel_float = accessor[index].fetch_pixel_float;
	image->store_scanline_float = accessor[index].store_scan_line_float;
}

#ifdef __cplusplus
}
#endif
