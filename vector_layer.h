#ifndef _INCLUDED_VECTOR_LAYER_H_
#define _INCLUDED_VECTOR_LAYER_H_

#include "types.h"
#include "vector.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _eVECTOR_LAYER_FLAGS
{
	VECTOR_LAYER_RASTERIZE_ALL = 0x01,
	VECTOR_LAYER_RASTERIZE_TOP = 0x02,
	VECTOR_LAYER_RASTERIZE_ACTIVE = 0x04,
	VECTOR_LAYER_RASTERIZE_CHECKED = 0x08,
	VECTOR_LAYER_FIX_LINE = 0x10
} eVECTOR_LAYER_FLAGS;

typedef struct _VECTOR_LAYER
{
	uint32 num_lines;
	VECTOR_DATA *base, *active_data, *top_data;
	VECTOR_POINT* active_point;
	struct _VECTOR_LINE_LAYER* mix;
	uint32 flags;
} VECTOR_LAYER;

// 関数のプロトタイプ宣言
EXTERN VECTOR_LINE_LAYER* CreateVectorLineLayer(
	LAYER* work,
	VECTOR_LINE* line,
	VECTOR_LAYER_RECTANGLE* rect
);

EXTERN void DeleteVectorLayer(VECTOR_LAYER** layer);

EXTERN void DeleteVectorLineLayer(VECTOR_LINE_LAYER** layer);

#ifdef __cplusplus
}
#endif

#endif	// #ifndef _INCLUDED_VECTOR_LAYER_H_
