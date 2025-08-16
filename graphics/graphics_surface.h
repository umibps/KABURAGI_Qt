#ifndef _INCLUDED_GRAPHICS_SURFACE_H_
#define _INCLUDED_GRAPHICS_SURFACE_H_

#include <stdio.h>
#include <limits.h>
#include "graphics_types.h"
#include "graphics_list.h"
#include "graphics_status.h"
#include "graphics_compositor.h"
#include "graphics_device.h"
#include "graphics_damage.h"
#include "graphics_types.h"
#include "../pixel_manipulate/pixel_manipulate.h"
#include "../utils.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_IMAGE_SIZE SHRT_MAX
#define DEFAULT_SURFACE_RESOLUTION 72.0
#define DEFAULT_FALLBACK_SURFACE_RESOLUTION 300.0

typedef enum _eGRAPHICS_SURFACE_TYPE
{
	GRAPHICS_SURFACE_TYPE_IMAGE,
	GRAPHICS_SURFACE_TYPE_RECORDING,
	GRAPHICS_SURFACE_TYPE_SUBSURFACE,
	GRAPHICS_SURFACE_TYPE_SNAPSHOT,
	NUM_GRAPHICS_SURFACE_TYPE
} eGRAPHICS_SURFACE_TYPE;

typedef struct _GRAPHICS_SURFACE_BACKEND
{
	eGRAPHICS_SURFACE_TYPE type;
	eGRAPHICS_STATUS (*finish)(void* surface);
	struct _GRAPHICS_CONTEXT* (*create_context)(void* surface);
	struct _GRAPHICS_SURFACE* (*create_similar)(void* surface,
		eGRAPHICS_CONTENT content, int width, int height);
	struct _GRAPHICS_IMAGE_SURFACE* (*create_similar_image)(void* surface, eGRAPHICS_FORMAT format,
		int width, int height);
	struct _GRAPHICS_IMAGE_SURFACE* (*map_to_image)(void* surface,
		const GRAPHICS_RECTANGLE_INT* extents);
	eGRAPHICS_INTEGER_STATUS (*unmap_image)(void* surface,
		struct _GRAPHICS_IMAGE_SURFACE* image);

	struct _GRAPHICS_SURFACE* (*source)(void* abstract_surface,
		GRAPHICS_RECTANGLE_INT* extents);

	eGRAPHICS_STATUS (*acquire_source_image)(void* abstract_surface,
		struct _GRAPHICS_IMAGE_SURFACE** image_out, void** image_extra);

	void (*release_source_image)(void* abstract_surface,
		struct _GRAPHICS_IMAGE* image_out, void* image_extra);

	struct _GRAPHICS_SURFACE* (*snapshot)(void* surface);

	eGRAPHICS_INTEGER_STATUS (*copy_page)(void* surface);

	eGRAPHICS_INTEGER_STATUS (*show_page)(void* surface);

	int (*get_extents)(void* surface, GRAPHICS_RECTANGLE_INT* extents);

	// void (*get_font_options)

	eGRAPHICS_STATUS (*flush)(void* surface, unsigned int flags);

	eGRAPHICS_STATUS (*mark_dirty_rectangle)(void* surface,
		int x, int y, int width, int height);

	eGRAPHICS_INTEGER_STATUS (*paint)(void* surface,
		eGRAPHICS_OPERATOR op, const struct _GRAPHICS_PATTERN* source, struct _GRAPHICS_CLIP* clip);

	eGRAPHICS_INTEGER_STATUS (*mask)(void* surface,
		eGRAPHICS_OPERATOR op, const struct _GRAPHICS_PATTERN* source, const struct _GRAPHICS_PATTERN* mask,
			struct _GRAPHICS_CLIP* clip);

	eGRAPHICS_INTEGER_STATUS (*stroke)(void* surface,
		eGRAPHICS_OPERATOR op, const struct _GRAPHICS_PATTERN* source, const struct _GRAPHICS_PATH_FIXED* path,
			const struct _GRAPHICS_STROKE_STYLE* style, const GRAPHICS_MATRIX* matrix, const GRAPHICS_MATRIX* matrix_inverse,
				double tolerance, eGRAPHICS_ANTIALIAS antialias, const struct _GRAPHICS_CLIP* clip);

	eGRAPHICS_INTEGER_STATUS (*fill)(void* surface,
		eGRAPHICS_OPERATOR op, const struct _GRAPHICS_PATTERN* source, const struct _GRAPHICS_PATH_FIXED* path,
			eGRAPHICS_FILL_RULE fill_rule, double tolerance, eGRAPHICS_ANTIALIAS antialias, const struct _GRAPHICS_CLIP* clip);

	eGRAPHICS_INTEGER_STATUS (*fill_stroke)(void* surface,
		eGRAPHICS_OPERATOR fill_op, const struct _GRAPHICS_PATTERN* fill_source, eGRAPHICS_FILL_RULE fill_rule,
			double tolerance, eGRAPHICS_ANTIALIAS fill_antialias, const struct _GRAPHICS_PATH_FIXED* path,
				eGRAPHICS_OPERATOR stroke_op, const struct _GRAPHICS_PATTERN* stroke_source, const struct _GRAPHICS_STROKE_STYLE* stroke_style,
					const GRAPHICS_MATRIX* stroke_matrix, const GRAPHICS_MATRIX* stroke_matrix_inverse, double stroke_tolerance,
						eGRAPHICS_ANTIALIAS *stroke_antialias, const struct _GRAPHICS_CLIP* clip);
} GRAPHICS_SURFACE_BACKEND;

typedef void (*GRAPHICS_SURFACE_FUNCTION)(struct _GRAPHICS_SURFACE*);

typedef struct _GRAPHICS_SURFACE
{
	eGRAPHICS_SURFACE_TYPE type;
	GRAPHICS_SURFACE_BACKEND *backend;
	eGRAPHICS_CONTENT content;
	int reference_count;
	eGRAPHICS_STATUS status;
	unsigned int unique_id;
	unsigned int serial;
	GRAPHICS_DAMAGE *damage;
	unsigned finishing : 1;
	unsigned finished : 1;
	unsigned is_clear : 1;
	unsigned has_font_options : 1;
	unsigned owns_device : 1;
	unsigned is_vector : 1;
	unsigned own_memory : 1;

	GRAPHICS_MATRIX device_transform;
	GRAPHICS_MATRIX device_transform_inverse;
	GRAPHICS_LIST device_transform_observers;

	FLOAT_T x_resolution;
	FLOAT_T y_resolution;

	FLOAT_T x_fallback_resolution;
	FLOAT_T y_fallback_resolution;

	struct _GRAPHICS_SURFACE *snapshot_of;
	GRAPHICS_SURFACE_FUNCTION snapshot_detach;
	GRAPHICS_LIST snapshots;
	GRAPHICS_LIST snapshot;

	void *graphics;
} GRAPHICS_SURFACE;

typedef struct _GRAPHICS_IMAGE_SURFACE
{
	GRAPHICS_SURFACE base;
	PIXEL_MANIPULATE_IMAGE image;

	GRAPHICS_COMPOSITOR *compositor;

	GRAPHICS_SURFACE *parent;
	ePIXEL_MANIPULATE_FORMAT pixel_format;
	eGRAPHICS_FORMAT format;
	uint8 *data;

	int width;
	int height;
	int stride;
	int depth;

	unsigned int owns_data : 1;
	unsigned int transparency : 2;
	unsigned int color : 2;
} GRAPHICS_IMAGE_SURFACE;

typedef struct _GRAPHICS_SUBSURFACE
{
	GRAPHICS_SURFACE base;

	GRAPHICS_RECTANGLE_INT extents;

	GRAPHICS_SURFACE *target;
	GRAPHICS_SURFACE *snapshot;
} GRAPHICS_SUBSURFACE;

typedef struct _GRAPHICS_IMAGE_SOURCE
{
	GRAPHICS_SURFACE base;

	PIXEL_MANIPULATE_IMAGE image;
	unsigned int opaque_solid : 1;
} GRAPHICS_IMAGE_SOURCE;

typedef enum _eGRAPHICS_COMMAND_TYPE
{
	GRAPHICS_COMMAND_PAINT,
	GRAPHICS_COMMAND_MASK,
	GRAPHICS_COMMAND_STROKE,
	GRAPHICS_COMMAND_FILL,
	GRAPHICS_COMMAND_SHOW_TEXT_GLYPHS
} eGRAPHICS_COMMAND_TYPE;

typedef enum _eGRAPHICS_RECORDING_REGION_TYPE
{
   GRAPHICS_RECORDING_REGION_ALL,
   GRAPHICS_RECORDING_REGION_NATIVE,
   GRAPHICS_RECORDING_REGION_IMAGE_FALLBACK
} eGRAPHICS_RECORDING_REGION_TYPE;

typedef struct _GRAPHICS_COMMAND_HEADER
{
	eGRAPHICS_COMMAND_TYPE type;
	eGRAPHICS_RECORDING_REGION_TYPE region;
	eGRAPHICS_OPERATOR op;
	GRAPHICS_RECTANGLE_INT extents;
	GRAPHICS_CLIP *clip;

	int index;
	struct _GRAPHICS_COMMAND_HEADER *chain;
} GRAPHICS_COMMAND_HEADER;

typedef struct _GRAPHICS_RECORDING_SURFACE
{
	GRAPHICS_SURFACE base;

	GRAPHICS_RECTANGLE extents_pixels;
	GRAPHICS_RECTANGLE_INT extents;
	int unbounded;

	BYTE_ARRAY commands;
	unsigned int *indices;
	unsigned int num_indices;
	int optimize_clears : 1;
	int has_bilevel_alpha : 1;
	int has_only_op_over : 1;

	struct _bbtree
	{
		GRAPHICS_BOX extents;
		struct _bbtree *left, *right;
		GRAPHICS_COMMAND_HEADER *chain;
	} bbtree;
} GRAPHICS_RECORDING_SURFACE;

typedef struct _GRAPHICS_SNAPSHOT_SURFACE
{
	GRAPHICS_SURFACE base;

	GRAPHICS_SURFACE *target;
	GRAPHICS_SURFACE *clone;
} GRAPHICS_SNAPSHOT_SURFACE;

static INLINE void GraphicsSurfaceReleaseSourceImage(
	GRAPHICS_SURFACE* surface,
	GRAPHICS_IMAGE_SURFACE* image,
	void* image_extra
)
{
	if(surface->backend->release_source_image != NULL)
	{
		surface->backend->release_source_image(surface, (struct _GRAPHICS_IMAGE*)image, image_extra);
	}
}

static INLINE int GraphicsSurfaceIsSnapshot(GRAPHICS_SURFACE* surface)
{
	return surface->backend->type == GRAPHICS_SURFACE_TYPE_SNAPSHOT;
}

static INLINE void GraphicsSurfaceResetStatus(GRAPHICS_SURFACE* surface)
{
	surface->status = GRAPHICS_STATUS_SUCCESS;
}

static INLINE int GraphicsImageSurfaceCalculateStride(int width, eGRAPHICS_FORMAT format)
{
	switch(format)
	{
	case GRAPHICS_FORMAT_ARGB32:
		return width * 4;
	case GRAPHICS_FORMAT_RGB24:
		return width * 3;
	case GRAPHICS_FORMAT_A8:
		return width;
	}
	return -1;
}

#define GRAPHICS_SURFACE_HAS_DEVICE_TRANSFORM(surface) (FALSE == GRAPHICS_MATRIX_IS_IDENTITY((&(surface)->device_transform)))

extern void InitializeGraphicsSurface(
	GRAPHICS_SURFACE* surface,
	GRAPHICS_SURFACE_BACKEND* backend,
	eGRAPHICS_CONTENT content,
	int is_vector,
	void* graphics
);

extern GRAPHICS_SURFACE* GraphicsSurfaceCreateScratch(
	GRAPHICS_SURFACE* other,
	eGRAPHICS_CONTENT content,
	int width,
	int height,
	const GRAPHICS_COLOR* color
);

extern int GraphicsImageSurfaceInitialize(
	GRAPHICS_IMAGE_SURFACE* surface,
	eGRAPHICS_FORMAT format,
	int width,
	int height,
	int stride,
	void* graphics
);
extern GRAPHICS_SURFACE* GraphicsImageSurfaceCreate(eGRAPHICS_FORMAT format, int width, int height, void* graphics);
extern GRAPHICS_SURFACE* GraphicsImageSurfaceCreateWithContent(
	eGRAPHICS_CONTENT content,
	int width,
	int height,
	void* graphics
);
extern INLINE int ReleaseGraphicsSurface(GRAPHICS_SURFACE* surface);
extern void GraphicsSurfaceFinish(GRAPHICS_SURFACE* surface);
extern void DestroyGraphicsSurface(GRAPHICS_SURFACE* surface);
extern eGRAPHICS_STATUS GraphicsSurfaceSetError(GRAPHICS_SURFACE* surface, eGRAPHICS_INTEGER_STATUS status);
extern int GraphicsSurfaceGetExtents(GRAPHICS_SURFACE* surface, GRAPHICS_RECTANGLE_INT* extents);
extern GRAPHICS_SURFACE* GraphicsSurfaceDefaultSource(void* surface, GRAPHICS_RECTANGLE_INT* extents);
extern eGRAPHICS_STATUS GraphicsSurfacePaint(
		GRAPHICS_SURFACE* surface,
		eGRAPHICS_OPERATOR op,
		const GRAPHICS_PATTERN* source,
		const GRAPHICS_CLIP* clip
);
extern eGRAPHICS_STATUS GraphicsSurfaceOffsetPaint(
	GRAPHICS_SURFACE* target,
	int x, int y,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_CLIP* clip
);
extern eGRAPHICS_STATUS GraphicsSurfaceOffsetMask(
	GRAPHICS_SURFACE* target,
	int x, int y,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATTERN* mask,
	const GRAPHICS_CLIP* clip
);
extern eGRAPHICS_STATUS GraphicsSurfaceMask(
	GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATTERN* mask,
	const GRAPHICS_CLIP* clip
);
extern GRAPHICS_SURFACE* GraphicsSurfaceReference(GRAPHICS_SURFACE* surface);
extern eGRAPHICS_STATUS GraphicsSurfaceStroke(
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
);
extern eGRAPHICS_STATUS GraphicsSurfaceOffsetStroke(
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
);
extern eGRAPHICS_STATUS GraphicsSurfaceFill(
	GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rule,
	FLOAT_T tolerance,
	eGRAPHICS_ANTIALIAS antialias,
	const GRAPHICS_CLIP* clip
);
extern eGRAPHICS_STATUS GraphicsSurfaceOffsetFill(
	GRAPHICS_SURFACE* surface,
	int x, int y,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rule,
	FLOAT_T tolerance,
	eGRAPHICS_ANTIALIAS antialias,
	const GRAPHICS_CLIP* clip
);
extern eGRAPHICS_STATUS GraphicsSurfaceAcquireSourceImage(
	GRAPHICS_SURFACE* surface,
	GRAPHICS_IMAGE_SURFACE** image_out,
	void** image_extra
);
extern void GraphicsSurfaceAttachSnapshot(
	GRAPHICS_SURFACE* surface,
	GRAPHICS_SURFACE* snapshot,
	GRAPHICS_SURFACE_FUNCTION detach_function
);
extern GRAPHICS_SURFACE* GraphicsSurfaceHasSnapshot(
	GRAPHICS_SURFACE* surface,
	const GRAPHICS_SURFACE_BACKEND* backend
);
extern void InitializeGraphicsImageSurfaceBackend(GRAPHICS_SURFACE_BACKEND* backend);
extern void InitializeGraphicsSubsurfaceBackend(GRAPHICS_SURFACE_BACKEND* backend);
extern GRAPHICS_SURFACE* GraphicsSurfaceCreateSimilarImage(
	GRAPHICS_SURFACE* other,
	eGRAPHICS_FORMAT format,
	int width,
	int height
);
extern eGRAPHICS_STATUS InitializeGraphicsImageSurfaceForData(
	GRAPHICS_IMAGE_SURFACE* surface,
	uint8* data,
	eGRAPHICS_FORMAT format,
	int width,
	int height,
	int stride,
	void* graphics
);
extern GRAPHICS_SURFACE* GraphicsImageSurfaceCreateForData(
	uint8* data,
	eGRAPHICS_FORMAT format,
	int width,
	int height,
	int stride,
	void* graphics
);
extern GRAPHICS_SURFACE* GraphicsImageSourceCreateForPattern(
	GRAPHICS_SURFACE* dst,
	const GRAPHICS_PATTERN* pattern,
	int is_mask,
	const GRAPHICS_RECTANGLE_INT* extents,
	const GRAPHICS_RECTANGLE_INT* sample,
	int* src_x,
	int* src_y,
	void* graphics
);
extern eGRAPHICS_STATUS InitializeGraphicsSurfaceForRectangle(
	GRAPHICS_SUBSURFACE *surface,
	GRAPHICS_SURFACE* target,
	FLOAT_T x, FLOAT_T y,
	FLOAT_T width, FLOAT_T height
);
extern GRAPHICS_SURFACE* GraphicsSurfaceCreateForRectangle(
	GRAPHICS_SURFACE* target,
	FLOAT_T x, FLOAT_T y,
	FLOAT_T width, FLOAT_T height
);
extern void InitializeGraphicsImageSourceBackend(GRAPHICS_SURFACE_BACKEND* backend);
extern eGRAPHICS_STATUS InitializeGraphicsImageSurfaceForRectangle(
	GRAPHICS_IMAGE_SURFACE* surface,
	GRAPHICS_IMAGE_SURFACE* target,
	FLOAT_T x, FLOAT_T y,
	FLOAT_T width, FLOAT_T height
);
extern GRAPHICS_SURFACE* GraphicsImageSurfaceCreateForRectangle(
	GRAPHICS_IMAGE_SURFACE* target,
	FLOAT_T x, FLOAT_T y,
	FLOAT_T width, FLOAT_T height
);

#ifdef __cplusplus
}
#endif

static INLINE GRAPHICS_SURFACE* GraphicsSurfaceSnapshotGetTarget(GRAPHICS_SURFACE* surface)
{
	GRAPHICS_SNAPSHOT_SURFACE *snapshot = (GRAPHICS_SNAPSHOT_SURFACE*)surface;
	GRAPHICS_SURFACE *target;

	target = GraphicsSurfaceReference(snapshot->target);

	return target;
}

#endif // #ifndef _INCLUDED_GRAPHICS_SURFACE_H_
