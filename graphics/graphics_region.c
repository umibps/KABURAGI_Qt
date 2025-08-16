#include "graphics_types.h"
#include "graphics.h"
#include "graphics_private.h"
#include "../pixel_manipulate/pixel_manipulate_region.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

void GraphicsRegionFinish(GRAPHICS_REGION* region)
{
	ASSERT(region->reference_count > 0);
	PixelManipulateRegion32Finish(&region->region);
}

INLINE void ReleaseGraphcisRegion(GRAPHICS_REGION* region)
{
	if(region == NULL || region->reference_count <= 0)
	{
		return;
	}

	GraphicsRegionFinish(region);
}

void DestroyGraphicsRegion(GRAPHICS_REGION* region)
{
	ReleaseGraphcisRegion(region);
	MEM_FREE_FUNC(region);
}

GRAPHICS_REGION* GraphicsRegionReference(GRAPHICS_REGION* region)
{
	if(region == NULL || region->reference_count < 0)
	{
		return NULL;
	}

	region->reference_count++;
	return region;
}

GRAPHICS_REGION* CreateGraphicsRegionRectangles(const GRAPHICS_RECTANGLE_INT* rectangles, int count)
{
	PIXEL_MANIPULATE_BOX32 stack_pboxes[GRAPHICS_STACK_ARRAY_LENGTH(PIXEL_MANIPULATE_BOX32)];
	PIXEL_MANIPULATE_BOX32 *pboxes = stack_pboxes;
	GRAPHICS_REGION *region;
	int i;

	region = (GRAPHICS_REGION*)MEM_ALLOC_FUNC(sizeof(*region));
	if(UNLIKELY(region == NULL))
	{
		return NULL;
	}

	region->reference_count = 1;
	region->status = GRAPHICS_STATUS_SUCCESS;

	if(count == 1)
	{
		InitializePixelManipulateRegion32Rectangle(&region->region,
			rectangles->x, rectangles->y, rectangles->width, rectangles->height);

		return region;
	}

	if(count > sizeof(stack_pboxes) / sizeof(stack_pboxes[0]))
	{
		pboxes = (PIXEL_MANIPULATE_BOX32*)MEM_ALLOC_FUNC(sizeof(*pboxes) * count);
		if(pboxes == NULL)
		{
			free(region);
			return NULL;
		}
	}

	for(i=0; i<count; i++)
	{
		pboxes[i].x1 = rectangles[i].x;
		pboxes[i].y1 = rectangles[i].y;
		pboxes[i].x2 = rectangles[i].x + rectangles[i].width;
		pboxes[i].y2 = rectangles[i].x + rectangles[i].height;
	}

	i = InitializePixelManipulateRegion32Rectangles(&region->region, pboxes, count);

	if(pboxes != stack_pboxes)
	{
		MEM_FREE_FUNC(pboxes);
	}

	if(UNLIKELY(i == 0))
	{
		MEM_FREE_FUNC(region);
		return NULL;
	}

	return region;
}

eGRAPHICS_REGION_OVERLAP GraphicsRegionContainsRectangle(
	const GRAPHICS_REGION* region,
	const GRAPHICS_RECTANGLE_INT* rectangle
)
{
	PIXEL_MANIPULATE_BOX32 pbox;
	ePIXEL_MANIPULATE_REGION_OVERLAP poverlap;

	if(region->status)
	{
		return GRAPHICS_REGION_OVERLAP_OUT;
	}

	pbox.x1 = rectangle->x;
	pbox.y1 = rectangle->y;
	pbox.x2 = rectangle->x + rectangle->width;
	pbox.y2 = rectangle->y + rectangle->height;

	poverlap = PixelManipulateRegion32ContainsRectangle(&region->region, &pbox);
	switch(poverlap)
	{
	default:
	case PIXEL_MANIPULATE_REGION_OVERLAP_OUT: return GRAPHICS_REGION_OVERLAP_OUT;
	case PIXEL_MANIPULATE_REGION_OVERLAP_IN:  return GRAPHICS_REGION_OVERLAP_IN;
	case PIXEL_MANIPULATE_REGION_OVERLAP_PART: return GRAPHICS_REGION_OVERLAP_PART;
	}
}

#ifdef __cplusplus
}
#endif
