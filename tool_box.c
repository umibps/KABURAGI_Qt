#include "application.h"
#include "brushes.h"

#ifdef __cplusplus
extern "C" {
#endif

void WriteToolBoxData(
	APPLICATION* app,
	const char* common_tool_file_path,
	const char* brush_file_path,
	const char* vector_brush_file_path
)
{
	WriteBrushDetailData(&app->tool_box, brush_file_path, app);
}

#ifdef __cplusplus
}
#endif
