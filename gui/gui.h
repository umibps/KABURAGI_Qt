#ifndef _INCLUDED_GUI_H_
#define _INCLUDED_GUI_H_

#include "types.h"
#include "dialog.h"

typedef struct _LAYER_SET_WIDGETS
{
	void *show_child_button;
	void *button_image;
} LAYER_SET_WIDGETS;

typedef struct _LAYER_VIEW_WIDGETS
{
	void *layer_view;
} LAYER_VIEW_WIDGETS;

/*
* TEXT_LAYER_BALLOON_DATA_WIDGETS構造体
* 吹き出しの詳細形状を設定するウィジェット群
*/
typedef struct _TEXT_LAYER_BALLOON_DATA_WIDGETS
{
	void *num_edge;
	void *edge_size;
	void *random_seed;
	void *edge_random_size;
	void *edge_random_distance;
	void *num_children;
	void *start_child_size;
	void *end_child_size;
} TEXT_LAYER_BALLOON_DATA_WIDGETS;

struct _TOOL_WINDOW_WIDGETS
{
	void *tool_box;
};

#ifdef __cplusplus
extern "C" {
#endif

EXTERN void* GetMainWindowWidget(void* app);
EXTERN void WidgetUpdate(void* widget);
EXTERN void WidgetSetSensitive(void* widget, int sensitive);
EXTERN void BoxClear(void* layout);
EXTERN void MousePressEventToEventState(void* event, EVENT_STATE* state);
EXTERN void MouseMotionEventToEventState(void* event, EVENT_STATE* state);
EXTERN void MouseReleaseEventToEventState(void* event, EVENT_STATE* state);
EXTERN void TabletEventToEventState(void* event, EVENT_STATE* state);

EXTERN void* TimerInMotionQueueExecutionNew(unsigned int time_milliseconds);
EXTERN int TimerElapsedTime(void* timer);
EXTERN void DeleteTimer(void* timer);

EXTERN char* GetImageFileSaveaPath(APPLICATION* app);

#ifdef __cplusplus
}
#endif

#endif  /* #ifndef _INCLUDED_GUI_H_ */
