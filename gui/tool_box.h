#ifndef _INCLUDED_TOOL_BOX_GUI_H_
#define _INCLUDED_TOOL_BOX_GUI_H_

#include "../types.h"
#include "../layer.h"

#ifdef __cplusplus
extern "C" {
#endif

EXTERN void ChangeCurrentTool(eLAYER_TYPE layer_type, void* application);

EXTERN void ChangeDetailUI(
	eLAYER_TYPE layer_type,
	void* target_tool,
	int is_common_tool,
	void* application
);

EXTERN void UpdateColorChooseArea(void* ui);

#ifdef __cplusplus
}
#endif

#endif // #ifndef _INCLUDED_TOOL_BOX_GUI_H_
