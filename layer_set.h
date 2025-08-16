#ifndef _INCLUDED_LAYER_SET_H_
#define _INCLUDED_LAYER_SET_H_

#include "gui/gui.h"

typedef struct _LAYER_SET
{
	struct _LAYER *active_under;
	LAYER_SET_WIDGETS *widgets;
	struct _LAYER_SET *layer_set;
} LAYER_SET;

#endif	// #ifndef _INCLUDED_LAYER_SET_H_
