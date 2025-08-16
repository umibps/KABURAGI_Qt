#ifndef _INCLUDED_GUI_DIALOG_H_
#define _INCLUDED_GUI_DIALOG_H_

#include "gui_enum.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void* MessageDialogNew(const char* message, void* parent_window, eDIALOG_TYPE dialog_type);
extern eDIALOG_RESULT MessageDialogExecute(void* dialog);
extern void MessageDialogDestroy(void* dialog);
extern void MessageDialogMoveToParentCenter(void* dialog);

EXTERN char* GetImageFileOpenPath(APPLICATION* app);
EXTERN char* GetImageFileSaveaPath(APPLICATION* app);

#ifdef __cplusplus
}
#endif

#endif  /* #ifndef _INCLUDED_GUI_DIALOG_H_ */
