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
		*core = LoadColorPickerDetailData(file, section_name, app);
	}
}

#ifdef __cplusplus
}
#endif
