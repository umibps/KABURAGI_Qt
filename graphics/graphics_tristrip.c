#include <string.h>
#include "graphics_types.h"
#include "graphics_private.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

void a() {}

void InitializeGraphicsTristrip(GRAPHICS_TRISTRIP* strip)
{
	//(void)memset(strip, 0, sizeof(*strip));

	strip->status = GRAPHICS_STATUS_SUCCESS;

	strip->num_limits = 0;
	strip->num_points = 0;

	strip->size_points = sizeof(strip->points_embedded) / sizeof(strip->points_embedded[0]);
	strip->points = strip->points_embedded;
}

void GraphicsTristripFinish(GRAPHICS_TRISTRIP* strip)
{
	if(strip->points != strip->points_embedded)
	{
		MEM_FREE_FUNC(strip->points);
	}
}

void GraphicsTristripLimit(
	GRAPHICS_TRISTRIP* strip,
	const GRAPHICS_BOX* limits,
	int num_limits
)
{
	strip->limits = limits;
	strip->num_limits = num_limits;
}

void InitializeGraphicsTristripWithClip(GRAPHICS_TRISTRIP* strip, const GRAPHICS_CLIP* clip)
{
	InitializeGraphicsTristrip(strip);
	if(strip != NULL)
	{
		GraphicsTristripLimit(strip, clip->boxes, clip->num_boxes);
	}
}

static int GraphicsTristripGrow(GRAPHICS_TRISTRIP* strip)
{
	GRAPHICS_POINT *points;
	int new_size = 4 * strip->size_points;

	if(strip->points == strip->points_embedded)
	{
		points = (GRAPHICS_POINT*)MEM_ALLOC_FUNC(new_size * sizeof(GRAPHICS_POINT));
		if(points != NULL)
		{
			(void)memcpy(points, strip->points, sizeof(strip->points_embedded));
		}
	}
	else
	{
		points = (GRAPHICS_POINT*)MEM_REALLOC_FUNC(strip->points, new_size * sizeof(GRAPHICS_POINT));
	}

	if(UNLIKELY(points == NULL))
	{
		strip->status = GRAPHICS_STATUS_NO_MEMORY;
		return FALSE;
	}

	strip->points = points;
	strip->size_points = new_size;
	return TRUE;
}

void GraphicsTristripAddPoint(GRAPHICS_TRISTRIP* strip, const GRAPHICS_POINT* p)
{
	if(UNLIKELY(strip->num_points == strip->size_points))
	{
		if(UNLIKELY(FALSE == GraphicsTristripGrow(strip)))
		{
			return;
		}
	}

	strip->points[strip->num_points++] = *p;
}

void GraphicsTristripMoveTo(GRAPHICS_TRISTRIP* strip, const GRAPHICS_POINT* p)
{
	if(strip->num_points == 0)
	{
		return;
	}

	GraphicsTristripAddPoint(strip, &strip->points[strip->num_points-1]);
	GraphicsTristripAddPoint(strip, p);
}

void GraphicsTristripTranslate(GRAPHICS_TRISTRIP* strip, int x, int y)
{
	GRAPHICS_FLOAT_FIXED x_offset, y_offset;
	GRAPHICS_POINT *p;
	int i;

	x_offset = GraphicsFixedFromInteger(x);
	y_offset = GraphicsFixedFromInteger(y);

	for(i = 0, p = strip->points; i < strip->num_points; i++)
	{
		p->x += x_offset;
		p->y += y_offset;
	}
}

void GraphicsTristripExtents(const GRAPHICS_TRISTRIP* strip, GRAPHICS_BOX* extents)
{
	int i;

	if(strip->num_points == 0)
	{
		extents->point1.x = extents->point1.y = 0;
		extents->point2.x = extents->point2.y = 0;
		return;
	}

	extents->point2 = extents->point1 = strip->points[0];
	for(i = 1; i < strip->num_points; i++)
	{
		const GRAPHICS_POINT* p = &strip->points[i];

		if(p->x < extents->point1.x)
		{
			extents->point1.x = p->x;
	}
		else if(p->x > extents->point2.x)
		{
			extents->point2.x = p->x;
		}

		if(p->y < extents->point1.y)
		{
			extents->point1.y = p->y;
		}
		else if(p->y > extents->point2.y)
		{
			extents->point2.y = p->y;
		}
	}
}

#ifdef __cplusplus
}
#endif
