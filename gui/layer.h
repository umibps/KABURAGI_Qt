#ifndef _INCLUDED_GUI_LAYER_H_
#define _INCLUDED_GUI_LAYER_H_

#include "../types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _TEXT_LAYER_WIDGETS
{
	void *text_field;
	void *buffer;
} TEXT_LAYER_WIDGETS;

EXTERN void UpdateLayerThumbnail(LAYER* layer);
EXTERN void UpdateLayerNameLabel(LAYER* layer);

EXTERN void UpdateLayerNameLabel(LAYER* layer);

EXTERN void LayerViewSetActiveLayer(void* view, LAYER* layer);
EXTERN void LayerViewMoveActiveLayer(void* view, LAYER* layer);

#ifdef __cplusplus
}
#endif

#endif	/* #ifndef _INCLUDED_GUI_LAYER_H_ */
