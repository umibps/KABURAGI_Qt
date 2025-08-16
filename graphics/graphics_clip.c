#include <string.h>
#include <limits.h>
#include "graphics.h"
#include "graphics_function.h"
#include "graphics_region.h"
#include "graphics_private.h"
#include "graphics_path.h"
#include "../memory.h"

#ifdef __cplusplus
extern "C" {
#endif

static GRAPHICS_CLIP* GraphicsClipPathCopyWithTranslation(
	GRAPHICS_CLIP* clip,
	GRAPHICS_CLIP_PATH* other_path,
	int fx, int fy
);

void InitializeGraphicsClip(GRAPHICS_CLIP* clip)
{
	const GRAPHICS_RECTANGLE_INT unbounded =
	{
		GRAPHICS_RECTANGLE_INT_MIN, GRAPHICS_RECTANGLE_INT_MIN,
		GRAPHICS_RECTANGLE_INT_MAX - GRAPHICS_RECTANGLE_INT_MIN,
		GRAPHICS_RECTANGLE_INT_MAX - GRAPHICS_RECTANGLE_INT_MIN,
		1, 0
	};

	clip->extents = unbounded;

	clip->path = NULL;
	clip->boxes = NULL;
	clip->num_boxes = 0;
	clip->region = NULL;
	clip->is_region = FALSE;
	clip->clip_all = FALSE;
}

static void GraphicsClipExtractRegion(GRAPHICS_CLIP* clip)
{
	GRAPHICS_RECTANGLE_INT stack_rectangles[GRAPHICS_STACK_ARRAY_LENGTH(GRAPHICS_RECTANGLE_INT)];
	GRAPHICS_RECTANGLE_INT *r = stack_rectangles;
	int is_region;
	int i;

	if(clip->num_boxes == 0)
	{
		return;
	}

	if(clip->num_boxes > sizeof(stack_rectangles) / sizeof(stack_rectangles[0]))
	{
		r = (GRAPHICS_RECTANGLE_INT*)MEM_ALLOC_FUNC(sizeof(*r) * clip->num_boxes);
		if(r == NULL)
		{
			return;
		}
	}

	is_region = clip->path == NULL;
	for(i=0; i<clip->num_boxes; i++)
	{
		GRAPHICS_BOX *b = &clip->boxes[i];
		if(is_region)
		{
			is_region = GRAPHICS_FIXED_IS_INTEGER(b->point1.x | b->point1.y | b->point2.x | b->point2.y);
		}
		r[i].x = GRAPHICS_FIXED_INTEGER_FLOOR(b->point1.x);
		r[i].y = GRAPHICS_FIXED_INTEGER_FLOOR(b->point1.y);
		r[i].width = GRAPHICS_FIXED_INTEGER_CEIL(b->point2.x) - r[i].x;
		r[i].height = GRAPHICS_FIXED_INTEGER_CEIL(b->point2.y) - r[i].y;
	}

	clip->is_region = is_region;

	clip->region = CreateGraphicsRegionRectangles(r, i);

	if(r != stack_rectangles)
	{
		MEM_FREE_FUNC(r);
	}
}

GRAPHICS_REGION* GraphicsClipGetRegion(const GRAPHICS_CLIP* clip)
{
	if(clip == NULL)
	{
		return NULL;
	}

	if(clip->region == NULL)
	{
		GraphicsClipExtractRegion((GRAPHICS_CLIP*)clip);
	}

	return clip->region;
}

GRAPHICS_RECTANGLE_INT* GraphicsClipGetExtents(const GRAPHICS_CLIP* clip, void* graphics)
{
	GRAPHICS *g = (GRAPHICS*)graphics;

	if(clip == NULL)
	{
		return &g->unbounded_rectangle;
	}

	if(GraphicsClipIsAllClipped(clip))
	{
		return &g->empty_rectangle;
	}

	return &clip->extents;
}

GRAPHICS_CLIP* CreateGraphicsClip(void)
{
	GRAPHICS_CLIP *clip;

	clip = (GRAPHICS_CLIP*)MEM_ALLOC_FUNC(sizeof(*clip));

	InitializeGraphicsClip(clip);
	clip->owm_memory = 1;

	return clip;
}

void GraphicsClipPathDestroy(GRAPHICS_CLIP_PATH* clip_path)
{
	ASSERT(clip_path->reference_count);

	if(REFERENCE_COUNT_DECREMENT_AND_TEST(clip_path->reference_count) == FALSE)
	{
		return;
	}

	GraphicsPathFixedFinish(&clip_path->path);

	if(clip_path->prev != NULL)
	{
		GraphicsClipPathDestroy(clip_path->prev);
	}

	if(clip_path->own_memory)
	{
		MEM_FREE_FUNC(clip_path);
	}
}

void GraphicsClipRelease(GRAPHICS_CLIP* clip)
{
	if(clip == NULL || clip->clip_all)
	{
		return;
	}

	if(clip->path != NULL)
	{
		GraphicsClipPathDestroy(clip->path);
	}

	if(clip->boxes != &clip->embedded_box)
	{
		MEM_FREE_FUNC(clip->boxes);
	}
	DestroyGraphicsRegion(clip->region);
}

void GraphicsClipDestroy(GRAPHICS_CLIP* clip)
{
	if(clip == NULL)
	{
		return;
	}
	GraphicsClipRelease(clip);
	if(clip->owm_memory)
	{
		MEM_FREE_FUNC(clip);
	}
}

GRAPHICS_CLIP_PATH* GraphicsClipReference(GRAPHICS_CLIP_PATH* path)
{
	path->reference_count++;
	return path;
}

GRAPHICS_CLIP* _GraphicsClipCopy(const GRAPHICS_CLIP* clip)
{
	GRAPHICS_CLIP *copy;

	if(clip == NULL || clip->clip_all)
	{
		return (GRAPHICS_CLIP*)clip;
	}

	copy = CreateGraphicsClip();

	if(clip->path != NULL)
	{
		copy->path = GraphicsClipReference(clip->path);
	}

	if(clip->num_boxes != 0)
	{
		if(clip->num_boxes == 1)
		{
			copy->boxes = &copy->embedded_box;
		}
		else
		{
			copy->boxes = (GRAPHICS_BOX*)MEM_ALLOC_FUNC(sizeof(*copy->boxes) * clip->num_boxes);
			if(copy->boxes == NULL)
			{
				copy->clip_all = 1;
				return copy;
			}
		}

		(void)memcpy(copy->boxes, clip->boxes, clip->num_boxes * sizeof(*clip->boxes));
		copy->num_boxes = clip->num_boxes;
	}

	copy->extents = clip->extents;
	copy->region = GraphicsRegionReference(clip->region);
	copy->is_region = clip->is_region;

	return copy;
}

GRAPHICS_CLIP* GraphicsClipCopy(GRAPHICS_CLIP* clip, const GRAPHICS_CLIP* other)
{
	GRAPHICS_CLIP *copy;

	if(other == NULL || other->clip_all)
	{
		return (GRAPHICS_CLIP*)clip;
	}

	if(clip == NULL)
	{
		copy = CreateGraphicsClip();
	}
	else
	{
		GraphicsClipRelease(clip);
		InitializeGraphicsClip(clip);
		copy = clip;
	}

	if(other->path != NULL)
	{
		copy->path = GraphicsClipReference(other->path);
	}

	if(other->num_boxes > 0)
	{
		if(other->num_boxes == 1)
		{
			copy->boxes = &copy->embedded_box;
		}
		else
		{
			copy->boxes = MEM_CALLOC_FUNC(other->num_boxes, sizeof(*copy->boxes));
			if(UNLIKELY(copy->boxes == NULL))
			{
				return copy;
			}
		}

		(void)memcpy(copy->boxes, other->boxes, other->num_boxes * sizeof(*copy->boxes));
		copy->num_boxes = other->num_boxes;
	}

	copy->extents = other->extents;
	copy->region = GraphicsRegionReference(other->region);
	copy->is_region = other->is_region;
	copy->clip_all = other->clip_all;

	return copy;
}

GRAPHICS_CLIP* GraphicsClipCopyWithTranslation(
	const GRAPHICS_CLIP* clip,
	int translate_x,
	int translate_y,
	GRAPHICS_CLIP* result
)
{
	GRAPHICS_CLIP *copy;
	const GRAPHICS_CLIP zero_clip = {0};
	int fx, fy, i;

	if(clip == NULL || GraphicsClipIsAllClipped(clip))
	{
		return (GRAPHICS_CLIP*)clip;
	}

	if(translate_x == 0 || translate_y == 0)
	{
		return _GraphicsClipCopy(clip);
	}

	if(result == NULL)
	{
		copy = CreateGraphicsClip();
	}
	else
	{
		copy = result;
	}
	*copy = zero_clip;

	fx = GraphicsFixedFromInteger(translate_x);
	fy = GraphicsFixedFromInteger(translate_y);

	if(clip->num_boxes != 0)
	{
		if(clip->num_boxes == 1)
		{
			copy->boxes = &copy->embedded_box;
		}
		else
		{
			copy->boxes = (GRAPHICS_BOX*)MEM_ALLOC_FUNC(clip->num_boxes * sizeof(GRAPHICS_BOX));
			if(copy->boxes == NULL)
			{
				return (GRAPHICS_CLIP*)clip;
			}
		}

		for(i=0; i<clip->num_boxes; i++)
		{
			copy->boxes[i].point1.x = clip->boxes[i].point1.x + fx;
			copy->boxes[i].point1.y = clip->boxes[i].point1.y + fy;
			copy->boxes[i].point2.x = clip->boxes[i].point2.x + fx;
			copy->boxes[i].point2.y = clip->boxes[i].point2.y + fy;
		}

		copy->num_boxes = clip->num_boxes;
	}

	copy->extents = clip->extents;
	copy->extents.x += translate_x;
	copy->extents.y += translate_y;

	if(clip->path == NULL)
	{
		return copy;
	}

	return GraphicsClipPathCopyWithTranslation(copy, clip->path, fx, fy);
}

GRAPHICS_CLIP* GraphicsClipCopyRegion(const GRAPHICS_CLIP* clip, GRAPHICS_CLIP* result)
{
	GRAPHICS_CLIP *copy;
	int i;

	if(clip == NULL || GraphicsClipIsAllClipped(clip))
	{
		return (GRAPHICS_CLIP*)clip;
	}

	ASSERT(clip->num_boxes);

	if(result = NULL)
	{
		copy = CreateGraphicsClip();
	}
	else
	{
		copy = result;
		InitializeGraphicsClip(copy);
	}
	copy->extents = clip->extents;

	if(clip->num_boxes == 1)
	{
		copy->boxes = &copy->embedded_box;
	}
	else
	{
		copy->boxes = MEM_ALLOC_FUNC(clip->num_boxes * sizeof(*clip->boxes));
		if(copy->boxes == NULL)
		{
			return copy;
		}
	}

	for(i=0; i<clip->num_boxes; i++)
	{
		copy->boxes[i].point1.x = GRAPHICS_FIXED_FLOOR(clip->boxes[i].point1.x);
		copy->boxes[i].point1.y = GRAPHICS_FIXED_FLOOR(clip->boxes[i].point1.y);
		copy->boxes[i].point2.x = GRAPHICS_FIXED_CEIL(clip->boxes[i].point2.x);
		copy->boxes[i].point2.y = GRAPHICS_FIXED_CEIL(clip->boxes[i].point2.y);
	}
	copy->num_boxes = clip->num_boxes;

	copy->region = GraphicsRegionReference(clip->region);
	copy->is_region = TRUE;

	return copy;
}

GRAPHICS_CLIP* GraphicsClipCopyPath(const GRAPHICS_CLIP* clip, GRAPHICS_CLIP* result)
{
	GRAPHICS_CLIP *copy;

	if(clip == NULL || clip->clip_all)
	{
		return (GRAPHICS_CLIP*)clip;
	}

	if(result == NULL)
	{
		copy = CreateGraphicsClip();
	}
	else
	{
		copy = result;
		InitializeGraphicsClip(copy);
	}
	copy->extents = clip->extents;
	if(clip->path != NULL)
	{
		copy->path = GraphicsClipPathReference(clip->path);
	}

	return copy;
}

int GraphicsClipContainsRectangleBox(
	const GRAPHICS_CLIP* clip,
	const GRAPHICS_RECTANGLE_INT* rect,
	const GRAPHICS_BOX* box
)
{
	int i;

	if(clip == NULL)	// NULLはno clip扱い
	{
		return TRUE;
	}

	if(clip->clip_all)
	{
		return FALSE;
	}

	if(clip->path != NULL)
	{
		return FALSE;
	}

	if(GRAPHICS_RECTANGLE_CONTAINS_RECTANGLE(&clip->extents, rect) == FALSE)
	{
		return FALSE;
	}

	if(clip->num_boxes == 0)
	{
		return TRUE;
	}

	for(i=0; i<clip->num_boxes; i++)
	{
		if(box->point1.x >= clip->boxes[i].point1.x
			&& box->point1.y >= clip->boxes[i].point1.y
			&& box->point2.x <= clip->boxes[i].point2.x
			&& box->point1.y <= clip->boxes[i].point2.y
		)
		{
			return TRUE;
		}
	}

	return FALSE;
}

int GraphicsClipContainsRectangle(const GRAPHICS_CLIP* clip, const GRAPHICS_RECTANGLE_INT* rect)
{
	GRAPHICS_BOX box;

	GraphicsBoxFromRectangleInt(&box, rect);
	return GraphicsClipContainsRectangleBox(clip, &rect, &box);
}

int GraphicsClipContainsBox(const GRAPHICS_CLIP* clip, const GRAPHICS_BOX* box)
{
	GRAPHICS_RECTANGLE_INT rectangle;

	GraphicsBoxRoundToRectangle(box, &rectangle);
	return GraphicsClipContainsRectangleBox(clip, &rectangle, box);
}

GRAPHICS_CLIP* GraphicsClipIntersectRectangleBox(
	GRAPHICS_CLIP* clip,
	const GRAPHICS_RECTANGLE_INT* r,
	const GRAPHICS_BOX* box
)
{
	GRAPHICS_BOX extents_box;
	int changed = FALSE;
	int i, j;

	if(clip == NULL)
	{
		clip = CreateGraphicsClip();
	}

	if(clip->num_boxes == 0)
	{
		clip->boxes = &clip->embedded_box;
		clip->boxes[0] = *box;
		clip->num_boxes = 1;
		if(clip->path == NULL)
		{
			clip->extents = *r;
		}
		else
		{
			if(FALSE == GraphicsRectangleIntersect(&clip->extents, r))
			{
				clip->clip_all = 1;
				return clip;
			}
		}
		if(clip->path == NULL)
		{
			clip->is_region = GraphicsBoxIsPixelAligned(box) ? TRUE : FALSE;
		}
		return clip;
	}

	if(clip->num_boxes == 1
		&& clip->boxes[0].point1.x >= box->point1.x
		&& clip->boxes[0].point1.y >= box->point1.y
		&& clip->boxes[0].point2.x <= box->point2.x
		&& clip->boxes[0].point2.y <= box->point2.y
	)
	{
		return clip;
	}

	for(i = j = 0; i < clip->num_boxes; i++)
	{
		GRAPHICS_BOX *b = &clip->boxes[j];

		if(j != i)
		{
			*b = clip->boxes[i];
		}

		if(box->point1.x > b->point1.x)
		{
			b->point1.x = box->point1.x, changed = TRUE;
		}
		if(box->point2.x < b->point2.x)
		{
			b->point2.x = box->point2.x, changed = TRUE;
		}

		if(box->point1.y > b->point1.y)
		{
			b->point1.y = box->point1.y, changed = TRUE;
		}
		if(box->point2.y < b->point2.y)
		{
			b->point2.y = box->point2.y, changed = TRUE;
		}

		j += b->point2.x > b->point1.x && b->point2.y > b->point1.y;
	}
	clip->num_boxes = j;

	if(clip->num_boxes == 0)
	{
		clip->clip_all = 1;
		return clip;
	}

	if(changed == FALSE)
	{
		return clip;
	}

	extents_box = clip->boxes[0];
	for(i=1; i<clip->num_boxes; i++)
	{
		if(clip->boxes[i].point1.x < extents_box.point1.x)
		{
			extents_box.point1.x = clip->boxes[i].point1.x;
		}

		if(clip->boxes[i].point1.y < extents_box.point1.y)
		{
			extents_box.point1.y = clip->boxes[i].point1.y;
		}

		if(clip->boxes[i].point2.x > extents_box.point2.x)
		{
			extents_box.point2.x = clip->boxes[i].point2.x;
		}

		if(clip->boxes[i].point2.y > extents_box.point2.y)
		{
			extents_box.point2.y = clip->boxes[i].point2.y;
		}
	}

	if(clip->path == NULL)
	{
		GraphicsBoxRoundToRectangle(&extents_box, &clip->extents);
	}
	else
	{
		GRAPHICS_RECTANGLE_INT extents_rect;

		GraphicsBoxRoundToRectangle(&extents_box, &extents_rect);
		if(FALSE == GraphicsRectangleIntersect(&clip->extents, &extents_rect))
		{
			clip->clip_all = 1;
			return clip;
		}
	}

	if(clip->region != NULL)
	{
		DestroyGraphicsRegion(clip->region);
		clip->region = NULL;
	}

	clip->is_region = FALSE;
	return clip;
}

GRAPHICS_CLIP* GraphicsClipIntersectRectangle(GRAPHICS_CLIP* clip, const GRAPHICS_RECTANGLE_INT* r)
{
	GRAPHICS_BOX box;

	if(GraphicsClipIsAllClipped(clip))
	{
		return clip;
	}

	if(r->width == 0 || r->height == 0)
	{
		clip->clip_all = 1;
		return clip;
	}

	GraphicsBoxFromRectangleInt(&box, r);

	return GraphicsClipIntersectRectangleBox(clip, r, &box);
}

GRAPHICS_CLIP* GraphicsClipIntersectBox(GRAPHICS_CLIP* clip, const GRAPHICS_BOX* box)
{
	GRAPHICS_RECTANGLE_INT r;

	if(GraphicsClipIsAllClipped(clip))
	{
		return clip;
	}

	GraphicsBoxRoundToRectangle(box, &r);
	if(r.width == 0 || r.height == 0)
	{
		clip->clip_all = 1;
		return clip;
	}

	return GraphicsClipIntersectRectangleBox(clip, &r, box);
}

GRAPHICS_CLIP* GraphicsClipIntersectBoxes(GRAPHICS_CLIP* clip, const GRAPHICS_BOXES* boxes)
{
	GRAPHICS_BOXES clip_boxes;
	GRAPHICS_BOX limits;
	GRAPHICS_RECTANGLE_INT extents;

	if(GraphicsClipIsAllClipped(clip))
	{
		return clip;
	}

	if(boxes->num_boxes == 0)
	{
		clip->clip_all = TRUE;
		return clip;
	}

	if(boxes->num_boxes == 1)
	{
		return GraphicsClipIntersectBox(clip, boxes->chunks.base);
	}

	if(clip == NULL)
	{
		clip = CreateGraphicsClip();
	}

	if(clip->num_boxes != 0)
	{
		InitializeGraphicsBoxesForArray(&clip_boxes, clip->boxes, clip->num_boxes);
		if(UNLIKELY(GraphicsBoxesIntersect(&clip_boxes, boxes, &clip_boxes)))
		{
			clip->clip_all = TRUE;
			goto OUT;
		}

		if(clip->boxes != &clip->embedded_box)
		{
			MEM_FREE_FUNC(clip->boxes);
		}

		clip->boxes = NULL;
		boxes = &clip_boxes;
	}

	if(boxes->num_boxes == 0)
	{
		clip->clip_all = TRUE;
		goto OUT;
	}

	GraphicsBoxesCopyToClip(boxes, clip);

	GraphicsBoxesExtents(boxes, &limits);

	GraphicsBoxRoundToRectangle(&limits, &extents);
	if(clip->path == NULL)
	{
		clip->extents = extents;
	}
	else if(FALSE == GraphicsRectangleIntersect(&clip->extents, &extents))
	{
		clip->clip_all = TRUE;
		goto OUT;
	}

	if(clip->region != NULL)
	{
		DestroyGraphicsRegion(clip->region);
		clip->region = NULL;
	}
	clip->is_region = FALSE;

OUT:
	if(boxes == &clip_boxes)
	{
		GraphicsBoxesFinish(&clip_boxes);
	}

	return clip;
}

typedef struct _REDUCE
{
	GRAPHICS_CLIP *clip;
	GRAPHICS_BOX limit;
	GRAPHICS_BOX extents;
	int inside;

	GRAPHICS_POINT current_point;
	GRAPHICS_POINT last_move_to;
} REDUCE;

static void AddClippedEdge(REDUCE* r, const GRAPHICS_POINT* point1, const GRAPHICS_POINT* point2, int y1, int y2)
{
	int32 x;

	x = GraphicsEdgeComputeIntersectionXforY(
				point1, point2, y1);
	if(x < r->extents.point1.x)
	{
		r->extents.point1.x = x;
	}

	x = GraphicsEdgeComputeIntersectionXforY(point1, point2, y2);
	if(x > r->extents.point2.x)
	{
		r->extents.point2.x = x;
	}

	if(y1 < r->extents.point1.y)
	{
		r->extents.point1.y = y1;
	}

	if(y2 > r->extents.point2.y)
	{
		r->extents.point2.y = y2;
	}

	r->inside = TRUE;
}

static void AddEdge(REDUCE* r, const GRAPHICS_POINT* point1, GRAPHICS_POINT* point2)
{
	int top, bottom;
	int top_y, bottom_y;
	int n;

	if(point1->y < point2->y)
	{
		top = point1->y;
		bottom = point2->y;
	}
	else
	{
		top = point2->y;
		bottom = point1->y;
	}

	if(bottom < r->limit.point1.y || top > r->limit.point2.y)
	{
		return;
	}

	if(point1->x > point2->x)
	{
		const GRAPHICS_POINT *temp = point1;
		point1 = point2;
		point2 = temp;
	}

	if(point2->x <= r->limit.point1.x || point1->x >= r->limit.point2.x)
	{
		return;
	}

	for(n=0; n<r->clip->num_boxes; n++)
	{
		const GRAPHICS_BOX *limits = &r->clip->boxes[n];

		if(bottom < limits->point1.y || top > limits->point2.y)
		{
			continue;
		}

		if(point2->x <= limits->point1.x || point1->x >= limits->point2.x)
		{
			continue;
		}

		if(point1->x >= limits->point1.x && point2->x <= limits->point1.x)
		{
			top_y = top;
			bottom_y = bottom;
		}
		else
		{
			int point1_y, point2_y;

			point1_y = GraphicsEdgeComputeIntersectionYforX(point1, point2, limits->point1.x);
			point2_y = GraphicsEdgeComputeIntersectionYforX(point1, point2, limits->point2.x);

			if(point1_y < point2_y)
			{
				top_y = point1_y;
				bottom_y = point2_y;
			}
			else
			{
				top_y = point2_y;
				bottom_y = point1_y;
			}

			if(top_y < top)
			{
				top_y = top;
			}
			if(bottom_y > bottom)
			{
				bottom_y = bottom;
			}
		}

		if(top_y < limits->point1.y)
		{
			top_y = limits->point1.y;
		}

		if(bottom_y > limits->point2.y)
		{
			bottom_y = limits->point2.y;
		}

		if(bottom_y >> top_y)
		{
			AddClippedEdge(r, point1, point2, top_y, bottom_y);
		}
	}
}

static eGRAPHICS_STATUS ReduceLineTo(void* closure, const GRAPHICS_POINT* point)
{
	REDUCE *r = (REDUCE*)closure;

	AddEdge(r, &r->current_point, point);
	r->current_point = *point;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS ReduceClose(void* closure)
{
	REDUCE *r = (REDUCE*)closure;
	return ReduceLineTo(r, &r->last_move_to);
}

static eGRAPHICS_STATUS ReduceMoveTo(void* closure, const GRAPHICS_POINT* point)
{
	REDUCE *r = (REDUCE*)closure;
	eGRAPHICS_STATUS status;

	status = ReduceClose(closure);

	r->current_point = *point;
	r->last_move_to = *point;

	return status;
}

GRAPHICS_CLIP* GraphicsClipReduceToBoxes(GRAPHICS_CLIP* clip)
{
	REDUCE r;
	GRAPHICS_CLIP_PATH *clip_path;
	eGRAPHICS_STATUS status;

	if(clip->path == NULL)
	{
		return clip;
	}

	r.clip = clip;
	r.extents.point1.x = r.extents.point1.y = INT_MAX;
	r.extents.point2.x = r.extents.point2.y = INT_MIN;
	r.inside = FALSE;

	r.limit.point1.x = GraphicsFixedFromInteger(clip->extents.x);
	r.limit.point1.y = GraphicsFixedFromInteger(clip->extents.y);
	r.limit.point2.x = GraphicsFixedFromInteger(clip->extents.x + clip->extents.width);
	r.limit.point2.y = GraphicsFixedFromInteger(clip->extents.y + clip->extents.height);

	clip_path = clip->path;
	do
	{
		r.current_point.x = 0;
		r.current_point.y = 0;
		r.last_move_to = r.current_point;

		status = GraphicsPathFixedInterpretFlat(&clip_path->path,
			ReduceMoveTo, ReduceLineTo, ReduceClose, &r, clip_path->tolerance);
		ASSERT(status == GRAPHICS_STATUS_SUCCESS);
		ReduceClose(&r);
	} while((clip_path = clip_path->prev) != NULL);

	if(r.inside == FALSE)
	{
		GraphicsClipPathDestroy(clip->path);
		clip->path = NULL;
	}

	return GraphicsClipIntersectBox(clip, &r.extents);
}

GRAPHICS_CLIP* GraphicsClipReduceToRectangle(const GRAPHICS_CLIP* clip, const GRAPHICS_RECTANGLE_INT* r)
{
	GRAPHICS_CLIP *copy;

	if(GraphicsClipIsAllClipped(clip))
	{
		return (GRAPHICS_CLIP*)clip;
	}

	if(GraphicsClipContainsRectangle(clip, r) != FALSE)
	{
		return GraphicsClipIntersectRectangle(NULL, r);
	}

	copy = GraphicsClipCopyIntersectRectangle(clip, r);
	if(GraphicsClipIsAllClipped(copy))
	{
		return copy;
	}

	return GraphicsClipReduceToBoxes(copy);
}

GRAPHICS_CLIP* GraphicsClipReduceForComposite(const GRAPHICS_CLIP* clip, GRAPHICS_COMPOSITE_RECTANGLES* extents)
{
	const GRAPHICS_RECTANGLE_INT *r;

	r = extents->is_bounded ? &extents->bounded : &extents->unbounded;
	return GraphicsClipReduceToRectangle(clip, r);
}

GRAPHICS_CLIP* GraphicsClipIntersectRectilinearPath(
	GRAPHICS_CLIP* clip,
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rule,
	eGRAPHICS_ANTIALIAS antialias
)
{
	eGRAPHICS_STATUS status;
	GRAPHICS_BOXES boxes;

	InitializeGraphicsBoxes(&boxes);
	status = GraphicsPathFixedFillRectilinearToBoxes(path, fill_rule, antialias, &boxes);
	if(LIKELY(status == GRAPHICS_STATUS_SUCCESS && boxes.num_boxes != 0))
	{
		clip = GraphicsClipIntersectBoxes(clip, &boxes);
	}
	else
	{
		clip->clip_all = TRUE;
	}
	GraphicsBoxesFinish(&boxes);

	return clip;
}

void InitializeGraphicsClipPath(GRAPHICS_CLIP_PATH* clip_path, GRAPHICS_CLIP* clip)
{
	clip_path->reference_count = 1;

	clip_path->prev = clip->path;
	clip->path = clip_path;
}

GRAPHICS_CLIP_PATH* CreateGraphicsClipPath(GRAPHICS_CLIP* clip)
{
	GRAPHICS_CLIP_PATH *clip_path;

	clip_path = (GRAPHICS_CLIP_PATH*)MEM_ALLOC_FUNC(sizeof(*clip_path));

	InitializeGraphicsClipPath(clip_path, clip);
	clip_path->own_memory = 1;

	return clip_path;
}

GRAPHICS_CLIP* GraphicsClipIntersectPath(
	GRAPHICS_CLIP* clip,
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rule,
	FLOAT_T tolerance,
	eGRAPHICS_ANTIALIAS antialias
)
{
	GRAPHICS_CLIP_PATH *clip_path;
	eGRAPHICS_STATUS status;
	GRAPHICS_RECTANGLE_INT extents;
	GRAPHICS_BOX box;

	if(GraphicsClipIsAllClipped(clip))
	{
		return clip;
	}

	if(path->fill_is_empty)
	{
		return (clip->clip_all) ? clip : NULL;
	}

	if(GraphicsPathFixedIsBox(path, &box))
	{
		if(antialias == GRAPHICS_ANTIALIAS_NONE)
		{
			box.point1.x = GRAPHICS_FIXED_ROUND_DOWN(box.point1.x);
			box.point1.y = GRAPHICS_FIXED_ROUND_DOWN(box.point1.y);
			box.point2.x = GRAPHICS_FIXED_ROUND_DOWN(box.point2.x);
			box.point2.y = GRAPHICS_FIXED_ROUND_DOWN(box.point2.y);
		}

		return GraphicsClipIntersectBox(clip, &box);
	}

	if(path->fill_is_rectilinear)
	{
		return GraphicsClipIntersectRectilinearPath(clip, path, fill_rule, antialias);
	}

	GraphicsPathFixedApproximateClipExtents(path, &extents);
	if(extents.width == 0 || extents.height == 0)
	{
		clip->clip_all = TRUE;
		return clip;
	}

	clip = GraphicsClipIntersectRectangle(clip, &extents);
	if(clip->clip_all)
	{
		return clip;
	}

	clip_path = CreateGraphicsClipPath(clip);

	status = InitializeGraphicsPathFixedCopy(&clip_path->path, path);
	if(UNLIKELY(status))
	{
		clip->clip_all = TRUE;
		return clip;
	}

	clip_path->fill_rule = fill_rule;
	clip_path->tolerance = tolerance;
	clip_path->antialias = antialias;

	if(clip->region != NULL)
	{
		DestroyGraphicsRegion(clip->region);
		clip->region = NULL;
	}

	clip->is_region = FALSE;
	return clip;
}

static GRAPHICS_CLIP* GraphicsClipPathCopyWithTranslation(
	GRAPHICS_CLIP* clip,
	GRAPHICS_CLIP_PATH* other_path,
	int fx, int fy
)
{
	eGRAPHICS_STATUS status;
	GRAPHICS_CLIP_PATH *clip_path;

	if(other_path->prev != NULL)
	{
		clip = GraphicsClipPathCopyWithTranslation(clip, other_path->prev, fx, fy);
	}

	if(GraphicsClipIsAllClipped(clip))
	{
		return clip;
	}

	clip_path = CreateGraphicsClipPath(clip);
	if(clip_path == NULL)
	{
		return clip;
	}

	status = InitializeGraphicsPathFixedCopy(&clip_path->path, &other_path->path);
	if(UNLIKELY(status))
	{
		return clip;
	}

	GraphicsPathFixedTranslate(&clip_path->path, fx, fy);

	clip_path->fill_rule = other_path->fill_rule;
	clip_path->tolerance = other_path->tolerance;
	clip_path->antialias = other_path->antialias;

	return clip;
}

void GraphicsClip(GRAPHICS_CONTEXT* context)
{
	eGRAPHICS_STATUS status;

	if(UNLIKELY(context->status))
	{
		return;
	}

	status = context->backend->clip(context);
	if(UNLIKELY(status))
	{
		// SET ERROR
	}
}

int GraphicsClipIsRegion(const GRAPHICS_CLIP* clip)
{
	if(clip == NULL)
	{
		return TRUE;
	}

	if(clip->is_region)
	{
		return TRUE;
	}

	if(clip->path != NULL)
	{
		return FALSE;
	}

	if(clip->num_boxes == 0)
	{
		return TRUE;
	}

	if(clip->region == NULL)
	{
		GraphicsClipExtractRegion((GRAPHICS_CLIP*)clip);
	}

	return clip->is_region;
}

eGRAPHICS_STATUS GraphicsClipCombineWithSurface(
	const GRAPHICS_CLIP* clip,
	struct _GRAPHICS_SURFACE* dst,
	int dst_x, int dst_y
)
{
	GRAPHICS_CLIP_PATH *copy_path;
	GRAPHICS_CLIP_PATH *clip_path;
	GRAPHICS_CLIP *copy;
	GRAPHICS_CLIP local_clip;
	eGRAPHICS_STATUS status = GRAPHICS_STATUS_SUCCESS;

	copy = GraphicsClipCopyWithTranslation(clip, -dst_x, -dst_y, &local_clip);
	copy_path = copy->path;
	copy->path = NULL;

	if(copy->boxes != NULL)
	{
		status = GraphicsSurfacePaint(dst, GRAPHICS_OPERATOR_IN, &((GRAPHICS*)dst->graphics)->white_pattern.base, copy);
	}

	clip = NULL;
	if(GraphicsClipIsRegion(copy))
	{
		clip = copy;
	}
	
	clip_path = copy_path;
	while(status == GRAPHICS_STATUS_SUCCESS && clip_path != NULL)
	{
		status = GraphicsSurfaceFill(dst, GRAPHICS_OPERATOR_IN, &((GRAPHICS*)dst->graphics)->white_pattern.base,
			&clip_path->path, clip_path->fill_rule, clip_path->tolerance, clip_path->antialias, clip);
		clip_path = clip_path->prev;
	}
	copy->path = copy_path;
	GraphicsClipDestroy(copy);
	return status;
}

static int CanConvertToPolygon(const GRAPHICS_CLIP* clip)
{
	GRAPHICS_CLIP_PATH *clip_path = clip->path;
	eGRAPHICS_ANTIALIAS antialias = clip_path->antialias;

	while((clip_path = clip_path->prev) != NULL)
	{
		if(clip_path->antialias != antialias)
		{
			return FALSE;
		}
	}

	return TRUE;
}

eGRAPHICS_INTEGER_STATUS GraphicsClipGetPolygon(
	const GRAPHICS_CLIP* clip,
	GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE* fill_rule,
	eGRAPHICS_ANTIALIAS* antialias
)
{
	eGRAPHICS_STATUS status;
	GRAPHICS_CLIP_PATH *clip_path;

	if(GraphicsClipIsAllClipped(clip))
	{
		InitializeGraphicsPolygon(polygon, NULL, 0);
		return GRAPHICS_INTEGER_STATUS_SUCCESS;
	}

	if(clip->path == NULL)
	{
		*fill_rule = GRAPHICS_FILL_RULE_WINDING;
		*antialias = GRAPHICS_ANTIALIAS_DEFAULT;
		return InitializeGraphicsPolygonBoxArray(polygon, clip->boxes, clip->num_boxes);
	}

	if(FALSE == CanConvertToPolygon(clip))
	{
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	}

	if(clip->num_boxes < 2)
	{
		InitializeGraphicsPolygonWithClip(polygon, clip);
	}
	else
	{
		InitializeGraphicsPolygon(polygon, NULL, 0);
	}

	clip_path = clip->path;
	*fill_rule = clip_path->fill_rule;
	*antialias = clip_path->antialias;

	status = GraphicsPathFixedFillToPolygon(&clip_path->path,
		clip_path->tolerance, polygon);
	if(UNLIKELY(status))
	{
		goto error;
	}

	if(clip->num_boxes > 1)
	{
		status = GraphicsPolygonIntersectWithBoxes(polygon, fill_rule,
					clip->boxes, clip->num_boxes);
		if(UNLIKELY(status))
		{
			goto error;
		}
	}

	polygon->limits = NULL;
	polygon->num_limits = 0;

	while((clip_path = clip_path->prev) != NULL)
	{
		GRAPHICS_POLYGON next;

		InitializeGraphicsPolygon(&next, NULL, 0);
		status = GraphicsPathFixedFillToPolygon(&clip_path->path,
					clip_path->tolerance, &next);
		if(status == GRAPHICS_STATUS_SUCCESS)
		{
			status = GraphicsPolygonIntersect(polygon, *fill_rule,
						&next, clip_path->fill_rule);
		}
		GraphicsPolygonFinish(&next);
		if(UNLIKELY(status))
		{
			goto error;
		}

		*fill_rule = GRAPHICS_FILL_RULE_WINDING;
	}

	return GRAPHICS_STATUS_SUCCESS;

error:
	GraphicsPolygonFinish(polygon);
	return status;
}

struct _GRAPHICS_SURFACE* GraphicsClipGetImage(
	const GRAPHICS_CLIP* clip,
	struct _GRAPHICS_SURFACE* target,
	const GRAPHICS_RECTANGLE_INT* extents
)
{
	GRAPHICS *graphics;
	GRAPHICS_SURFACE *surface;
	eGRAPHICS_STATUS status;

	graphics = (GRAPHICS*)target->graphics;

	surface = GraphicsSurfaceCreateSimilarImage(target, GRAPHICS_FORMAT_A8,
				extents->width, extents->height);
	if(UNLIKELY(surface->status))
	{
		return surface;
	}

	status = GraphicsSurfacePaint(surface, GRAPHICS_OPERATOR_SOURCE, &graphics->white_pattern, NULL);
	if(LIKELY(status == GRAPHICS_STATUS_SUCCESS))
	{
		status = GraphicsClipCombineWithSurface(clip, surface, extents->x, extents->y);
	}

	if(UNLIKELY(status))
	{
		DestroyGraphicsSurface(surface);
		surface = NULL;
	}

	return surface;
}

GRAPHICS_CLIP* GraphicsClipFromBoxes(const GRAPHICS_BOXES* boxes, GRAPHICS_CLIP* result)
{
	GRAPHICS_BOX extents;
	GRAPHICS_CLIP *clip;

	if(result != NULL)
	{
		InitializeGraphicsClip(result);
		clip = result;
	}
	else
	{
		result = CreateGraphicsClip();
		clip = result;
	}
	if(UNLIKELY(GraphicsBoxesCopyToClip(boxes, clip)))
	{
		return clip;
	}

	GraphicsBoxesExtents(boxes, &extents);
	GraphicsBoxRoundToRectangle(&extents, &clip->extents);

	return clip;
}

#ifdef __cplusplus
}
#endif
