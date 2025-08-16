#ifndef _INCLUDED_DRAW_WINDOW_GUI_H_
#define _INCLUDED_DRAW_WINDOW_GUI_H_

#include "../draw_window.h"

#ifdef __cplusplus
extern "C" {
#endif

EXTERN void UpdateCanvasWidget(DRAW_WINDOW_WIDGETS_PTR widgets);
EXTERN void UpdateCanvasWidgetArea(DRAW_WINDOW_WIDGETS_PTR widgets, int x, int y, int width, int height);
EXTERN void ExcecuteCanvasWidgetColorHistoryPopupMenu(DRAW_WINDOW_WIDGETS_PTR widgets);
EXTERN void ForceUpdateCanvasWidget(DRAW_WINDOW_WIDGETS_PTR widgets);

#ifdef __cplusplus
}
#endif

#endif // #ifndef _INCLUDED_DRAW_WINDOW_GUI_H_
