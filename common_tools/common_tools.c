#include <string.h>
#include "ini_file.h"
#include "application.h"
#include "common_tools.h"
#include "tool_box.h"
#include "../gui/common_tools_gui.h"

#ifdef __cplusplus
extern "C" {
#endif

void LoadCommonToolDetailData(
	COMMON_TOOL_CORE** core,
	INI_FILE_PTR file,
	const char* section_name,
	const char* tool_type,
	APPLICATION* app
)
{
	if(StringCompareIgnoreCase(tool_type, "COLOR_PICKER") == 0)
	{
		COLOR_PICKER *picker;
		*core = (COMMON_TOOL_CORE*)(picker = (COLOR_PICKER*)MEM_ALLOC_FUNC(sizeof(*picker)));
		(void)memset(picker, 0, sizeof(*picker));
		
		picker->mode = (uint8)IniFileGetInteger(file, section_name, "MODE");
		picker->core.app = app;
		app->tool_box.color_picker.mode = picker->mode;

		picker->core.tool_type = TYPE_COLOR_PICKER;
		picker->core.name = IniFileStrdup(file, section_name, "NAME");
		picker->core.image_file_path = IniFileStrdup(file, section_name, "IMAGE");
		picker->core.press_function = (common_tool_function)ColorPickerButtonPressCallback;
		picker->core.motion_function = (common_tool_function)ColorPickerMotionCallback;
		picker->core.release_function = (common_tool_function)ColorPickerReleaseCallback;
		picker->core.display_function = (common_tool_display_function)ColorPickerDrawCursor;
		picker->core.tool_data = ColorPickerGUI_New(app, &picker->core);
		picker->core.create_detail_ui = CreateColorPickerDetailUI;
	}
}

#ifdef __cplusplus
}
#endif
