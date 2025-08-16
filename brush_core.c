#include <string.h>
#include "brush_core.h"
#include "application.h"
#include "ini_file.h"
#include "utils.h"
#include "memory.h"
#include "brushes.h"
#include "memory_stream.h"
#include "gui/draw_window.h"
#include "gui/brushes_gui.h"
#include "graphics/graphics_matrix.h"

#ifdef __cplusplus
extern "C" {
#endif

void InitializeBrushCore(BRUSH_CORE* core, APPLICATION* app)
{
	core->app = app;
	core->color = &app->tool_box.color_chooser.rgb;
	core->back_color = &app->tool_box.color_chooser.back_rgb;
}

/*
* DefaultToolUpdate関数
* デフォルトのツールアップデートの関数
* 引数
* window	: アクティブな描画領域
* x			: マウスカーソルのX座標
* y			: マウスカーソルのY座標
* dummy		: ダミーポインタ
*/
void DefaultToolUpdate(DRAW_WINDOW* canvas, FLOAT_T x, FLOAT_T y, void* dummy)
{
	canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_OVER;
}

/*
* BrushCoreSetCirclePattern関数
* ブラシの円形画像パターンを作成
* 引数
* core				: ブラシの基本情報
* r					: 半径
* outline_hardness	: 輪郭の硬さ
* blur				: ボケ足
* alpha				: 不透明度
* color				: 色
*/
void BrushCoreSetCirclePattern(
	BRUSH_CORE* core,
	FLOAT_T r,
	FLOAT_T outline_hardness,
	FLOAT_T blur,
	FLOAT_T alpha,
	const uint8 color[3]
)
{
	GRAPHICS_DEFAULT_CONTEXT context = {0};
	GRAPHICS_RADIAL_PATTERN radial_pattern = {0};
	FLOAT_T float_color[3] = {color[0] * DIV_PIXEL, color[1] * DIV_PIXEL, color[2] * DIV_PIXEL};

	if(core->temp_context.base.backend != NULL)
	{
		GraphicsPatternFinish(&core->brush_pattern.base);

		GraphicsSurfaceFinish(&core->brush_surface.base);

		GraphicsPatternFinish(&core->temp_pattern.base);

		GraphicsSurfaceFinish(&core->temp_surface.base);

		DestroyGraphicsContext(&core->temp_context.base);
	}

	if(core->brush_pattern_buffer == NULL)
	{
		core->brush_pattern_buffer = (uint8*)MEM_ALLOC_FUNC(
			BRUSH_MAXIMUM_CIRCLE_SIZE * BRUSH_MAXIMUM_CIRCLE_SIZE * 4);
		core->brush_pattern_buffer_size = BRUSH_MAXIMUM_CIRCLE_SIZE * BRUSH_MAXIMUM_CIRCLE_SIZE * 4;
	}
	(void)memset(core->brush_pattern_buffer, 0, core->brush_pattern_buffer_size);

	if(core->temp_pattern_buffer == NULL)
	{
		core->temp_pattern_buffer = (uint8*)MEM_ALLOC_FUNC(
			BRUSH_MAXIMUM_CIRCLE_SIZE * BRUSH_MAXIMUM_CIRCLE_SIZE * 4);
	}
	(void)memset(core->temp_pattern_buffer, 0, core->brush_pattern_buffer_size);

	InitializeGraphicsImageSurfaceForData(&core->brush_surface, core->brush_pattern_buffer,
		GRAPHICS_FORMAT_ARGB32, (int)(r*2+1), (int)(r*2+1), (int)(r*2+1)*4, &core->app->graphics);
	InitializeGraphicsDefaultContext(&context, &core->brush_surface.base, &core->app->graphics);
	InitializeGraphicsPatternRadial(&radial_pattern, r, r, 0, r, r, r, &core->app->graphics);
	GraphicsPatternSetExtend(&radial_pattern.base.base, GRAPHICS_EXTEND_NONE);
	GraphicsPatternAddColorStopRGBA(&radial_pattern.base.base, 0, float_color[0],
		float_color[1], float_color[2], alpha);
	if(blur > 0)
	{
		GraphicsPatternAddColorStopRGBA(&radial_pattern.base.base, 1-blur, float_color[0],
			float_color[1], float_color[2], alpha);
	}
	GraphicsPatternAddColorStopRGBA(&radial_pattern.base.base, 1, float_color[0],
		float_color[1], float_color[2], alpha * outline_hardness);

	GraphicsSetOperator(&context.base, GRAPHICS_OPERATOR_SOURCE);
	GraphicsSetAntialias(&context.base, GRAPHICS_ANTIALIAS_BEST);
	GraphicsSetSource(&context.base, &radial_pattern.base.base);
	GraphicsPaint(&context.base);

	InitializeGraphicsImageSurfaceForData(&core->temp_surface, core->temp_pattern_buffer,
		GRAPHICS_FORMAT_ARGB32, (int)(r*2+1), (int)(r*2+1), (int)(r*2+1)*4, &core->app->graphics);
	InitializeGraphicsDefaultContext(&core->temp_context, &core->temp_surface.base, &core->app->graphics);
	GraphicsSetOperator(&core->temp_context.base, GRAPHICS_OPERATOR_SOURCE);
	InitializeGraphicsPatternForSurface(&core->temp_pattern.surface, &core->temp_surface.base);

	InitializeGraphicsPatternForSurface(&core->brush_pattern.surface, &core->brush_surface.base);

	GraphicsPatternFinish(&radial_pattern.base.base);
	DestroyGraphicsContext(&context.base);
}

/*
* BrushCoreSetGrayCirclePattern関数
* ブラシのグレースケール円形画像パターンを作成
* 引数
* core				: ブラシの基本情報
* r					: 半径
* outline_hardness	: 輪郭の硬さ
* blur				: ボケ足
* alpha				: 不透明度
*/
void BrushCoreSetGrayCirclePattern(
	BRUSH_CORE* core,
	FLOAT_T r,
	FLOAT_T outline_hardness,
	FLOAT_T blur,
	FLOAT_T alpha
)
{
	GRAPHICS_DEFAULT_CONTEXT context = {0};
	GRAPHICS_RADIAL_PATTERN radial_pattern = {0};
	int width = (int)(r * 2 + 1);

	core->stride = width + (4 - (width % 4)) % 4;
	
	if(core->temp_context.base.backend != NULL)
	{
		GraphicsPatternFinish(&core->brush_pattern.base);

		GraphicsSurfaceFinish(&core->brush_surface.base);

		GraphicsPatternFinish(&core->temp_pattern.base);

		GraphicsSurfaceFinish(&core->temp_surface.base);

		DestroyGraphicsContext(&core->temp_context.base);
	}

	if(core->brush_pattern_buffer == NULL)
	{
		core->brush_pattern_buffer = (uint8*)MEM_ALLOC_FUNC(
			BRUSH_MAXIMUM_CIRCLE_SIZE * BRUSH_MAXIMUM_CIRCLE_SIZE * 4);
		core->brush_pattern_buffer_size = BRUSH_MAXIMUM_CIRCLE_SIZE * BRUSH_MAXIMUM_CIRCLE_SIZE * 4;
	}

	if(core->temp_pattern_buffer == NULL)
	{
		core->temp_pattern_buffer = (uint8*)MEM_ALLOC_FUNC(
			BRUSH_MAXIMUM_CIRCLE_SIZE * BRUSH_MAXIMUM_CIRCLE_SIZE * 4);
	}

	InitializeGraphicsImageSurfaceForData(&core->brush_surface, core->brush_pattern_buffer,
		GRAPHICS_FORMAT_A8, width, width, core->stride, &core->app->graphics);
	InitializeGraphicsDefaultContext(&context, &core->brush_surface.base, &core->app->graphics);
	GraphicsSetOperator(&context.base, GRAPHICS_OPERATOR_SOURCE);
	InitializeGraphicsPatternRadial(&radial_pattern, r, r, 0, r, r, r, &core->app->graphics);
	GraphicsPatternAddColorStopRGBA(&radial_pattern.base.base, 0, 0, 0, 0, alpha);
	GraphicsPatternAddColorStopRGBA(&radial_pattern.base.base, 1-blur, 0, 0, 0, alpha);
	GraphicsPatternAddColorStopRGBA(&radial_pattern.base.base, 1, 0, 0, 0, alpha*outline_hardness);
	GraphicsPatternSetExtend(&radial_pattern.base.base, GRAPHICS_EXTEND_NONE);
	GraphicsSetSource(&context.base, &radial_pattern.base.base);
	GraphicsPaint(&context.base);

	InitializeGraphicsImageSurfaceForData(&core->temp_surface, core->temp_pattern_buffer,
		GRAPHICS_FORMAT_A8, width, width, core->stride, &core->app->graphics);
	InitializeGraphicsDefaultContext(&core->temp_context, &core->temp_surface.base, &core->app->graphics);
	GraphicsSetOperator(&core->temp_context.base, GRAPHICS_OPERATOR_SOURCE);
	InitializeGraphicsPatternForSurface(&core->temp_pattern.surface, &core->temp_surface.base);
	GraphicsPatternSetExtend(&core->temp_pattern.base, GRAPHICS_EXTEND_NONE);

	InitializeGraphicsPatternForSurface(&core->brush_pattern.surface, &core->brush_surface.base);

	GraphicsPatternFinish(&radial_pattern.base.base);
	DestroyGraphicsContext(&context.base);
}

void ClearBeforeCursorPosition(
	DRAW_WINDOW* canvas,
	FLOAT_T before_x,
	FLOAT_T before_y,
	FLOAT_T current_x,
	FLOAT_T current_y,
	BRUSH_CORE* core
)
{
#define ZOOM_MARGIN 1.275
#define UPDATE_AREA_MARGIN 2
#define BRUSH_UPDATE_MARGIN 7
	FLOAT_T x, y;
	FLOAT_T width, height;
	FLOAT_T radius = core->radius * canvas->zoom_rate * ZOOM_MARGIN + BRUSH_UPDATE_MARGIN;

	if(before_x < current_x)
	{
		x = before_x - radius;
		width = current_x - before_x + radius * 2;
	}
	else
	{
		x = current_x - radius;
		width = before_x - current_x + radius * 2;
	}
	
	if(before_y < current_y)
	{
		y = before_y - radius;
		height = current_y - before_y + radius * 2;
	}
	else
	{
		y = current_y - radius;
		height = before_y - current_y + radius * 2;
	}
	
	UpdateCanvasWidgetArea(canvas->widgets, (int)x, (int)y, (int)width, (int)height);
	
#undef UPDATE_AREA_MARGIN
#undef ZOOM_MARGIN
#undef BRUSH_UPDATE_MARGIN
}

#define CURSOR_LINE_WIDTH (1)

void DefaultBrushDrawCursor(
	DRAW_WINDOW* canvas,
	FLOAT_T x,
	FLOAT_T y,
	BRUSH_CORE* core
)
{
	FLOAT_T radius = (core->radius * canvas->zoom_rate) + CURSOR_LINE_WIDTH;
	int diameter = (int)(radius * 2 + 0.5);
	int stride = diameter * 4;
	GRAPHICS_SURFACE_PATTERN pattern;
	GRAPHICS_MATRIX matrix;
	
	InitializeGraphicsPatternForSurface(&pattern, &core->brush_cursor_surface.base);
	InitializeGraphicsMatrixTranslate(&matrix, - (x - radius + CURSOR_LINE_WIDTH),
										- (y - radius + CURSOR_LINE_WIDTH) );
	GraphicsPatternSetMatrix(&pattern.base, &matrix);
	GraphicsSetSource(&canvas->disp_layer->context.base, &pattern.base);
	GraphicsPaint(&canvas->disp_layer->context.base);

	canvas->last_cursor_drawn_x = core->before_x;
	canvas->last_cursor_drawn_y = core->before_y;
}

void DefaultBrushButtonUpdate(DRAW_WINDOW* canvas, FLOAT_T x, FLOAT_T y, BRUSH_CORE* core)
{
#define UPDATE_MARGIN 3
	FLOAT_T r = core->radius * canvas->zoom_rate + UPDATE_MARGIN;
	UpdateCanvasWidgetArea(canvas->widgets, (int)(x - r -1), (int)(y - r - 1),
							(int)(r * 2 + UPDATE_MARGIN), (int)(r * 2 + UPDATE_MARGIN));
#undef UPDATE_MARGIN
}

void DefaultBrushMotionUpdate(DRAW_WINDOW* canvas, FLOAT_T x, FLOAT_T y, BRUSH_CORE* core)
{
#define ZOOM_MARGIN 1.275
#define BRUSH_UPDATE_MARGIN 7
#define UPDATE_AREA_MARGIN 4
	FLOAT_T start_x, start_y;
	FLOAT_T width, height;
	FLOAT_T r = core->radius * canvas->zoom_rate * ZOOM_MARGIN + BRUSH_UPDATE_MARGIN;
	
	if(canvas->before_cursor_x < x)
	{
		start_x = canvas->before_cursor_x - r;
		width = r * 2 + x - canvas->before_cursor_x;
	}
	else
	{
		start_x = x - r;
		width = r * 2 + canvas->before_cursor_x - x;
	}
	
	if(canvas->before_cursor_y < y)
	{
		start_y = canvas->before_cursor_y - r;
		height = r * 2 + y - canvas->before_cursor_y;
	}
	else
	{
		start_y = y - r;
		height = r * 2 + canvas->before_cursor_y - y;
	}
	
	if((core->flags & BRUSH_FLAG_ANTI_ALIAS))
	{
	#define ANTIALIAS_MARGIN 4
		start_x -= ANTIALIAS_MARGIN;
		width += ANTIALIAS_MARGIN;
		start_y -= ANTIALIAS_MARGIN;
		height += ANTIALIAS_MARGIN;
		r += ANTIALIAS_MARGIN / 2;
	#undef ANTIALIAS_MARGIN
	}
	
#undef BRUSH_UPDATE_MARGIN
#undef ZOOM_MARGIN

	if(core->cursor_update_info.update)
	{
		if(core->cursor_update_info.start_x > start_x)
		{
			int current_x = x - r;
			int current_end_x = x + r;
			int current_y = y - r;
			int current_end_y = y + r;
			int difference;
			
			if(current_x < 0)
			{
				current_x = 0;
			}
			else if(current_x > canvas->disp_layer->width)
			{
				core->cursor_update_info.update = FALSE;
			}
			
			if(current_end_x > canvas->disp_layer->width)
			{
				current_end_x = canvas->disp_layer->width;
			}
			else if(current_end_x <= 0)
			{
				core->cursor_update_info.update = FALSE;
			}
			
			if(current_y < 0)
			{
				current_y = 0;
			}
			else if(current_y > canvas->disp_layer->height)
			{
				core->cursor_update_info.update = FALSE;
			}
			
			if(current_end_y > canvas->disp_layer->height)
			{
				current_end_y = canvas->disp_layer->height;
			}
			else if(current_end_y <= 0)
			{
				core->cursor_update_info.update = FALSE;
			}
			
			difference = core->cursor_update_info.start_x - start_x;
			if(core->cursor_update_info.start_x > start_x)
			{
				core->cursor_update_info.start_x = start_x;
				core->cursor_update_info.width += difference;
			}
			if(core->cursor_update_info.start_x + core->cursor_update_info.width < current_end_x)
			{
				core->cursor_update_info.width = current_end_x - core->cursor_update_info.start_x;
			}
			
			difference = core->cursor_update_info.start_y - start_y;
			if(core->cursor_update_info.start_y > start_y)
			{
				core->cursor_update_info.start_y = start_y;
				core->cursor_update_info.height += difference;
			}
			if(core->cursor_update_info.start_y + core->cursor_update_info.height < current_end_y)
			{
				core->cursor_update_info.height = current_end_y - core->cursor_update_info.start_y;
			}
		}
	}
	else
	{
		core->cursor_update_info.update = TRUE;
		
		ClearBeforeCursorPosition(canvas, canvas->last_cursor_drawn_x, canvas->last_cursor_drawn_y, x, y, core);
		
		core->cursor_update_info.start_x = x - r;
		core->cursor_update_info.width = r * 2;
		if(core->cursor_update_info.start_x < 0)
		{
			core->cursor_update_info.width += core->cursor_update_info.start_x;
			core->cursor_update_info.start_x = 0;
		}
		if(core->cursor_update_info.width > canvas->disp_layer->width)
		{
			core->cursor_update_info.width = canvas->disp_layer->width;
		}
		else if(core->cursor_update_info.width <= 0)
		{
			core->cursor_update_info.update = FALSE;
		}
		
		core->cursor_update_info.start_y = y - r;
		core->cursor_update_info.height = r * 2;
		if(core->cursor_update_info.start_y < 0)
		{
			core->cursor_update_info.height += core->cursor_update_info.start_y;
			core->cursor_update_info.start_y = 0;
		}
		if(core->cursor_update_info.height > canvas->disp_layer->height)
		{
			core->cursor_update_info.height = canvas->disp_layer->height;
		}
		else if(core->cursor_update_info.height <= 0)
		{
			core->cursor_update_info.update = FALSE;
		}
	}

	//UpdateCanvasWidgetArea(canvas->widgets, (int)(x - r) - UPDATE_AREA_MARGIN, (int)(y - r) - UPDATE_AREA_MARGIN,
	//						(int)(r*2) + UPDATE_AREA_MARGIN, (int)(r*2) + UPDATE_AREA_MARGIN);
	ClearBeforeCursorPosition(canvas, canvas->before_cursor_x, canvas->before_cursor_y, x, y, core);

	canvas->cursor_x = (x - canvas->half_size) * canvas->cos_value
		- (y - canvas->half_size) * canvas->sin_value + canvas->add_cursor_x;
	canvas->cursor_y = (y - canvas->half_size) * canvas->sin_value
		+ (y - canvas->half_size) * canvas->cos_value + canvas->add_cursor_y;
		
	canvas->before_cursor_x = x;
	canvas->before_cursor_y = y;
	
#undef UPDATE_AREA_MARGIN
}

void DefaultBrushChangeZoom(FLOAT_T zoom, BRUSH_CORE* core)
{
	GRAPHICS_DEFAULT_CONTEXT context = {0};
	FLOAT_T radius = (core->radius * zoom) + CURSOR_LINE_WIDTH;
	int diameter = (int)(radius * 2 + 0.5) + CURSOR_LINE_WIDTH * 2;
	int stride = diameter * 4;
	size_t buffer_size = stride * diameter;
	if(core->brush_cursor_buffer == NULL || buffer_size > core->brush_cursor_buffer_size)
	{
		core->brush_cursor_buffer = (uint8*)MEM_REALLOC_FUNC(core->brush_cursor_buffer, buffer_size);
		if(core->brush_cursor_buffer == NULL)
		{
			return;
		}
		core->brush_cursor_buffer_size = buffer_size;
	}
	
	(void)memset(core->brush_cursor_buffer, 0, core->brush_cursor_buffer_size);
	InitializeGraphicsImageSurfaceForData(&core->brush_cursor_surface, core->brush_cursor_buffer,
								GRAPHICS_FORMAT_ARGB32, diameter, diameter, stride, &core->app->graphics);
	InitializeGraphicsDefaultContext(&context, &core->brush_cursor_surface, &core->app->graphics);
		
	GraphicsSetAntialias(&context.base, GRAPHICS_ANTIALIAS_GOOD);
	GraphicsSetLineWidth(&context.base, CURSOR_LINE_WIDTH);

	GraphicsSetSourceRGB(&context.base, 0, 0, 0);
	GraphicsArc(&context.base, radius + CURSOR_LINE_WIDTH, radius + CURSOR_LINE_WIDTH, radius, 0, 2 * M_PI);
	GraphicsStroke(&context.base);

	GraphicsSetSourceRGB(&context.base, 1, 1, 1);
	GraphicsArc(&context.base, radius + CURSOR_LINE_WIDTH, radius + CURSOR_LINE_WIDTH,
					radius + CURSOR_LINE_WIDTH, 0, 2 * M_PI);
	GraphicsStroke(&context.base);

	core->cursor_blend_mode = LAYER_BLEND_NORMAL;
}

void DefaultBrushChangeColor(const uint8* color, void* data)
{
	BRUSH_CORE *core= (BRUSH_CORE*)data;

	BrushCoreSetCirclePattern(core, core->radius, core->outline_hardness, core->blur, core->opacity, color);
}

typedef struct _BRUSH_HISTORY_DATA
{
	int32 x, y;
	int32 width, height;
	int32 name_len;
	char *layer_name;
	uint8 *pixels;
} BRUSH_HISTORY_DATA;

void BrushCoreUndoRedo(DRAW_WINDOW* canvas, void* p)
{
	BRUSH_HISTORY_DATA data;
	LAYER* layer = canvas->layer;
	uint8* buff = (uint8*)p;
	uint8* before_data;
	int i;

	(void)memcpy(&data, buff, offsetof(BRUSH_HISTORY_DATA, layer_name));
	buff += offsetof(BRUSH_HISTORY_DATA, layer_name);
	data.layer_name = (char*)buff;
	buff += data.name_len;
	data.pixels = buff;

	while(strcmp(layer->name, data.layer_name) != 0)
	{
		layer = layer->next;
	}

	before_data = (uint8*)MEM_ALLOC_FUNC(data.height*data.width*layer->channel);
	for(i=0; i<data.height; i++)
	{
		(void)memcpy(&before_data[i*data.width*layer->channel],
			&layer->pixels[(data.y+i)*layer->stride+data.x*layer->channel],
			data.width*layer->channel);
	}

	for(i=0; i<data.height; i++)
	{
		(void)memcpy(&layer->pixels[(data.y+i)*layer->stride+data.x*layer->channel],
			&data.pixels[i*data.width*layer->channel], data.width*layer->channel);
	}

	(void)memcpy(data.pixels, before_data, data.height*data.width*layer->channel);

	MEM_FREE_FUNC(before_data);
}

void AddBrushHistory(
	BRUSH_CORE* core,
	LAYER* active
)
{
	BRUSH_HISTORY_DATA data;
	MEMORY_STREAM_PTR stream;
	int i;

	data.x = (int32)core->min_x - 1;
	data.y = (int32)core->min_y - 1;
	data.width = (int32)(core->max_x + 1.5 - core->min_x);
	data.height = (int32)(core->max_y + 1.5 - core->min_y);
	if(data.x < 0)
	{
		data.x = 0;
	}
	else if(data.x > active->width)
	{
		return;
	}
	if(data.y < 0)
	{
		data.y = 0;
	}
	else if(data.y > active->height)
	{
		return;
	}
	if (data.width < 0)
	{
		return;
	}
	else if(data.x + data.width > active->width)
	{
		data.width = active->width - data.x;
	}
	if(data.height < 0)
	{
		return;
	}
	else if(data.y + data.height > active->height)
	{
		data.height = active->height - data.y;
	}
	data.name_len = (int32)strlen(active->name) + 1;

	stream = CreateMemoryStream(
		offsetof(BRUSH_HISTORY_DATA, layer_name)
		+ data.name_len+data.height*data.width*active->channel);
	(void)MemWrite(&data, offsetof(BRUSH_HISTORY_DATA, layer_name), 1, stream);
	(void)MemWrite(active->name, 1, data.name_len, stream);
	for(i=0; i<data.height; i++)
	{
		(void)MemWrite(&active->pixels[(data.y+i)*active->stride+data.x*active->channel],
			1, data.width * active->channel, stream);
	}
	AddHistory(
		&active->window->history,
		core->name,
		stream->buff_ptr,
		(uint32)stream->data_size,
		BrushCoreUndoRedo,
		BrushCoreUndoRedo
	);
	(void)DeleteMemoryStream(stream);

	{
		int index, set_bytes;;
		for(i=0, index = data.y * active->stride + data.x * 4, set_bytes = data.width * 4;
				i<data.height; i++, index += active->stride)
		{
			(void)memset(&active->window->anti_alias->pixels[index], 0, set_bytes);
		}
	}

	{
		DRAW_WINDOW *canvas;
		FLOAT_T x0, y0, x1, y1;
		FLOAT_T start_x, start_y;
		FLOAT_T width, height;

		canvas = GetActiveDrawWindow(core->app);
		x0 = ((core->min_x-canvas->width/2)*canvas->cos_value + (core->min_y-canvas->height/2)*canvas->sin_value) * canvas->zoom_rate
			+ canvas->rev_add_cursor_x;
		y0 = (- (core->min_x-canvas->width/2)*canvas->sin_value + (core->min_y-canvas->height/2)*canvas->cos_value) * canvas->zoom_rate
			+ canvas->rev_add_cursor_y;

		x1 = ((core->max_x-canvas->width/2)*canvas->cos_value + (core->max_y-canvas->height/2)*canvas->sin_value) * canvas->zoom_rate
			+ canvas->rev_add_cursor_x;
		y1 = (- (core->max_x-canvas->width/2)*canvas->sin_value + (core->max_y-canvas->height/2)*canvas->cos_value) * canvas->zoom_rate
			+ canvas->rev_add_cursor_y;

		if(x0 < x1)
		{
			start_x = x0;
			width = x1 - x0;
		}
		else
		{
			start_x = x1;
			width = x0 - x1;
		}

		if(y0 < y1)
		{
			start_y = y0;
			height = y1 - y0;
		}
		else
		{
			start_y = y1;
			height = y0 - y1;
		}

		UpdateCanvasWidgetArea(canvas->widgets, (int)start_x, (int)start_y, (int)width, (int)height);
	}

	active->flags |= LAYER_MODIFIED;
}

typedef struct _EDIT_SELECTION_DATA
{
	int32 x, y;
	int32 width, height;
	uint8 *pixels;
} EDIT_SELECTION_DATA;

void EditSelectionUndoRedo(DRAW_WINDOW* canvas, void* p)
{
	EDIT_SELECTION_DATA *data = (EDIT_SELECTION_DATA*)p;
	uint8 *buff = (uint8*)p;
	uint8 *pixels, *src, *before_data;
	int i;

	before_data = (uint8*)MEM_ALLOC_FUNC(data->width*data->height);
	src = &canvas->selection->pixels[data->y*canvas->selection->stride+data->x];
	pixels = before_data;
	for(i=0; i<data->height; i++)
	{
		(void)memcpy(pixels, src, data->width);
		pixels += data->width;
		src += canvas->selection->stride;
	}

	pixels = &canvas->selection->pixels[data->y*canvas->selection->stride+data->x];
	src = &buff[offsetof(EDIT_SELECTION_DATA, pixels)];
	for(i=0; i<data->height; i++)
	{
		(void)memcpy(pixels, src, data->width);
		pixels += canvas->selection->stride;
		src += data->width;
	}

	(void)memcpy(&buff[offsetof(EDIT_SELECTION_DATA, pixels)], before_data,
		data->width * data->height);

	MEM_FREE_FUNC(before_data);

	if((canvas->flags & DRAW_WINDOW_EDIT_SELECTION) == 0)
	{
		if(UpdateSelectionArea(&canvas->selection_area, canvas->selection, canvas->disp_temp) == FALSE)
		{
			canvas->flags &= ~(DRAW_WINDOW_HAS_SELECTION_AREA);
		}
		else
		{
			canvas->flags |= DRAW_WINDOW_HAS_SELECTION_AREA;
		}
	}
}

void AddSelectionEditHistory(BRUSH_CORE* core, LAYER* selection)
{
	EDIT_SELECTION_DATA data;
	uint8 *buff, *pixels, *src;
	int i;

	data.x = (int32)core->min_x - 1;
	data.y = (int32)core->min_y - 1;
	data.width = (int32)(core->max_x + 1 - core->min_x);
	data.height = (int32)(core->max_y + 1 - core->min_y);
	if(data.x < 0) data.x = 0;
	if(data.y < 0) data.y = 0;
	if(data.x + data.width > selection->width) data.width = selection->width - data.x;
	if(data.y + data.height > selection->height) data.height = selection->height - data.y;

	buff = (uint8*)MEM_ALLOC_FUNC(offsetof(EDIT_SELECTION_DATA, pixels) + data.width*data.height);

	(void)memcpy(buff, &data, offsetof(EDIT_SELECTION_DATA, pixels));

	pixels = &buff[offsetof(EDIT_SELECTION_DATA, pixels)];
	src = &selection->pixels[data.y*selection->stride+data.x];
	for(i=0; i<data.height; i++)
	{
		(void)memcpy(pixels, src, data.width);
		pixels += data.width;
		src += selection->stride;
	}

	AddHistory(
		&selection->window->history,
		core->name,
		buff,
		offsetof(EDIT_SELECTION_DATA, pixels) + data.width * data.height,
		EditSelectionUndoRedo,
		EditSelectionUndoRedo
	);

	MEM_FREE_FUNC(buff);
}

int ReadBrushData(
	APPLICATION* app,
	INI_FILE_PTR file,
	char* section_name
)
{
	BRUSH_CORE *core;
	char *brush_type;
	int x, y;

	x = IniFileGetInteger(file, section_name, "X");
	y = IniFileGetInteger(file, section_name, "Y");
	if(x < 0 || x >= BRUSH_TABLE_WIDTH || y < 0 || y >= BRUSH_TABLE_HEIGHT)
	{
		return -1;
	}

	brush_type = IniFileStrdup(file, section_name, "TYPE");
	if(brush_type == NULL)
	{
		return -1;
	}
	
	LoadBrushDetailData(&core, file, section_name, brush_type, app);
	app->tool_box.brushes[y][x] = core;

	MEM_FREE_FUNC(brush_type);
	
	return 0;
}

int InitializeBrushTable(APPLICATION* app, INI_FILE_PTR file)
{
	int i;
	
	for(i=0; i<file->section_count; i++)
	{
		ReadBrushData(app, file, file->section[i].section_name);
	}
}

int ReadBrushInitializeFile(APPLICATION* app, const char* file_path)
{
	INI_FILE_PTR file;
	FILE *fp = NULL;
	MEMORY_STREAM_PTR mem_stream;
	void *stream;
	size_t file_size;
	int initialized = FALSE;
	
	size_t (*read_func)(void*, size_t, size_t, void*);
	
	const char brushes_default_data[] =
#include "brush_resource.txt"
	
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
		mem_stream = CreateMemoryStream(sizeof(brushes_default_data));
		stream = (void*)mem_stream;
		read_func = (size_t (*)(void*, size_t, size_t, void*))MemRead;
		file_size = sizeof(brushes_default_data);
		if(mem_stream == NULL)
		{
			return -1;
		}
		(void)memcpy(mem_stream->buff_ptr, brushes_default_data, sizeof(brushes_default_data));
	}
	
	file = CreateIniFile(stream, read_func, file_size, INI_READ);
	InitializeBrushTable(app, file);
	
	if(fp != NULL)
	{
		(void)fclose(fp);
	}
	else
	{
		DeleteMemoryStream(mem_stream);
	}
	return 0;
}

#ifdef __cplusplus
}
#endif
