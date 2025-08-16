#ifndef _INCLUDED_GRAPHICS_FUNCTION_H_
#define _INCLUDED_GRAPHICS_FUNCTION_H_

#include "graphics.h"

#define REFERENCE_COUNT_DECREMENT_AND_TEST(count) ((--(count) == 0))

#define GRAPHICS_CONTAINER_OF(pointer, type, member) ((type*)((char*)(pointer) - (char*) &((type*)0)->member))
#define GRAPHICS_LIST_ENTRY(pointer, type, member) GRAPHICS_CONTAINER_OF(pointer, type, member)

#define GRAPHICS_PATH_HEAD(path) (&(path)->buffer.base)
#define GRAPHICS_PATH_BUFFER_NEXT(position) GRAPHICS_LIST_ENTRY((position)->link.next, GRAPHICS_PATH_BUFFER, link)

#ifdef __cplusplus
extern "C" {
#endif

extern void GraphicsStrokeStyleFinish(GRAPHICS_STROKE_STYLE* style);
extern void GraphicsPathFixedFinish(GRAPHICS_PATH_FIXED* path);
extern void ReleaseGraphcisArray(GRAPHICS_ARRAY* array);

#ifdef __cplusplus
}
#endif

#endif #ifndef _INCLUDED_GRAPHICS_FUNCTION_H_

