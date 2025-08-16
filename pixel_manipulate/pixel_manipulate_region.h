#ifndef _INCLUDED_PIXEL_MANIPULATE_REGION_H_
#define _INCLUDED_PIXEL_MANIPULATE_REGION_H_

#include "pixel_manipulate.h"

#define PIXEL_MANIPULATE_REGION32_NUM_RECTS(region)  ((region)->data != NULL ? (region)->data->num_rects : 1)

#define PIXEL_MANIPULATE_REGION_EMPTY_BOX ((PIXEL_MANIPULATE_BOX32*)1)
#define PIXEL_MANIPULATE_REGION_EMPTY_DATA ((PIXEL_MANIPULATE_REGION32_DATA*)2)

#ifdef __cplusplus
extern "C" {
#endif

extern void InitializePixelManipulateRegion32(PIXEL_MANIPULATE_REGION32* region);

extern void InitializePixelManipulateRegion32Rectangle(
	PIXEL_MANIPULATE_REGION32* region,
	int x,
	int y,
	unsigned int width,
	unsigned int height
);
extern int InitializePixelManipulateRegion32Rectangles(
	PIXEL_MANIPULATE_REGION32* region,
	const PIXEL_MANIPULATE_BOX32* boxes,
	int count
);

extern void PixelManipulateRegion32Finish(PIXEL_MANIPULATE_REGION32* region);

extern int PixelManipulateRegion32Copy(PIXEL_MANIPULATE_REGION32* dest, PIXEL_MANIPULATE_REGION32* src);

extern int PixelManipulateRegion32Intersect(
	PIXEL_MANIPULATE_REGION32* new_region,
	PIXEL_MANIPULATE_REGION32* region1,
	PIXEL_MANIPULATE_REGION32* region2
);
extern int PixelManipulateRegion32IntersectRectangle(
	PIXEL_MANIPULATE_REGION32* dest,
	PIXEL_MANIPULATE_REGION32* source,
	int x,
	int y,
	unsigned int width,
	unsigned int height
);
extern void PixelManipulateSetExtents(PIXEL_MANIPULATE_REGION32* region);
extern ePIXEL_MANIPULATE_REGION_OVERLAP PixelManipulateRegion32ContainsRectangle(
	PIXEL_MANIPULATE_REGION32* region,
	PIXEL_MANIPULATE_BOX32* prect
);

#ifdef __cplusplus
}
#endif

#endif // #ifndef _INCLUDED_PIXEL_MANIPULATE_REGION_H_
