#include <string.h>
#include "common_tools.h"
#include "ini_file.h"
#include "application.h"
#include "utils.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

int ReadCommonToolData(
	APPLICATION* app,
	INI_FILE_PTR file,
	char* section_name
)
{
	char *tool_type;
	int x, y;
	
	x = IniFileGetInteger(file, section_name, "X");
	y = IniFileGetInteger(file, section_name, "Y");
	if(x < 0 || x >= BRUSH_TABLE_WIDTH || y < 0 || y >= BRUSH_TABLE_HEIGHT)
	{
		return -1;
	}
	
	tool_type = IniFileStrdup(file, section_name, "TYPE");
	if(tool_type == NULL)
	{
		return -1;
	}
	
	LoadCommonToolDetailData(&app->tool_box.common_tools[y][x], file, section_name, tool_type, app);
	
	MEM_FREE_FUNC(tool_type);
	
	return 0;
}

void InitializeCommonToolTable(APPLICATION* app, INI_FILE_PTR file)
{
#define MAX_STR_LENGTH 256
	char hot_key[4];
	int i;

	for(i=0; i<file->section_count; i++)
	{
		ReadCommonToolData(app, file, file->section[i].section_name);
	}
}

int ReadCommonToolInitializeFile(APPLICATION* app, const char* file_path)
{
	INI_FILE_PTR file;
	FILE *fp = NULL;
	MEMORY_STREAM_PTR mem_stream;
	void *stream;
	size_t file_size;
	
	size_t (*read_func)(void*, size_t, size_t, void*);
	int initialized = FALSE;
	
	const char comon_tool_default_data[] =
#include "common_tool_resource.txt"
	
	if(file_path != NULL)
	{
		if((fp = fopen(file_path, "rb")) != NULL)
		{
			read_func = (size_t (*)(void*, size_t, size_t, void*))fread;
			stream = (void*)fp;
			(void)fseek(fp, 0, SEEK_END);
			file_size = ftell(fp);
			rewind(fp);
			initialized = TRUE;
		}
	}
	
	if(initialized == FALSE)
	{
		mem_stream = CreateMemoryStream(sizeof(comon_tool_default_data));
		stream = (void*)mem_stream;
		read_func = (size_t (*)(void*, size_t, size_t, void*))MemRead;
		file_size = sizeof(comon_tool_default_data);
		if(mem_stream == NULL)
		{
			return -1;
		}
		(void)memcpy(mem_stream->buff_ptr, comon_tool_default_data, sizeof(comon_tool_default_data));
	}
	
	file = CreateIniFile(stream, read_func, file_size, INI_READ);
	InitializeCommonToolTable(app, file);
	
	if(fp != NULL)
	{
		(void)fclose(fp);
	}
	else
	{
		(void)DeleteMemoryStream(mem_stream);
	}
	return 0;
}

void SetDefaultColorPickerCallbacks(APPLICATION* app)
{
	app->tool_box.color_picker.core.press_function = ColorPickerButtonPressCallback;
	app->tool_box.color_picker.core.motion_function = ColorPickerMotionCallback;
	app->tool_box.color_picker.core.release_function = ColorPickerReleaseCallback;
	app->tool_box.color_picker.core.display_function = ColorPickerDrawCursor;
}

#ifdef __cplusplus
}
#endif
