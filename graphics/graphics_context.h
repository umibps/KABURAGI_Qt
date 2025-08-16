#ifndef _INCLUDED_GRAPHICS_CONTEXT_H_
#define _INCLUDED_GRAPHICS_CONTEXT_H_

#include "graphics.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void DestroyGraphicsContext(struct _GRAPHICS_CONTEXT* context);

extern void GraphicsDefaultContextFinish(GRAPHICS_DEFAULT_CONTEXT* context);

#ifdef __cplusplus
}
#endif

#endif // #ifndef _INCLUDED_GRAPHICS_CONTEXT_H_
