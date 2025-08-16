#ifndef _INCLUDED_DISPLAY_FILTER_H_
#define _INCLUDED_DISPLAY_FILTER_H_

#include "types.h"

typedef enum _eDISPLAY_FUNCION_TYPE
{
	DISPLAY_FUNC_TYPE_NO_CONVERT,
	DISPLAY_FUNC_TYPE_GRAY_SCALE,
	DISPLAY_FUNC_TYPE_GRAY_SCALE_YIQ,
	DISPLAY_FUNC_TYPE_ICC_PROFILE,
	NUM_DISPLAY_FUNCION_TYPE
} eDISPLAY_FUNCION_TYPE;

/*
* DISPLAY_FILTER構造体
* 表示時に適用するフィルターの情報
*/
typedef struct _DISPLAY_FILTER
{
	void (*filter_funcion)(
		uint8* source, uint8* destination, int size, void* filter_data);
	void *filter_data;
} DISPLAY_FILTER;

#endif /* #ifndef _INCLUDED_DISPLAY_FILTER_H_ */

