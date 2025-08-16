#ifndef _INCLUDED_COMMON_TOOLS_GUI_H_
#define _INCLUDED_COMMON_TOOLS_GUI_H_

#include "../types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void CreateDefaultCommonToolDetailUI(APPLICATION* app, COMMON_TOOL_CORE* core);

extern void* ColorPickerGUI_New(APPLICATION* app, COMMON_TOOL_CORE* core);

#define CreateColorPickerDetailUI CreateDefaultCommonToolDetailUI

#ifdef __cplusplus
}
#endif

#endif	/* #ifndef _INCLUDED_COMMON_TOOLS_GUI_H_ */