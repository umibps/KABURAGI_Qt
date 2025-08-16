#ifndef _INCLUDED_PIXEL_MANIPULATE_COMPOSITE_H_
#define _INCLUDED_PIXEL_MANIPULATE_COMPOSITE_H_

#include "types.h"
#include "pixel_manipulate_operator.h"
#include "pixel_manipulate_format.h"

typedef enum _ePIXEL_MANIPULATE_ITERATION_FLAGS
{
	PIXEL_MANIPULATE_ITERATION_NARROW =			 (1 << 0),
	PIXEL_MANIPULATE_ITERATION_WIDE =			   (1 << 1),
	PIXEL_MANIPULATE_ITERATION_LOCALIZED_ALPHA =	(1 << 2),
	PIXEL_MANIPULATE_ITERATION_IGNORE_ALPHA =	   (1 << 3),
	PIXEL_MANIPULATE_ITERATION_IGNORE_RGB =		 (1 << 4),
	PIXEL_MANIPULATE_ITERATION_SRC =				(1 << 5),
	PIXEL_MANIPULATE_ITERATION_DEST =			   (1 << 6)
} ePIXEL_MANIPULATE_ITERATION_FLAGS;

typedef uint32* (*pixel_manipulate_iteration_get_scanline)(struct _PIXEL_MANIPULATE_ITERATION* iter, const uint32* mask);
typedef void (*pixel_manipulate_iteration_write_back)(struct _PIXEL_MANIPULATE_ITERATION* iter);
typedef void (*pixel_manipulate_iteration_finish)(struct _PIXEL_MANIPULATE_ITERATION* iter);

typedef struct _PIXEL_MANIPULATE_ITERATION
{
	union _PIXEL_MANIPULATE_IMAGE *image;
	uint32 *buffer;
	int x, y;
	int width;
	int height;
	ePIXEL_MANIPULATE_ITERATION_FLAGS iterate_flags;
	uint32 image_flags;

	pixel_manipulate_iteration_get_scanline get_scanline;
	pixel_manipulate_iteration_write_back write_back;
	pixel_manipulate_iteration_finish finish;

	void *data;
	uint8 *bits;
	int stride;
} PIXEL_MANIPULATE_ITERATION;

typedef struct _PIXEL_MANIPULATE_COMPOSITE_INFO
{
	ePIXEL_MANIPULATE_OPERATE op;
	union _PIXEL_MANIPULATE_IMAGE *src_image;
	union _PIXEL_MANIPULATE_IMAGE *mask_image;
	union _PIXEL_MANIPULATE_IMAGE *dest_image;
	int32 src_x;
	int32 src_y;
	int32 mask_x;
	int32 mask_y;
	int32 dest_x;
	int32 dest_y;
	int32 width;
	int32 height;

	uint32 src_flags;
	uint32 mask_flags;
	uint32 dest_flags;
} PIXEL_MANIPULATE_COMPOSITE_INFO;

typedef void (*pixel_manipulate_combine32_function)(
	struct _PIXEL_MANIPULATE_IMPLEMENTATION* imp, ePIXEL_MANIPULATE_OPERATE op, uint32* dest, const uint32* src, const uint32* mask, int width);

typedef void (*pixel_manipulate_combine_float_function)(
	struct _PIXEL_MANIPULATE_IMPLEMENTATION* imp, ePIXEL_MANIPULATE_OPERATE op, float* dest, const float* src, const float* mask, int num_pixels);

typedef void (*pixel_manipulate_composite_function)(
	struct _PIXEL_MANIPULATE_IMPLEMENTATION* imp, PIXEL_MANIPULATE_COMPOSITE_INFO* info);

typedef int (*pixel_manipulate_block_transfer_function)(
	struct _PIXEL_MANIPULATE_IMPLEMENTATION* imp,
	uint32* src_bits,
	uint32* dst_bits,
	int src_stride,
	int dest_stride,
	int src_bit_per_pixel,
	int dest_bit_per_pixel,
	int src_x,
	int src_y,
	int dest_x,
	int dest_y,
	int width,
	int height
);

typedef int (*pixel_manipualte_fill_function)(
	struct _PIXEL_MANIPULATE_IMPLEMENTATION* imp,
	uint32* bits,
	int stride,
	int bit_per_pixel,
	int x,
	int y,
	int width,
	int height,
	uint32 filler
);

typedef void (*pixel_manipulate_iteration_initializer)(
	PIXEL_MANIPULATE_ITERATION* iter, const struct _PIXEL_MANIPULATE_ITERATION_INFO* info);

typedef struct _PIXEL_MANIPULATE_ITERATION_INFO
{
	enum _ePIXEL_MANIPULATE_FORMAT format;
	uint32 image_flags;
	ePIXEL_MANIPULATE_ITERATION_FLAGS iteration_flags;
	pixel_manipulate_iteration_initializer initializer;
	pixel_manipulate_iteration_get_scanline get_scanline;
	pixel_manipulate_iteration_write_back write_back;
} PIXEL_MANIPULATE_ITERATION_INFO;

typedef struct _PIXEL_MANIPULATE_FAST_PATH
{
	ePIXEL_MANIPULATE_OPERATE op;
	enum _ePIXEL_MANIPULATE_FORMAT src_format;
	uint32 src_flags;
	enum _ePIXEL_MANIPULATE_FORMAT mask_format;
	uint32 mask_flags;
	enum _ePIXEL_MANIPULATE_FORMAT dest_format;
	uint32 dest_flags;
	pixel_manipulate_composite_function function;
} PIXEL_MANIPULATE_FAST_PATH;

typedef struct _PIXEL_MANIPULATE_IMPLEMENTATION
{
	struct _PIXEL_MANIPULATE_IMPLEMENTATION *top_level;
	struct _PIXEL_MANIPULATE_IMPLEMENTATION *fallback;
	const PIXEL_MANIPULATE_FAST_PATH *fast_paths;
	const PIXEL_MANIPULATE_ITERATION_INFO *iteration_info;

	pixel_manipulate_block_transfer_function block_transfer;
	pixel_manipualte_fill_function fill;

	pixel_manipulate_combine32_function combine32[PIXEL_MANIPULATE_NUM_OPERATORS];
	pixel_manipulate_combine32_function combine32_ca[PIXEL_MANIPULATE_NUM_OPERATORS];
	pixel_manipulate_combine_float_function combine_float[PIXEL_MANIPULATE_NUM_OPERATORS];
	pixel_manipulate_combine_float_function combine_float_ca[PIXEL_MANIPULATE_NUM_OPERATORS];
} PIXEL_MANIPULATE_IMPLEMENTATION;

#ifdef __cplusplus
extern "C" {
#endif

extern void PixelManipulateImplementationLookupComposite(
	PIXEL_MANIPULATE_IMPLEMENTATION* top_level,
	ePIXEL_MANIPULATE_OPERATE op,
	ePIXEL_MANIPULATE_FORMAT src_format,
	uint32 src_flags,
	ePIXEL_MANIPULATE_FORMAT mask_format,
	uint32 mask_flags,
	ePIXEL_MANIPULATE_FORMAT dest_format,
	uint32 dest_flags,
	PIXEL_MANIPULATE_IMPLEMENTATION** out_implementation,
	pixel_manipulate_composite_function* out_function
);

extern pixel_manipulate_combine32_function PixelManipulateImplementationLookupCombiner(
	PIXEL_MANIPULATE_IMPLEMENTATION* imp,
	ePIXEL_MANIPULATE_OPERATE op,
	int component_alpha,
	int narrow
);

extern void PixelManipulateComposite32(
	ePIXEL_MANIPULATE_OPERATE op,
	union _PIXEL_MANIPULATE_IMAGE* src,
	union _PIXEL_MANIPULATE_IMAGE* mask,
	union _PIXEL_MANIPULATE_IMAGE* dest,
	int32 src_x,
	int32 src_y,
	int32 mask_x,
	int32 mask_y,
	int32 dest_x,
	int32 dest_y,
	int32 width,
	int32 height
);

extern uint32 PixelManipulateImageGetSolid(
	PIXEL_MANIPULATE_IMPLEMENTATION* implementation,
	union _PIXEL_MANIPULATE_IMAGE* image,
	ePIXEL_MANIPULATE_FORMAT format
);

extern uint32* PixelManipulateIteraterationInitializeBitsStride(
	PIXEL_MANIPULATE_ITERATION* iter,
	const PIXEL_MANIPULATE_ITERATION_INFO* info
);

extern PIXEL_MANIPULATE_IMPLEMENTATION* PixelManipulateImplementationCreate(PIXEL_MANIPULATE_IMPLEMENTATION* fallback, const PIXEL_MANIPULATE_FAST_PATH* fast_paths);

extern void PixelManipulateImplementationIterationInitialize(
	PIXEL_MANIPULATE_IMPLEMENTATION* implementation,
	PIXEL_MANIPULATE_ITERATION* iter,
	union _PIXEL_MANIPULATE_IMAGE* image,
	int x,
	int y,
	int width,
	int height,
	uint8* buffer,
	ePIXEL_MANIPULATE_ITERATION_FLAGS iter_flags,
	uint32 image_flags
);

extern PIXEL_MANIPULATE_IMPLEMENTATION*
	PixelManipulateImplementationCreateGeneral(void);

#if defined(USE_X86_MMX) && USE_X86_MMX != 0
extern PIXEL_MANIPULATE_IMPLEMENTATION*
	PixelManipulateImplementationCreateMMX(PIXEL_MANIPULATE_IMPLEMENTATION* fallback);
#endif

#if defined(USE_SSE2) && USE_SSE2 != 0
extern PIXEL_MANIPULATE_IMPLEMENTATION *
	PixelManipulateImplementationCreateSSE2(PIXEL_MANIPULATE_IMPLEMENTATION *fallback);
#endif

#if defined(USE_SSSE3) && USE_SSSE3 != 0
extern PIXEL_MANIPULATE_IMPLEMENTATION*
	PixelManipulateImplementationCreateSSSE3(PIXEL_MANIPULATE_IMPLEMENTATION* fallback);
#endif

extern PIXEL_MANIPULATE_IMPLEMENTATION*
	PixelManipulateX86GetImplementations(PIXEL_MANIPULATE_IMPLEMENTATION* imp);

extern PIXEL_MANIPULATE_IMPLEMENTATION* PixelManipulateImplementationCreateNoOperator(PIXEL_MANIPULATE_IMPLEMENTATION *fallback);

extern int PixelManipulateImplementationFill(
	PIXEL_MANIPULATE_IMPLEMENTATION* imp,
	uint32* bits,
	int stride,
	int bpp,
	int x,
	int y,
	int width,
	int height,
	uint32 filler
);

extern int PixelManipulateImplementationBlockTransfer(
	PIXEL_MANIPULATE_IMPLEMENTATION* imp,
	uint32* src_bits,
	uint32* dst_bits,
	int src_stride,
	int dst_stride,
	int src_bpp,
	int dst_bpp,
	int src_x,
	int src_y,
	int dst_x,
	int dst_y,
	int width,
	int height
);

extern void PixelManipulateSetupCombinerFunctions32(PIXEL_MANIPULATE_IMPLEMENTATION *imp);

#ifdef __cplusplus
}
#endif

#endif // #ifndef _INCLUDED_PIXEL_MANIPULATE_COMPOSITE_H_
