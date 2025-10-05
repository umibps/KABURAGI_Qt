#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "draw_window.h"
#include "application.h"
#include "graphics/graphics_matrix.h"
#include "gui/gui.h"
#include "gui/draw_window.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

DRAW_WINDOW* CreateDrawWindow(
	int32 width,
	int32 height,
	uint8 channel,
	const char* name,
	void* tab,
	uint16 window_id,
	APPLICATION* app
)
{
	DRAW_WINDOW *ret = MEM_CALLOC_FUNC(1, sizeof(*ret));
	char layer_name[MAX_LAYER_NAME_LENGTH];
	int i;

	ret->original_width = width;
	ret->original_height = height;

	width += (4 - (width % 4)) % 4;
	height += (4 - (height % 4)) % 4;

	// 値のセット
	ret->channel = channel;
	ret->file_name = (name == NULL) ? NULL : MEM_STRDUP_FUNC(name);
	ret->width = width;
	ret->height = height;
	ret->stride = width * channel;
	ret->pixel_buf_size = ret->stride * height;
	ret->zoom = 100;
	ret->zoom_rate = 1;
	ret->rev_zoom = 1;
	ret->sin_value = 0;
	ret->cos_value = 1;
	ret->layer_blend_functions = app->layer_blend_functions;
	ret->app = app;
	ret->cursor_x = ret->cursor_y = -50000;

	// 背景のピクセルメモリを確保
	ret->back_ground = (uint8*)MEM_CALLOC_FUNC(ret->pixel_buf_size, sizeof(uint8));
	(void)memset(ret->back_ground, 0xFF, sizeof(*ret->back_ground)*ret->pixel_buf_size);
	// ブラシ用のバッファを確保
	ret->brush_buffer = (uint8*)MEM_CALLOC_FUNC(ret->pixel_buf_size, sizeof(uint8));
	// 不透明保護用のバッファを確保
	ret->alpha_lock = (uint8*)MEM_CALLOC_FUNC(ret->width * ret->height, sizeof(uint8));

	// レイヤーの名前をセット
	(void)sprintf(layer_name, "%s 1", app->labels->layer_window.new_layer);
	// 最初のレイヤーを作成する
	ret->active_layer = ret->layer = CreateLayer(0, 0, width, height,
		channel, TYPE_NORMAL_LAYER, NULL, NULL, layer_name, ret);
	ret->num_layer++;

	// 作業用のレイヤーを作成
	ret->work_layer = CreateLayer(0, 0, width, height, 4, TYPE_NORMAL_LAYER,
		NULL, NULL, NULL, ret);

	// アクティブレイヤーと作業用レイヤーを一時的に合成するレイヤー
	ret->temp_layer = CreateLayer(0, 0, width, height, 5, TYPE_NORMAL_LAYER,
		NULL, NULL, NULL, ret);

	// レイヤーを合成したもの
	ret->mixed_layer = CreateLayer(0, 0, width, height, 4, TYPE_NORMAL_LAYER,
		NULL, NULL, NULL, ret);
	InitializeGraphicsPatternForSurface(&ret->mixed_pattern, &ret->mixed_layer->surface.base);
	GraphicsPatternSetFilter(&ret->mixed_pattern.base, GRAPHICS_FILTER_FAST);

	// 描画用のレイヤーを作成
	ret->disp_layer = CreateLayer(0, 0, width, height, channel,
	   TYPE_NORMAL_LAYER, NULL, NULL, NULL, ret);
	(void)memset(ret->disp_layer->pixels, 0xff,
					sizeof(*ret->disp_layer->pixels)*ret->pixel_buf_size);
	ret->scaled_mixed = CreateLayer(0, 0, width, height, channel,
		TYPE_NORMAL_LAYER, NULL, NULL, NULL, ret);
	ret->anti_alias = CreateLayer(0, 0, width, height, channel,
		TYPE_NORMAL_LAYER, NULL, NULL, NULL, ret);

	// 選択領域のレイヤーを作成
	ret->selection = CreateLayer(0, 0, width, height, 1, TYPE_NORMAL_LAYER,
		NULL, NULL, NULL, ret);

	// 一番上でエフェクト表示を行うレイヤーを作成
	ret->effect = CreateLayer(0, 0, width, height, 4, TYPE_NORMAL_LAYER,
		NULL, NULL, NULL, ret);

	// テクスチャ用のレイヤーを作成
	ret->texture = CreateLayer(0, 0, width, height, 1, TYPE_NORMAL_LAYER,
		NULL, NULL, NULL, ret);

	// 下のレイヤーでマスキング、バケツツールでのマスキング用
	ret->mask = CreateLayer(0, 0, width, height, 4, TYPE_NORMAL_LAYER,
		NULL, NULL, NULL, ret);
	ret->mask_temp = CreateLayer(0, 0, width, height, 4, TYPE_NORMAL_LAYER,
		NULL, NULL, NULL, ret);
	InitializeGraphicsImageSurfaceForData(&ret->alpha_surface, ret->mask->pixels,
									GRAPHICS_FORMAT_A8, width, height, width, &app->graphics);
	InitializeGraphicsDefaultContext(&ret->alpha_context, &ret->alpha_surface, &app->graphics);
	InitializeGraphicsImageSurfaceForData(&ret->alpha_temp, ret->temp_layer->pixels,
									GRAPHICS_FORMAT_A8, width, height, width, &app->graphics);
	InitializeGraphicsDefaultContext(&ret->alpha_temp_context, &ret->alpha_temp, &app->graphics);
	InitializeGraphicsImageSurfaceForData(&ret->gray_mask_temp, &ret->mask_temp->pixels,
									GRAPHICS_FORMAT_A8, width, height, width, &app->graphics);
	InitializeGraphicsDefaultContext(&ret->gray_mask_context, &ret->gray_mask_temp, &app->graphics);
		
	// 表示用に拡大・縮小した後の一次記憶メモリ
	ret->disp_temp = CreateDispTempLayer(0, 0, width, height, 4, TYPE_NORMAL_LAYER, ret);

	// 表示高速化用にアクティブなレイヤーより下の合成結果を保存する為
	ret->under_active = CreateLayer(0, 0, width, height, 4, TYPE_NORMAL_LAYER, NULL, NULL, NULL, ret);

	// レイヤー合成のフラグを立てる
	ret->flags = DRAW_WINDOW_UPDATE_ACTIVE_UNDER;

	// レイヤー合成の関数ポインタ設定
	ret->layer_blend_functions = app->layer_blend_functions;
	ret->part_layer_blend_functions = app->part_layer_blend_functions;
	ret->blend_selection_functions = app->blend_selection_functions;
	
	ResizeCanvasDispTempLayer(ret, width, height);

	return ret;
}

INLINE LAYER* SearchLayeByCursorColorSkipLayerSet(LAYER* layer_set)
{
	LAYER *next;
	LAYER *target;

	next = layer_set;
	target = next->layer_set;
	while(next->layer_set != NULL)
	{
		if(next->layer_set != target)
		{
			next = SearchLayeByCursorColorSkipLayerSet(next);
		}

		if(next == NULL)
		{
			return NULL;
		}
		next = next->prev;
		if(next == NULL)
		{
			return NULL;
		}
	}

	return next;
}

/*
* SearchLayerByCursorColor関数
* カーソル位置にある色でレイヤーを探す
* 引数
* canvdas	: キャンバス
* x			: カーソルのX座標
* y			: カーソルのY座標
* 返り値
*	該当するレイヤー。見つからなければNULL
*/
LAYER* SearchLayerByCursorColor(DRAW_WINDOW* canvas, int x, int y)
{
	LAYER *top;
	LAYER *previous;
	int skip_check;

	if(x < 0 || y < 0 || x >= canvas->width || y >= canvas->height)
	{
		return NULL;
	}

	top = canvas->layer;
	while(top->next != NULL)
	{
		top = top->next;
	}

	previous = top;
	do
	{
		skip_check = TRUE;
		if(previous->layer_type == TYPE_ADJUSTMENT_LAYER)
		{
			previous = previous->prev;
		}
		else if(previous->layer_type == TYPE_LAYER_SET)
		{
			if((previous->flags & LAYER_FLAG_INVISIBLE) != 0)
			{
				previous = SearchLayeByCursorColorSkipLayerSet(previous);
			}
			else
			{
				previous = previous->prev;
			}
		}
		if((previous->flags & LAYER_FLAG_INVISIBLE) == 0)
		{
			if(x >= previous->x && x < previous->x + previous->width
				&& y >= previous->y && y < previous->y + previous->height)
			{
				LAYER *mask = NULL;
				if((previous->flags & LAYER_MASKING_WITH_UNDER_LAYER) != FALSE)
				{
					LAYER *mask_target = NULL;
					LAYER *check = previous->prev;
					while(check != NULL)
					{
						if((check->flags & LAYER_MASKING_WITH_UNDER_LAYER) == FALSE)
						{
							mask = check;
							break;
						}
						check = check->prev;
					}
					if(check == NULL)
					{
						return NULL;
					}
				}

				if(mask == NULL)
				{
					if(previous->pixels[(y - previous->y) * previous->stride
						+ (x - previous->x) * 4 + 3] > 0)
					{
						return previous;
					}
				}
				else
				{
					if(mask->pixels[(y - mask->y) * mask->stride
						+ (x - mask->x) * 4 + 3] > 0)
					{
						return previous;
					}
				}
			}
			previous = previous->prev;
		}
		else
		{
			previous = previous->prev;
		}
	} while(previous != NULL);

	return NULL;
}

/*
* ExecuteMotionQueue関数
* 待ち行列に溜まったマウスの処理を行う
*  一定時間を超えたら処理を中断する
* 引数
* canvas	: 対応する描画領域
*/
void ExecuteMotionQueue(DRAW_WINDOW* canvas)
{
// 処理を中断する時間 (ミリ秒)
#define EXECUTE_MILLI_SECONDS 12
	APPLICATION *app = canvas->app;
	MOTION_QUEUE motion_queue = app->tool_box.motion_queue;
	brush_update_function update_function = (brush_update_function)DefaultToolUpdate;
	void *update_data = NULL;
	FLOAT_T x, y;
	FLOAT_T x0, y0;
	FLOAT_T display_before_x, display_before_y;
	void *timer;
	int executed_points = 0;
	int index;

	if(motion_queue.num_items == 0)
	{
		motion_queue.start_index = 0;
		return;
	}

	display_before_x = canvas->before_cursor_x;
	display_before_y = canvas->before_cursor_y;

	x = motion_queue.queue[motion_queue.start_index].state.cursor_x, y = motion_queue.queue[motion_queue.start_index].state.cursor_y;

	// 画面更新用のデータを取得
	if(canvas->transform == NULL)
	{
		if((app->tool_box.flags & TOOL_USING_BRUSH) != 0)
		{
			if(canvas->active_layer->layer_type == TYPE_NORMAL_LAYER)
			{
				update_function = app->tool_box.active_brush[app->input]->motion_update;
				update_data = (void*)app->tool_box.active_brush[app->input];
			}
			else if(canvas->active_layer->layer_type == TYPE_VECTOR_LAYER)
			{
				update_function = app->tool_box.active_vector_brush[app->input]->motion_update;
				update_data = (void*)app->tool_box.active_vector_brush[app->input];
			}
		}
		else
		{
			update_function = app->tool_box.active_common_tool->motion_update;
			update_data = (void*)app->tool_box.active_common_tool;
		}
	}
	
	timer = TimerInMotionQueueExecutionNew(EXECUTE_MILLI_SECONDS);

	if((app->tool_box.flags & TOOL_USING_BRUSH) == 0)
	{
		do
		{
			index = (motion_queue.start_index + executed_points) % motion_queue.max_items;
			x = motion_queue.queue[index].state.cursor_x, y = motion_queue.queue[index].state.cursor_y;

			app->tool_box.active_common_tool->motion_function(
				canvas, app->tool_box.active_common_tool,
					(void*)(&motion_queue.queue[index].state));

			x0 = ((x-canvas->width/2)*canvas->cos_value + (y-canvas->height/2)*canvas->sin_value) * canvas->zoom_rate
				+ canvas->rev_add_cursor_x;
			y0 = (- (x-canvas->width/2)*canvas->sin_value + (y-canvas->height/2)*canvas->cos_value) * canvas->zoom_rate
				+ canvas->rev_add_cursor_y;

			update_function(canvas, x, y, update_data);

			canvas->before_cursor_x = x0;
			canvas->before_cursor_y = y0;
			executed_points++;
		} while(TimerElapsedTime(timer) <= EXECUTE_MILLI_SECONDS
			&& executed_points < motion_queue.num_items);
	}
	else
	{
		if(canvas->active_layer->layer_type == TYPE_NORMAL_LAYER
				|| (canvas->flags & DRAW_WINDOW_EDIT_SELECTION) != 0)
		{
			canvas->flags |= DRAW_WINDOW_UPDATE_PART | DRAW_WINDOW_UPDATE_ACTIVE_UNDER;
			do
			{
				index = (motion_queue.start_index + executed_points) % motion_queue.max_items;
				x = motion_queue.queue[index].state.cursor_x, y = motion_queue.queue[index].state.cursor_y;

				app->tool_box.active_brush[app->input]->motion_function(
					canvas,	app->tool_box.active_brush[app->input], (void*)(&motion_queue.queue[index].state)
				);

				x0 = ((x-canvas->width/2)*canvas->cos_value + (y-canvas->height/2)*canvas->sin_value) * canvas->zoom_rate
					+ canvas->rev_add_cursor_x;
				y0 = (- (x-canvas->width/2)*canvas->sin_value + (y-canvas->height/2)*canvas->cos_value) * canvas->zoom_rate
					+ canvas->rev_add_cursor_y;

				update_function(canvas, x0, y0, update_data);


				canvas->before_cursor_x = x0;
				canvas->before_cursor_y = y0;
				executed_points++;
			} while(TimerElapsedTime(timer) <= EXECUTE_MILLI_SECONDS
				&& executed_points < motion_queue.num_items);
		}
		else if(canvas->active_layer->layer_type == TYPE_VECTOR_LAYER)
		{
			canvas->flags |= DRAW_WINDOW_UPDATE_PART | DRAW_WINDOW_UPDATE_ACTIVE_UNDER;
			do
			{
				index = (motion_queue.start_index + executed_points) % motion_queue.max_items;
				x = motion_queue.queue[index].state.cursor_x, y = motion_queue.queue[index].state.cursor_y;

				if((motion_queue.queue[index].state.mouse_key_flag & MOUSE_KEY_FLAG_SHIFT) != 0)
				{
					app->tool_box.vector_control.mode = CONTROL_POINT_DELETE;
					app->tool_box.vector_control_core.motion_func(
						canvas, &app->tool_box.vector_control_core, (void*)(&motion_queue.queue[index].state)
					);
				}
				else if((motion_queue.queue[index].state.mouse_key_flag & MOUSE_KEY_FLAG_CONTROL) != 0 ||
					(app->tool_box.vector_control.flags & CONTROL_POINT_TOOL_HAS_POINT) != 0)
				{
					app->tool_box.vector_control.mode = CONTROL_POINT_MOVE;
					app->tool_box.vector_control_core.motion_func(
						canvas, &app->tool_box.vector_control_core, (void*)(&motion_queue.queue[index].state)
					);
				}
				else
				{
					app->tool_box.active_vector_brush[app->input]->motion_func(
						canvas, app->tool_box.active_vector_brush[app->input], (void*)(&motion_queue.queue[index].state)
					);
				}

				x0 = ((x-canvas->width/2)*canvas->cos_value + (y-canvas->height/2)*canvas->sin_value) * canvas->zoom_rate
					+ canvas->rev_add_cursor_x;
				y0 = (- (x-canvas->width/2)*canvas->sin_value + (y-canvas->height/2)*canvas->cos_value) * canvas->zoom_rate
					+ canvas->rev_add_cursor_y;

				update_function(canvas, x, y, update_data);

				canvas->before_cursor_x = x0;
				canvas->before_cursor_y = y0;
				executed_points++;
			} while(TimerElapsedTime(timer) <= EXECUTE_MILLI_SECONDS
				&& executed_points < motion_queue.num_items);

			if((canvas->active_layer->layer_data.vector_layer->flags &
				(VECTOR_LAYER_RASTERIZE_ALL | VECTOR_LAYER_RASTERIZE_TOP | VECTOR_LAYER_RASTERIZE_ACTIVE)) != 0)
			{
				canvas->update.width = canvas->width,	canvas->update.height = canvas->height;
			}
		}
		else
		{
			do
			{
				index = (motion_queue.start_index + executed_points) % motion_queue.max_items;
				x = motion_queue.queue[index].state.cursor_x, y = motion_queue.queue[index].state.cursor_y;

				TextLayerMotionCallBack(canvas, &motion_queue.queue[index].state);

				x0 = ((x-canvas->width/2)*canvas->cos_value + (y-canvas->height/2)*canvas->sin_value) * canvas->zoom_rate
					+ canvas->rev_add_cursor_x;
				y0 = (- (x-canvas->width/2)*canvas->sin_value + (y-canvas->height/2)*canvas->cos_value) * canvas->zoom_rate
					+ canvas->rev_add_cursor_y;

				update_function(canvas, x, y, update_data);

				canvas->before_cursor_x = x0;
				canvas->before_cursor_y = y0;
				executed_points++;
			} while(TimerElapsedTime(timer) <= EXECUTE_MILLI_SECONDS
				&& executed_points < motion_queue.num_items);
		}
	}

	app->tool_box.motion_queue.num_items -= executed_points;
	if(app->tool_box.motion_queue.num_items == 0)
	{
		app->tool_box.motion_queue.start_index = 0;
	}
	else
	{
		app->tool_box.motion_queue.start_index += executed_points;
	}

	//canvas->before_cursor_x = display_before_x;
	//canvas->before_cursor_y = display_before_y;

	DeleteTimer(timer);
}

/*
* ResizeCanvasDispTempLayer関数
* 表示用の一時保存レイヤーの幅、高さを変更
* canvas		: サイズを変更するキャンバス
* new_width		: キャンバスの新しい幅
* new_height	: キャンバスの新しい高さ
*/
void ResizeCanvasDispTempLayer(
	DRAW_WINDOW* canvas,
	int32 new_width,
	int32 new_height
)
{
	APPLICATION *app = canvas->app;
	GRAPHICS_MATRIX matrix;
	const GRAPHICS_DEFAULT_CONTEXT zero_context = {0};
	const GRAPHICS_IMAGE_SURFACE zero_surface = {0};

	canvas->disp_size = (int)(2 * sqrt((new_width/2)*(new_width/2)
								+ (new_height/2)*(new_height/2)) + 1);
	canvas->disp_stride = canvas->disp_size * 4;
	canvas->half_size = canvas->disp_size * 0.5;
	canvas->trans_x = - canvas->half_size + ((new_width / 2) * canvas->cos_value
						+ (new_height / 2) * canvas->sin_value);
	canvas->trans_y = - canvas->half_size - ((new_width / 2) * canvas->sin_value
						- (new_height / 2) * canvas->cos_value);

	canvas->add_cursor_x = - (canvas->half_size - canvas->disp_layer->width / 2) + canvas->half_size;
	canvas->add_cursor_y = - (canvas->half_size - canvas->disp_layer->height / 2) + canvas->half_size;
	canvas->rev_add_cursor_x = canvas->disp_layer->width / 2 + (canvas->half_size - canvas->disp_layer->width / 2);
	canvas->rev_add_cursor_y = canvas->disp_layer->height / 2 + (canvas->half_size - canvas->disp_layer->height / 2);

	ReleaseLayerContext(canvas->disp_temp);

	GraphicsDefaultContextFinish(&canvas->disp_temp->context);
	canvas->disp_temp->context = zero_context;
	GraphicsSurfaceFinish(&canvas->disp_temp->surface.base);
	canvas->disp_temp->surface = zero_surface;

	canvas->disp_temp->width = new_width;
	canvas->disp_temp->height = new_height;
	canvas->disp_temp->stride = new_width * 4;

	canvas->disp_temp->pixels = (uint8*)MEM_REALLOC_FUNC(canvas->disp_temp->pixels,
				canvas->disp_stride * canvas->disp_size);
	InitializeGraphicsImageSurfaceForData(&canvas->disp_temp->surface, canvas->disp_temp->pixels,
				GRAPHICS_FORMAT_ARGB32, new_width, new_height, canvas->disp_temp->stride, &app->graphics);
	InitializeGraphicsDefaultContext(&canvas->disp_temp->context, &canvas->disp_temp->surface, &app->graphics);

	InitializeGraphicsMatrixRotate(&matrix, canvas->angle);
	GraphicsMatrixTranslate(&matrix, canvas->trans_x, canvas->trans_y);

	InitializeLayerContext(canvas->disp_temp);

	ReleaseCanvasContext(canvas);
	InitializeCanvasContext(canvas);
}

/*
* DrawWindowChangeZoom関数
* キャンバスの表示拡大縮小率を変更する
* 引数
* canvas	: 拡大縮小率を変更するキャンバス
* zoom		: 変更後の拡大縮小率
*/
void DrawWindowChangeZoom(DRAW_WINDOW* canvas, int16 zoom)
{
	// レイヤー合成時の表示用拡大縮小設定を更新
	GRAPHICS_MATRIX matrix;
	InitializeGraphicsMatrixScale(&matrix, 1/(zoom*0.01), 1/(zoom*0.01));
	GraphicsPatternSetMatrix(&canvas->mixed_pattern.base, &matrix);

	// キャンバスの拡大縮小率設定を更新
	canvas->zoom = zoom;
	canvas->zoom_rate = zoom * 0.01;
	canvas->rev_zoom = 1 / canvas->zoom_rate;

	// 表示に使うレイヤーの幅、高さ情報を更新
	ResizeLayerBuffer(canvas->disp_layer,
		(int32)(canvas->width*canvas->zoom_rate), (int32)(canvas->height*canvas->zoom_rate));
	ResizeLayerBuffer(canvas->scaled_mixed,
		(int32)(canvas->width * canvas->zoom_rate), (int32)(canvas->height * canvas->zoom_rate));
	ResizeLayerBuffer(canvas->effect,
		(int32)(canvas->width * canvas->zoom_rate), (int32)(canvas->height * canvas->zoom_rate));
	ResizeCanvasDispTempLayer(canvas,
		(int32)(canvas->width*canvas->zoom_rate), (int32)(canvas->height*canvas->zoom_rate));

	ResizeDrawWindowWidgets(canvas);

	canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_UNDER;

	if(canvas->flags & DRAW_WINDOW_HAS_SELECTION_AREA)
	{
		UpdateSelectionArea(&canvas->selection_area, canvas->selection, canvas->disp_temp);
	}

	UpdateCanvasWidget(canvas->widgets);
}

/*
* DrawWindowChangeRotate関数
* キャンバスの表示回転角を変更する
* 引数
* canvas	: 回転角を変更するキャンバス
* angle		: 新しい表示回転角(°)
*/
void DrawWindowChangeRotate(DRAW_WINDOW* canvas, int angle)
{
	GRAPHICS_MATRIX matrix;
	FLOAT_T radian;

	radian = canvas->angle = - (FLOAT_T)angle * M_PI / 180.0;

	// キャンバスの回転角記録メモリを更新
	canvas->sin_value = sin(radian);
	canvas->cos_value = cos(radian);

	canvas->trans_x = -canvas->half_size + ((canvas->disp_layer->width / 2) * canvas->cos_value + (canvas->disp_layer->height / 2) * canvas->sin_value);
	canvas->trans_y = -canvas->half_size - ((canvas->disp_layer->width / 2) * canvas->sin_value - (canvas->disp_layer->height / 2) * canvas->cos_value);

	canvas->add_cursor_x = -(canvas->half_size - canvas->disp_layer->width / 2) + canvas->half_size;
	canvas->add_cursor_y = -(canvas->half_size - canvas->disp_layer->height / 2) + canvas->half_size;

	InitializeGraphicsMatrixRotate(&matrix, radian);
	GraphicsMatrixTranslate(&matrix, canvas->trans_x, canvas->trans_y);
	GraphicsPatternSetMatrix(&canvas->mixed_pattern.base, &matrix);
}

#ifdef __cplusplus
}
#endif
