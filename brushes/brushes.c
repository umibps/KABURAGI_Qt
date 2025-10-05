#include <string.h>
#include "../brushes.h"
#include "../ini_file.h"
#include "../application.h"
#include "../gui/brushes_gui.h"

#ifdef __cplusplus
extern "C" {
#endif

void LoadDefaultBrushData(
	BRUSH_CORE* core,
	INI_FILE_PTR file,
	const char* section_name,
	APPLICATION* app
)
{
	core->name = IniFileStrdup(file, section_name, "NAME");
	core->image_file_path = IniFileStrdup(file, section_name, "IMAGE");

	core->color = &app->tool_box.color_chooser.rgb;
	core->back_color = &app->tool_box.color_chooser.back_rgb;
	core->app = app;

	core->base_scale = IniFileGetInteger(file, section_name, "MAGNIFICATION") - 1;
	if(core->base_scale < 0)
	{
		core->base_scale = 0;
	}
	else if(core->base_scale >=DEFAULT_BRUSH_NUM_BASE_SIZE)
	{
		core->base_scale = DEFAULT_BRUSH_NUM_BASE_SIZE - 1;
	}
	
	core->radius = IniFileGetDouble(file, section_name, "SIZE") * 0.5;
	if(core->radius > DEFAULT_BRUSH_SIZE_MAXIMUM * 0.5)
	{
		core->radius = DEFAULT_BRUSH_SIZE_MAXIMUM * 0.5;
	}
	else if(core->radius < DEFAULT_BRUSH_SIZE_MINIMUM * 0.5)
	{
		core->radius = DEFAULT_BRUSH_SIZE_MINIMUM * 0.5;
	}
	
	core->opacity = IniFileGetDouble(file, section_name, "FLOW");
	if(core->opacity > DEFAULT_BRUSH_FLOW_MAXIMUM)
	{
		core->opacity = DEFAULT_BRUSH_FLOW_MAXIMUM;
	}
	else if(core->opacity < DEFAULT_BRUSH_FLOW_MINIMUM)
	{
		core->opacity = DEFAULT_BRUSH_FLOW_MINIMUM;
	}
}

void LoadGeneralBrushData(
	GENERAL_BRUSH* brush,
	INI_FILE_PTR file,
	const char* section_name,
	APPLICATION* app
)
{
	LoadDefaultBrushData(&brush->core, file, section_name, app);

	brush->enter = IniFileGetInteger(file, section_name, "ENTER_SIZE");
	brush->out = IniFileGetInteger(file, section_name, "OUT_SIZE");
	brush->target = IniFileGetInteger(file, section_name, "TARGET");
	brush->blend_mode = IniFileGetInteger(file, section_name, "BLEND_MODE");
}

void WriteDefaultBrushData(
	BRUSH_CORE* core,
	INI_FILE_PTR file,
	const char* section_name,
	const char* brush_type
)
{
	(void)IniFileAddString(file, section_name, "TYPE", brush_type);

	(void)IniFileAddInteger(file, section_name, "MAGNIFICATION", core->base_scale, 10);
	(void)IniFileAddDouble(file, section_name, "SIZE", core->stride * 2, 2);
	(void)IniFileAddDouble(file, section_name, "FLOW", core->opacity * 100, 2);
}

void WriteGeneralBrushData(
	GENERAL_BRUSH* brush,
	INI_FILE_PTR file,
	const char* section_name,
	const char* brush_type
)
{
	WriteDefaultBrushData(&brush->core, file, section_name, brush_type);

	(void)IniFileAddDouble(file, section_name, "ENTER_SIZE", brush->enter * 100, 2);
	(void)IniFileAddDouble(file, section_name, "OUT_SIZE", brush->out * 100, 2);
	(void)IniFileAddDouble(file, section_name, "TARGET", brush->target, 10);
	(void)IniFileAddDouble(file, section_name, "BLEND_MODE", brush->blend_mode, 10);
}

void LoadBrushDetailData(
	BRUSH_CORE** core,
	INI_FILE_PTR file,
	const char* section_name,
	const char* brush_type,
	APPLICATION* app
)
{
	DRAW_WINDOW *active_canvas = GetActiveDrawWindow(app);
	
	if(StringCompareIgnoreCase(brush_type, "PENCIL") == STRING_EQUAL)
	{
		*core = LoadPencilData(file, section_name, app);
	}
	else if(StringCompareIgnoreCase(brush_type, "ERASER") == STRING_EQUAL)
	{
		*core = LoadEraserData(file, section_name, app);
	}
	else if(StringCompareIgnoreCase(brush_type, "BUCKET") == STRING_EQUAL)
	{
		*core = LoadBucketData(file, section_name, app);
	}
	else if(StringCompareIgnoreCase(brush_type, "BLEND_BRUSH") == STRING_EQUAL)
	{
		*core = LoadBlendBrushData(file, section_name, app);
	}

	if(*core != NULL)
	{
		(*core)->app = app;

		// ブラシカーソル初期化の代わりにキャンバス拡大率変更のコールバック使用
		if((*core)->change_zoom != NULL)
		{
			(*core)->change_zoom((active_canvas != NULL) ? active_canvas->zoom_rate : 1, *core);
		}
	}
}

/*
* WriteBrushDetailData関数
* ブラシの詳細設定を書き出す
* 引数
* tool_box	: ツールボックスウィンドウ
* file_path	: 書き出すファイルのパス
* app		: アプリケーションを管理する構造体のアドレス
* 返り値
*	正常終了:0	失敗:負の値
*/
int WriteBrushDetailData(TOOL_BOX* tool_box, const char* file_path, APPLICATION* app)
{
	FILE *fp;
	INI_FILE_PTR file;
	char brush_section_name[1024];
	char *write_string, brush_name[1024], hot_key[2] = {0};
	int brush_id = 1;
	int x, y;

	if((fp = fopen(file_path, "w")) == NULL)
	{
		return -1;
	}

	file = CreateIniFile((void*)fp, NULL, 0, INI_WRITE);

	// 文字コード記録
	if(tool_box->brush_code != NULL && *tool_box->brush_code != '\0')
	{
		IniFileAddString(file, "CODE", "CODE_TYPE", tool_box->brush_code);
	}
	else
	{
		IniFileAddString(file, "CODE", "CODE_TYPE", "UTF-8");
	}

	// フォントファイル名を記録
	IniFileAddString(file, "FONT", "FONT_FILE", tool_box->font_file);

	// ブラシテーブルの内容を記録
	for(y=0; y<BRUSH_TABLE_HEIGHT; y++)
	{
		for(x = 0; x<BRUSH_TABLE_WIDTH; x++)
		{
			if(tool_box->brushes[y][x] != NULL
				&& tool_box->brushes[y][x]->name != NULL)
			{
				(void)sprintf(brush_section_name, "BRUSH%d", brush_id);

				(void)strcpy(brush_name, tool_box->brushes[y][x]->name);
				StringRepalce(brush_name, "\n", "\\n");
				if(tool_box->brush_code == NULL ||
					StringCompareIgnoreCase(tool_box->brush_code, "UTF-8") == STRING_EQUAL)
				{
					write_string = MEM_STRDUP_FUNC(brush_name);
				}
				else
				{
					write_string = FromUTF8(brush_name, tool_box->brush_code);
				}
				(void)IniFileAddString(file, brush_section_name, "NAME", write_string);
				MEM_FREE_FUNC(write_string);

				(void)IniFileAddString(file, brush_section_name,
					"IMAGE", tool_box->brushes[y][x]->image_file_path);
				(void)IniFileAddInteger(file, brush_section_name,
					"X", x, 10);
				(void)IniFileAddInteger(file, brush_section_name,
					"Y", y, 10);
				hot_key[0] = tool_box->brushes[y][x]->hot_key;
				(void)IniFileAddString(file, brush_section_name,
					"HOT_KEY", hot_key);

				switch(tool_box->brushes[y][x]->brush_type)
				{
				case BRUSH_TYPE_PENCIL:
					WritePencilData(file, tool_box->brushes[y][x], brush_section_name);
					break;
				case BRUSH_TYPE_ERASER:
					WriteEraserData(file, tool_box->brushes[y][x], brush_section_name);
					break;
				case BRUSH_TYPE_BUCKET:
					WriteBucketData(file, tool_box->brushes[y][x], brush_section_name);
					break;
				case BRUSH_TYPE_BLEND_BRUSH:
					WriteBlendBrushData(file, tool_box->brushes[y][x], brush_section_name);
					break;
				}

				brush_id++;
			}
		}
	}

	WriteIniFileText(file, (int (*)(void*, const char*, ...))fprintf);
	file->delete_func(file);
	(void)fclose(fp);
}

#ifdef __cplusplus
}
#endif
