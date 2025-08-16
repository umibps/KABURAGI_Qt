#ifndef _INCLUDED_VECTOR_BRUSH_CORE_H_
#define _INCLUDED_VECTOR_BRUSH_CORE_H_

#include "types.h"
#include "brush_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*vector_bruch_core_func)(struct _DRAW_WINDOW* window, struct _VECTOR_BRUSH_CORE *core, EVENT_STATE* state);

typedef struct _VECTOR_BRUSH_CORE
{
	struct _APPLICATION *app;
	void* brush_data;
	uint8 (*color)[3];
	uint8 (*back_color)[3];

	char* name;
	char* image_file_path;

	uint8 *brush_cursor_buffer;
	size_t brush_cursor_buffer_size;
	GRAPHICS_IMAGE_SURFACE brush_cursor_surface;
	eLAYER_BLEND_MODE cursor_blend_mode;
	
	vector_bruch_core_func press_func, motion_func, release_func;
	void (*key_press_func)(struct _DRAW_WINDOW* window, void* key, void* data);
	void (*draw_cursor)(struct _DRAW_WINDOW* window, FLOAT_T x, FLOAT_T y, void* data);
	brush_update_function button_update, motion_update;
	void (*create_detail_ui)(struct _APPLICATION* app, void* data);
	void (*color_change)(const uint8 color[3], void* data);
	void* button;

	size_t detail_data_size;

	uint16 brush_type;
	char hot_key;
} VECTOR_BRUSH_CORE;

#ifdef __cplusplus
}
#endif

#endif	// #ifndef _INCLUDED_VECTOR_BRUSH_CORE_H_
