#include <string.h>
#include "layer.h"
#include "draw_window.h"
#include "application.h"

#ifdef __cplusplus
extern "C" {
#endif

static void SaveLayerAlpha(LAYER* layer)
{
	uint8 *buf;
	int i;

	buf = layer->window->alpha_lock;
	for(i=0; i<layer->height; i++)
	{
		uint8 *ptr;
		int j;

		for(j=0, ptr = &layer->pixels[i*layer->stride+3]; j<layer->width; j++, ptr+=4, buf++)
		{
			*buf = *ptr;
		}
	}
}

static void RestoreLayerAlpha(LAYER* layer)
{
	uint8 *buf;
	int i;

	buf = layer->window->alpha_lock;
	for(i=0; i<layer->height; i++)
	{
		uint8 *ptr;
		int j;

		for(j=0, ptr = &layer->pixels[i*layer->stride]; j<layer->width; j++, ptr+=4, buf++)
		{
			if(ptr[0] > *buf) ptr[0] = *buf;
			if(ptr[1] > *buf) ptr[1] = *buf;
			if(ptr[2] > *buf) ptr[2] = *buf;
			ptr[3] = *buf;
		}
	}
}

#define DEFAULT_BLEND_OPERATION 	\
	if((dst->flags & LAYER_LOCK_OPACITY) != 0) \
	{ \
		SaveLayerAlpha(dst); \
	} \
\
	if((src->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0) \
	{ \
		LAYER* mask_source = src->prev; \
		GRAPHICS_SURFACE_PATTERN pattern = {0}; \
		(void)memset(window->mask->pixels, 0, src->stride*src->height); \
		GraphicsSetOperator(&window->mask->context.base, GRAPHICS_OPERATOR_OVER); \
		GraphicsSetSourceSurface(&window->mask->context.base, \
								 &src->surface.base, src->x, src->y, &pattern); \
		GraphicsPaintWithAlpha(&window->mask->context.base, src->alpha * 0.01); \
\
		while((mask_source->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0) \
		{ \
			mask_source = mask_source->prev; \
		} \
		if(mask_source == window->active_layer) \
		{ \
			GraphicsSetOperator(&window->mask_temp->context.base, GRAPHICS_OPERATOR_OVER); \
			(void)memset(window->mask_temp->pixels, 0, mask_source->stride*mask_source->height); \
			InitializeGraphicsPatternForSurface(&pattern, &window->work_layer->surface.base); \
			GraphicsSetSourceSurface(&window->mask_temp->context.base, NULL, 0, 0, &pattern); \
			GraphicsPaint(&window->mask_temp->context.base); \
			mask_source = window->mask_temp; \
		} \
		InitializeGraphicsPatternForSurface(&pattern, &window->mask->surface.base); \
		GraphicsSetSourceSurface(&dst->context.base, NULL, 0, 0, &pattern); \
		GraphicsMaskSurface(&dst->context.base, &mask_source->surface.base, 0, 0); \
	} \
	else \
	{ \
		GRAPHICS_SURFACE_PATTERN pattern = {0}; \
		GraphicsSetSourceSurface(&dst->context.base, &src->surface.base, src->x, src->y, &pattern); \
		GraphicsPaintWithAlpha(&dst->context.base, src->alpha * (FLOAT_T)0.01); \
	} \
\
	if((dst->flags & LAYER_LOCK_OPACITY) != 0) \
	{ \
		RestoreLayerAlpha(dst); \
	} \

void BlendNormal_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_CONTEXT *context = &dst->context.base;
	DRAW_WINDOW *window = src->window;
	GraphicsSetOperator(context, GRAPHICS_OPERATOR_OVER);
	
	// DEFAULT_BLEND_OPERATION

	if((dst->flags & LAYER_LOCK_OPACITY) != 0) \
	{ \
		SaveLayerAlpha(dst); \
	} \
\
	if((src->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0) \
	{ \
		LAYER* mask_source = src->prev; \
		GRAPHICS_SURFACE_PATTERN pattern = { 0 }; \
		(void)memset(window->mask->pixels, 0, src->stride* src->height); \
		GraphicsSetOperator(&window->mask->context.base, GRAPHICS_OPERATOR_OVER); \
		GraphicsSetSourceSurface(&window->mask->context.base, \
			& src->surface.base, src->x, src->y, &pattern); \
		GraphicsPaintWithAlpha(&window->mask->context.base, src->alpha * 0.01); \
		\
		while((mask_source->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0) \
		{ \
			mask_source = mask_source->prev; \
		} \
		if(mask_source == window->active_layer) \
		{ \
			GraphicsSetOperator(&window->mask_temp->context.base, GRAPHICS_OPERATOR_OVER); \
			(void)memset(window->mask_temp->pixels, 0, mask_source->stride* mask_source->height); \
			InitializeGraphicsPatternForSurface(&pattern, &window->work_layer->surface.base); \
			GraphicsSetSourceSurface(&window->mask_temp->context.base, NULL, 0, 0, &pattern); \
			GraphicsPaint(&window->mask_temp->context.base); \
					mask_source = window->mask_temp; \
		} \
		InitializeGraphicsPatternForSurface(&pattern, &window->mask->surface.base); \
		GraphicsSetSourceSurface(&dst->context.base, NULL, 0, 0, &pattern); \
		GraphicsMaskSurface(&dst->context.base, &mask_source->surface.base, 0, 0); \
	} \
	else \
	{ \
		GRAPHICS_SURFACE_PATTERN pattern = { 0 }; \
		GraphicsSetSourceSurface(&dst->context.base, &src->surface.base, src->x, src->y, &pattern); \
		GraphicsPaintWithAlpha(&dst->context.base, src->alpha* (FLOAT_T)0.01); \
	} \
\
	if((dst->flags & LAYER_LOCK_OPACITY) != 0) \
	{ \
		RestoreLayerAlpha(dst); \
	} \

	/*
	int i, j, k;

	for(i=0; i<src->height && i+src->y<dst->height; i++)
	{
		for(j=0; j<src->width && j+src->x<dst->width; j++)
		{
			if(src->pixels[i*src->stride+j*src->channel+3] != 0)
			{
				if(src->pixels[i*src->stride+j*src->channel+3] == 0xff)
				{
					(void)memcpy(&dst->pixels[(i+src->y)*dst->stride+j*dst->channel],
						&src->pixels[i*src->stride+j*src->channel], dst->channel);
				}
				else
				{
					for(k=0; k<src->channel; k++)
					{
						dst->pixels[(i+src->y)*dst->stride+j*dst->channel+k] =
							(uint32)(((int)src->pixels[i*src->stride+j*src->channel+k]
							- (int)dst->pixels[(i+src->y)*dst->stride+(j+src->x)*dst->channel+k])
								* src->pixels[i*src->stride+j*src->channel+3] >> 8)
								+ dst->pixels[(i+src->y)*dst->stride+(j+src->x)*dst->channel+k];
					}
				}
			}
		}
	}
	*/
}

void BlendAdd_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_CONTEXT *context = &dst->context.base;
	DRAW_WINDOW *window = src->window;
	GraphicsSetOperator(context, GRAPHICS_OPERATOR_ADD);

	DEFAULT_BLEND_OPERATION
}

void BlendMultiply_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_CONTEXT *context = &dst->context.base;
	DRAW_WINDOW *window = src->window;
	GraphicsSetOperator(context, GRAPHICS_OPERATOR_MULTIPLY);

	DEFAULT_BLEND_OPERATION
}

void BlendScreen_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_CONTEXT *context = &dst->context.base;
	DRAW_WINDOW *window = src->window;
	GraphicsSetOperator(context, GRAPHICS_OPERATOR_SCREEN);

	DEFAULT_BLEND_OPERATION
}

void BlendOverlay_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_CONTEXT *context = &dst->context.base;
	DRAW_WINDOW *window = src->window;
	GraphicsSetOperator(context, GRAPHICS_OPERATOR_OVERLAY);

	DEFAULT_BLEND_OPERATION
}

void BlendLighten_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_CONTEXT *context = &dst->context.base;
	DRAW_WINDOW *window = src->window;
	GraphicsSetOperator(context, GRAPHICS_OPERATOR_LIGHTEN);

	DEFAULT_BLEND_OPERATION
}

void BlendDarken_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_CONTEXT *context = &dst->context.base;
	DRAW_WINDOW *window = src->window;
	GraphicsSetOperator(context, GRAPHICS_OPERATOR_DARKEN);

	DEFAULT_BLEND_OPERATION
}

void BlendDodge_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_CONTEXT *context = &dst->context.base;
	DRAW_WINDOW *window = src->window;
	GraphicsSetOperator(context, GRAPHICS_OPERATOR_COLOR_DODGE);

	DEFAULT_BLEND_OPERATION
}

void BlendBurn_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_CONTEXT *context = &dst->context.base;
	DRAW_WINDOW *window = src->window;
	GraphicsSetOperator(context, GRAPHICS_OPERATOR_COLOR_BURN);

	DEFAULT_BLEND_OPERATION
}

void BlendHardLight_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_CONTEXT *context = &dst->context.base;
	DRAW_WINDOW *window = src->window;
	GraphicsSetOperator(context, GRAPHICS_OPERATOR_HARD_LIGHT);

	DEFAULT_BLEND_OPERATION
}

void BlendSoftLight_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_CONTEXT *context = &dst->context.base;
	DRAW_WINDOW *window = src->window;
	GraphicsSetOperator(context, GRAPHICS_OPERATOR_SOFT_LIGHT);

	DEFAULT_BLEND_OPERATION
}

void BlendDifference_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_CONTEXT *context = &dst->context.base;
	DRAW_WINDOW *window = src->window;
	GraphicsSetOperator(context, GRAPHICS_OPERATOR_DIFFERENCE);

	DEFAULT_BLEND_OPERATION
}

void BlendExclusion_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_CONTEXT *context = &dst->context.base;
	DRAW_WINDOW *window = src->window;
	GraphicsSetOperator(context, GRAPHICS_OPERATOR_EXCLUSION);

	DEFAULT_BLEND_OPERATION
}

void BlendHslHue_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_CONTEXT *context = &dst->context.base;
	DRAW_WINDOW *window = src->window;
	GraphicsSetOperator(context, GRAPHICS_OPERATOR_HSL_HUE);

	DEFAULT_BLEND_OPERATION
}

void BlendHslSaturation_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_CONTEXT *context = &dst->context.base;
	DRAW_WINDOW *window = src->window;
	GraphicsSetOperator(context, GRAPHICS_OPERATOR_HSL_SATURATION);

	DEFAULT_BLEND_OPERATION
}

void BlendHslColor_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_CONTEXT *context = &dst->context.base;
	DRAW_WINDOW *window = src->window;
	GraphicsSetOperator(context, GRAPHICS_OPERATOR_HSL_COLOR);

	DEFAULT_BLEND_OPERATION
}

void BlendHslLuminosity_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_CONTEXT *context = &dst->context.base;
	DRAW_WINDOW *window = src->window;
	GraphicsSetOperator(context, GRAPHICS_OPERATOR_HSL_LUMINOSITY);

	DEFAULT_BLEND_OPERATION
}

void BlendBinalize_c(LAYER* src, LAYER* dst)
{
#define BINALIZE_THRESHOLD 128
	GRAPHICS_SURFACE_PATTERN pattern;
	int i;

	GraphicsSetOperator(&dst->context.base, GRAPHICS_OPERATOR_OVER);

	if((dst->flags & LAYER_LOCK_OPACITY) != 0)
	{
		SaveLayerAlpha(dst);
	}

	if((src->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0)
	{
		LAYER *mask_source = src->prev;
		(void)memset(src->window->mask->pixels, 0, src->stride * src->height);
		GraphicsSetOperator(&src->window->mask->context.base, GRAPHICS_OPERATOR_OVER);
		for(i=0; i<src->width * src->height; i++)
		{
			if(src->window->mask_temp->pixels[i*4+3] >= BINALIZE_THRESHOLD)
			{
				src->window->mask_temp->pixels[i*4+0] =
					(src->pixels[i*4+0] >= 128) ? 255 : 0;
				src->window->mask_temp->pixels[i*4+1] =
					(src->pixels[i*4+1] >= 128) ? 255 : 0;
				src->window->mask_temp->pixels[i*4+2] =
					(src->pixels[i*4+2] >= 128) ? 255 : 0;
				src->window->mask_temp->pixels[i*4+3] = 255;
			}
			else
			{
				src->window->mask_temp->pixels[i*4+0] = 0;
				src->window->mask_temp->pixels[i*4+1] = 0;
				src->window->mask_temp->pixels[i*4+2] = 0;
				src->window->mask_temp->pixels[i*4+3] = 0;
			}
		}
		GraphicsSetSourceSurface(&src->window->mask->context.base,
								 &src->window->mask_temp->surface.base, src->x, src->y, &pattern);
		GraphicsPaintWithAlpha(&src->window->mask->context.base, src->alpha * (FLOAT_T)0.01);

		while((mask_source->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0)
		{
			mask_source = mask_source->prev;
		}
		if(mask_source == src->window->active_layer)
		{
			GraphicsSetOperator(&src->window->mask_temp->context.base, GRAPHICS_OPERATOR_OVER);
			(void)memcpy(src->window->mask_temp->pixels, mask_source->pixels,
						  mask_source->stride * mask_source->height);
			GraphicsSetSourceSurface(&src->window->mask_temp->context.base,
									 &src->window->work_layer->surface.base, 0, 0, &pattern);
			GraphicsPaint(&src->window->mask_temp->context.base);
			mask_source = src->window->mask_temp;
		}

		GraphicsSetSourceSurface(&dst->context.base,
								 &src->window->mask->surface.base, 0, 0, &pattern);
		GraphicsMaskSurface(&dst->context.base, &mask_source->surface.base, 0, 0);
	}
	else
	{
		for(i=0; i<src->width * src->height; i++)
		{
			if(src->window->mask_temp->pixels[i*4+3] >= BINALIZE_THRESHOLD)
			{
				src->window->mask_temp->pixels[i*4+0] =
					(src->pixels[i*4+0] >= 128) ? 255 : 0;
				src->window->mask_temp->pixels[i*4+1] =
					(src->pixels[i*4+1] >= 128) ? 255 : 0;
				src->window->mask_temp->pixels[i*4+2] =
					(src->pixels[i*4+2] >= 128) ? 255 : 0;
				src->window->mask_temp->pixels[i*4+3] = 255;
			}
		}
		GraphicsSetSourceSurface(&dst->context.base,
								 &src->window->mask_temp->surface.base, src->x, src->y, &pattern);
		GraphicsPaintWithAlpha(&dst->context.base, src->alpha * (FLOAT_T)0.01);
	}

	if((dst->flags & LAYER_LOCK_OPACITY) != 0)
	{
		RestoreLayerAlpha(dst);
	}
#undef BINALIZE_THRESHOLD
}

void BlendColorReverse_c(LAYER* src, LAYER* dst)
{
#define COLOR_REVERSE_THRESHOLD 0x80
	GRAPHICS_SURFACE_PATTERN pattern;
	int i;

	GraphicsSetOperator(&dst->context.base, GRAPHICS_OPERATOR_OVER);

	if((dst->flags & LAYER_LOCK_OPACITY) != 0)
	{
		SaveLayerAlpha(dst);
	}

	if((src->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0)
	{
		LAYER *mask_source = src->prev;
		(void)memset(src->window->mask->pixels, 0, src->stride * src->height);
		GraphicsSetOperator(&src->window->mask->context.base, GRAPHICS_OPERATOR_OVER);
		for(i=0; i<src->width * src->height; i++)
		{
			if(src->pixels[i*4+3] >= COLOR_REVERSE_THRESHOLD)
			{
				src->window->mask_temp->pixels[i*4+0] = 0xFF - dst->pixels[i*4+0];
				src->window->mask_temp->pixels[i*4+1] = 0xFF - dst->pixels[i*4+1];
				src->window->mask_temp->pixels[i*4+2] = 0xFF - dst->pixels[i*4+2];
				src->window->mask_temp->pixels[i*4+3] = src->pixels[i*4+3];
			}
		}
		GraphicsSetSourceSurface(&src->window->mask->context.base,
								 &src->window->mask_temp->surface.base, src->x, src->y, &pattern);
		GraphicsPaintWithAlpha(&src->window->mask->context.base, src->alpha * (FLOAT_T)0.01);

		while((mask_source->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0)
		{
			mask_source = mask_source->prev;
		}
		if(mask_source == src->window->active_layer)
		{
			GraphicsSetOperator(&src->window->mask_temp->context.base, GRAPHICS_OPERATOR_OVER);
			(void)memcpy(src->window->mask_temp->pixels, mask_source->pixels,
						  mask_source->stride * mask_source->height);
			GraphicsSetSourceSurface(&src->window->mask_temp->context.base,
									 &src->window->work_layer->surface.base, 0, 0, &pattern);
			GraphicsPaint(&src->window->mask_temp->context.base);
			mask_source = src->window->mask_temp;
		}

		GraphicsSetSourceSurface(&dst->context.base,
								 &src->window->mask->surface.base, 0, 0, &pattern);
		GraphicsMaskSurface(&dst->context.base, &mask_source->surface.base, 0, 0);
	}
	else
	{
		for(i=0; i<src->width * src->height; i++)
		{
			if(src->window->mask_temp->pixels[i*4+3] >= COLOR_REVERSE_THRESHOLD)
			{
				dst->pixels[i*4+0] = 0xFF - dst->pixels[i*4+0];
				dst->pixels[i*4+1] = 0xFF - dst->pixels[i*4+1];
				dst->pixels[i*4+2] = 0xFF - dst->pixels[i*4+2];
			}
		}
	}

	if((dst->flags & LAYER_LOCK_OPACITY) != 0)
	{
		RestoreLayerAlpha(dst);
	}
}

void BlendGreater_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_SURFACE_PATTERN pattern;
	int i;

	GraphicsSetOperator(&dst->context.base, GRAPHICS_OPERATOR_OVER);

	if((dst->flags & LAYER_LOCK_OPACITY) != 0)
	{
		SaveLayerAlpha(dst);
	}

	if((src->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0)
	{
		LAYER *mask_source = src->prev;
		(void)memset(src->window->mask->pixels, 0, src->stride * src->height);
		GraphicsSetOperator(&src->window->mask->context.base, GRAPHICS_OPERATOR_OVER);
		for(i=0; i<src->width * src->height; i++)
		{
			if(src->pixels[i*4+3] > dst->pixels[i*4+3])
			{
				dst->pixels[i*4+0] = (uint8)(
					(((int)src->pixels[i*4+0] - (int)dst->pixels[i*4+0])
						* src->pixels[i*4+3] >> 8) + dst->pixels[i*4+0]);
				dst->pixels[i*4+1] = (uint8)(
					(((int)src->pixels[i*4+1] - (int)dst->pixels[i*4+1])
						 * src->pixels[i*4+3] >> 8) + dst->pixels[i*4+1]);
				dst->pixels[i*4+2] = (uint8)(
					(((int)src->pixels[i*4+2] - (int)dst->pixels[i*4+2])
						 * src->pixels[i*4+3] >> 8) + dst->pixels[i*4+2]);
			}
		}
		GraphicsSetSourceSurface(&src->window->mask->context.base,
								 &src->window->mask_temp->surface.base, src->x, src->y, &pattern);
		GraphicsPaintWithAlpha(&src->window->mask->context.base, src->alpha * (FLOAT_T)0.01);

		while((mask_source->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0)
		{
			mask_source = mask_source->prev;
		}
		if(mask_source == src->window->active_layer)
		{
			GraphicsSetOperator(&src->window->mask_temp->context.base, GRAPHICS_OPERATOR_OVER);
			(void)memcpy(src->window->mask_temp->pixels, mask_source->pixels,
						  mask_source->stride * mask_source->height);
			GraphicsSetSourceSurface(&src->window->mask_temp->context.base,
									 &src->window->work_layer->surface.base, 0, 0, &pattern);
			GraphicsPaint(&src->window->mask_temp->context.base);
			mask_source = src->window->mask_temp;
		}

		GraphicsSetSourceSurface(&dst->context.base,
								 &src->window->mask->surface.base, 0, 0, &pattern);
		GraphicsMaskSurface(&dst->context.base, &mask_source->surface.base, 0, 0);
	}
	else
	{
		for(i=0; i<src->width * src->height; i++)
		{
			if(src->pixels[i*4+3] > dst->pixels[i*4+3])
			{
				dst->pixels[i*4+0] = (uint8)(
					(((int)src->pixels[i*4+0] - (int)dst->pixels[i*4+0])
						 * src->pixels[i*4+3] >> 8) + dst->pixels[i*4+0]);
				dst->pixels[i*4+1] = (uint8)(
					(((int)src->pixels[i*4+1] - (int)dst->pixels[i*4+1])
						 * src->pixels[i*4+3] >> 8) + dst->pixels[i*4+1]);
				dst->pixels[i*4+2] = (uint8)(
					(((int)src->pixels[i*4+2] - (int)dst->pixels[i*4+2])
						 * src->pixels[i*4+3] >> 8) + dst->pixels[i*4+2]);
			}
		}
	}

	if((dst->flags & LAYER_LOCK_OPACITY) != 0)
	{
		RestoreLayerAlpha(dst);
	}
}

void BlendAlphaMinus_c(LAYER* src, LAYER* dst)
{
	// GRAPHICS_SURFACE_PATTERN pattern;
	// GraphicsSetOperator(&dst->context.base, GRAPHICS_OPERATOR_DEST_OUT);
	// GraphicsSetSourceSurface(&dst->context.base, &src->surface.base, src->x, src->y, &pattern);
	// GraphicsPaintWithAlpha(&dst->context.base, src->alpha * 0.01);

	uint8 *src_pixel, *dst_pixel;
	FLOAT_T alpha;
	int i, j;

	for(i=0; i<dst->height; i++)
	{
		for(j = 0, src_pixel = &src->pixels[i * src->stride], dst_pixel = &dst->pixels[i * dst->stride];
			j < dst->width; j++, src_pixel += 4, dst_pixel += 4)
		{
			alpha = (1.0 - src_pixel[3] * DIV_PIXEL) * dst_pixel[3] * DIV_PIXEL;
			dst_pixel[3] = (uint8)(alpha * 255.5);
			dst_pixel[0] = (dst_pixel[0] > dst_pixel[3]) ? dst_pixel[3] : dst_pixel[0];
			dst_pixel[1] = (dst_pixel[1] > dst_pixel[3]) ? dst_pixel[3] : dst_pixel[1];
			dst_pixel[2] = (dst_pixel[2] > dst_pixel[3]) ? dst_pixel[3] : dst_pixel[2];
		}
	}
}

void BlendSource_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_SURFACE_PATTERN pattern;
	DRAW_WINDOW *canvas = src->window;
	if((src->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0)
	{
		LAYER *mask_source = src->prev;
		(void)memset(canvas->mask->pixels, 0, src->stride * src->height);
		GraphicsSetOperator(&canvas->mask->context.base, GRAPHICS_OPERATOR_OVER);
		GraphicsSetSourceSurface(&canvas->mask->context.base,
								 &src->surface.base, src->x, src->y, &pattern);
		GraphicsPaintWithAlpha(&canvas->mask->context.base, src->alpha * (FLOAT_T)0.01);

		while((mask_source->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0)
		{
			mask_source = mask_source->prev;
		}
		if(mask_source == canvas->active_layer)
		{
			GraphicsSetOperator(&canvas->mask_temp->context.base, GRAPHICS_OPERATOR_OVER);
			(void)memcpy(canvas->mask_temp->pixels, mask_source->pixels,
						  mask_source->stride * mask_source->height);
			GraphicsSetSourceSurface(&canvas->mask_temp->context.base,
									 &canvas->work_layer->surface.base, 0, 0, &pattern);
			GraphicsPaint(&canvas->mask_temp->context.base);
			mask_source = canvas->mask_temp;
		}

		GraphicsSetSourceSurface(&dst->context.base, &canvas->mask->surface.base,
								 0, 0, &pattern);
		GraphicsMaskSurface(&dst->context.base, &mask_source->surface.base, 0, 0);
	}
	else
	{
		GraphicsSetSourceSurface(&dst->context.base, &src->surface.base,
								 src->x, src->y, &pattern);
		GraphicsPaintWithAlpha(&dst->context.base, src->alpha * (FLOAT_T)0.01);
	}
}

void BlendAtop_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_SURFACE_PATTERN pattern;
	GraphicsSetOperator(&dst->context.base, GRAPHICS_OPERATOR_ATOP);
	GraphicsSetSourceSurface(&dst->context.base, &src->surface.base,
								src->x,  src->y, &pattern);
	GraphicsPaintWithAlpha(&dst->context.base, src->alpha * (FLOAT_T)0.01);
}

void BlendOver_c(LAYER* src, LAYER* dst)
{
	GRAPHICS_SURFACE_PATTERN pattern;
	GraphicsSetOperator(&dst->context.base, GRAPHICS_OPERATOR_OVER);
	GraphicsSetSourceSurface(&dst->context.base, &src->surface.base,
							 src->x,  src->y, &pattern);
	GraphicsPaintWithAlpha(&dst->context.base, src->alpha * (FLOAT_T)0.01);
}

void BlendSourceOver_c(LAYER* src, LAYER* dst)
{
	uint8 *pixel, *dst_pixel;
	uint8 alpha;
	int i;

	for(i=0; i<src->width * src->height; i++)
	{
		pixel = &src->pixels[i*4];
		dst_pixel = &dst->pixels[i*4];
		alpha = pixel[3];
		dst_pixel[0] = ((0xFF - alpha+1)*dst_pixel[0] + alpha*pixel[0]) >> 8;
		dst_pixel[1] = ((0xFF - alpha+1)*dst_pixel[1] + alpha*pixel[1]) >> 8;
		dst_pixel[2] = ((0xFF - alpha+1)*dst_pixel[2] + alpha*pixel[2]) >> 8;
		dst_pixel[3] = ((0xFF - alpha+1)*dst_pixel[3] + alpha*pixel[3]) >> 8;
	}
}

void SetLayerBlendFunctionsArray(void (*layer_blend_functions[])(LAYER* src, LAYER* dst))
{
	layer_blend_functions[LAYER_BLEND_NORMAL] = BlendNormal_c;
	layer_blend_functions[LAYER_BLEND_ADD] = BlendAdd_c;
	layer_blend_functions[LAYER_BLEND_MULTIPLY] = BlendMultiply_c;
	layer_blend_functions[LAYER_BLEND_SCREEN] = BlendScreen_c;
	layer_blend_functions[LAYER_BLEND_OVERLAY] = BlendOverlay_c;
	layer_blend_functions[LAYER_BLEND_LIGHTEN] = BlendLighten_c;
	layer_blend_functions[LAYER_BLEND_DARKEN] = BlendDarken_c;
	layer_blend_functions[LAYER_BLEND_DODGE] = BlendDodge_c;
	layer_blend_functions[LAYER_BLEND_BURN] = BlendBurn_c;
	layer_blend_functions[LAYER_BLEND_HARD_LIGHT] = BlendHardLight_c;
	layer_blend_functions[LAYER_BLEND_SOFT_LIGHT] = BlendSoftLight_c;
	layer_blend_functions[LAYER_BLEND_DIFFERENCE] = BlendDifference_c;
	layer_blend_functions[LAYER_BLEND_EXCLUSION] = BlendExclusion_c;
	layer_blend_functions[LAYER_BLEND_HSL_HUE] = BlendHslHue_c;
	layer_blend_functions[LAYER_BLEND_HSL_SATURATION] = BlendHslSaturation_c;
	layer_blend_functions[LAYER_BLEND_HSL_COLOR] = BlendHslColor_c;
	layer_blend_functions[LAYER_BLEND_HSL_LUMINOSITY] = BlendHslLuminosity_c;
	layer_blend_functions[LAYER_BLEND_BINALIZE] = BlendBinalize_c;
	layer_blend_functions[LAYER_BLEND_COLOR_REVERSE] = BlendColorReverse_c;
	layer_blend_functions[LAYER_BLEND_GREATER] = BlendGreater_c;
	layer_blend_functions[LAYER_BLEND_ALPHA_MINUS] = BlendAlphaMinus_c;
	layer_blend_functions[LAYER_BLEND_SOURCE] = BlendSource_c;
	layer_blend_functions[LAYER_BLEND_ATOP] = BlendAtop_c;
	layer_blend_functions[LAYER_BLEND_SOURCE_OVER] = BlendSourceOver_c;
	layer_blend_functions[LAYER_BLEND_OVER] = BlendOver_c;
}

#define SIZE_PIXEL_MARGIN 0.999

void SaveLayerAlphaPart(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	uint8 *pixels;
	int x, y;
	int width, height;
	uint8 *buffer;
	int i;
	
	pixels = dst->pixels;
	
	x = (int)update->x;
	y = (int)update->y;
	
	width = (int)(update->width + SIZE_PIXEL_MARGIN);
	height = (int)(update->height + SIZE_PIXEL_MARGIN);
	if(x + width > src->width)
	{
		width = src->width - x;
	}
	if(y + height > src->height)
	{
		height = src->height - y;
	}
	
	buffer = src->window->alpha_lock;
	for(i=0; i<height; i++)
	{
		uint8 *pointer;
		int j;
		
		for(j=0, pointer = &pixels[(y+i)*src->stride+x*4+3]; j<width; j++, pointer+=4, buffer++)
		{
			*buffer = *pointer;
		}
	}
}

void RestoreLayerAlphaPart(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	uint8 *pixels;
	int x, y;
	int width, height;
	uint8 *buffer;
	int i;
	
	pixels = dst->pixels;
	
	x = (int)update->x;
	y = (int)update->y;
	
	width = (int)(update->width + SIZE_PIXEL_MARGIN);
	height = (int)(update->height + SIZE_PIXEL_MARGIN);
	if(x + width > src->width)
	{
		width = src->width - x;
	}
	if(y + height > src->height)
	{
		height = src->height - y;
	}
	
	buffer = src->window->alpha_lock;
	for(i=0; i<height; i++)
	{
		uint8 *pointer;
		int j;
		
		for(j=0, pointer = &pixels[(y+i)*src->stride+x*4]; j<width; j++, pointer+=4, buffer++)
		{
			pointer[0] = (pointer[0] > *buffer) ? *buffer : pointer[0];
			pointer[1] = (pointer[1] > *buffer) ? *buffer : pointer[1];
			pointer[2] = (pointer[2] > *buffer) ? *buffer : pointer[2];
			pointer[3] = *buffer;
		}
	}
}

void DummyPartBlend(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{

}

#define DEFAULT_PART_BLEND_OPERATION(BLEND_OPERATION) \
	DRAW_WINDOW *canvas = src->window;  \
\
	GraphicsSetOperator(&update->context.base, (BLEND_OPERATION)); \
\
	if((dst->flags & LAYER_LOCK_OPACITY) != 0)  \
	{   \
		SaveLayerAlphaPart(src, dst, update);   \
	}   \
\
	if((src->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0)  \
	{   \
		LAYER *mask_source = src->prev; \
		GRAPHICS_IMAGE_SURFACE update_surface = {0};	\
		GRAPHICS_DEFAULT_CONTEXT update_context = {0};  \
		GRAPHICS_IMAGE_SURFACE temp_surface = {0};	  \
		GRAPHICS_DEFAULT_CONTEXT update_temp = {0};	 \
		GRAPHICS_SURFACE_PATTERN pattern = {0};		 \
		const GRAPHICS_SURFACE_PATTERN zero_pattern = {0};	\
\
		InitializeGraphicsImageSurfaceForRectangle(&update_surface, &canvas->mask->surface, \
												update->x, update->y, update->width, update->height);   \
		InitializeGraphicsDefaultContext(&update_context, &update_surface, &canvas->app->graphics);	 \
		(void)memset(src->window->mask->pixels, 0, src->stride*src->height);	\
		GraphicsSetOperator(&update_context.base, GRAPHICS_OPERATOR_OVER);	  \
		GraphicsSetSourceSurface(&update_context.base, &src->surface.base, - update->x, - update->y, &pattern); \
		GraphicsPaintWithAlpha(&update_context.base, src->alpha * (FLOAT_T)0.01);   \
\
		while((mask_source->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0)   \
		{   \
			mask_source = mask_source->prev;	\
		}   \
		if(mask_source == canvas->active_layer) \
		{   \
			InitializeGraphicsImageSurfaceForRectangle(&temp_surface, &canvas->mask_temp->surface, update->x, update->y,	\
													update->width, update->height); \
			GraphicsSetOperator(&update_temp.base, GRAPHICS_OPERATOR_OVER); \
			(void)memcpy(canvas->mask_temp->pixels, mask_source->pixels, mask_source->stride*mask_source->height);  \
			pattern = zero_pattern;	\
			GraphicsSetSourceSurface(&update_temp.base, &canvas->work_layer->surface.base, - update->x, - update->y, &pattern); \
			GraphicsPaint(&update_temp.base);   \
		}   \
\
		pattern = zero_pattern;	\
		GraphicsSetSourceSurface(&update->context.base, &update_surface.base, 0, 0, &pattern);  \
		GraphicsMaskSurface(&update->context.base, &mask_source->surface.base, - update->x, -update->y);	\
	}   \
	else	\
	{   \
		GRAPHICS_SURFACE_PATTERN pattern = {0};   \
		GraphicsSetSourceSurface(&update->context.base, &src->surface.base, - update->x, - update->y, &pattern);	\
		GraphicsPaintWithAlpha(&update->context.base, src->alpha * (FLOAT_T)0.01);  \
	}   \
\
	if((dst->flags & LAYER_LOCK_OPACITY) != 0)  \
	{   \
		RestoreLayerAlphaPart(src, dst, update);	\
	}   \
	
void PartBlendNormal_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	// DEFAULT_PART_BLEND_OPERATION(GRAPHICS_OPERATOR_OVER);

	DRAW_WINDOW *canvas = src->window;  \
\
	GraphicsSetOperator(&update->context.base, (GRAPHICS_OPERATOR_OVER)); \
\
	if((dst->flags & LAYER_LOCK_OPACITY) != 0)  \
	{   \
		SaveLayerAlphaPart(src, dst, update);   \
	}   \
\
	if((src->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0)  \
	{   \
		LAYER *mask_source = src->prev; \
		GRAPHICS_IMAGE_SURFACE update_surface = {0};	\
		GRAPHICS_DEFAULT_CONTEXT update_context = {0};  \
		GRAPHICS_IMAGE_SURFACE temp_surface = {0};	  \
		GRAPHICS_DEFAULT_CONTEXT update_temp = {0};	 \
		GRAPHICS_SURFACE_PATTERN pattern = {0};		 \
		const GRAPHICS_SURFACE_PATTERN zero_pattern = {0};	\
\
		InitializeGraphicsImageSurfaceForRectangle(&update_surface, &canvas->mask->surface, \
												update->x, update->y, update->width, update->height);   \
		InitializeGraphicsDefaultContext(&update_context, &update_surface, &canvas->app->graphics);	 \
		(void)memset(src->window->mask->pixels, 0, src->stride*src->height);	\
		GraphicsSetOperator(&update_context.base, GRAPHICS_OPERATOR_OVER);	  \
		GraphicsSetSourceSurface(&update_context.base, &src->surface.base, - update->x, - update->y, &pattern); \
		GraphicsPaintWithAlpha(&update_context.base, src->alpha * (FLOAT_T)0.01);   \
\
		while((mask_source->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0)   \
		{   \
			mask_source = mask_source->prev;	\
		}   \
		if(mask_source == canvas->active_layer) \
		{   \
			InitializeGraphicsImageSurfaceForRectangle(&temp_surface, &canvas->mask_temp->surface, update->x, update->y,	\
													update->width, update->height); \
			InitializeGraphicsDefaultContext(&update_temp, &temp_surface, &canvas->app->graphics); \
			GraphicsSetOperator(&update_temp.base, GRAPHICS_OPERATOR_OVER); \
			(void)memcpy(canvas->mask_temp->pixels, mask_source->pixels, mask_source->stride*mask_source->height);  \
			pattern = zero_pattern;	\
			GraphicsSetSourceSurface(&update_temp.base, &canvas->work_layer->surface.base, - update->x, - update->y, &pattern); \
			GraphicsPaint(&update_temp.base);   \
		}   \
\
		pattern = zero_pattern;	\
		GraphicsSetSourceSurface(&update->context.base, &update_surface.base, 0, 0, &pattern);  \
		GraphicsMaskSurface(&update->context.base, &mask_source->surface.base, - update->x, -update->y);	\
	}   \
	else	\
	{   \
		GRAPHICS_SURFACE_PATTERN pattern = {0};   \
		GraphicsSetSourceSurface(&update->context.base, &src->surface.base, - update->x, - update->y, &pattern);	\
		GraphicsPaintWithAlpha(&update->context.base, src->alpha * (FLOAT_T)0.01);  \
	}   \
\
	if((dst->flags & LAYER_LOCK_OPACITY) != 0)  \
	{   \
		RestoreLayerAlphaPart(src, dst, update);	\
	}   \
}

void PartBlendAdd_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	DEFAULT_PART_BLEND_OPERATION(GRAPHICS_OPERATOR_ADD);
}

void PartBlendMultiply_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	DEFAULT_PART_BLEND_OPERATION(GRAPHICS_OPERATOR_MULTIPLY);
}

void PartBlendScreen_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	DEFAULT_PART_BLEND_OPERATION(GRAPHICS_OPERATOR_SCREEN);
}

void PartBlendOverlay_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	DEFAULT_PART_BLEND_OPERATION(GRAPHICS_OPERATOR_OVERLAY);
}

void PartBlendLighten_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	DEFAULT_PART_BLEND_OPERATION(GRAPHICS_OPERATOR_LIGHTEN);
}

void PartBlendDarken_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	DEFAULT_PART_BLEND_OPERATION(GRAPHICS_OPERATOR_DARKEN);
}

void PartBlendDodge_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	DEFAULT_PART_BLEND_OPERATION(GRAPHICS_OPERATOR_COLOR_DODGE);
}

void PartBlendBurn_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	DEFAULT_PART_BLEND_OPERATION(GRAPHICS_OPERATOR_COLOR_BURN);
}

void PartBlendHardLight_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	DEFAULT_PART_BLEND_OPERATION(GRAPHICS_OPERATOR_HARD_LIGHT);
}

void PartBlendSoftLight_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	DEFAULT_PART_BLEND_OPERATION(GRAPHICS_OPERATOR_SOFT_LIGHT);
}

void PartBlendDifference_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	DEFAULT_PART_BLEND_OPERATION(GRAPHICS_OPERATOR_DIFFERENCE);
}

void PartBlendExclusion_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	DEFAULT_PART_BLEND_OPERATION(GRAPHICS_OPERATOR_EXCLUSION);
}

void PartBlendHslHue_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	DEFAULT_PART_BLEND_OPERATION(GRAPHICS_OPERATOR_HSL_HUE);
}

void PartBlendHslSaturation_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	DEFAULT_PART_BLEND_OPERATION(GRAPHICS_OPERATOR_HSL_SATURATION);
}

void PartBlendHslColor_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	DEFAULT_PART_BLEND_OPERATION(GRAPHICS_OPERATOR_HSL_COLOR);
}

void PartBlendHslLuminosity_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	DEFAULT_PART_BLEND_OPERATION(GRAPHICS_OPERATOR_HSL_LUMINOSITY);
}

void PartBlendBinalize_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
#define BINALIZE_THRESHOLD 128
	DRAW_WINDOW *canvas;
	int start_x, start_y;
	int width, height;
	int i, j;
	
	canvas = src->window;
	
	if((dst->flags & LAYER_LOCK_OPACITY) != 0)
	{
		SaveLayerAlphaPart(src, dst, update);
	}
	
	start_x = (int)update->x;
	width = (int)(update->x + update->width + SIZE_PIXEL_MARGIN);
	if(start_x < 0)
	{
		width += start_x; 
		start_x = 0;
	}
	if(width > src->width)
	{
		width = src->width;
	}
	else if(width <= 0)
	{
		goto function_end;
	}
	
	start_y = (int)update->y;
	height = (int)(update->y + update->height + SIZE_PIXEL_MARGIN);
	if(start_y < 0)
	{
		height += start_y;
		start_y = 0;
	}
	if(height > src->height)
	{
		height = src->height;
	}
	else if(height <= 0)
	{
		goto function_end;
	}
	
	GraphicsSetOperator(&update->context.base, GRAPHICS_OPERATOR_OVER);
	if((src->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0)
	{
		LAYER *mask_source = src->prev;
		
		GRAPHICS_IMAGE_SURFACE update_surface = {0};
		GRAPHICS_DEFAULT_CONTEXT update_context = {0};
		GRAPHICS_IMAGE_SURFACE temp_surface = {0};
		GRAPHICS_DEFAULT_CONTEXT temp_context = {0};
		GRAPHICS_SURFACE_PATTERN pattern;
		
		InitializeGraphicsImageSurfaceForRectangle(&update_surface, &canvas->mask->surface,
							update->x, update->y, update->width, update->height);
		InitializeGraphicsDefaultContext(&update_context, &update_surface, &canvas->app->graphics);
		InitializeGraphicsImageSurfaceForRectangle(&temp_surface, &canvas->mask_temp->surface,
							update->x, update->y, update->width, update->height);
		InitializeGraphicsDefaultContext(&temp_context, &temp_surface, &canvas->app->graphics);
		
		(void)memset(canvas->mask->pixels, 0, src->stride * src->height);
		GraphicsSetOperator(&update_context.base, GRAPHICS_OPERATOR_OVER);
		for(i=0; i<height; i++)
		{
			for(j=0; j<width; j++)
			{
				if(src->pixels[(start_y+i)*src->stride+(start_x+j)*4+3] >= BINALIZE_THRESHOLD)
				{
					canvas->mask_temp->pixels[(start_y+i)*src->stride+(start_x+j)*4+0]
						= (src->pixels[(start_y+i)*src->stride+(start_x+j)*4+0] >= BINALIZE_THRESHOLD) ? 255 : 0;
					canvas->mask_temp->pixels[(start_y+i)*src->stride+(start_x+j)*4+1]
						= (src->pixels[(start_y+i)*src->stride+(start_x+j)*4+1] >= BINALIZE_THRESHOLD) ? 255 : 0;
					canvas->mask_temp->pixels[(start_y+i)*src->stride+(start_x+j)*4+2]
						= (src->pixels[(start_y+i)*src->stride+(start_x+j)*4+2] >= BINALIZE_THRESHOLD) ? 255 : 0;
					canvas->mask_temp->pixels[(start_y+i)*src->stride+(start_x+j)*4+3] = 255;
				}
				else
				{
					canvas->mask_temp->pixels[(start_y+i)*src->stride+(start_x+j)*4+0] = 0;
					canvas->mask_temp->pixels[(start_y+i)*src->stride+(start_x+j)*4+1] = 0;
					canvas->mask_temp->pixels[(start_y+i)*src->stride+(start_x+j)*4+2] = 0;
					canvas->mask_temp->pixels[(start_y+i)*src->stride+(start_x+j)*4+3] = 0;
				}
			}
		}
		
		GraphicsSetSourceSurface(&update_context.base, &canvas->mask_temp->surface.base, - update->x, - update->y, &pattern);
		GraphicsPaintWithAlpha(&update_context.base, src->alpha * (FLOAT_T)0.01);
		
		while((mask_source->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0)
		{
			mask_source = mask_source->prev;
		}
		if(mask_source == canvas->active_layer)
		{
			GraphicsSetOperator(&temp_context.base, GRAPHICS_OPERATOR_OVER);
			(void)memcpy(canvas->mask_temp->pixels, mask_source->pixels,
							mask_source->stride*mask_source->height);
			GraphicsSetSourceSurface(&temp_context.base, &canvas->work_layer->surface.base,
										- update->x, - update->y, &pattern);
			GraphicsPaint(&temp_context.base);
			// mask_source = canvas->mask_temp;
		}
		
		GraphicsSetSourceSurface(&update->context.base, &update_surface.base, 0, 0, &pattern);
		GraphicsMaskSurface(&update->context.base, &temp_surface.base, 0, 0);
	}
	else
	{
		GRAPHICS_SURFACE_PATTERN pattern;
		for(i=0; i<height; i++)
		{
			for(j=0; j<width; j++)
			{
				if(src->pixels[(start_y+i)*src->stride+(start_x+j)*4+3] >= BINALIZE_THRESHOLD)
				{
					canvas->mask_temp->pixels[(start_y+i)*src->stride+(start_x+j)*4+0]
						= (src->pixels[(start_y+i)*src->stride+(start_x+j)*4+0] >= BINALIZE_THRESHOLD) ? 255 : 0;
					canvas->mask_temp->pixels[(start_y+i)*src->stride+(start_x+j)*4+1]
						= (src->pixels[(start_y+i)*src->stride+(start_x+j)*4+1] >= BINALIZE_THRESHOLD) ? 255 : 0;
					canvas->mask_temp->pixels[(start_y+i)*src->stride+(start_x+j)*4+2]
						= (src->pixels[(start_y+i)*src->stride+(start_x+j)*4+2] >= BINALIZE_THRESHOLD) ? 255 : 0;
					canvas->mask_temp->pixels[(start_y+i)*src->stride+(start_x+j)*4+3] = 255;
				}
				else
				{
					canvas->mask_temp->pixels[(start_y+i)*src->stride+(start_x+j)*4+0] = 0;
					canvas->mask_temp->pixels[(start_y+i)*src->stride+(start_x+j)*4+1] = 0;
					canvas->mask_temp->pixels[(start_y+i)*src->stride+(start_x+j)*4+2] = 0;
					canvas->mask_temp->pixels[(start_y+i)*src->stride+(start_x+j)*4+3] = 0;
				}
			}
		}
		GraphicsSetSourceSurface(&update->context.base, &canvas->mask_temp->surface.base,
									- update->x, - update->y, &pattern);
		GraphicsPaintWithAlpha(&update->context.base, src->alpha * (FLOAT_T)0.01);
	}
	
function_end:
	if((dst->flags & LAYER_LOCK_OPACITY) != 0)
	{
		RestoreLayerAlphaPart(src, dst, update);
	}
}

#define PartBlendColorReverse_c DummyPartBlend
#define PartBlendGreater_c DummyPartBlend

void PartBlendAlphaMinus_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	// GRAPHICS_SURFACE_PATTERN pattern;
	// GraphicsSetOperator(&update->context.base, GRAPHICS_OPERATOR_DEST_OUT);
	// GraphicsSetSourceSurface(&update->context.base, &src->surface.base, - update->x, - update->y, &pattern);
	// GraphicsPaintWithAlpha(&update->context.base, src->alpha * (FLOAT_T)0.01);

	int i, j;
	int start_x = (int)update->x, start_y = (int)update->y;
	int end_x = (int)(update->x + update->width+0.5), end_y = (int)(update->y + update->height + 0.5);
	int width, height;

	FLOAT_T alpha;
	uint8 *src_pixel, *dst_pixel;

	if(start_x < 0)
	{
		start_x = 0;
	}
	if(start_y < 0)
	{
		start_y = 0;
	}
	if(end_x > dst->width)
	{
		end_x = dst->width;
	}
	if(end_y > dst->height)
	{
		end_y = dst->height;
	}

	width = end_x - start_x,	height = end_y - start_y;

	for(i=0; i<height; i++)
	{
		for(j=0, src_pixel=&src->pixels[(start_y+i)*src->stride+start_x*4], 
					dst_pixel = &dst->pixels[(start_y+i)*dst->stride+start_x*4]; j<width;
					j++, src_pixel+=4, dst_pixel+=4
		)
		{
			alpha = (1.0-src_pixel[3]*DIV_PIXEL) * dst_pixel[3]*DIV_PIXEL;
			dst_pixel[3] = (uint8)(alpha * 255.5);
			dst_pixel[0] = (dst_pixel[0] > dst_pixel[3]) ? dst_pixel[3] : dst_pixel[0];
			dst_pixel[1] = (dst_pixel[1] > dst_pixel[3]) ? dst_pixel[3] : dst_pixel[1];
			dst_pixel[2] = (dst_pixel[2] > dst_pixel[3]) ? dst_pixel[3] : dst_pixel[2];
		}
	}
}

void PartBlendSource_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	DRAW_WINDOW *canvas = src->window;
	
	GraphicsSetOperator(&update->context.base, GRAPHICS_OPERATOR_SOURCE);
	if((src->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0)
	{
		LAYER *mask_source = src->prev;
		GRAPHICS_IMAGE_SURFACE update_surface = {0};
		GRAPHICS_IMAGE_SURFACE temp_surface = {0};
		GRAPHICS_DEFAULT_CONTEXT update_context = {0};
		GRAPHICS_DEFAULT_CONTEXT temp_context = {0};
		GRAPHICS_SURFACE_PATTERN pattern;
		
		InitializeGraphicsImageSurfaceForRectangle(&temp_surface, &canvas->mask->surface,
													update->x, update->y, update->width, update->height);
		InitializeGraphicsDefaultContext(&update_context, &update_surface, &canvas->app->graphics);
		
		(void)memset(canvas->mask->pixels, 0, src->stride*src->height);
		GraphicsSetOperator(&update_context.base, GRAPHICS_OPERATOR_OVER);
		GraphicsSetSourceSurface(&update_context.base, &src->surface.base, - update->x, - update->y, &pattern);
		GraphicsPaintWithAlpha(&update_context.base, src->alpha * (FLOAT_T)0.01);
		
		while((mask_source->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0)
		{
			mask_source = mask_source->prev;
		}
		if(mask_source == canvas->active_layer)
		{
			InitializeGraphicsImageSurfaceForRectangle(&temp_surface, &canvas->mask_temp->surface,
											update->x, update->y, update->width, update->height);
			InitializeGraphicsDefaultContext(&temp_context, &temp_surface, &canvas->app->graphics);
			(void)memcpy(canvas->mask_temp->pixels, mask_source->pixels, mask_source->stride*mask_source->height);
			GraphicsSetSourceSurface(&temp_context.base, &canvas->work_layer->surface.base, - update->x, - update->y, &pattern);
			GraphicsPaint(&temp_context.base);
		}
		
		GraphicsSetSourceSurface(&update->context.base, &update_surface.base, 0, 0, &pattern);
		GraphicsMaskSurface(&update->context.base, &mask_source->surface.base, - update->x, - update->y);
	}
	else
	{
		GRAPHICS_SURFACE_PATTERN pattern;
		GraphicsSetSourceSurface(&update->context.base, &src->surface.base, - update->x, - update->y, &pattern);
		GraphicsPaintWithAlpha(&update->context.base, src->alpha * (FLOAT_T)0.01);
	}
}

void PartBlendAtop_c(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update)
{
	GRAPHICS_SURFACE_PATTERN pattern;
	GraphicsSetOperator(&update->context.base, GRAPHICS_OPERATOR_ATOP);
	GraphicsSetSourceSurface(&update->context.base, &src->surface.base, - update->x, - update->y, &pattern);
	GraphicsPaintWithAlpha(&update->context.base, src->alpha * (FLOAT_T)0.01);
}

#define PartBlendSourceOver_c DummyPartBlend

void SetPartLayerBlendFunctionsArray(void (*layer_part_blend_functions[])(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update))
{
	layer_part_blend_functions[LAYER_BLEND_NORMAL] = PartBlendNormal_c;
	layer_part_blend_functions[LAYER_BLEND_ADD] = PartBlendAdd_c;
	layer_part_blend_functions[LAYER_BLEND_MULTIPLY] = PartBlendMultiply_c;
	layer_part_blend_functions[LAYER_BLEND_SCREEN] = PartBlendScreen_c;
	layer_part_blend_functions[LAYER_BLEND_OVERLAY] = PartBlendOverlay_c;
	layer_part_blend_functions[LAYER_BLEND_LIGHTEN] = PartBlendLighten_c;
	layer_part_blend_functions[LAYER_BLEND_DARKEN] = PartBlendDarken_c;
	layer_part_blend_functions[LAYER_BLEND_DODGE] = PartBlendDodge_c;
	layer_part_blend_functions[LAYER_BLEND_BURN] = PartBlendBurn_c;
	layer_part_blend_functions[LAYER_BLEND_HARD_LIGHT] = PartBlendHardLight_c;
	layer_part_blend_functions[LAYER_BLEND_SOFT_LIGHT] = PartBlendSoftLight_c;
	layer_part_blend_functions[LAYER_BLEND_DIFFERENCE] = PartBlendDifference_c;
	layer_part_blend_functions[LAYER_BLEND_EXCLUSION] = PartBlendExclusion_c;
	layer_part_blend_functions[LAYER_BLEND_HSL_HUE] = PartBlendHslHue_c;
	layer_part_blend_functions[LAYER_BLEND_HSL_SATURATION] = PartBlendHslSaturation_c;
	layer_part_blend_functions[LAYER_BLEND_HSL_COLOR] = PartBlendHslColor_c;
	layer_part_blend_functions[LAYER_BLEND_HSL_LUMINOSITY] = PartBlendHslLuminosity_c;
	layer_part_blend_functions[LAYER_BLEND_BINALIZE] = PartBlendBinalize_c;
	layer_part_blend_functions[LAYER_BLEND_COLOR_REVERSE] = PartBlendColorReverse_c;
	layer_part_blend_functions[LAYER_BLEND_GREATER] = PartBlendGreater_c;
	layer_part_blend_functions[LAYER_BLEND_ALPHA_MINUS] = PartBlendAlphaMinus_c;
	layer_part_blend_functions[LAYER_BLEND_SOURCE] = PartBlendSourceOver_c;
	layer_part_blend_functions[LAYER_BLEND_ATOP] = PartBlendAtop_c;
	layer_part_blend_functions[LAYER_BLEND_SOURCE_OVER] = PartBlendSourceOver_c;
	layer_part_blend_functions[LAYER_BLEND_OVER] = PartBlendNormal_c;
}

#ifdef __cplusplus
}
#endif
