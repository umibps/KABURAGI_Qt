#ifndef _INCLUDED_GRAPHICS_REGION_H_
#define _INCLUDED_GRAPHICS_REGION_H_

#include "graphics_types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern GRAPHICS_REGION* CreateGraphicsRegionRectangles(const GRAPHICS_RECTANGLE_INT* rectangles, int count);
extern void GraphicsRegionFinish(GRAPHICS_REGION* region);
extern INLINE void ReleaseGraphcisRegion(GRAPHICS_REGION* region);
extern void DestroyGraphicsRegion(GRAPHICS_REGION* region);
extern GRAPHICS_REGION* GraphicsRegionReference(GRAPHICS_REGION* region);
extern void GraphicsBoxAddCurveTo(
	GRAPHICS_BOX* extents,
	const GRAPHICS_POINT* a,
	const GRAPHICS_POINT* b,
	const GRAPHICS_POINT* c,
	const GRAPHICS_POINT* d
);
extern void GraphicsBoxFromRectangle(GRAPHICS_BOX* box, const GRAPHICS_RECTANGLE_INT* rectangle);
extern void InitializeGraphicsBoxes(GRAPHICS_BOXES* boxes);
extern void GraphicsBoxesFinish(GRAPHICS_BOXES* boxes);
extern void GraphicsBoxesGetExtents(const GRAPHICS_BOX* boxes, int num_boxes, GRAPHICS_BOX* extents);
extern eGRAPHICS_STATUS GraphicsBoxesAdd(GRAPHICS_BOXES* boxes, eGRAPHICS_ANTIALIAS antialias, const GRAPHICS_BOX* box);
extern void GraphicsBoxesClear(GRAPHICS_BOXES* boxes);
extern void GraphicsBoxesExtents(const GRAPHICS_BOXES* boxes, GRAPHICS_BOX* box);
extern eGRAPHICS_STATUS GraphicsBoxesIntersect(const GRAPHICS_BOXES* a, const GRAPHICS_BOXES* b, GRAPHICS_BOXES* out);
extern int GraphicsBoxesCopyToClip(const GRAPHICS_BOXES* boxes, struct _GRAPHICS_CLIP* clip);
extern eGRAPHICS_STATUS GraphicsBentleyOttmannTessellateBoxes(
	const GRAPHICS_BOXES* in,
	eGRAPHICS_FILL_RULE fill_rule,
	GRAPHICS_BOXES* out
);
extern eGRAPHICS_STATUS GraphicsBentleyOttmannTessellateRectilinearPolygonTolBoxes(
	const GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE fill_rule,
	GRAPHICS_BOXES* boxes
);
extern eGRAPHICS_REGION_OVERLAP GraphicsRegionContainsRectangle(
	const GRAPHICS_REGION* region,
	const GRAPHICS_RECTANGLE_INT* rectangle
);
extern eGRAPHICS_STATUS GraphicsRasterisePolygonToBoxes(
	GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE fill_rule,
	GRAPHICS_BOXES* boxes
);

#ifdef __cplusplus
}
#endif

#endif // #ifndef _INCLUDED_GRAPHICS_REGION_H_
