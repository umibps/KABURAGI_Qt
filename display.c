#include <string.h>
#include "draw_window.h"
#include "application.h"
#include "gui/layer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _eUPDATE_MODE
{
	NO_UPDATE,
	UPDATE_ALL,
	UPDATE_PART
} eUPDATE_MODE;

eDRAW_WINDOW_DIPSLAY_UPDATE_RESULT LayerBlendForDisplay(DRAW_WINDOW* canvas)
{
	APPLICATION *app = canvas->app;
	eDRAW_WINDOW_DIPSLAY_UPDATE_RESULT result
				= DRAW_WINDOW_DIPSLAY_UPDATE_RESULT_NO_UPDATE;
	GRAPHICS_SURFACE *update_surface;
	UPDATE_RECTANGLE part_update = {0};
	LAYER *layer = NULL, *blend_layer = NULL;
	int y;
	eUPDATE_MODE update_mode = UPDATE_ALL;
	int update_active_under = 0;
	int blend_mode;
	int clear_x, clear_y, clear_width, clear_height;
	int clear_part = FALSE;

	canvas->flags |= DRAW_WINDOW_FIRST_DRAW;
	update_surface = &canvas->mixed_layer->surface.base;

	if((canvas->flags & DRAW_WINDOW_EDIT_SELECTION) == 0)
	{
		if((canvas->flags & DRAW_WINDOW_UPDATE_PART) == 0)
		{
			if((canvas->flags & DRAW_WINDOW_UPDATE_ACTIVE_UNDER) != 0)
			{   // 全レイヤー合成
				(void)memcpy(canvas->mixed_layer->pixels, canvas->back_ground, canvas->pixel_buf_size);
				layer = canvas->layer;
				result = DRAW_WINDOW_DIPSLAY_UPDATE_RESULT_ALL;
			}
			else if((canvas->flags & DRAW_WINDOW_UPDATE_ACTIVE_OVER) != 0)
			{
				if(canvas->active_layer == canvas->layer)
				{   // アクティブレイヤーとその上のレイヤーを合成
						// 合成を開始はアクティブレイヤー
					if(canvas->active_layer == canvas->layer)
					{
						(void)memcpy(canvas->mixed_layer->pixels, canvas->back_ground, canvas->pixel_buf_size);
					}
					else
					{
						(void)memcpy(canvas->mixed_layer->pixels, canvas->under_active->pixels, canvas->pixel_buf_size);
					}
					layer = canvas->active_layer;
				}
				result = DRAW_WINDOW_DIPSLAY_UPDATE_RESULT_ALL;
			}
			else
			{   // 更新フラグ無し⇒レイヤー合成しない
				update_mode = NO_UPDATE;
			}
		}
		else
		{
			int stride;
			int copy_x, end_y;

			copy_x = (int)canvas->update.x;
			if(canvas->update.x < 0)
			{
				canvas->update.x = 0;
				copy_x = 0;
			}

			if(copy_x < canvas->width)
			{
				stride = ((copy_x + (int)canvas->update.width + 1) * 4) > canvas->stride ?
							(canvas->width - copy_x) * 4 : ((int)canvas->update.width + 1) * 4;
			}
			else
			{
				stride = 0;
				goto NO_UPDATE_BLEND;
			}

			if(stride > 0 && canvas->update.height > 0)
			{
				if(canvas->update.y < 0)
				{
					canvas->update.y = 0;
				}

				if(canvas->update.x + canvas->update.width > canvas->width)
				{
					canvas->update.width = canvas->width - canvas->update.x;
				}

				if(canvas->update.y + canvas->update.height <= canvas->height)
				{
					end_y = (int)canvas->update.height;
				}
				else
				{
					end_y = (int)(canvas->update.height = canvas->height - canvas->update.y);
				}

				if(end_y >= 0)
				{
					if(canvas->active_layer == canvas->layer)
					{	// アクティブレイヤーが一番下ならば背景のピクセルデータをコピー
						for(y=0; y<end_y; y++)
						{
							(void)memcpy(&canvas->mixed_layer->pixels[(y+(int)canvas->update.y)*canvas->mixed_layer->stride+copy_x*4],
												&canvas->back_ground[(y+(int)canvas->update.y)*canvas->mixed_layer->stride+copy_x*4], stride);
						}
					}
					else
					{	// そうでなければアクティブレイヤーより下の合成済みのデータをコピー
						for(y=0; y<end_y; y++)
						{
							(void)memcpy(&canvas->mixed_layer->pixels[(y+(int)canvas->update.y)*canvas->mixed_layer->stride+copy_x*4],
												&canvas->under_active->pixels[(y+(int)canvas->update.y)*canvas->mixed_layer->stride+copy_x*4], stride);
						}
					}
					layer = canvas->active_layer;
				}
				clear_part = TRUE;
				clear_x = (int)canvas->update.x;
				clear_y = (int)canvas->update.y;
				clear_width = (int)canvas->update.width;
				clear_height = (int)canvas->update.height;
			}
			else
			{
				canvas->flags &= ~(DRAW_WINDOW_UPDATE_PART);
				result = DRAW_WINDOW_DIPSLAY_UPDATE_RESULT_ALL;
				goto execute_update;
			}
			InitializeGraphicsImageSurfaceForRectangle(&part_update.surface, &canvas->mixed_layer->surface,
													   canvas->update.x, canvas->update.y, canvas->update.width, canvas->update.height);
			InitializeGraphicsDefaultContext(&part_update.context, &part_update.surface.base, &canvas->app->graphics);
			part_update.x = canvas->update.x,   part_update.y = canvas->update.y;
			part_update.width = canvas->update.width,   part_update.height = canvas->update.height;
			// canvas->temp_update = part_update;
			InitializeGraphicsImageSurfaceForRectangle(&canvas->temp_update.surface, &canvas->temp_layer->surface,
													   canvas->update.x, canvas->update.y, canvas->update.width, canvas->update.height);
			InitializeGraphicsDefaultContext(&canvas->temp_update.context, &canvas->temp_update.surface, &canvas->app->graphics);
			canvas->temp_update.x = canvas->update.x,   canvas->temp_update.y = canvas->update.y;
			canvas->temp_update.width = canvas->update.width,   canvas->temp_update.height = canvas->update.height;

			update_mode = UPDATE_PART;
			result = DRAW_WINDOW_DIPSLAY_UPDATE_RESULT_PART;
		}
	}
	else
	{
		if(canvas->update.need_update)
		{
			update_mode = UPDATE_ALL;
		}
		else
		{
			canvas->flags &= ~(DRAW_WINDOW_UPDATE_PART);
			update_mode = NO_UPDATE;
			goto NO_UPDATE_BLEND;
		}
	}

	if(update_mode == UPDATE_ALL)
	{
		// 一番上のレイヤーに辿り着くまでループ
		while(layer != NULL)
		{   // レイヤーセット内のレイヤーであれば
			if(layer->layer_set != NULL)
			{	// 全更新ならば
				if((canvas->flags & DRAW_WINDOW_UPDATE_ACTIVE_UNDER) != 0)
				{	// レイヤーセット内を更新
					MixLayerSet(layer, &layer, canvas);
				}	// 全更新ならば
						// if((window->flags & DRAW_WINDOW_UPDATE_ACTIVE_UNDER) != 0)
				else if(layer->layer_set == canvas->active_layer_set)
				{
					MixLayerSetActiveOver(layer, &layer, canvas);
				}	// else if(layer->layer_set == window->active_layer_set)
				else
				{
					layer = layer->layer_set;
				}
			}	// if(layer->layer_set != NULL)

			// 合成レイヤーと合成方法を一度記憶して
			blend_layer = layer;
			blend_mode = layer->layer_mode;

			// 非表示レイヤーになっていないことを確認
			if((blend_layer->flags & LAYER_FLAG_INVISIBLE) == 0)
			{	// もし、合成するレイヤーがアクティブレイヤーなら
				if(layer == canvas->active_layer)
				{
					if(layer->layer_type == TYPE_NORMAL_LAYER)
					{	// 通常レイヤーは
							// 作業レイヤーとアクティブレイヤーを一度合成してから下のレイヤーと合成
						(void)memcpy(canvas->temp_layer->pixels, layer->pixels, layer->stride*layer->height);
						canvas->layer_blend_functions[canvas->work_layer->layer_mode](canvas->work_layer, canvas->temp_layer);
						blend_layer = canvas->temp_layer;
						blend_layer->alpha = layer->alpha;
						blend_layer->flags = layer->flags;
						blend_layer->prev = layer->prev;
					}
					else if(layer->layer_type == TYPE_VECTOR_LAYER)
					{	// ベクトルレイヤーは
							// レイヤーのラスタライズ処理を行なってから作業レイヤーと下のレイヤーを合成
						RasterizeVectorLayer(canvas, layer, layer->layer_data.vector_layer);
						if(canvas->work_layer->layer_mode != LAYER_BLEND_NORMAL)
						{
							canvas->layer_blend_functions[canvas->work_layer->layer_mode](canvas->work_layer, layer);
						}
					}
					else if(layer->layer_type == TYPE_TEXT_LAYER)
					{	// テキストレイヤーは
							// テキストの内容をラスタライズ処理してから下のレイヤーと合成
						RenderTextLayer(canvas, layer, layer->layer_data.text_layer);
					}

					while(layer->next != NULL && layer->next->layer_type == TYPE_ADJUSTMENT_LAYER)
					{
						if((layer->next->flags & LAYER_FLAG_INVISIBLE) != 0)
						{
							layer->next->layer_data.adjustment_layer->filter_func(
								layer->next->layer_data.adjustment_layer, layer->pixels, layer->next->pixels,
									layer->width * layer->height, layer);
						}
						layer->next->layer_data.adjustment_layer->update(
							layer->next->layer_data.adjustment_layer, layer, canvas->mixed_layer,
								0, 0, layer->width, layer->height);
						blend_layer = layer->next;
						layer = layer->next;
					}

					// サムネイル更新
					if(layer->widget != NULL)
					{
						UpdateLayerThumbnail(layer);
					}
				}

				// 合成する対象と方法が確定したので合成を実行する
				canvas->layer_blend_functions[blend_mode](blend_layer, canvas->mixed_layer);
				// 合成したらデータを元に戻す
				canvas->temp_layer->alpha = 100;
				canvas->temp_layer->flags = 0;
				canvas->temp_layer->prev = NULL;
				GraphicsSetOperator(&canvas->temp_layer->context.base, GRAPHICS_OPERATOR_OVER);
			}	// 非表示レイヤーになっていないことを確認
					// if((blend_layer->flags & LAYER_FLAG_INVISIBLE) == 0)

			// 次のレイヤーへ
			layer = layer->next;

			// 次に合成するレイヤーがアクティブレイヤーなら
			if(layer == canvas->active_layer)
			{	// アクティブレイヤーより下のレイヤーの合成データを更新
				(void)memcpy(canvas->under_active->pixels, canvas->mixed_layer->pixels, canvas->pixel_buf_size);
			}
		}	// 一番上のレイヤーに辿り着くまでループ
				// while(layer != NULL)
	}
	else
	{   // 一番上のレイヤーに辿り着くまでループ
		while(layer != NULL)
		{
			// レイヤーセット内のレイヤーであれば
			if(layer->layer_set != NULL)
			{	// 全更新ならば
				if(layer->layer_set == canvas->active_layer_set)
				{
					MixLayerSetActiveOver(layer, &layer, canvas);
				}	// else if(layer->layer_set == window->active_layer_set)
				else
				{
					layer = layer->layer_set;
				}
			}	// if(layer->layer_set != NULL)

			// 合成レイヤーと合成方法を一度記憶して
			blend_layer = layer;
			blend_mode = layer->layer_mode;
{
static int count=0;
count++;}
			// 非表示レイヤーになっていないことを確認
			if((blend_layer->flags & LAYER_FLAG_INVISIBLE) == 0)
			{	// もし、合成するレイヤーがアクティブレイヤーなら
				if(layer == canvas->active_layer)
				{
					if(layer->layer_type == TYPE_NORMAL_LAYER)
					{	// 通常レイヤーは
							// 作業レイヤーとアクティブレイヤーを一度合成してから下のレイヤーと合成
						(void)memcpy(canvas->temp_layer->pixels, layer->pixels, layer->stride*layer->height);
						canvas->part_layer_blend_functions[canvas->work_layer->layer_mode](canvas->work_layer, canvas->temp_layer, &canvas->temp_update);
						blend_layer = canvas->temp_layer;
						blend_layer->alpha = layer->alpha;
						blend_layer->flags = layer->flags;
						blend_layer->prev = layer->prev;
					}
					else if(layer->layer_type == TYPE_VECTOR_LAYER)
					{	// ベクトルレイヤーは
							// レイヤーのラスタライズ処理を行なってから作業レイヤーと下のレイヤーを合成
						RasterizeVectorLayer(canvas, layer, layer->layer_data.vector_layer);
						if(canvas->work_layer->layer_mode != LAYER_BLEND_NORMAL)
						{
							canvas->layer_blend_functions[canvas->work_layer->layer_mode](canvas->work_layer, layer);
						}
					}
					else if(layer->layer_type == TYPE_TEXT_LAYER)
					{	// テキストレイヤーは
							// テキストの内容をラスタライズ処理してから下のレイヤーと合成
						RenderTextLayer(canvas, layer, layer->layer_data.text_layer);
					}

					if(layer->next != NULL && layer->next->layer_type == TYPE_ADJUSTMENT_LAYER)
					{
						LAYER *src;
						int start_x = (int)canvas->temp_update.x;
						int start_y = (int)canvas->temp_update.y;
						int update_width = (int)canvas->temp_update.width;
						int update_height = (int)canvas->temp_update.height;
						int update_stride = update_width * 4;

						do
						{
							if((layer->next->flags & LAYER_FLAG_INVISIBLE) != 0)
							{
								int i;

								src = (layer->next->layer_data.adjustment_layer->target == ADJUSTMENT_LAYER_TARGET_UNDER_LAYER) ?
										layer : canvas->mixed_layer;
								for(i=0; i<update_height; i++)
								{
									(void)memcpy(&canvas->mask_temp->pixels[i*update_stride],
													&src->pixels[src->stride*i + start_x *4], update_stride);
								}
								layer->next->layer_data.adjustment_layer->filter_func(
										layer->next->layer_data.adjustment_layer, canvas->mask_temp->pixels, canvas->mask->pixels,
											update_width * update_height, layer);
								for(i=0; i<update_height; i++)
								{
									(void)memcpy(&layer->next->pixels[src->stride*i + start_x *4],
													&canvas->mask->pixels[i*update_stride], update_stride);
								}
							}

							layer->next->layer_data.adjustment_layer->update(
									layer->layer_data.adjustment_layer, layer, canvas->mixed_layer,
										start_x, start_y, update_width, update_height);
							blend_layer = layer->next;
							layer = layer->next;
						} while(layer->next != NULL && layer->next->layer_type == TYPE_ADJUSTMENT_LAYER);
					}

					// サムネイル更新
					UpdateLayerThumbnail(layer);
				}

				// 合成する対象と方法が確定したので合成を実行する
				canvas->part_layer_blend_functions[blend_mode](blend_layer, layer, &part_update);
				// 合成したらデータを元に戻す
				canvas->temp_layer->alpha = 100;
				canvas->temp_layer->flags = 0;
				canvas->temp_layer->prev = NULL;
				GraphicsSetOperator(&canvas->temp_layer->context.base, GRAPHICS_OPERATOR_OVER);
			}	// 非表示レイヤーになっていないことを確認
					// if((blend_layer->flags & LAYER_FLAG_INVISIBLE) == 0)

			// 次のレイヤーへ
			layer = layer->next;

			// 次に合成するレイヤーがアクティブレイヤーなら
			if(layer == canvas->active_layer)
			{	// アクティブレイヤーより下のレイヤーの合成データを更新
				(void)memcpy(canvas->under_active->pixels, canvas->mixed_layer->pixels, canvas->pixel_buf_size);
			}
		}	// 一番上のレイヤーに辿り着くまでループ
					// while(layer != NULL)
	}

NO_UPDATE_BLEND:
	// アイレベルラインを引く
	if((canvas->perspective_ruler.flags & PERSPECTIVE_RULER_FLAGS_DRAW_LINE) != 0)
	{
#if defined(USE_BGR_COLOR_SPACE) && USE_BGR_COLOR_SPACE != 0
		GraphicsSetSourceRGBA(&canvas->mixed_layer->context.base, canvas->perspective_ruler.eye_level.line_color[2] * DIV_PIXEL,
			canvas->perspective_ruler.eye_level.line_color[1] * DIV_PIXEL, canvas->perspective_ruler.eye_level.line_color[0] * DIV_PIXEL, canvas->perspective_ruler.eye_level.line_color[3] * DIV_PIXEL);
#else
		GraphicsSetSourceRGBA(&canvas->mixed_layer->context.base, canvas->perspective_ruler.eye_level.line_color[0] * DIV_PIXEL,
			canvas->perspective_ruler.eye_level.line_color[1] * DIV_PIXEL, canvas->perspective_ruler.eye_level.line_color[2] * DIV_PIXEL, canvas->perspective_ruler.eye_level.line_color[3] * DIV_PIXEL);
#endif
		GraphicsMoveTo(&canvas->mixed_layer->context.base, 0, canvas->perspective_ruler.eye_level.height);
		GraphicsLineTo(&canvas->mixed_layer->context.base, canvas->mixed_layer->width, canvas->perspective_ruler.eye_level.height);
		GraphicsStroke(&canvas->mixed_layer->context.base);
		GraphicsMoveTo(&canvas->mixed_layer->context.base, 0, 0);
	}

	if(update_mode == UPDATE_ALL)
	{
		if(canvas->app->display_filter.filter_funcion != NULL)
		{
			canvas->app->display_filter.filter_funcion(canvas->mixed_layer->pixels,
				canvas->mixed_layer->pixels, canvas->width*canvas->height, canvas->app->display_filter.filter_data);
		}

		// 現在の拡大縮小率で表示用のデータに合成したデータを転写
		GraphicsSetOperator(&canvas->scaled_mixed->context.base, GRAPHICS_OPERATOR_SOURCE);
		GraphicsSetSource(&canvas->scaled_mixed->context.base, &canvas->mixed_pattern.base);
		GraphicsPaint(&canvas->scaled_mixed->context.base);
	}
	else if(update_mode == UPDATE_PART)
	{
		FLOAT_T zoom = canvas->zoom_rate;

		if(app->display_filter.filter_funcion != NULL)
		{
			int start_x = (int)canvas->update.x;
			int start_y = (int)canvas->update.y;
			int width = (int)canvas->update.width;
			int height = (int)canvas->update.height;
			int stride = width * 4;
			for(y=0; y<height; y++)
			{
				(void)memcpy(&canvas->temp_layer->pixels[y*stride],
					&canvas->mixed_layer->pixels[(start_y+y)*canvas->mixed_layer->stride + start_x*4], stride);
			}
			app->display_filter.filter_funcion(canvas->temp_layer->pixels,
				canvas->temp_layer->pixels, width*height, app->display_filter.filter_data);
			for(y=0; y<height; y++)
			{
				(void)memcpy(&canvas->mixed_layer->pixels[(start_y+y)*canvas->mixed_layer->stride + start_x*4],
					&canvas->temp_layer->pixels[y*stride], stride);
			}
		}

		GraphicsSave(&canvas->scaled_mixed->context.base);
		GraphicsRectangle(&canvas->scaled_mixed->context.base, (int)(canvas->update.x * zoom), (int)(canvas->update.y * zoom),
			(int)(canvas->update.width * zoom), (int)(canvas->update.height * zoom));
		GraphicsClip(&canvas->scaled_mixed->context.base);
		GraphicsSetOperator(&canvas->scaled_mixed->context.base, GRAPHICS_OPERATOR_OVER);
		GraphicsSetSource(&canvas->scaled_mixed->context.base, &canvas->mixed_pattern.base);
		GraphicsPaint(&canvas->scaled_mixed->context.base);
		GraphicsRestore(&canvas->scaled_mixed->context.base);

		DestroyGraphicsSurface(&part_update.surface.base);
		DestroyGraphicsContext(&part_update.context.base);
		DestroyGraphicsSurface(&canvas->temp_update.surface.base);
		DestroyGraphicsContext(&canvas->temp_update.context.base);

		canvas->flags &= ~(DRAW_WINDOW_UPDATE_PART);
	}

	if(clear_part == FALSE)
	{
		(void)memcpy(canvas->disp_layer->pixels, canvas->scaled_mixed->pixels,
			canvas->scaled_mixed->stride * canvas->scaled_mixed->height);
	}
	else
	{
		int i;
		int copy_x = (int)(clear_x * canvas->zoom_rate - 1);
		int copy_y = (int)(clear_y * canvas->zoom_rate - 1);
		int copy_width = (int)(clear_width * canvas->zoom_rate + 1);
		int copy_height = (int)(clear_height * canvas->zoom_rate + 1);
		int copy_bytes = copy_width * 4;

		if(copy_x < 0)
		{
			copy_x = 0;
		}
		if(copy_y < 0)
		{
			copy_y = 0;
		}
		if(copy_x + copy_width > canvas->disp_layer->width)
		{
			copy_width = canvas->disp_layer->width - copy_x;
		}
		if(copy_y + copy_height > canvas->disp_layer->height)
		{
			copy_height = canvas->disp_layer->height - copy_y;
		}
		for(i=0; i<copy_height; i++)
		{
			(void)memcpy(&canvas->disp_layer->pixels[(copy_y + i)*canvas->disp_layer->stride+copy_x*4],
				&canvas->scaled_mixed->pixels[(copy_y + i)*canvas->disp_layer->stride+copy_x*4], copy_bytes);
		}
	}

	// マウスカーソルの描画処理
		// ブラシが持つカーソル表示用の関数を呼び出す
	// 変形処理中はカーソル表示はしない
	if(canvas->transform == NULL)
	{
		eLAYER_BLEND_MODE cursor_blend_mode = INVALID_LAYER_BLEND_MODE;

		// クリッピング前の状態を記憶
		GraphicsSave(&canvas->disp_layer->context.base);

		if((app->tool_box.flags & TOOL_USING_BRUSH) == 0)
		{	// 通常・ベクトルレイヤー両方で使えるツール使用中
			app->tool_box.active_common_tool->display_function(
				canvas, app->tool_box.active_common_tool, canvas->cursor_x, canvas->cursor_y);

			canvas->layer_blend_functions[canvas->effect->layer_mode](canvas->effect, canvas->disp_layer);
			// 表示用に使用したデータを初期化
			canvas->effect->layer_mode = LAYER_BLEND_NORMAL;
			(void)memset(canvas->effect->pixels, 0, canvas->effect->stride*canvas->effect->height);
		}
		else
		{
			BRUSH_UPDATE_INFO *update_range = NULL;
			(void)memset(canvas->disp_temp->pixels, 0, canvas->disp_temp->stride*canvas->disp_temp->height);
			if(canvas->active_layer->layer_type == TYPE_NORMAL_LAYER || ((canvas->flags & DRAW_WINDOW_EDIT_SELECTION) != 0))
			{	// 通常レイヤー
				if(app->tool_box.active_brush[app->input]->cursor_update_info.update)
				{
					app->tool_box.active_brush[app->input]->draw_cursor(
						canvas, canvas->cursor_x, canvas->cursor_y, canvas->app->tool_box.active_brush[app->input]);
					cursor_blend_mode = app->tool_box.active_brush[app->input]->cursor_blend_mode;
					update_range = &app->tool_box.active_brush[app->input]->cursor_update_info;
					update_range->update = FALSE;
				}
			}
			else if(canvas->active_layer->layer_type == TYPE_VECTOR_LAYER)
			{	// ベクトルレイヤーの場合、ShiftキーあるいはCtrlキーが
					// 押されていたら制御点操作の表示方法にする
				if(canvas->state.mouse_key_flag & (MOUSE_KEY_FLAG_SHIFT | MOUSE_KEY_FLAG_CONTROL))
				{
					app->tool_box.vector_control_core.draw_cursor(
						canvas, canvas->cursor_x, canvas->cursor_y, &app->tool_box.vector_control_core);
				}
				else
				{	// そうでなければブラシが持つカーソル表示関数を呼び出す
					app->tool_box.active_vector_brush[app->input]->draw_cursor(
						canvas, canvas->cursor_x, canvas->cursor_y, app->tool_box.active_vector_brush[app->input]);
					cursor_blend_mode = app->tool_box.active_vector_brush[app->input]->cursor_blend_mode;
				}
			}
			else if(canvas->active_layer->layer_type == TYPE_TEXT_LAYER)
			{	// テキストレイヤーならテキストの描画領域を表示
				DisplayTextLayerRange(canvas, canvas->active_layer->layer_data.text_layer);
			}
			// 作成したデータを表示データに合成
			if(cursor_blend_mode != INVALID_LAYER_BLEND_MODE && update_range != NULL)
			{
				UPDATE_RECTANGLE update_rectangle;
				eGRAPHICS_STATUS status;
				update_rectangle.x = update_range->start_x, update_rectangle.y = update_range->start_y;
				update_rectangle.width = update_range->width, update_rectangle.height = update_range->height;
				status = InitializeGraphicsImageSurfaceForRectangle(&update_rectangle.surface, &canvas->disp_layer->surface,
							update_rectangle.x, update_rectangle.y, update_rectangle.width, update_rectangle.height);
				if(status == GRAPHICS_STATUS_SUCCESS)
				{
					InitializeGraphicsDefaultContext(&update_rectangle.context, &update_rectangle.surface, &app->graphics);
					canvas->part_layer_blend_functions[cursor_blend_mode](canvas->disp_temp, canvas->disp_layer, &update_rectangle);
				}
			}
		}	// マウスカーソルの描画処理
					// ブラシが持つカーソル表示用の関数を呼び出す
			// if((window->app->tool_window.flags & TOOL_USING_BRUSH) == 0) else

		// 描画終了したので設定復元
		GraphicsRestore(&canvas->disp_layer->context.base);

		// 選択範囲の編集中であれば内容を描画
		if((canvas->flags & DRAW_WINDOW_EDIT_SELECTION) != 0)
		{
			DisplayEditSelection(canvas);
		}
		else
		{
			// 選択範囲があれば表示
			if((canvas->flags & DRAW_WINDOW_HAS_SELECTION_AREA) != 0)
			{
				DrawSelectionArea(&canvas->selection_area, canvas);
				canvas->layer_blend_functions[LAYER_BLEND_NORMAL](canvas->effect, canvas->disp_layer);
				(void)memset(canvas->effect->pixels, 0, canvas->effect->stride*canvas->effect->height);
			}
		}
	}	// 変形処理中はカーソル表示はしない
		// if(window->transform == NULL)
	else
	{
		(void)memset(canvas->disp_temp->pixels, 0, canvas->disp_temp->stride*canvas->disp_temp->height);
		//DisplayTransform(canvas);
	}

	// 左右反転表示中なら表示内容を左右反転
	if((canvas->flags & DRAW_WINDOW_DISPLAY_HORIZON_REVERSE) != 0)
	{
		const int width = canvas->disp_layer->width;
		const int stride = canvas->disp_layer->stride;

#ifdef _OPENMP
#pragma omp parallel for firstprivate(width, stride, window)
		for(y=0; y<window->disp_layer->height; y++)
		{
			uint8 *ref = &window->disp_temp->pixels[y*stride];
			uint8 *src = &window->disp_layer->pixels[(y+1)*stride-4];
			int x;

			for(x=0; x<width; x++, ref+=4, src-=4)
			{
				ref[0] = src[0], ref[1] = src[1], ref[2] = src[2], ref[3] = src[3];
			}
			(void)memcpy(src+4, &window->disp_temp->pixels[y*stride], stride);
		}
#else
		uint8 *ref;
		uint8 *src;
		int x;

		for(y=0; y<canvas->disp_layer->height; y++)
		{
			ref = canvas->disp_temp->pixels;
			src = &canvas->disp_layer->pixels[(y+1)*canvas->disp_layer->stride-4];
			for(x=0; x<width; x++, ref += 4, src -= 4)
			{
				ref[0] = src[0], ref[1] = src[1], ref[2] = src[2], ref[3] = src[3];
			}
			(void)memcpy(src+4, canvas->disp_temp->pixels, canvas->disp_layer->stride);
		}
#endif
	}

	// 更新があればナビゲーションとプレビューウィンドウを更新する
	if(update_mode != NO_UPDATE)
	{
		NavigationUpdate(app->navigation.widgets);
	}

execute_update:
	
	// パース定規が有効になっている場合、消失点を描画
	if((canvas->flags & DRAW_WINDOW_ACTIVATE_PERSPECTIVE_RULER) != 0)
	{
		// DisplayPerspectiveRulerMarkerOnWidget(canvas, (void*)cairo_p);
	}
	// 画面更新が終わったのでフラグを下ろす
	canvas->flags &= ~(DRAW_WINDOW_UPDATE_ACTIVE_UNDER
		| DRAW_WINDOW_UPDATE_ACTIVE_OVER | DRAW_WINDOW_UPDATE_AREA_INITIALIZED);
	canvas->update.need_update = FALSE;
	
	return result;
}

/*
* MixLayerForSave関数
* 保存/自動選択/バケツツールのために背景ピクセルデータ無しでレイヤーを合成
* 引数
* canvas	: 合成を実施するキャンバス
* 返り値
*	合成されたレイヤー 使用後はDeleteLayer必要
*/
LAYER* MixLayerForSave(DRAW_WINDOW* canvas)
{
	LAYER *result = CreateLayer(0, 0, canvas->width, canvas->height,
								4, TYPE_NORMAL_LAYER, NULL, NULL, NULL, canvas);
	LAYER *source = canvas->layer;
	LAYER *blend;
	int blend_mode;

	// 非表示以外の全てのレイヤーを合成
	while(source != NULL)
	{
		blend = source;
		blend_mode = source->layer_mode;

		if((source->flags & LAYER_FLAG_INVISIBLE) == 0
			&& source->layer_type != TYPE_LAYER_SET)
		{
			while(source->next != NULL && source->next->layer_type == TYPE_ADJUSTMENT_LAYER)
			{
				if((source->next->flags & LAYER_FLAG_INVISIBLE) != 0)
				{
					source->next->layer_data.adjustment_layer->filter_func(
						source->layer_data.adjustment_layer, source->pixels, source->next->pixels,
							source->width * source->height, source);
				}
				source->next->layer_data.adjustment_layer->update(
					source->layer_data.adjustment_layer, source, result,
						0, 0, source->width, source->height
				);
				blend = source->next;
				source = source->next;
			}

			if(!(blend->layer_set != NULL
				&& (blend->layer_set->flags & LAYER_FLAG_INVISIBLE) != 0))
			{
				canvas->layer_blend_functions[blend_mode](blend, result);
			}
		}

		source = source->next;
	}

	return result;
}

#ifdef __cplusplus
}
#endif
