#include <string.h>
#include "pixel_manipulate_composite.h"
#include "pixel_manipulate.h"
#include "pixel_manipulate_private.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_CACHED_FAST_PATHS 8

typedef struct _CACHE
{
	struct
	{
		PIXEL_MANIPULATE_IMPLEMENTATION *implementation;
		PIXEL_MANIPULATE_FAST_PATH fast_path;
	} cache[NUM_CACHED_FAST_PATHS];
} CACHE;

static void DummyCompositeRectangle(PIXEL_MANIPULATE_IMPLEMENTATION* implementation, PIXEL_MANIPULATE_COMPOSITE_INFO* info)
{

}

void PixelManipulateImplementationLookupComposite(
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
)
{
	static CACHE fast_path_cache;
	PIXEL_MANIPULATE_IMPLEMENTATION *implementation;
	CACHE *cache;
	int i;

	cache = &fast_path_cache;

	for(i=0; i<NUM_CACHED_FAST_PATHS; i++)
	{
		const PIXEL_MANIPULATE_FAST_PATH *info = &(cache->cache[i].fast_path);

		if(info->op == op
			&& info->src_format == src_format
			&& info->mask_format == mask_format
			&& info->dest_format == dest_format
			&& info->src_flags == src_flags
			&& info->mask_flags == mask_flags
			&& info->dest_flags == dest_flags
			&& info->function != NULL
		)
		{
			*out_implementation = cache->cache[i].implementation;
			*out_function = cache->cache[i].fast_path.function;

			goto update_cache;
		}
	}

	for(implementation = top_level; implementation != NULL; implementation = implementation->fallback)
	{
		const PIXEL_MANIPULATE_FAST_PATH *info = implementation->fast_paths;

		while(info->op != PIXEL_MANIPULATE_OPERATE_NONE)
		{
			if((info->op == op || info->op == PIXEL_MANIPULATE_OPERATE_ANY)
				&& ((info->src_format == src_format)
					|| (info->src_format == PIXEL_MANIPULATE_FORMAT_ANY))
				&& ((info->mask_format == mask_format)
					|| (info->mask_format == PIXEL_MANIPULATE_FORMAT_ANY))
				&& ((info->dest_format == dest_format)
					|| (info->dest_format == PIXEL_MANIPULATE_FORMAT_ANY))
				&& (info->src_flags & src_flags) == info->src_flags
				&& (info->mask_flags & mask_flags) == info->mask_flags
				&& (info->dest_flags & dest_flags) == info->dest_flags
			)
			{
				*out_implementation = implementation;
				*out_function = info->function;

				i = NUM_CACHED_FAST_PATHS - 1;

				goto update_cache;
			}

			++info;
		}
	}

	*out_implementation = NULL;
	*out_function = DummyCompositeRectangle;
	return;

update_cache:
	if(i != 0)
	{
		while(i--)
		{
			cache->cache[i + 1] = cache->cache[i];
		}

		cache->cache[0].implementation = *out_implementation;
		cache->cache[0].fast_path.op = op;
		cache->cache[0].fast_path.src_format = src_format;
		cache->cache[0].fast_path.src_flags = src_flags;
		cache->cache[0].fast_path.mask_format = mask_format;
		cache->cache[0].fast_path.mask_flags = mask_flags;
		cache->cache[0].fast_path.dest_format = dest_format;
		cache->cache[0].fast_path.dest_flags = dest_flags;
		cache->cache[0].fast_path.function = *out_function;
	}
}

static uint32* getScanlineNull(PIXEL_MANIPULATE_ITERATION* iter, const uint32* mask)
{
	return NULL;
}

void PixelManipulateImplementationIterationInitialize(
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
)
{
	ePIXEL_MANIPULATE_FORMAT format;

	iter->image = image;
	iter->buffer = (uint32*)buffer;
	iter->x = x;
	iter->y = y;
	iter->width = width;
	iter->height = height;
	iter->iterate_flags = iter_flags;
	iter->image_flags = image_flags;
	iter->finish = NULL;

	if(iter->image == NULL)
	{
		iter->get_scanline = getScanlineNull;
		return;
	}

	format = iter->image->common.extended_format_code;

	while(implementation != NULL)
	{
		if(implementation->iteration_info != NULL)
		{
			const PIXEL_MANIPULATE_ITERATION_INFO *info;

			for(info = implementation->iteration_info; info->format != PIXEL_MANIPULATE_FORMAT_NULL; info++)
			{
				if((info->format == PIXEL_MANIPULATE_FORMAT_ANY || info->format == format)
					&& (info->image_flags & image_flags) == info->image_flags
					&& (info->iteration_flags & iter_flags) == info->iteration_flags)
				{
					iter->get_scanline = info->get_scanline;
					iter->write_back = info->write_back;

					if(info->initializer != NULL)
					{
						info->initializer(iter, info);
					}
					return;
				}
			}
		}
		implementation = implementation->fallback;
	}
}

PIXEL_MANIPULATE_IMPLEMENTATION* PixelManipulateImplementationCreate(PIXEL_MANIPULATE_IMPLEMENTATION* fallback, const PIXEL_MANIPULATE_FAST_PATH* fast_paths)
{
	PIXEL_MANIPULATE_IMPLEMENTATION *implementation;

	if((implementation = (PIXEL_MANIPULATE_IMPLEMENTATION*)MEM_ALLOC_FUNC(sizeof(*implementation))))
	{
		PIXEL_MANIPULATE_IMPLEMENTATION *d;

		(void)memset(implementation, 0, sizeof(*implementation));

		implementation->fallback = fallback;
		implementation->fast_paths = fast_paths;

		for(d = implementation; d != NULL; d = d->fallback)
		{
			d->top_level = implementation;
		}
	}

	return implementation;
}

int PixelManipulateImplementationFill(
	PIXEL_MANIPULATE_IMPLEMENTATION* imp,
	uint32* bits,
	int stride,
	int bpp,
	int x,
	int y,
	int width,
	int height,
	uint32 filler
)
{
	while(imp != NULL)
	{
		if(imp->fill != NULL &&
			((*imp->fill)(imp, bits, stride, bpp, x, y, width, height, filler)))
		{
			return TRUE;
		}

		imp = imp->fallback;
	}

	return FALSE;
}

int PixelManipulateImplementationBlockTransfer(
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
)
{
	while(imp != NULL)
	{
		if(imp->block_transfer != NULL && (*imp->block_transfer)(imp, src_bits, dst_bits,
			src_stride, dst_stride, src_bpp, dst_bpp, src_x, src_y, dst_x, dst_y, width, height))
		{
			return TRUE;
		}

		imp = imp->fallback;
	}

	return FALSE;
}

static void dummy_combine(
	PIXEL_MANIPULATE_IMPLEMENTATION* imp,
	ePIXEL_MANIPULATE_OPERATE op,
	uint32* pd,
	const uint32* ps,
	const uint32* pm,
	int w
)
{
}

pixel_manipulate_combine32_function PixelManipulateImplementationLookupCombiner(
	PIXEL_MANIPULATE_IMPLEMENTATION* imp,
	ePIXEL_MANIPULATE_OPERATE op,
	int component_alpha,
	int narrow
)
{
	while(imp != NULL)
	{
		pixel_manipulate_combine32_function f = NULL;

		switch((narrow << 1 ) | component_alpha)
		{
		case 0:
			f = (pixel_manipulate_combine32_function)imp->combine_float[op];
			break;
		case 1:
			f = (pixel_manipulate_combine32_function)imp->combine_float_ca[op];
			break;
		case 2:
			f = (pixel_manipulate_combine32_function)imp->combine32[op];
			break;
		case 3:
			f = imp->combine32_ca[op];
			break;
		}

		if(f != NULL)
		{
			return f;
		}

		imp = imp->fallback;
	}

	return dummy_combine;
}

#ifdef __cplusplus
}
#endif
