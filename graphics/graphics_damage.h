#ifndef _INCLUDED_GRAPHICS_DAMAGE_H_
#define _INCLUDED_GRAPHICS_DAMAGE_H_

#include "graphics_types.h"
#include "graphics_status.h"

typedef struct _GRAPHICS_DAMAGE_CHUNK
{
	struct _GRAPHICS_DAMAGE_CHUNK *next;
	GRAPHICS_BOX *base;
	int count;
	int size;
} GRAPHICS_DAMAGE_CHUNK;

typedef struct _GRAPHICS_DAMAGE
{
	eGRAPHICS_STATUS status;
	GRAPHICS_REGION *region;

	int dirty;
	int remain;
	int own_memory : 1;

	GRAPHICS_DAMAGE_CHUNK chunks, *tail;
	GRAPHICS_BOX boxes[32];
} GRAPHICS_DAMAGE;

#ifdef __cplusplus
extern "C" {
#endif

extern void InitializeGraphisDamage(GRAPHICS_DAMAGE* damage);
extern GRAPHICS_DAMAGE* CreateGraphicsDamage(void);
extern INLINE void ReleaseGraphicsDamage(GRAPHICS_DAMAGE* damage);
extern void DestroyGraphicsDamage(GRAPHICS_DAMAGE* damage);
extern GRAPHICS_DAMAGE* GraphicsDamageAddRectangle(GRAPHICS_DAMAGE* damage, const GRAPHICS_RECTANGLE_INT* r);

#ifdef __cplusplus
}
#endif

#endif // _INCLUDED_GRAPHICS_DAMAGE_H_
