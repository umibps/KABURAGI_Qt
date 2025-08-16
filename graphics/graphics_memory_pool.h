#ifndef _INCLUDED_GRAPHICS_MEMORY_POOL_H_
#define _INCLUDED_GRAPHICS_MEMORY_POOL_H_

#include "types.h"

typedef struct _GRAPHICS_FREELIST_NODE
{
	struct _GRAPHICS_FREELIST_NODE *next;
} GRAPHICS_FREELIST_NODE;

typedef struct _GRAPHICS_FREELIST
{
	GRAPHICS_FREELIST_NODE *first_free_node;
	unsigned int node_size;
} GRAPHICS_FREELIST;

typedef struct _GRAPHICS_FREELIST_POOL
{
	struct _GRAPHICS_FREELIST_POOL *next;
	unsigned int size, remain;
	uint8 *data;
} GRAPHICS_FREELIST_POOL;

typedef struct _GRAPHICS_MEMORY_POOL
{
	GRAPHICS_FREELIST_NODE *first_free_node;
	GRAPHICS_FREELIST_POOL *pools;
	GRAPHICS_FREELIST_POOL *free_pools;
	unsigned int node_size;
	GRAPHICS_FREELIST_POOL embedded_pool;
	uint8 embedded_data[1024];
} GRAPHICS_MEMORY_POOL;


#ifdef __cplusplus
extern "C" {
#endif

extern void InitializeGraphicsMemoryPool(GRAPHICS_MEMORY_POOL* pool, unsigned int node_size);
extern void GraphicsMemoryPoolFinish(GRAPHICS_MEMORY_POOL* free_pool);
extern void* GraphicsFreelistAllocation(GRAPHICS_FREELIST* freelist);
extern void* GraphicsMemoryPoolAllocationFromNewPool(GRAPHICS_MEMORY_POOL* memory_pool);

#ifdef __cplusplus
}
#endif

static INLINE void* GraphicsMemoryPoolAllocationFromPool(GRAPHICS_MEMORY_POOL* memory_pool)
{
	GRAPHICS_FREELIST_POOL *pool;
	uint8 *pointer;

	pool = memory_pool->pools;
	if(memory_pool->node_size > pool->remain)
	{
		return GraphicsMemoryPoolAllocationFromNewPool(memory_pool);
	}

	pointer = pool->data;
	pool->data += memory_pool->node_size;
	pool->remain -= memory_pool->node_size;
	return (void*)pointer;
}

static INLINE void* GraphicsMemoryPoolAllocation(GRAPHICS_MEMORY_POOL* pool)
{
	GRAPHICS_FREELIST_NODE *node;

	node = pool->first_free_node;
	if(node == NULL)
	{
		return GraphicsMemoryPoolAllocationFromPool(pool);
	}

	pool->first_free_node = node->next;

	return (void*)node;
}

static INLINE void GraphicsMemoryPoolFree(GRAPHICS_MEMORY_POOL* pool, void* pointer)
{
	GRAPHICS_FREELIST_NODE *node = (GRAPHICS_FREELIST_NODE*)pointer;

	node->next = pool->first_free_node;
	pool->first_free_node = node;
}

static INLINE void GraphicsMemoryPoolReset(GRAPHICS_MEMORY_POOL* memory_pool)
{
	while(memory_pool->pools != &memory_pool->embedded_pool)
	{
		GRAPHICS_FREELIST_POOL *pool = memory_pool->pools;
		memory_pool->pools = pool->next;
		pool->next = memory_pool->free_pools;
		memory_pool->free_pools = pool;
	}

	memory_pool->embedded_pool.remain = sizeof(memory_pool->embedded_data);
	memory_pool->embedded_pool.data = memory_pool->embedded_data;
}

#endif // #ifndef _INCLUDED_GRAPHICS_MEMORY_POOL_H_

