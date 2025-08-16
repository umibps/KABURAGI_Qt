#ifndef _INCLUDED_GRAPHICS_LIST_H_
#define _INCLUDED_GRAPHICS_LIST_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ASSERT
# if defined(_DEBUG)
#  include <assert.h>
#  define ASSERT(expr) assert(expr)
# else
#  define ASSERT(expr)
# endif
#endif

typedef struct _GRAPHICS_LIST
{
	struct _GRAPHICS_LIST *next, *prev;
} GRAPHICS_LIST;

#define GRAPHICS_LIST_FOR_EACH_ENTRY(position, type, head, member) \
	for(position = GRAPHICS_CONTAINER_OF((head)->next, type, member); \
		&position->member != (head); \
		position = GRAPHICS_CONTAINER_OF(position->member.next, type, member))

static INLINE int GraphicsListIsEmpty(const GRAPHICS_LIST* head);

#ifdef _DEBUG
static INLINE void GraphicsListValidate(const GRAPHICS_LIST* link)
{
	// ASSERT(link->next->prev == link);
	// ASSERT(link->prev->next == link);
}

static INLINE void GraphicsListValidateIsEmpty(const GRAPHICS_LIST* head)
{
	ASSERT(head->next == NULL || (GraphicsListIsEmpty(head) != FALSE
			|| head->next == head->prev));
}
#else
# define GraphicsListValidate(link)
# define GraphicsListValidateIsEmpty(head)
#endif

static INLINE void _GraphicsListAddInternal(GRAPHICS_LIST* entry, GRAPHICS_LIST* previous, GRAPHICS_LIST* next)
{
	next->prev = entry;
	entry->next = next;
	entry->prev = previous;
	previous->next = entry;
}

static INLINE int GraphicsListIsEmpty(const GRAPHICS_LIST* head)
{
	GraphicsListValidate(head);
	return head->next == head;
}

static INLINE void InitializeGraphicsList(GRAPHICS_LIST* entry)
{
	entry->next = entry;
	entry->prev = entry;
}

static INLINE void DeleteGraphicsListOne(GRAPHICS_LIST* prev, GRAPHICS_LIST* next)
{
	next->prev = prev;
	prev->next = next;
}

static INLINE void DeleteGraphicsListInternal(GRAPHICS_LIST* list)
{
	DeleteGraphicsListOne(list->prev, list->next);
}

static INLINE void DeleteGraphicsList(GRAPHICS_LIST* list)
{
	DeleteGraphicsListInternal(list);
	InitializeGraphicsList(list);
}

static INLINE void DestroyGraphicsList(GRAPHICS_LIST* entry)
{
	DeleteGraphicsListOne(entry->prev, entry->next);
	InitializeGraphicsList(entry);
}

static INLINE void GraphicsListAdd(GRAPHICS_LIST* entry, GRAPHICS_LIST* head)
{
	GraphicsListValidate(head);
	GraphicsListValidateIsEmpty(entry);
	_GraphicsListAddInternal(entry, head, head->next);
	GraphicsListValidate(head);
}

static INLINE void GraphicsListAddTail(GRAPHICS_LIST* entry, GRAPHICS_LIST* head)
{
	GraphicsListValidate(head);
	GraphicsListValidateIsEmpty(entry);
	_GraphicsListAddInternal(entry, head->prev, head);
	GraphicsListValidate(head);
}

#ifdef __cplusplus
}
#endif

#endif // #ifndef _INCLUDED_GRAPHICS_LIST_H_
