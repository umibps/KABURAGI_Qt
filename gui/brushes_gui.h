#ifndef _INCLUDED_BRUSHES_GUI_H_
#define _INCLUDED_BRUSHES_GUI_H_

#include "../types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void CreateDefaultBrushDetailUI(APPLICATION* app, BRUSH_CORE* core);

extern void* PencilGUI_New(APPLICATION* app, BRUSH_CORE* core);
extern void* EraserGUI_New(APPLICATION* app, BRUSH_CORE* core);
extern void* BlendBrushGUI_New(APPLICATION* app, BRUSH_CORE* core);

#define CreatePencilDetailUI CreateDefaultBrushDetailUI
#define CreateEraserDetailUI CreateDefaultBrushDetailUI
#define CreateBlendBrushDetailUI CreateDefaultBrushDetailUI

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _INCLUDED_BRUSHES_GUI_H_ */
