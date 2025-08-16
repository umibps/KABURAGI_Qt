#ifndef _INCLUDED_GRAPHICS_PEN_H_
#define _INCLUDED_GRAPHICS_PEN_H_

#include "graphics_types.h"
#include "graphics_status.h"

#ifdef __cplusplus
extern "C" {
#endif

extern eGRAPHICS_STATUS InitializeGraphicsPen(
	struct _GRAPHICS_PEN* pen,
	FLOAT_T radius,
	FLOAT_T tolerance,
	const GRAPHICS_MATRIX* matrix
);

extern void GraphicsPenFinish(struct _GRAPHICS_PEN* pen);

extern void GraphicsPenFindActiveClockwiseVertices(
	const struct _GRAPHICS_PEN* pen,
	const struct _GRAPHICS_SLOPE* in,
	const struct _GRAPHICS_SLOPE* out,
	int* start,
	int* stop
);
extern void GraphicsPenFindActiveCounterClockwiseVertices(
	const struct _GRAPHICS_PEN* pen,
	const struct _GRAPHICS_SLOPE* in,
	const struct _GRAPHICS_SLOPE* out,
	int* start,
	int* stop
);
extern int GraphicsPenFindActiveClockwiseVertexIndex(const struct _GRAPHICS_PEN* pen, const GRAPHICS_SLOPE* slope);
extern int GraphicsPenFindActiveCounterClockwiseVertexIndex(const struct _GRAPHICS_PEN* pen, const GRAPHICS_SLOPE* slope);
extern int GraphicsPenVerticesNeeded(FLOAT_T tolerance, FLOAT_T radius, const GRAPHICS_MATRIX* matrix);

#ifdef __cplusplus
}
#endif

#endif // #ifndef _INCLUDED_GRAPHICS_PEN_H_

