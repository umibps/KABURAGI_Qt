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
	const char* section_name
)
{
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
		PENCIL *pencil;
		pencil = (PENCIL*)(*core = (BRUSH_CORE*)MEM_ALLOC_FUNC(sizeof(*pencil)));
		(void)memset(pencil, 0, sizeof(*pencil));

		InitializeBrushCore(&pencil->core, app);
		LoadDefaultBrushData(*core, file, section_name);
		
		pencil->core.brush_type = BRUSH_TYPE_PENCIL;
		pencil->core.name = IniFileStrdup(file, section_name, "NAME");
		pencil->core.image_file_path = IniFileStrdup(file, section_name, "IMAGE");
		pencil->core.press_function = PencilPressCallBack;
		pencil->core.motion_function = PencilMotionCallBack;
		pencil->core.release_function = PencilReleaseCallBack;
		pencil->core.draw_cursor = PencilDrawCursor;
		pencil->core.brush_data = PencilGUI_New(app, &pencil->core);
		pencil->core.create_detail_ui = CreatePencilDetailUI;
		pencil->core.button_update = PencilButtonUpdate;
		pencil->core.motion_update = PencilMotionUpdate;
		pencil->core.change_zoom = PencilChangeZoom;
		pencil->core.change_color = PcncilChangeColor;
	}
	else if(StringCompareIgnoreCase(brush_type, "ERASER") == STRING_EQUAL)
	{
		ERASER *eraser;
		eraser = (ERASER*)(*core = (BRUSH_CORE*)MEM_ALLOC_FUNC(sizeof(*eraser)));
		(void)memset(eraser, 0, sizeof(*eraser));

		InitializeBrushCore(&eraser->core, app);
		LoadDefaultBrushData(*core, file, section_name);

		eraser->core.brush_type = BRUSH_TYPE_ERASER;
		eraser->core.name = IniFileStrdup(file, section_name, "NAME");
		eraser->core.image_file_path = IniFileStrdup(file, section_name, "IMAGE");
		eraser->core.press_function = EraserPressCallBack;
		eraser->core.motion_function = EraserMotionCallBack;
		eraser->core.release_function = EraserReleaseCallBack;
		eraser->core.draw_cursor = EraserDrawCursor;
		eraser->core.brush_data = EraserGUI_New(app, &eraser->core);
		eraser->core.create_detail_ui = CreateEraserDetailUI;
		eraser->core.button_update = EraserButtonUpdate;
		eraser->core.motion_update = EraserMotionUpdate;
		eraser->core.change_zoom = EraserChangeZoom;
	}
	else if(StringCompareIgnoreCase(brush_type, "BLEND_BRUSH") == STRING_EQUAL)
	{
		BLEND_BRUSH *brush;
		brush = (BLEND_BRUSH*)(*core = (BRUSH_CORE*)MEM_ALLOC_FUNC(sizeof(*brush)));
		(void)memset(brush, 0, sizeof(*brush));

		InitializeBrushCore(&brush->core, app);
		LoadDefaultBrushData(*core, file, section_name);

		brush->core.brush_type = BRUSH_TYPE_BLEND_BRUSH;
		brush->core.name = IniFileStrdup(file, section_name, "NAME");
		brush->core.image_file_path = IniFileStrdup(file, section_name, "IMAGE");
		brush->core.press_function = BlendBrushPressCallBack;
		brush->core.motion_function = BlendBrushMotionCallBack;
		brush->core.release_function = BlendBrushReleaseCallBack;
		brush->core.draw_cursor = BlendBrushDrawCursor;
		brush->core.brush_data = BlendBrushGUI_New(app, &brush->core);
		brush->core.create_detail_ui = CreateBlendBrushDetailUI;
		brush->core.button_update = BlendBrushButtonUpdate;
		brush->core.motion_update = BlendBrushMotionUpdate;
		brush->core.change_zoom = BlendBrushChangeZoom;
		brush->core.change_color = BlendBrushChangeColor;
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

#ifdef __cplusplus
}
#endif
