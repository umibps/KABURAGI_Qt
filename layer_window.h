#ifndef _INCLUDED_LAYER_WINDOW_H_
#define _INCLUDED_LAYER_WINDOW_H_

#include "layer.h"
#include "gui/layer.h"

#define LAYER_THUMBNAIL_SIZE 32

#ifdef __cplusplus
extern "C" {
#endif

EXTERN void LayerViewAddLayer(LAYER *layer, LAYER *bottom, void *view, uint16 num_layer);

EXTERN void DeleteLayerWidget(LAYER* layer);

#ifdef __cplusplus
}
#endif

#endif // _INCLUDED_LAYER_WINDOW_H_

