#ifndef _INCLUDED_COMMON_TOOLS_H_
#define _INCLUDED_COMMON_TOOLS_H_

#include "types.h"
#include "brush_core.h"
#include "utils.h"
#include "ini_file.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*common_tool_function)(DRAW_WINDOW* window, COMMON_TOOL_CORE* core, EVENT_STATE* state);
typedef void (*common_tool_display_function)(DRAW_WINDOW* window, COMMON_TOOL_CORE* core, FLOAT_T x, FLOAT_T y);

typedef enum _eCOMMON_TOOL_TYPE
{
    TYPE_COLOR_PICKER
} eCOMMON_TOOL_TYPE;

typedef struct _COMMON_TOOL_CORE
{
	FLOAT_T min_x, min_y, max_x, max_y;
	char *name;
	char *image_file_path;

	void* tool_data;
	common_tool_function press_function, motion_function, release_function;
	common_tool_display_function display_function;
	brush_update_function button_update, motion_update;
	void (*create_detail_ui)(struct _APPLICATION* app, void* data);
	void* button;

	eCOMMON_TOOL_TYPE tool_type;
	APPLICATION *app;
	uint8 flags;
	char hot_key;
};

typedef enum _eCOMMON_TOOL_FLAGS
{
	COMMON_TOOL_DO_RELEASE_FUNC_AT_CHANGI_TOOL = 0x01
} eCOMMON_TOOL_FLAGS;

EXTERN int ReadCommonToolInitializeFile(APPLICATION* app, const char* file_path);
extern void LoadCommonToolDetailData(
	COMMON_TOOL_CORE** core,
	INI_FILE_PTR file,
	const char* section_name,
	const char* tool_type,
	APPLICATION* app
);

typedef enum _eCOLOR_PICKER_SOURCE
{
	COLOR_PICKER_SOURCE_ACTIVE_LAYER,
	COLOR_PICKER_SOURCE_CANVAS
} eCOLOR_PICKER_SOURCE;

typedef struct _COLOR_PICKER
{
    COMMON_TOOL_CORE core;
	uint8 mode;
} COLOR_PICKER;

extern void ColorPickerButtonPressCallback(
	DRAW_WINDOW* canvas,
	EVENT_STATE* state,
	COMMON_TOOL_CORE* core
);
#define ColorPickerMotionCallback ToolDummyFunction
#define ColorPickerReleaseCallback ToolDummyFunction
#define ColorPickerDrawCursor ToolDummyDisplay

extern void* ColorPickerGUI_New(APPLICATION* app, COMMON_TOOL_CORE* core);

extern void SetDefaultColorPickerCallbacks(APPLICATION* app);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _INCLUDED_COMMON_TOOLS_H_ */
