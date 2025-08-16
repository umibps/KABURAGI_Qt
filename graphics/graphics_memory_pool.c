#include "graphics_memory_pool.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

void InitializeGraphicsMemoryPool(GRAPHICS_MEMORY_POOL* pool, unsigned int node_size)
{
	const GRAPHICS_MEMORY_POOL zero_data = {0};
	*pool = zero_data;

	pool->first_free_node = NULL;
	pool->pools = &pool->embedded_pool;
	pool->free_pools = NULL;
	pool->node_size = node_size;

	pool->embedded_pool.next = NULL;
	pool->embedded_pool.size = sizeof(pool->embedded_data);
	pool->embedded_pool.data = pool->embedded_data;
}

void GraphicsMemoryPoolFinish(GRAPHICS_MEMORY_POOL* free_pool)
{
	GRAPHICS_FREELIST_POOL *pool;

	pool = free_pool->pools;
	while(pool != &free_pool->embedded_pool)
	{
		GRAPHICS_FREELIST_POOL *next = pool->next;
		MEM_FREE_FUNC(pool);
		pool = next;
	}

	pool = free_pool->free_pools;
	while(pool != NULL)
	{
		GRAPHICS_FREELIST_POOL *next = pool->next;
		MEM_FREE_FUNC(pool);
		pool = next;
	}
}

void* GraphicsFreelistAllocation(GRAPHICS_FREELIST* freelist)
{
	if(freelist->first_free_node != NULL)
	{
		GRAPHICS_FREELIST_NODE *node;

		node = freelist->first_free_node;
		freelist->first_free_node = node->next;
		return node;
	}

	return MEM_ALLOC_FUNC(freelist->node_size);
}

void* GraphicsMemoryPoolAllocationFromNewPool(GRAPHICS_MEMORY_POOL* memory_pool)
{
	GRAPHICS_FREELIST_POOL *pool;
	int pool_size;

	if(memory_pool->free_pools != NULL)
	{
		pool = memory_pool->free_pools;
		memory_pool->free_pools = pool->next;

		pool_size = pool->size;
	}
	else
	{
		if(memory_pool != &memory_pool->embedded_pool)
		{
			pool_size = 2 * memory_pool->pools->size;
		}
		else
		{
			pool_size = (128 * memory_pool->node_size + 8191) & -8192;
		}

		pool = (GRAPHICS_FREELIST_POOL*)MEM_ALLOC_FUNC(sizeof(*pool) + pool_size);
		if(pool == NULL)
		{
			return pool;
		}

		pool->size = pool_size;
	}

	pool->next = memory_pool->pools;
	memory_pool->pools = pool;

	pool->remain = pool_size - memory_pool->node_size;
	pool->data = (uint8*)(pool + 1) + memory_pool->node_size;

	return (void*)(pool + 1);
}

#ifdef __cplusplus
}
#endif
