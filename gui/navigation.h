#ifndef _NAVIGATION_GUI_H_
#define _NAVIGATION_GUI_H_

#include "../types.h"

EXTERN void NavigationSetDrawCanvasMatrix(
	NAVIGATION_WIDGETS* widgets,
	FLOAT_T zoom_x,
	FLOAT_T zoom_y,
	FLOAT_T angle,
	FLOAT_T trans_x,
	FLOAT_T trans_y
);

EXTERN void NavigationSetCanvasPattern(NAVIGATION_WIDGETS* widgets, DRAW_WINDOW* canvas);

#endif	/* _NAVIGATION_GUI_H_ */
