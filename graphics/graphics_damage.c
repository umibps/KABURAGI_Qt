#include <string.h>
#include "graphics_damage.h"
#include "graphics_region.h"
#include "graphics_private.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

INLINE void ReleaseGraphicsDamage(GRAPHICS_DAMAGE* damage)
{
	GRAPHICS_DAMAGE_CHUNK *chunk, *next;

	if(damage == NULL)
	{
		return;
	}

	for(chunk = damage->chunks.next;  chunk != NULL; chunk = next)
	{
		next = chunk->next;
		MEM_FREE_FUNC(chunk);
	}
	DestroyGraphicsRegion(damage->region);
}

void DestroyGraphicsDamage(GRAPHICS_DAMAGE* damage)
{
	ReleaseGraphicsDamage(damage);
	if(damage->own_memory)
	{
		MEM_FREE_FUNC(damage);
	}
}

void InitializeGraphisDamage(GRAPHICS_DAMAGE* damage)
{
	damage->status = GRAPHICS_STATUS_SUCCESS;
	damage->region = NULL;
	damage->dirty = 0;
	damage->tail = &damage->chunks;
	damage->chunks.base = damage->boxes;
	damage->chunks.size = sizeof(damage->boxes) / sizeof(damage->boxes[0]);
	damage->chunks.count = 0;
	damage->chunks.next = NULL;
	damage->remain = damage->chunks.size;
}

GRAPHICS_DAMAGE* CreateGraphicsDamage(void)
{
	GRAPHICS_DAMAGE *damage;
	damage = (GRAPHICS_DAMAGE*)MEM_ALLOC_FUNC(sizeof(*damage));
	if(damage == NULL)
	{
		return NULL;
	}
	InitializeGraphisDamage(damage);
	damage->own_memory = 1;
	return damage;
}

static GRAPHICS_DAMAGE* GraphicsDamageAddBoxes(GRAPHICS_DAMAGE* damage, const GRAPHICS_BOX* boxes, int count)
{
	GRAPHICS_DAMAGE_CHUNK *chunk;
	int n, size;

	if(damage == NULL)
	{
		damage = CreateGraphicsDamage();
	}
	if(damage->status != GRAPHICS_STATUS_SUCCESS)
	{
		return damage;
	}

	damage->dirty += count;

	n = count;
	if(n > damage->remain)
	{
		n = damage->remain;
	}

	(void)memcpy(damage->tail->base + damage->tail->count, boxes,
		n * sizeof(GRAPHICS_BOX));

	count -= n;
	damage->tail->count += n;
	damage->remain -= n;

	if(count == 0)
	{
		return damage;
	}

	size = 2 * damage->tail->size;
	if(size < count)
	{
		size = (count + 64) & ~(63);
	}
	chunk = (GRAPHICS_DAMAGE_CHUNK*)MEM_ALLOC_FUNC(sizeof(*chunk) + sizeof(GRAPHICS_BOX) * size);
	if(chunk == NULL)
	{
		DestroyGraphicsDamage(damage);
	}

	chunk->next = NULL;
	chunk->base = (GRAPHICS_BOX*)(chunk + 1);
	chunk->size = size;
	chunk->count = count;

	damage->tail->next = chunk;
	damage->tail = chunk;

	(void)memcpy(damage->tail->base, boxes + n, count * sizeof(GRAPHICS_BOX));
	damage->remain = size - count;

	return damage;
}

GRAPHICS_DAMAGE* GraphicsDamageAddRectangle(GRAPHICS_DAMAGE* damage, const GRAPHICS_RECTANGLE_INT* r)
{
	GRAPHICS_BOX box;

	box.point1.x = r->x;
	box.point1.y = r->y;
	box.point2.x = r->x + r->width;
	box.point2.y = r->y + r->height;

	return GraphicsDamageAddBoxes(damage, &box, 1);
}

#ifdef __cplusplus
}
#endif
