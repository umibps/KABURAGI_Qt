#ifndef _INCLUDED_GRAPHICS_CLIP_H_
#define _INCLUDED_GRAPHICS_CLIP_H_

#include "graphics_path.h"
#include "graphics_types.h"

typedef struct _GRAPHICS_CLIP_PATH
{
	int reference_count;
	int own_memory : 1;
	GRAPHICS_PATH_FIXED path;
	eGRAPHICS_FILL_RULE fill_rule;
	GRAPHICS_FLOAT tolerance;
	eGRAPHICS_ANTIALIAS antialias;
	struct _GRAPHICS_CLIP_PATH *prev;
} GRAPHICS_CLIP_PATH;

struct _GRAPHICS_CLIP
{
	GRAPHICS_RECTANGLE_INT extents;
	GRAPHICS_CLIP_PATH *path;

	GRAPHICS_BOX *boxes;
	int num_boxes;

	GRAPHICS_REGION *region;
	int is_region : 1;
	int clip_all : 1;
	int owm_memory : 1;

	GRAPHICS_BOX embedded_box;
};

#ifdef __cplusplus
extern "C" {
#endif

extern void IntializeGraphicsClip(GRAPHICS_CLIP* clip);
extern GRAPHICS_CLIP* CreateGraphicsClip(void);
extern GRAPHICS_REGION* GraphicsClipGetRegion(const GRAPHICS_CLIP* clip);
extern GRAPHICS_RECTANGLE_INT* GraphicsClipGetExtents(const GRAPHICS_CLIP* clip, void* graphics);
extern void GraphicsClipRelease(GRAPHICS_CLIP* clip);
extern void GraphicsClipDestroy(GRAPHICS_CLIP* clip);
extern void GraphicsClipPathDestroy(GRAPHICS_CLIP_PATH* clip_path);
extern void GraphicsClipDestroy(GRAPHICS_CLIP* clip);
extern GRAPHICS_CLIP* _GraphicsClipCopy(const GRAPHICS_CLIP* clip);
extern GRAPHICS_CLIP* GraphicsClipCopy(GRAPHICS_CLIP* clip, const GRAPHICS_CLIP* other);
extern GRAPHICS_CLIP* GraphicsClipCopyRegion(const GRAPHICS_CLIP* clip, GRAPHICS_CLIP* result);
extern GRAPHICS_CLIP* GraphicsClipIntersectRectangle(GRAPHICS_CLIP* clip, const GRAPHICS_RECTANGLE_INT* r);
extern GRAPHICS_CLIP* GraphicsClipIntersectBox(GRAPHICS_CLIP* clip, const GRAPHICS_BOX* box);
extern GRAPHICS_CLIP* GraphicsClipIntersectBoxes(GRAPHICS_CLIP* clip, const GRAPHICS_BOXES* boxes);
extern GRAPHICS_CLIP* GraphicsClipCopyWithTranslation(
	const GRAPHICS_CLIP* clip,
	int translate_x,
	int translate_y,
	GRAPHICS_CLIP* result
);

static INLINE GRAPHICS_CLIP* GraphicsClipCopyIntersectRectangle(const GRAPHICS_CLIP* clip, const GRAPHICS_RECTANGLE_INT* r)
{
	return GraphicsClipIntersectRectangle(_GraphicsClipCopy(clip), r);
}

static INLINE void GraphicsClipStealBoxes(GRAPHICS_CLIP* clip, GRAPHICS_BOXES* boxes)
{
	InitializeGraphicsBoxesForArray(boxes, clip->boxes, clip->num_boxes);
	clip->boxes = NULL;
	clip->num_boxes = 0;
}

static INLINE void GraphicsClipUnstealBoxes(GRAPHICS_CLIP* clip, GRAPHICS_BOXES* boxes)
{
	clip->boxes = boxes->chunks.base;
	clip->num_boxes = boxes->num_boxes;
}


static INLINE GRAPHICS_CLIP_PATH* GraphicsClipPathReference(GRAPHICS_CLIP_PATH* path)
{
	path->reference_count++;
	return path;
}

extern GRAPHICS_CLIP* GraphicsClipIntersectPath(
	GRAPHICS_CLIP* clip,
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rule,
	FLOAT_T tolerance,
	eGRAPHICS_ANTIALIAS antialias
);

extern GRAPHICS_CLIP* GraphicsClipReduceForComposite(const GRAPHICS_CLIP* clip, GRAPHICS_COMPOSITE_RECTANGLES* extents);
extern int GraphicsClipIsRegion(const GRAPHICS_CLIP* clip);
extern eGRAPHICS_STATUS GraphicsClipCombineWithSurface(
	const GRAPHICS_CLIP* clip,
	struct _GRAPHICS_SURFACE* dst,
	int dst_x, int dst_y
);

extern eGRAPHICS_STATUS GraphicsPolygonReduce(
	GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE fill_rule
);

extern int GraphicsClipContainsRectangle(const GRAPHICS_CLIP* clip, const GRAPHICS_RECTANGLE_INT* rect);
extern int GraphicsClipContainsBox(const GRAPHICS_CLIP* clip, const GRAPHICS_BOX* box);

extern GRAPHICS_CLIP* GraphicsClipFromBoxes(const GRAPHICS_BOXES* boxes, GRAPHICS_CLIP* result);

extern GRAPHICS_CLIP* GraphicsClipCopyPath(const GRAPHICS_CLIP* clip, GRAPHICS_CLIP* result);

#ifdef __cplusplus
}
#endif

#endif  // #ifndef _INCLUDED_GRAPHICS_CLIP_H_
