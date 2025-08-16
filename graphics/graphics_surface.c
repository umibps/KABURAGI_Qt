#include <string.h>
#include "graphics_surface.h"
#include "graphics_types.h"
#include "graphics_private.h"
#include "graphics_status.h"
#include "graphics_pattern.h"
#include "graphics_context.h"
#include "graphics.h"
#include "graphics_matrix.h"
#include "graphics_inline.h"
#include "../pixel_manipulate/pixel_manipulate.h"
#include "../pixel_manipulate/pixel_manipulate_format.h"
#include "../pixel_manipulate/pixel_manipulate_composite.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

static eGRAPHICS_STATUS GraphicsSurfaceFlushOne(GRAPHICS_SURFACE* surface, unsigned int flags);

ePIXEL_MANIPULATE_FORMAT GetPixelManipulateFormatFromGraphicsFormat(eGRAPHICS_FORMAT format)
{
	switch(format)
	{
	case GRAPHICS_FORMAT_ARGB32:
		return PIXEL_MANIPULATE_FORMAT_A8R8G8B8;
	case GRAPHICS_FORMAT_RGB24:
		return PIXEL_MANIPULATE_FORAMT_R8G8B8;
	case GRAPHICS_FORMAT_A8:
		return PIXEL_MANIPULATE_FORMAT_A8;
	}
	return PIXEL_MANIPULATE_FORMAT_INVALID;
}

eGRAPHICS_FORMAT GetGraphicsFormatFromPixelManipulateFormat(ePIXEL_MANIPULATE_FORMAT format)
{
	switch(format)
	{
	case PIXEL_MANIPULATE_FORMAT_A8R8G8B8:
		return GRAPHICS_FORMAT_ARGB32;
	case PIXEL_MANIPULATE_FORAMT_R8G8B8:
		return GRAPHICS_FORMAT_RGB24;
	case PIXEL_MANIPULATE_FORMAT_A8:
		return GRAPHICS_FORMAT_A8;
	}
	return GRAPHICS_FORMAT_INVALID;
}

static void CopyTransformedPattern(
	GRAPHICS_PATTERN* pattern,
	const GRAPHICS_PATTERN* original,
	const GRAPHICS_MATRIX* matrix_inverse
)
{
	InitializeGraphicsPatternStaticCopy(pattern, original);

	if(FALSE == GRAPHICS_MATRIX_IS_IDENTITY(matrix_inverse))
	{
		GraphicsPatternTransform(pattern, matrix_inverse);
	}
}

static void GraphicsSurfaceFinishSnapshots(GRAPHICS_SURFACE* surface)
{
	surface->finishing = TRUE;
	(void)GraphicsSurfaceFlushOne(surface, 0);
}

INLINE int ReleaseGraphicsSurface(GRAPHICS_SURFACE* surface)
{
	if(surface == NULL || GRAPHICS_REFERENCE_COUNT_IS_INVALID(surface))
	{
		return FALSE;
	}

	// ASSERT(GRAPHICS_REFERENCE_COUNT_HAS_REFERENCE(surface));

	if(GRAPHICS_REFERENCE_COUNT_DEC_AND_TEST(surface) == FALSE)
	{
		return FALSE;
	}

	ASSERT(surface->snapshot_of == NULL);

	if(surface->finished == FALSE)
	{
		GraphicsSurfaceFinishSnapshots(surface);

		if(GRAPHICS_REFERENCE_COUNT_GET_VALUE(surface) != FALSE)
		{
			return FALSE;
		}

		GraphicsSurfaceFinish(surface);
	}


	if(surface->damage != NULL)
	{
		DestroyGraphicsDamage(surface->damage);
	}

	// deffered cario_device_destroy
	return TRUE;
}

void GraphicsSurfaceFinish(GRAPHICS_SURFACE* surface)
{
	if(surface == NULL)
	{
		return;
	}

	if(surface->reference_count < 0)
	{
		return;
	}

	if(surface->finished)
	{
		return;
	}

	surface->finishing	= TRUE;
	if(surface->backend->finish != NULL)
	{
		surface->status = surface->backend->finish(surface);
	}

	surface->finished = TRUE;
	DestroyGraphicsSurface(surface);
}

void DestroyGraphicsSurface(GRAPHICS_SURFACE* surface)
{
	int free_memory;
	free_memory = ReleaseGraphicsSurface(surface);
	if(free_memory != FALSE && surface->own_memory)
	{
		MEM_FREE_FUNC(surface);
	}
}

static void GraphicsSurfaceDetachSnapshot(GRAPHICS_SURFACE* snapshot)
{
	ASSERT(snapshot->snapshot_of != NULL);

	snapshot->snapshot_of = NULL;
	DeleteGraphicsList(&snapshot->snapshot);

	if(snapshot->snapshot_detach != NULL)
	{
		snapshot->snapshot_detach(snapshot);
	}
}

static int GraphicsSurfaceHasSnapshots(GRAPHICS_SURFACE* surface)
{
	return !GraphicsListIsEmpty(&surface->snapshots);
}

static void GraphicsSurfaceDetachSnapshots(GRAPHICS_SURFACE* surface)
{
	while(GraphicsSurfaceHasSnapshots(surface))
	{
		GraphicsSurfaceDetachSnapshot((GRAPHICS_SURFACE*)surface->snapshots.next);
	}
}

static INLINE eGRAPHICS_STATUS GraphicsSurfaceFlushInline(GRAPHICS_SURFACE* surface, unsigned int flags)
{
	eGRAPHICS_STATUS status = GRAPHICS_STATUS_SUCCESS;
	if(surface->backend->flush != NULL)
	{
		status = surface->backend->flush(surface, flags);
	}
	return status;
}

static eGRAPHICS_STATUS GraphicsSurfaceFlush(GRAPHICS_SURFACE* surface, unsigned int flags)
{
	GraphicsSurfaceDetachSnapshots(surface);
	if(surface->snapshot_of != NULL)
	{
		GraphicsSurfaceDetachSnapshot(surface);
		// Deffered cairo_surface_mime_data
	}
	return GraphicsSurfaceFlushInline(surface, flags);
}

static eGRAPHICS_STATUS GraphicsSurfaceBeginModification(GRAPHICS_SURFACE* surface)
{
	ASSERT(surface->status == GRAPHICS_STATUS_SUCCESS);
	ASSERT(surface->finished == FALSE);

	return GraphicsSurfaceFlushOne(surface, 1);
}

void GraphicsSurfaceSetFallbackResolution(GRAPHICS_SURFACE* surface, FLOAT_T x_pixels_per_inch, FLOAT_T y_pixels_per_inch)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(surface->status))
	{
		return;
	}

	//ASSERT(surface->snap_shot_of == NULL);

	if(UNLIKELY(surface->finished))
	{
		GraphicsSurfaceSetError(surface, GRAPHICS_INTEGER_STATUS_SURFACE_FINISHED);
		return;
	}

	if(x_pixels_per_inch <= 0 || y_pixels_per_inch <= 0)
	{
		GraphicsSurfaceSetError(surface, GRAPHICS_INTEGER_STATUS_INVALID_ID_MATRIX);
		return;
	}

	status = GraphicsSurfaceBeginModification(surface);
	if(UNLIKELY(status))
	{
		GraphicsSurfaceSetError(surface, status);
		return;
	}

	surface->x_fallback_resolution = x_pixels_per_inch;
	surface->y_fallback_resolution = y_pixels_per_inch;
}

eGRAPHICS_STATUS GraphicsSurfaceSetError(GRAPHICS_SURFACE* surface, eGRAPHICS_INTEGER_STATUS status)
{
	if(status == GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO)
	{
		status = GRAPHICS_INTEGER_STATUS_SUCCESS;
	}

	if(status == GRAPHICS_INTEGER_STATUS_SUCCESS
			|| status >= (int)GRAPHICS_INTEGER_STATUS_LAST_STATUS)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	// TODO _cairo_status_set_error, return cairo_error(status);

	return GRAPHICS_STATUS_INVALID;
}

static void GraphicsSurfaceCopySimilarProperties(GRAPHICS_SURFACE* surface, GRAPHICS_SURFACE* other)
{
	if(other->has_font_options != FALSE || other->backend != surface->backend)
	{
		// TODO cairo_surface_get_font_options
	}

	GraphicsSurfaceSetFallbackResolution(surface, other->x_fallback_resolution, other->y_fallback_resolution);
}

int GraphicsSurfaceGetExtents(GRAPHICS_SURFACE* surface, GRAPHICS_RECTANGLE_INT* extents)
{
	int bounded;

	if(UNLIKELY(surface->status))
	{
		goto zero_extents;
	}
	if(UNLIKELY(surface->finished))
	{
		GraphicsSurfaceSetError(surface, GRAPHICS_STATUS_SURFACE_FINISHED);
		goto zero_extents;
	}

	bounded = FALSE;
	if(surface->backend->get_extents != NULL)
	{
		bounded = surface->backend->get_extents(surface, extents);
	}

	if(bounded == FALSE)
	{
		InitializeGraphicsUnboundedRectangle(extents);
	}

	return bounded;

zero_extents:
	extents->x = extents->y = 0;
	extents->width = extents->height = 0;
	return TRUE;
}

static eGRAPHICS_STATUS PatternHasError(const GRAPHICS_PATTERN* pattern)
{
	const GRAPHICS_SURFACE_PATTERN *surface_pattern;

	if(UNLIKELY(pattern->status))
	{
		return pattern->status;
	}

	if(pattern->type != GRAPHICS_PATTERN_TYPE_SURFACE)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	surface_pattern = (const GRAPHICS_SURFACE_PATTERN*)pattern;
	if(UNLIKELY(surface_pattern->surface->status))
	{
		return surface_pattern->surface->status;
	}

	if(UNLIKELY(surface_pattern->surface->finished))
	{
		return GRAPHICS_STATUS_SURFACE_FINISHED;
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static int NothingToDo(GRAPHICS_SURFACE* surface, eGRAPHICS_OPERATOR op, const GRAPHICS_PATTERN* source)
{
	if(GraphicsPatternIsClear(source))
	{
		if(op == GRAPHICS_OPERATOR_OVER || op == GRAPHICS_OPERATOR_ADD)
		{
			return TRUE;
		}

		if(op == GRAPHICS_OPERATOR_SOURCE)
		{
			op = GRAPHICS_OPERATOR_CLEAR;
		}
	}

	if(op == GRAPHICS_OPERATOR_CLEAR && surface->is_clear)
	{
		return TRUE;
	}

	if(op == GRAPHICS_OPERATOR_ATOP && (surface->content & GRAPHICS_CONTENT_COLOR) == 0)
	{
		return TRUE;
	}

	return FALSE;
}

eGRAPHICS_STATUS GraphicsSurfacePaint(
	GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_CLIP* clip
)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(surface->status))
	{
		return surface->status;
	}
	if(UNLIKELY(surface->finished))
	{
		return GRAPHICS_STATUS_INVALID;
	}

	if(GraphicsClipIsAllClipped(clip))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	status = PatternHasError(source);
	if(UNLIKELY(status))
	{
		return status;
	}

	status = surface->backend->paint(surface, op, source, clip);
	if(status != GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO)
	{
		surface->is_clear = op == GRAPHICS_OPERATOR_CLEAR && clip == NULL;
		surface->serial++;
	}

	return GraphicsSurfaceSetError(surface, status);
}

eGRAPHICS_STATUS GraphicsSurfaceOffsetPaint(
	GRAPHICS_SURFACE* target,
	int x, int y,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_CLIP* clip
)
{
	eGRAPHICS_STATUS status;
	GRAPHICS_CLIP *device_clip = (GRAPHICS_CLIP*)clip;
	GRAPHICS_CLIP local_clip;
	GRAPHICS_PATTERN_UNION source_copy;

	if(UNLIKELY(target->status))
	{
		return target->status;
	}

	if(GraphicsClipIsAllClipped(clip))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(x | y)
	{
		GRAPHICS_MATRIX m;

		device_clip = GraphicsClipCopyWithTranslation(clip, -x, -y, &local_clip);

		InitializeGraphicsMatrixTranslate(&m, x, y);
		CopyTransformedPattern(&source_copy.base, source, &m);
		source = &source_copy.base;
	}

	status = GraphicsSurfacePaint(target, op, source, device_clip);

	if(device_clip != clip)
	{
		GraphicsClipDestroy(device_clip);
	}

	return status;
}

eGRAPHICS_STATUS GraphicsSurfaceOffsetMask(
	GRAPHICS_SURFACE* target,
	int x, int y,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATTERN* mask,
	const GRAPHICS_CLIP* clip
)
{
	eGRAPHICS_STATUS status;
	GRAPHICS_CLIP* device_clip = (GRAPHICS_CLIP*)clip;
	GRAPHICS_CLIP local_clip;
	GRAPHICS_PATTERN_UNION source_copy;
	GRAPHICS_PATTERN_UNION mask_copy;

	if(UNLIKELY(target->status))
	{
		return target->status;
	}

	if(GraphicsClipIsAllClipped(clip))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(x | y)
	{
		GRAPHICS_MATRIX m;

		device_clip = GraphicsClipCopyWithTranslation(clip, -x, -y, &local_clip);

		InitializeGraphicsMatrixTranslate(&m, x, y);
		CopyTransformedPattern(&source_copy.base, source, &m);
		CopyTransformedPattern(&mask_copy.base, mask, &m);
		source = &source_copy.base;
		mask = &mask_copy.base;
	}

	status = GraphicsSurfaceMask(target, op, source, mask, device_clip);

	if(device_clip != clip)
	{
		GraphicsClipDestroy(device_clip);
	}

	return status;
}

eGRAPHICS_STATUS GraphicsSurfaceMask(
	GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATTERN* mask,
	const GRAPHICS_CLIP* clip
)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(surface->status))
	{
		return surface->status;
	}
	if(UNLIKELY(surface->finished))
	{
		return GraphicsSurfaceSetError(surface, GRAPHICS_STATUS_SURFACE_FINISHED);
	}

	if(GraphicsClipIsAllClipped(clip))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(GraphicsPatternIsClear(mask)
		&& GraphicsOperatorBoundedByMask(op))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	status = PatternHasError(mask);
	if(UNLIKELY(status))
	{
		return status;
	}

	if(NothingToDo(surface, op, source))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	status = GraphicsSurfaceBeginModification(surface);
	if(UNLIKELY(status))
	{
		return status;
	}

	status = surface->backend->mask(surface, op, source, mask, clip);
	if(status != GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO)
	{
		surface->is_clear = FALSE;
		surface->serial++;
	}

	return GraphicsSurfaceSetError(surface, status);
}

GRAPHICS_SURFACE* GraphicsSurfaceCreateScratch(
	GRAPHICS_SURFACE* other,
	eGRAPHICS_CONTENT content,
	int width,
	int height,
	const GRAPHICS_COLOR* color
)
{
	GRAPHICS_SURFACE *surface;
	eGRAPHICS_STATUS status;
	GRAPHICS_SOLID_PATTERN pattern;

	if(other->status != GRAPHICS_STATUS_SUCCESS)
	{
		return NULL;
	}

	if(other->backend->create_similar != NULL)
	{
		surface = other->backend->create_similar((void*)other,
					content, width, height);
	}
	if(surface == NULL)
	{
		surface = GraphicsSurfaceCreateSimilarImage(other, GetGraphicsFormatFromContent(content),
					width, height);
	}

	if(UNLIKELY(surface->status))
	{
		return surface;
	}

	if(color != NULL)
	{
		InitializeGraphicsPatternSolid(&pattern, color, other->graphics);
		status = GraphicsSurfacePaint(surface, color == &((GRAPHICS*)other->graphics)->color_transparent ?
			GRAPHICS_OPERATOR_CLEAR : GRAPHICS_OPERATOR_SOURCE, &pattern.base, NULL);
	}

	if(UNLIKELY(status))
	{
		DestroyGraphicsSurface(surface);
		surface = NULL;
	}

	return surface;
}

void GraphicsSurfaceSetDeviceOffset(GRAPHICS_SURFACE* surface, FLOAT_T x_offset, FLOAT_T y_offset)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(surface->status))
	{
		return;
	}

	if(UNLIKELY(surface->finished))
	{
		return;
	}

	status = GraphicsSurfaceBeginModification(surface);
	if(UNLIKELY(status))
	{
		return;
	}

	surface->device_transform.x0 = x_offset;
	surface->device_transform.y0 = y_offset;

	surface->device_transform_inverse = surface->device_transform;
	status = GraphicsMatrixInvert(&surface->device_transform_inverse);

	ASSERT(status == GRAPHICS_STATUS_SUCCESS);

	GraphicsObserversNotify(&surface->device_transform_observers, surface);
}

void GraphicsSurfaceSetDeviceScale(GRAPHICS_SURFACE* surface, FLOAT_T x_scale, FLOAT_T y_scale)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(surface->status))
	{
		return;
	}

	ASSERT(surface->snapshot_of == NULL);

	if(UNLIKELY(surface->finished))
	{
		GraphicsSurfaceSetError(surface, GRAPHICS_STATUS_SURFACE_FINISHED);
		return;
	}

	status = GraphicsSurfaceBeginModification(surface);
	if(UNLIKELY(status))
	{
		GraphicsSurfaceSetError(surface, status);
		return;
	}

	surface->device_transform.xx = x_scale;
	surface->device_transform.yy = y_scale;
	surface->device_transform.xy = 0.0;
	surface->device_transform.yx = 0.0;

	surface->device_transform_inverse = surface->device_transform;
	status = GraphicsMatrixInvert(&surface->device_transform_inverse);
	ASSERT(status == GRAPHICS_STATUS_SUCCESS);

	GraphicsObserversNotify(&surface->device_transform_observers, surface);
}

eGRAPHICS_STATUS GraphicsSurfaceStroke(
	GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* stroke_style,
	const GRAPHICS_MATRIX* matrix,
	const GRAPHICS_MATRIX* matrix_inverse,
	FLOAT_T tolerance,
	eGRAPHICS_ANTIALIAS antialias,
	const GRAPHICS_CLIP* clip
)
{
	eGRAPHICS_INTEGER_STATUS status;

	if(UNLIKELY(surface->status))
	{
		return surface->status;
	}
	if(UNLIKELY(surface->finished))
	{
		return GraphicsSurfaceSetError(surface, GRAPHICS_STATUS_SURFACE_FINISHED);
	}

	if(GraphicsClipIsAllClipped(clip))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	status = (eGRAPHICS_INTEGER_STATUS)PatternHasError(source);
	if(UNLIKELY(status))
	{
		return (eGRAPHICS_STATUS)status;
	}

	if(NothingToDo(surface, op, source))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	status = (eGRAPHICS_INTEGER_STATUS)GraphicsSurfaceBeginModification(surface);
	if(UNLIKELY(status))
	{
		return (eGRAPHICS_STATUS)status;
	}

	status = surface->backend->stroke(surface, op, source, path, stroke_style,
		matrix, matrix_inverse, tolerance, antialias, clip);

	if(status != GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO)
	{
		surface->is_clear = FALSE;
		surface->serial++;
	}

	return GraphicsSurfaceSetError(surface, status);
}

eGRAPHICS_STATUS GraphicsSurfaceFill(
	GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rule,
	FLOAT_T tolerance,
	eGRAPHICS_ANTIALIAS antialias,
	const GRAPHICS_CLIP* clip
)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(surface->status))
	{
		return surface->status;
	}
	if(UNLIKELY(surface->finished))
	{
		return GRAPHICS_STATUS_SURFACE_FINISHED;
	}

	if(GraphicsClipIsAllClipped(clip))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	status = PatternHasError(source);
	if(UNLIKELY(status))
	{
		return status;
	}

	if(FALSE != NothingToDo(surface, op, source))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	status = GraphicsSurfaceBeginModification(surface);
	if(UNLIKELY(status))
	{
		return status;
	}

	status = surface->backend->fill(surface, op, source,
				path, fill_rule, tolerance, antialias, clip);
	if(status != GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO)
	{
		surface->is_clear = FALSE;
		surface->serial++;
	}

	return GraphicsSurfaceSetError(surface, status);
}

eGRAPHICS_STATUS GraphicsSurfaceOffsetFill(
	GRAPHICS_SURFACE* surface,
	int x, int y,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rule,
	FLOAT_T tolerance,
	eGRAPHICS_ANTIALIAS antialias,
	const GRAPHICS_CLIP* clip
)
{
	eGRAPHICS_STATUS status;
	GRAPHICS_PATH_FIXED path_copy, *device_path = (GRAPHICS_PATH_FIXED*)path;
	GRAPHICS_CLIP *device_clip = (GRAPHICS_CLIP*)clip;
	GRAPHICS_CLIP local_clip;
	GRAPHICS_PATTERN_UNION source_copy;

	if(UNLIKELY(surface->status))
	{
		return surface->status;
	}

	if(GraphicsClipIsAllClipped(clip))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(x | y)
	{
		GRAPHICS_MATRIX m;

		device_clip = GraphicsClipCopyWithTranslation(clip, - x, - y, &local_clip);

		status = InitializeGraphicsPathFixedCopy(&path_copy, device_path);
		if(UNLIKELY(status))
		{
			goto FINISH;
		}

		GraphicsPathFixedTranslate(&path_copy, GraphicsFixedFromInteger(-x),
										GraphicsFixedFromInteger(-y));
		device_path = &path_copy;

		InitializeGraphicsMatrixTranslate(&m, x, y);
		CopyTransformedPattern(&source_copy.base, source, &m);
		source = &source_copy.base;
	}

	status = GraphicsSurfaceFill(surface, op, source, device_path,
				fill_rule, tolerance, antialias, device_clip);

FINISH:
	if(device_path != path)
	{
		GraphicsPathFixedFinish(device_path);
	}
	if(device_clip != clip)
	{
		GraphicsClipDestroy(device_clip);
	}

	return status;
}

static INLINE int GraphicsImageSurfaceIsSizeValid(int width, int height)
{
	return 0 <= width && width <= MAX_IMAGE_SIZE && 0 <= height && height <= MAX_IMAGE_SIZE;
}

int GraphicsImageSurfaceInitialize(
	GRAPHICS_IMAGE_SURFACE* surface,
	eGRAPHICS_FORMAT format,
	int width,
	int height,
	int stride,
	void* graphics
)
{
	GRAPHICS *g = (GRAPHICS*)graphics;
	ePIXEL_MANIPULATE_FORMAT pixel_manipulate_format;

	(void)memset(surface, 0, sizeof(*surface));

	pixel_manipulate_format = GetPixelManipulateFormatFromGraphicsFormat(format);

	if(stride <= 0)
	{
		stride = GraphicsImageSurfaceCalculateStride(width, format);
	}
#ifdef _DEBUG
	if(format == PIXEL_MANIPULATE_FORMAT_INVALID)
	{
		(void)fprintf(stderr, "Invalid Format @GraphicsImageSurfaceInitialize\n");
		return FALSE;
	}
#endif

#ifdef _DEBUG
	if(stride % 4 != 0)
	{
		(void)fprintf(stderr, "Invalid Stride @GraphicsImageSurfaceInitialize\n");
		return FALSE;
	}
#endif

	InitializeGraphicsSurface(&surface->base, &g->image_surface_backend,
		GetGraphicsContentFromFormat(format), FALSE, graphics);

	surface->width = width;
	surface->height = height;
	surface->stride = stride;
	surface->format = format;

	return TRUE;
}

int GraphicsFormatBitsPerPixel(eGRAPHICS_FORMAT format)
{
	switch(format)
	{
	case GRAPHICS_FORMAT_ARGB32:
	case GRAPHICS_FORMAT_RGB24:
		return 32;
	case GRAPHICS_FORMAT_A8:
		return 8;
	case GRAPHICS_FORMAT_A1 :
		return 1;
	}

	return 0;
}

int GraphicsFormatStrideForWidth(eGRAPHICS_FORMAT format, int width)
{
	int bpp;

	bpp = GraphicsFormatBitsPerPixel(format);
	if((unsigned)(width) >= (INT32_MAX - 7) / (unsigned) (bpp))
	{
		return -1;
	}

	return GRAPHICS_STRIDE_FOR_WIDTH_BPP(width, bpp);
}

eGRAPHICS_STATUS GraphicsSurfaceAcquireSourceImage(
	GRAPHICS_SURFACE* surface,
	GRAPHICS_IMAGE_SURFACE** image_out,
	void** image_extra
)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(surface->status))
	{
		return surface->status;
	}

	status = surface->backend->acquire_source_image(surface, image_out, image_extra);
	if(UNLIKELY(status))
	{
		return GraphicsSurfaceSetError(surface, status);
	}
	return GRAPHICS_STATUS_SUCCESS;
}

void GraphicsSurfaceAttachSnapshot(
	GRAPHICS_SURFACE* surface,
	GRAPHICS_SURFACE* snapshot,
	GRAPHICS_SURFACE_FUNCTION detach_function
)
{
	GraphicsSurfaceReference(surface);

	if(snapshot->snapshot_of != NULL)
	{
		GraphicsSurfaceDetachSnapshot(snapshot);
	}

	snapshot->snapshot_of = surface;
	snapshot->snapshot_detach = detach_function;

	GraphicsListAdd(&snapshot->snapshot, &surface->snapshots);
}

GRAPHICS_SURFACE* GraphicsImageSurfaceCreate(eGRAPHICS_FORMAT format, int width, int height, void* graphics)
{
	GRAPHICS_IMAGE_SURFACE *surface;
	PIXEL_MANIPULATE_IMAGE *image;

	if(width < 0 || height < 0)
	{
		return NULL;
	}

	surface = (GRAPHICS_IMAGE_SURFACE*)MEM_ALLOC_FUNC(sizeof(*surface));
	if(UNLIKELY(GraphicsImageSurfaceInitialize(surface, format, width, height, -1, graphics)))
	{
		MEM_FREE_FUNC(surface);
		return NULL;
	}
	surface->base.own_memory = 1;

	return (GRAPHICS_SURFACE*)surface;
}

GRAPHICS_SURFACE* GraphicsImageSurfaceCreateWithContent(
	eGRAPHICS_CONTENT content,
	int width,
	int height,
	void* graphics
)
{
	return GraphicsImageSurfaceCreate(GetGraphicsFormatFromContent(content), width, height, graphics);
}

int InitializeGraphicsImageSurfaceWithPixelManipulate(
	GRAPHICS_IMAGE_SURFACE* surface,
	PIXEL_MANIPULATE_IMAGE* pixel_manipulate,
	eGRAPHICS_FORMAT format,
	void* graphics
)
{
	return GraphicsImageSurfaceInitialize(surface, GetGraphicsFormatFromPixelManipulateFormat(format),
		pixel_manipulate->bits.width, pixel_manipulate->bits.height, pixel_manipulate->bits.row_stride, graphics);
}

GRAPHICS_SURFACE* CreateGraphicsImageSurfaceForPixelManipulate(PIXEL_MANIPULATE_IMAGE* pixel_manipulate, eGRAPHICS_FORMAT format, void* graphics)
{
	GRAPHICS_IMAGE_SURFACE *surface = (GRAPHICS_IMAGE_SURFACE*)MEM_ALLOC_FUNC(sizeof(*surface));
	if(FALSE == InitializeGraphicsImageSurfaceWithPixelManipulate(surface, pixel_manipulate, format, graphics))
	{
		return NULL;
	}
	surface->base.own_memory = 1;
	return &surface->base;
}

eGRAPHICS_STATUS InitializeGraphicsImageSurfaceWithPixelManipulateFormat(
	GRAPHICS_IMAGE_SURFACE* surface,
	uint8* data,
	ePIXEL_MANIPULATE_FORMAT format,
	int width,
	int height,
	int stride,
	void* graphics
)
{
	GRAPHICS *g = (GRAPHICS*)graphics;
	int result;
	if(GraphicsImageSurfaceIsSizeValid(width, height) == FALSE)
	{
		return GRAPHICS_STATUS_INVALID_SIZE;
	}

	InitializeGraphicsSurface(&surface->base, &g->image_surface_backend,
		GraphicsFormatToContent(format), FALSE, graphics);

	//result = PixelManipulateImageInitialize(&surface->image, format, width, height,
	//	(uint32*)data, stride, TRUE, &g->pixel_manipulate);
	result = PixelManipulateImageInitialize(&surface->image, format, width, height,
				(uint32*)data, stride, TRUE, &g->pixel_manipulate);
	if(result == FALSE)
	{
		return GRAPHICS_STATUS_INVALID;
	}

	surface->parent = NULL;

	surface->width = width;
	surface->height = height;
	surface->stride = stride;
	surface->base.own_memory = FALSE;
	surface->depth = GraphicsContentDepth(surface->base.content);
	surface->data = data;
	surface->format = format;
	surface->pixel_format = format;

	surface->base.is_clear = surface->width == 0 || surface->height == 0;

	return GRAPHICS_STATUS_SUCCESS;
}

GRAPHICS_SURFACE* GraphicsImageSurfaceCrateWithPixelManipulateFormat(
	uint8* pixel_data,
	ePIXEL_MANIPULATE_FORMAT format,
	int width,
	int height,
	int stride,
	void* graphics
)
{
	GRAPHICS_IMAGE_SURFACE *surface;
	eGRAPHICS_STATUS status;
	int owns_data = FALSE;
	surface = (GRAPHICS_IMAGE_SURFACE*)MEM_ALLOC_FUNC(sizeof(*surface));
	(void)memset(surface, 0, sizeof(*surface));
	if(pixel_data == NULL)
	{
		pixel_data = (uint8*)MEM_CALLOC_FUNC(height * stride, 1);
		owns_data = TRUE;
	}
	status = InitializeGraphicsImageSurfaceWithPixelManipulateFormat(surface,
		pixel_data, format, width, height, stride, graphics);

	if(UNLIKELY(status))
	{
		return NULL;
	}

	if(owns_data)
	{
		surface->owns_data = TRUE;
		surface->data = pixel_data;
	}

	surface->base.own_memory = 1;
	surface->compositor = &((GRAPHICS*)graphics)->spans_compositor.base;
	return &surface->base;
}

eGRAPHICS_STATUS InitializeGraphicsImageSurfaceForData(
	GRAPHICS_IMAGE_SURFACE* surface,
	uint8* data,
	eGRAPHICS_FORMAT format,
	int width,
	int height,
	int stride,
	void* graphics
)
{
	eGRAPHICS_STATUS status;
	int owns_data = FALSE;

	(void)memset(surface, 0, sizeof(*surface));
	if(data == NULL)
	{
		data = (uint8*)MEM_CALLOC_FUNC(height * stride, 1);
		owns_data = TRUE;
	}
	status = InitializeGraphicsImageSurfaceWithPixelManipulateFormat(surface,
		data, format, width, height, stride, graphics);

	surface->compositor = &((GRAPHICS*)graphics)->spans_compositor.base;

	if(owns_data)
	{
		surface->owns_data = TRUE;
		surface->data = data;
	}

	return status;
}

GRAPHICS_SURFACE* GraphicsImageSurfaceCreateForData(
	uint8* data,
	eGRAPHICS_FORMAT format,
	int width,
	int height,
	int stride,
	void* graphics
)
{
	ePIXEL_MANIPULATE_FORMAT pixel_format;
	int minimum_stride;

	if((stride & (GRAPHICS_STRIDE_ALIGNMENT - 1)) != 0)
	{
		return NULL;
	}

	if(FALSE == GraphicsImageSurfaceIsSizeValid(width, height))
	{
		return NULL;
	}

	minimum_stride = GraphicsFormatStrideForWidth(format, width);
	if(stride < 0)
	{
		if(stride > -minimum_stride)
		{
			return NULL;
		}
	}
	else
	{
		if(stride < minimum_stride)
		{
			return NULL;
		}
	}

	pixel_format = (ePIXEL_MANIPULATE_FORMAT)format;
	return GraphicsImageSurfaceCrateWithPixelManipulateFormat(data, pixel_format, width, height, stride, graphics);
}

eGRAPHICS_STATUS GraphicsImageSurfaceFinish(void *abstract_surface)
{
	GRAPHICS_IMAGE_SURFACE *surface = (GRAPHICS_IMAGE_SURFACE*)abstract_surface;

	// if(surface->image != NULL)
	//{
	//	PixelManipulateImageUnreference(&surface->image);
	//	surface->image = NULL;
	//}
	PixelManipulateImageUnreference(&surface->image);

	if(surface->owns_data)
	{
		MEM_FREE_FUNC(surface->data);
		surface->data = NULL;
	}

	if(surface->parent != NULL)
	{
		GRAPHICS_SURFACE *parent = surface->parent;
		surface->parent = NULL;
		DestroyGraphicsSurface(parent);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

GRAPHICS_SURFACE* GraphicsImageSurfaceCreateSimilar(
	void* abstract_other,
	eGRAPHICS_CONTENT content,
	int width,
	int height
)
{
	GRAPHICS_IMAGE_SURFACE *other = (GRAPHICS_IMAGE_SURFACE*)abstract_other;
	GRAPHICS_SURFACE *surface;

	if(GraphicsImageSurfaceIsSizeValid(width, height) == FALSE)
	{
		return NULL;
	}

	if(content == other->base.content)
	{
		surface = GraphicsImageSurfaceCrateWithPixelManipulateFormat(
			NULL, other->pixel_format, width, height, 0, other->base.graphics);
		return surface;
	}

	surface = GraphicsImageSurfaceCreateWithContent(content, width, height, other->base.graphics);

	return surface;
}

GRAPHICS_IMAGE_SURFACE* GraphicsImageSurfaceMapToImage(void* abstract_other, const GRAPHICS_RECTANGLE_INT* extents)
{
	GRAPHICS_IMAGE_SURFACE *other = (GRAPHICS_IMAGE_SURFACE*)abstract_other;
	GRAPHICS_SURFACE *surface;
	uint8 *data;

	data = other->data;
	data += extents->y + other->stride;
	data += extents->x * PIXEL_MANIPULATE_BIT_PER_PIXEL(other->pixel_format) / 8;

	surface = GraphicsImageSurfaceCrateWithPixelManipulateFormat(data,
		other->pixel_format, other->width, other->height, other->stride, other->base.graphics);
	GraphicsSurfaceSetDeviceOffset(surface, - extents->x, extents->y);
	return (GRAPHICS_IMAGE_SURFACE*)surface;
}

eGRAPHICS_INTEGER_STATUS GraphicsImageSurfaceUnmapImage(void* abstract_surface, GRAPHICS_IMAGE_SURFACE* image)
{
	GraphicsSurfaceFinish(&image->base);
	DestroyGraphicsSurface(&image->base);

	return GRAPHICS_STATUS_SUCCESS;
}

GRAPHICS_SURFACE* GraphicsImageSurfaceSource(void* abstract_surface, GRAPHICS_RECTANGLE_INT* extents)
{
	GRAPHICS_IMAGE_SURFACE *surface = (GRAPHICS_IMAGE_SURFACE*)abstract_surface;

	if(extents != NULL)
	{
		extents->x = extents->y = 0;
		extents->width = surface->width;
		extents->height = surface->height;
	}

	return &surface->base;
}

void GraphicsImageSurfaceReleaseSourceImage(void* abstract_surface, GRAPHICS_RECTANGLE_INT* extents)
{
	// Nothing to do
}

GRAPHICS_SURFACE* GraphicsImageSurfaceSnapshot(void* abstract_surface)
{
	GRAPHICS_IMAGE_SURFACE *image = (GRAPHICS_IMAGE_SURFACE*)abstract_surface;
	GRAPHICS_IMAGE_SURFACE *clone;

	if(image->owns_data && image->base.finishing)
	{
		clone = (GRAPHICS_IMAGE_SURFACE*)CreateGraphicsImageSurfaceForPixelManipulate(&image->image, image->format, image->base.graphics);
		if(UNLIKELY(clone->base.status))
		{
			return &clone->base;
		}
		image->owns_data = FALSE;
		clone->transparency = image->transparency;
		clone->color = image->color;
		clone->owns_data = TRUE;

		return &clone->base;
	}

	clone = (GRAPHICS_IMAGE_SURFACE*)GraphicsImageSurfaceCrateWithPixelManipulateFormat(
		NULL, image->image.type, image->width, image->height, 0, image->base.graphics);
	if(UNLIKELY(clone->base.status))
	{
		return &clone->base;
	}

	if(clone->stride == image->stride)
	{
		(void)memcpy(clone->data, image->data, clone->stride * clone->height);
	}
	else
	{
		PixelManipulateComposite32(PIXEL_MANIPULATE_OPERATE_SRC,
			&image->image, NULL, &clone->image, 0, 0, 0, 0, 0, 0, image->width, image->height);
	}
	clone->base.is_clear = FALSE;
	return &clone->base;
}

eGRAPHICS_STATUS GraphicsImageSurfaceAcquireSourceImage(void* abstract_surface, GRAPHICS_IMAGE_SURFACE** image_out, void** image_extra)
{
	*image_out = (GRAPHICS_IMAGE_SURFACE*)abstract_surface;
	*image_extra = NULL;

	return GRAPHICS_STATUS_SUCCESS;
}

int GraphicsImageSurfaceGetExtents(void* abstract_surface, GRAPHICS_RECTANGLE_INT* rectangle)
{
	GRAPHICS_IMAGE_SURFACE *surface = (GRAPHICS_IMAGE_SURFACE*)abstract_surface;

	rectangle->x = 0;
	rectangle->y = 0;
	rectangle->width = surface->width;
	rectangle->height = surface->height;

	return TRUE;
}

/*
GRAPHICS_SURFACE* GraphicsSurfaceGetSource(GRAPHICS_SURFACE* surface, GRAPHICS_RECTANGLE_INT* extents)
{
	return surface->backend->source(surface, extents);
}
*/

GRAPHICS_SURFACE* GraphicsSurfaceHasSnapshot(
	GRAPHICS_SURFACE* surface,
	const GRAPHICS_SURFACE_BACKEND* backend
)
{
	GRAPHICS_SURFACE *snapshot;

	GRAPHICS_LIST_FOR_EACH_ENTRY(snapshot,GRAPHICS_SURFACE,&surface->snapshots,snapshot)
	{
		if(snapshot->backend == backend)
		{
			return snapshot;
		}
	}

	return NULL;
}

GRAPHICS_SURFACE* GraphicsSurfaceDefaultSource(void* surface, GRAPHICS_RECTANGLE_INT* extents)
{
	if(extents != NULL)
	{
		GraphicsSurfaceGetSource(surface, extents);
	}
	return surface;
}

eGRAPHICS_INTEGER_STATUS GraphicsImageSurfacePaint(
	void* abstract_surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_CLIP* clip
)
{
	GRAPHICS_IMAGE_SURFACE *surface = (GRAPHICS_IMAGE_SURFACE*)abstract_surface;
	
	return GraphicsCompositorPaint(surface->compositor, &surface->base, op, source, clip);
}

eGRAPHICS_INTEGER_STATUS GraphicsImageSurfaceMask(
	void* abstract_surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATTERN* mask,
	const GRAPHICS_CLIP* clip
)
{
	GRAPHICS_IMAGE_SURFACE *surface = (GRAPHICS_IMAGE_SURFACE*)abstract_surface;

	return GraphicsCompositorMask(surface->compositor, &surface->base, op, source, mask, clip);
}

eGRAPHICS_INTEGER_STATUS GraphicsImageSurfaceStroke(
	void* abstract_surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_MATRIX* matrix,
	const GRAPHICS_MATRIX* matrix_inverse,
	FLOAT_T tolerance,
	eGRAPHICS_ANTIALIAS antialias,
	const GRAPHICS_CLIP* clip
)
{
	GRAPHICS_IMAGE_SURFACE *surface = (GRAPHICS_IMAGE_SURFACE*)abstract_surface;

	return GraphicsCompositorStroke(surface->compositor, &surface->base, op, source, path, style,
		matrix, matrix_inverse, tolerance, antialias, clip);
}

eGRAPHICS_INTEGER_STATUS GraphicsImageSurfaceFill(
	void* abstract_surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rule,
	FLOAT_T tolerance,
	eGRAPHICS_ANTIALIAS antialias,
	const GRAPHICS_CLIP* clip
)
{
	GRAPHICS_IMAGE_SURFACE *surface = (GRAPHICS_IMAGE_SURFACE*)abstract_surface;

	return GraphicsCompositorFill(surface->compositor, &surface->base, op, source, path,
		fill_rule, tolerance, antialias, clip);
}

GRAPHICS_SURFACE* GraphicsSurfaceReference(GRAPHICS_SURFACE* surface)
{
	if(surface == NULL || surface->reference_count < 0)
	{
		return surface;
	}

	ASSERT(surface->reference_count >= 0);

	surface->reference_count++;

	return surface;
}

int InitializeGraphicsRecordingSurface(
	GRAPHICS_RECORDING_SURFACE* surface,
	eGRAPHICS_CONTENT content,
	const GRAPHICS_RECTANGLE* extents
)
{
	// TODO
	//InitializeGraphicsSur
}

GRAPHICS_SURFACE* GraphicsRecordingSurfaceCreate(eGRAPHICS_CONTENT content, const GRAPHICS_RECTANGLE* extents)
{
	return NULL;
}

void InitializeGraphicsSurface(
	GRAPHICS_SURFACE* surface,
	GRAPHICS_SURFACE_BACKEND* backend,
	eGRAPHICS_CONTENT content,
	int is_vector,
	void* graphics
)
{
	static unsigned int id = 1;
	surface->backend = backend;
	// TODO surface->device = device
	surface->content = content;
	surface->type = backend->type;
	surface->is_vector = is_vector;

	surface->reference_count = 1;
	surface->status = GRAPHICS_STATUS_SUCCESS;
	surface->unique_id = id++;

	surface->finished = FALSE;
	surface->finishing = FALSE;
	surface->is_clear = FALSE;
	surface->serial = 0;
	surface->damage = NULL;
	surface->owns_device = FALSE;

	InitializeGraphicsMatrixIdentity(&surface->device_transform);
	InitializeGraphicsMatrixIdentity(&surface->device_transform_inverse);
	InitializeGraphicsList(&surface->device_transform_observers);

	surface->x_resolution = DEFAULT_SURFACE_RESOLUTION;
	surface->y_resolution = DEFAULT_SURFACE_RESOLUTION;

	surface->x_fallback_resolution = DEFAULT_FALLBACK_SURFACE_RESOLUTION;
	surface->y_fallback_resolution = DEFAULT_FALLBACK_SURFACE_RESOLUTION;

	InitializeGraphicsList(&surface->snapshots);
	surface->snapshot_of = NULL;

	// TODO surface->has_font_options = FALSE;

	surface->graphics = graphics;
}

void InitializeGraphicsImageSurfaceBackend(GRAPHICS_SURFACE_BACKEND* backend)
{
	backend->type = GRAPHICS_SURFACE_TYPE_IMAGE;
	backend->finish = GraphicsImageSurfaceFinish;
	backend->create_context = GraphicsDefaultContextCreate;
	backend->create_similar = GraphicsImageSurfaceCreateSimilar;
	backend->create_similar_image = NULL;
	backend->map_to_image = GraphicsImageSurfaceMapToImage;
	backend->unmap_image = GraphicsImageSurfaceUnmapImage;
	backend->source = GraphicsImageSurfaceSource;
	backend->copy_page = NULL;
	backend->show_page = NULL;
	backend->get_extents = GraphicsImageSurfaceGetExtents;
	backend->flush = NULL;
	backend->paint = GraphicsImageSurfacePaint;
	backend->mask = GraphicsImageSurfaceMask;
	backend->stroke = GraphicsImageSurfaceStroke;
	backend->fill = GraphicsImageSurfaceFill;
	backend->fill_stroke = NULL;
}

static eGRAPHICS_STATUS GraphicsSurfaceFlushOne(GRAPHICS_SURFACE* surface, unsigned int flags)
{
	eGRAPHICS_STATUS status = GRAPHICS_STATUS_SUCCESS;
	if(surface->backend->flush != NULL)
	{
		status = surface->backend->flush(surface, flags);
	}
	return status;
}

GRAPHICS_SURFACE* GraphicsSurfaceCreateSimilarImage(
	GRAPHICS_SURFACE* other,
	eGRAPHICS_FORMAT format,
	int width,
	int height
)
{
	GRAPHICS_SURFACE *image;

	if(UNLIKELY(other->status))
	{
		return NULL;
	}
	if(UNLIKELY(other->finished))
	{
		return NULL;
	}

	if(UNLIKELY(width < 0 || height < 0))
	{
		GRAPHICS_STATUS_INVALID_SIZE;
	}

	image = NULL;
	if(other->backend->create_similar)
	{
		image = other->backend->create_similar(other, format, width, height);
	}

	if(image == NULL)
	{
		image = GraphicsImageSurfaceCreate(format, width, height, other->graphics);
	}

	ASSERT(image->is_clear);

	return image;
}

eGRAPHICS_STATUS GraphicsSurfaceOffsetStroke(
	GRAPHICS_SURFACE* surface,
	int x, int y,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* stroke_style,
	const GRAPHICS_MATRIX* matrix,
	const GRAPHICS_MATRIX* matrix_inverse,
	FLOAT_T tolerance,
	eGRAPHICS_ANTIALIAS antialias,
	const GRAPHICS_CLIP* clip
)
{
	GRAPHICS_PATH_FIXED path_copy, *device_path = (GRAPHICS_PATH_FIXED*)path;
	GRAPHICS_CLIP *device_clip = (GRAPHICS_CLIP*)clip;
	GRAPHICS_CLIP local_clip;
	GRAPHICS_MATRIX device_matrix = *matrix;
	GRAPHICS_MATRIX device_matrix_inverse = *matrix_inverse;
	GRAPHICS_PATTERN_UNION source_copy;
	eGRAPHICS_STATUS status;

	if(UNLIKELY(surface->status))
	{
		return surface->status;
	}

	if(clip->clip_all)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(x | y)
	{
		GRAPHICS_MATRIX m;

		device_clip = GraphicsClipCopyWithTranslation(clip, -x, -y, &local_clip);

		status = InitializeGraphicsPathFixedCopy(&path_copy, device_path);
		if(UNLIKELY(status))
		{
			goto FINISH;
		}

		GraphicsPathFixedTranslate(&path_copy, GraphicsFixedFromInteger(-x),
			GraphicsFixedFromInteger(-y));
		device_path = &path_copy;

		InitializeGraphicsMatrixTranslate(&m, -x, -y);
		GraphicsMatrixMultiply(&device_matrix, &device_matrix, &m);

		InitializeGraphicsMatrixTranslate(&m, x, y);
		CopyTransformedPattern(&source_copy.base, source, &m);
		source = &source_copy.base;
		GraphicsMatrixMultiply(&device_matrix_inverse, &m, &device_matrix_inverse);
	}

	status = GraphicsSurfaceStroke(surface, op, source,
				device_path, stroke_style, &device_matrix, &device_matrix_inverse, tolerance, antialias, device_clip);
FINISH:
	if(device_path != path)
	{
		GraphicsPathFixedFinish(device_path);
	}
	if(device_clip != clip)
	{
		GraphicsClipDestroy(device_clip);
	}

	return status;
}

static eGRAPHICS_STATUS GraphicsSurfaceSubsurfaceFinish(void* abstract_surface)
{
	GRAPHICS_SUBSURFACE *surface = (GRAPHICS_SUBSURFACE*)abstract_surface;

	DestroyGraphicsSurface(surface->target);
	DestroyGraphicsSurface(surface->snapshot);

	return GRAPHICS_STATUS_SUCCESS;
}

static GRAPHICS_SURFACE* GraphicsSurfaceSubsurfaceCreateSimilar(
	void* other,
	eGRAPHICS_CONTENT content,
	int width,
	int height
)
{
	GRAPHICS_SUBSURFACE *surface = (GRAPHICS_SUBSURFACE*)other;

	if(surface->target->backend->create_similar == NULL)
	{
		return NULL;
	}

	return surface->target->backend->create_similar(surface->target, content, width, height);
}

static GRAPHICS_SURFACE* GraphicsSurfaceSubsurfaceCreateSimilarImage(
	void* other,
	eGRAPHICS_FORMAT format,
	int width,
	int height
)
{
	GRAPHICS_SUBSURFACE *surface = (GRAPHICS_SUBSURFACE*)other;

	if(surface->target->backend->create_similar_image == NULL)
	{
		return NULL;
	}

	return surface->target->backend->create_similar_image(surface->target, format, width, height);
}

static GRAPHICS_IMAGE_SURFACE* GraphicsSurfaceSubsurfaceMapToImage(
	void* abstract_surface,
	const GRAPHICS_RECTANGLE_INT* extents
)
{
	GRAPHICS_SUBSURFACE *surface = (GRAPHICS_SUBSURFACE*)abstract_surface;
	GRAPHICS_RECTANGLE_INT target_extents;

	target_extents.x = extents->x + surface->extents.x;
	target_extents.y = extents->y + surface->extents.y;
	target_extents.width = extents->width;
	target_extents.height = extents->height;

	// TODO
	return NULL;
}

static eGRAPHICS_STATUS GraphicsSurfaceSubsurfacePaint(
	void* abstract_surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN** source,
	const GRAPHICS_CLIP* clip
)
{
	GRAPHICS_SUBSURFACE *surface = (GRAPHICS_SUBSURFACE*)abstract_surface;
	GRAPHICS_RECTANGLE_INT rect = {0, 0, surface->extents.width, surface->extents.height};
	eGRAPHICS_STATUS status;
	GRAPHICS_CLIP *target_clip;

	target_clip = GraphicsClipCopyIntersectRectangle(clip, &rect);
	status = GraphicsSurfaceOffsetPaint(surface->target,
		-surface->extents.x, -surface->extents.y, op, source, target_clip);
	GraphicsClipDestroy(target_clip);
	return status;
}

static eGRAPHICS_STATUS GraphicsSurfaceSubsurfaceMask(
	void* abstract_surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATTERN* mask,
	const GRAPHICS_CLIP* clip
)
{
	GRAPHICS_SUBSURFACE *surface = (GRAPHICS_SUBSURFACE*)abstract_surface;
	GRAPHICS_RECTANGLE_INT rect = {0, 0, surface->extents.width, surface->extents.height};
	eGRAPHICS_STATUS status;
	GRAPHICS_CLIP *target_clip;

	target_clip = GraphicsClipCopyIntersectRectangle(clip, &rect);
	status = GraphicsSurfaceOffsetMask(surface->target,
		-surface->extents.x, -surface->extents.y, op, source, mask, target_clip);
	GraphicsClipDestroy(target_clip);
	return status;
}

static eGRAPHICS_STATUS GraphicsSurfaceSubsurfaceFill(
	void* abstract_surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rule,
	FLOAT_T tolerance,
	eGRAPHICS_ANTIALIAS antialias,
	const GRAPHICS_CLIP* clip
)
{
	GRAPHICS_SUBSURFACE *surface = (GRAPHICS_SUBSURFACE*)abstract_surface;
	GRAPHICS_RECTANGLE_INT rect = {0, 0, surface->extents.width, surface->extents.height};
	eGRAPHICS_STATUS status;
	GRAPHICS_CLIP *target_clip;

	target_clip = GraphicsClipCopyIntersectRectangle(clip, &rect);
	status = GraphicsSurfaceOffsetFill(surface->target,
		-surface->extents.x, -surface->extents.y, op, source, path,
		fill_rule, tolerance, antialias, target_clip);
	GraphicsClipDestroy(target_clip);
	return status;
}

static eGRAPHICS_STATUS GraphicsSurfaceSubsurfaceStroke(
	void* abstract_surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* stroke_style,
	const GRAPHICS_MATRIX* matrix,
	const GRAPHICS_MATRIX* matrix_inverse,
	FLOAT_T tolerance,
	eGRAPHICS_ANTIALIAS antialias,
	const GRAPHICS_CLIP* clip
)
{
	GRAPHICS_SUBSURFACE *surface = (GRAPHICS_SUBSURFACE*)abstract_surface;
	GRAPHICS_RECTANGLE_INT rect = {0, 0, surface->extents.width, surface->extents.height};
	eGRAPHICS_STATUS status;
	GRAPHICS_CLIP *target_clip;

	target_clip = GraphicsClipCopyIntersectRectangle(clip, &rect);
	status = GraphicsSurfaceOffsetStroke(surface->target,
		-surface->extents.x, -surface->extents.y, op, source, path,
		stroke_style, matrix, matrix_inverse, tolerance, antialias, target_clip);
	GraphicsClipDestroy(target_clip);
	return status;
}

static eGRAPHICS_STATUS GraphicsSurfaceSubsurfaceFlush(void* abstract_surface, unsigned flags)
{
	GRAPHICS_SUBSURFACE *surface = (GRAPHICS_SUBSURFACE*)abstract_surface;
	return GraphicsSurfaceFlush(surface->target, flags);
}

static eGRAPHICS_STATUS GraphicsSurfaceSubsurfaceMarkDirty(
	void* abstract_surface,
	int x, int y,
	int width, int height
)
{
	GRAPHICS_SUBSURFACE *surface = (GRAPHICS_SUBSURFACE*)abstract_surface;
	eGRAPHICS_STATUS status = GRAPHICS_STATUS_SUCCESS;

	if(surface->target->backend->mark_dirty_rectangle != NULL)
	{
		GRAPHICS_RECTANGLE_INT rect, extents;

		rect.x = x;
		rect.y = y;
		rect.width = width;
		rect.height = height;

		if(GraphicsRectangleIntersect(&rect, &extents))
		{
			status = surface->target->backend->mark_dirty_rectangle(surface->target,
				rect.x + surface->extents.x, rect.y + surface->extents.y, rect.width, rect.height);
		}
	}

	return status;
}

static int GraphicsSurfaceSubsurfaceGetExtents(void* abstract_surface, GRAPHICS_RECTANGLE_INT* extents)
{
	GRAPHICS_SUBSURFACE *surface = (GRAPHICS_SUBSURFACE*)abstract_surface;

	extents->x = 0;
	extents->y = 0;
	extents->width = surface->extents.width;
	extents->height = surface->extents.height;

	return TRUE;
}

static GRAPHICS_SURFACE* GraphicsSurfaceSubsurfaceSource(
	void* abstract_surface,
	GRAPHICS_RECTANGLE_INT* extents
)
{
	GRAPHICS_SUBSURFACE *surface = (GRAPHICS_SUBSURFACE*)abstract_surface;
	GRAPHICS_SURFACE *source;

	source = GraphicsSurfaceGetSource(surface->target, extents);
	if(extents != NULL)
	{
		*extents = surface->extents;
	}

	return source;
}

static eGRAPHICS_STATUS GraphicsSurfaceSubsurfaceAcquireSourceImage(
	void* abstract_surface,
	GRAPHICS_IMAGE_SURFACE** image_out,
	void** extra_out
)
{
	GRAPHICS_SUBSURFACE *surface = (GRAPHICS_SUBSURFACE*)abstract_surface;
	GRAPHICS_SURFACE_PATTERN pattern;
	GRAPHICS_SURFACE *image;
	eGRAPHICS_STATUS status;

	image = GraphicsImageSurfaceCreateWithContent(surface->base.content,
					surface->extents.width, surface->extents.height, surface->base.graphics);
	if(UNLIKELY(image->status))
	{
		return image->status;
	}

	InitializeGraphicsPatternForSurface(&pattern, surface->target);
	InitializeGraphicsMatrixTranslate(&pattern.base.matrix, surface->extents.x, surface->extents.y);
	pattern.base.filter = GRAPHICS_FILTER_NEAREST;
	status = GraphicsSurfacePaint(image, GRAPHICS_OPERATOR_SOURCE, &pattern.base, NULL);
	GraphicsPatternFinish(&pattern.base);
	if(UNLIKELY(status))
	{
		DestroyGraphicsSurface(image);
		return status;
	}

	*image_out = (GRAPHICS_IMAGE_SURFACE*)image;
	*extra_out = NULL;
	return GRAPHICS_STATUS_SUCCESS;
}

static void GraphicsSurfaceSubsurfaceReleaseSourceImage(
	void* abstract_surface,
	GRAPHICS_IMAGE_SURFACE* image,
	void* abstract_extra
)
{
	DestroyGraphicsSurface(&image->base);
}

static GRAPHICS_SURFACE* GraphicsSurfaceSubsurfaceSnapshot(void* abstract_surface)
{
	GRAPHICS_SUBSURFACE *surface = (GRAPHICS_SUBSURFACE*)abstract_surface;
	GRAPHICS_SURFACE_PATTERN pattern;
	GRAPHICS_SURFACE *clone;
	eGRAPHICS_STATUS status;

	clone = GraphicsSurfaceCreateScratch(surface->target, surface->target->content,
				surface->extents.width, surface->extents.height, NULL);
	if(UNLIKELY(clone->status))
	{
		return clone;
	}

	InitializeGraphicsPatternForSurface(&pattern, surface->target);
	InitializeGraphicsMatrixTranslate(&pattern.base.matrix,
				surface->extents.x, surface->extents.y);
	pattern.base.filter = GRAPHICS_FILTER_NEAREST;
	status = GraphicsSurfacePaint(clone, GRAPHICS_OPERATOR_SOURCE, &pattern.base, NULL);
	GraphicsPatternFinish(&pattern.base);

	if(UNLIKELY(status))
	{
		DestroyGraphicsSurface(clone);
		clone = NULL;
	}

	return clone;
}

static GRAPHICS_CONTEXT* GraphicsSurfaceSubsurfaceCreateContext(void* target)
{
	GRAPHICS_SUBSURFACE *surface = (GRAPHICS_SUBSURFACE*)target;
	return surface->target->backend->create_context(&surface->base);
}

void InitializeGraphicsSubsurfaceBackend(GRAPHICS_SURFACE_BACKEND* backend)
{
	backend->type = GRAPHICS_SURFACE_TYPE_SUBSURFACE;
	backend->finish = GraphicsSurfaceSubsurfaceFinish;

	backend->create_context = GraphicsSurfaceSubsurfaceCreateContext;

	backend->create_similar = GraphicsSurfaceSubsurfaceCreateSimilar;
	backend->create_similar_image = GraphicsSurfaceSubsurfaceCreateSimilarImage;
	backend->map_to_image = NULL; // map_to_image
	backend->unmap_image = NULL;	// umpat_image

	backend->source = GraphicsSurfaceSubsurfaceSource;
	backend->acquire_source_image = GraphicsSurfaceSubsurfaceAcquireSourceImage;
	backend->release_source_image = GraphicsSurfaceSubsurfaceReleaseSourceImage;
	backend->snapshot = GraphicsSurfaceSubsurfaceSnapshot;

	backend->copy_page = NULL;
	backend->show_page = NULL;

	backend->get_extents = GraphicsSurfaceSubsurfaceGetExtents;

	backend->flush = GraphicsSurfaceSubsurfaceFlush;
	backend->mark_dirty_rectangle = GraphicsSurfaceSubsurfaceMarkDirty;

	backend->paint = GraphicsSurfaceSubsurfacePaint;
	backend->mask = GraphicsSurfaceSubsurfaceMask;
	backend->stroke = GraphicsSurfaceSubsurfaceStroke;
	backend->fill = GraphicsSurfaceSubsurfaceFill;
}

eGRAPHICS_STATUS InitializeGraphicsSurfaceForRectangle(
	GRAPHICS_SUBSURFACE *surface,
	GRAPHICS_SURFACE* target,
	FLOAT_T x, FLOAT_T y,
	FLOAT_T width, FLOAT_T height
)
{
	GRAPHICS *graphics = (GRAPHICS*)(target->graphics);

	if(UNLIKELY(width < 0 || height < 0))
	{
		return GRAPHICS_STATUS_INVALID_SIZE;
	}

	if(UNLIKELY(target->status))
	{
		return target->status;
	}
	if(UNLIKELY(target->finished))
	{
		return GRAPHICS_STATUS_SURFACE_FINISHED;
	}

	x *= target->device_transform.xx;
	y *= target->device_transform.yy;

	width += target->device_transform.x0;
	height += target->device_transform.y0;

	InitializeGraphicsSurface(&surface->base, &graphics->subsurface_backend,
		target->content, target->is_vector, graphics);

	surface->extents.x = (int)CEIL(x);
	surface->extents.y = (int)CEIL(y);
	surface->extents.width = (int)FLOOR(x + width) - surface->extents.x;
	surface->extents.height = (int)FLOOR(y + height) - surface->extents.y;
	if((surface->extents.width | surface->extents.height) < 0)
	{
		surface->extents.width = surface->extents.height = 0;
	}

	if(target->backend->type == GRAPHICS_SURFACE_TYPE_SUBSURFACE)
	{
		GRAPHICS_SUBSURFACE *sub = (GRAPHICS_SUBSURFACE*)target;
		surface->extents.x += sub->extents.x;
		surface->extents.y += sub->extents.y;
		target = sub->target;
	}

	surface->target = GraphicsSurfaceReference(target);
	surface->base.type = surface->target->type;

	surface->snapshot = NULL;

	GraphicsSurfaceSetDeviceScale(&surface->base,
		target->device_transform.xx, target->device_transform.yy);

	return GRAPHICS_STATUS_SUCCESS;
}

GRAPHICS_SURFACE* GraphicsSurfaceCreateForRectangle(
	GRAPHICS_SURFACE* target,
	FLOAT_T x, FLOAT_T y,
	FLOAT_T width, FLOAT_T height
)
{
	GRAPHICS_SUBSURFACE *surface;

	surface = (GRAPHICS_SUBSURFACE*)MEM_ALLOC_FUNC(sizeof(*surface));
	if(surface == NULL)
	{
		return NULL;
	}

	if(UNLIKELY(InitializeGraphicsSurfaceForRectangle(surface, target, x, y, width, height)))
	{
		return NULL;
	}
	surface->base.own_memory = TRUE;

	return surface;
}

static GRAPHICS_IMAGE_SURFACE* GetOriginalImageSurface(GRAPHICS_IMAGE_SURFACE* surface)
{
	while(surface->parent != NULL)
	{
		surface = (GRAPHICS_IMAGE_SURFACE*)surface->parent;
	}
	return surface;
}

eGRAPHICS_STATUS InitializeGraphicsImageSurfaceForRectangle(
	GRAPHICS_IMAGE_SURFACE* surface,
	GRAPHICS_IMAGE_SURFACE* target,
	FLOAT_T x, FLOAT_T y,
	FLOAT_T width, FLOAT_T height
)
{
	GRAPHICS_IMAGE_SURFACE *original_surface;
	eGRAPHICS_STATUS status;
	uint8 *original_bytes;
	int int_x, int_y;
	int int_width, int_height;

	if(surface == NULL || target == NULL)
	{
		return GRAPHICS_STATUS_NULL_POINTER;
	}
	original_surface = GetOriginalImageSurface(target);

	if(UNLIKELY(width < 0 || height < 0))
	{
		return GRAPHICS_STATUS_INVALID_SIZE;
	}
	
	if(UNLIKELY(x >= target->width || y >= target->height))
	{
		return GRAPHICS_STATUS_INVALID_POSITION;
	}

	if(UNLIKELY(target->base.status))
	{
		return target->base.status;
	}
	if(UNLIKELY(target->base.finished))
	{
		return GRAPHICS_STATUS_SURFACE_FINISHED;
	}

	x *= target->base.device_transform.xx;
	y *= target->base.device_transform.yy;
	int_x = (int)CEIL(x);
	if(int_x < 0)
	{
		int_x = 0;
	}
	int_y = (int)CEIL(y);
	if(int_y < 0)
	{
		int_y = 0;
	}

	width += target->base.device_transform.x0;
	int_width = (int)FLOOR(width);
	if(int_x + int_width > target->width)
	{
		int_width = target->width - int_x;
	}
	height += target->base.device_transform.y0;
	int_height = (int)FLOOR(height);
	if(int_y + int_height > target->height)
	{
		int_height = target->height - int_y;
	}

	original_bytes = (uint8*)original_surface->image.bits.bits;
	InitializeGraphicsImageSurfaceForData(surface,
		&original_bytes[int_y*original_surface->stride
			+ (int_x * original_surface->stride) / original_surface->width], target->format,
				int_width, int_height, original_surface->width * (original_surface->stride/original_surface->width),
					target->base.graphics);

	surface->base.type =target->base.type;
	surface->parent = target;

	return GRAPHICS_STATUS_SUCCESS;
}

GRAPHICS_SURFACE* GraphicsImageSurfaceCreateForRectangle(
	GRAPHICS_IMAGE_SURFACE* target,
	FLOAT_T x, FLOAT_T y,
	FLOAT_T width, FLOAT_T height
)
{
	GRAPHICS_IMAGE_SURFACE *surface;
	eGRAPHICS_STATUS status;

	surface = (GRAPHICS_IMAGE_SURFACE*)MEM_ALLOC_FUNC(sizeof(*surface));
	if(surface == NULL)
	{
		return NULL;
	}

	(void)memset(surface, 0, sizeof(*surface));
	status = InitializeGraphicsImageSurfaceForRectangle(surface, target, x, y, width, height);
	surface->base.own_memory = TRUE;

	if(UNLIKELY(status))
	{
		GraphicsSurfaceSetError(surface, status);
	}

	return &surface->base;
}


#ifdef __cplusplus
}
#endif
