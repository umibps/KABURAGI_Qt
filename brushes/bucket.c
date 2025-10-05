#include <string.h>
#include "../draw_window.h"
#include "../application.h"
#include "../brushes.h"
#include "../anti_alias.h"
#include "../display.h"
#include "../graphics/graphics_surface.h"
#include "../graphics/graphics_matrix.h"
#include "../gui/brushes_gui.h"

#ifdef _OPENMP
# include <omp.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
* BucketPressCallBack関数
* 鉛筆ツール使用時のマウスクリックに対するコールバック関数
* 引数
* canvas	: 描画領域の情報
* state		: マウスの状態
*/
void BucketPressCallBack(
	DRAW_WINDOW* canvas,
	BRUSH_CORE* core,
	EVENT_STATE* state
)
{
	int x, y;

	x = (int)state->cursor_x, y = (int)state->cursor_y;

	// キャンバス外へのクリックなら終了
	if(state->cursor_x < 0 || state->cursor_x > canvas->width
		|| state->cursor_y < 0 || state->cursor_y > canvas->height)
	{
		return;
	}

	if(state->mouse_key_flag & MOUSE_KEY_FLAG_LEFT)
	{
		BUCKET *bucket = (BUCKET*)core;
		LAYER *target;
		uint8 *buffer = &canvas->temp_layer->pixels[canvas->width*canvas->height];
		int min_x, min_y, max_x, max_y;
		uint8 channel = (bucket->select_mode == BUCKET_SELECT_MODE_RGB) ? 3
							: (bucket->select_mode == BUCKET_SELECT_MODE_ALPHA) ? 1 : 4;
		int i;

		if((canvas->active_layer->flags & LAYER_LOCK_OPACITY) == 0)
		{
			canvas->work_layer->layer_mode == LAYER_BLEND_NORMAL;
		}
		else
		{
			canvas->work_layer->layer_mode == LAYER_BLEND_ATOP;
		}

		switch(bucket->target)
		{
		case BUCKET_TARGET_ACTIVE_LAYER:
			target = canvas->active_layer;
			break;
		case BUCKET_TARGET_CANVAS:
			target = MixLayerForSave(canvas);
			break;
		default:
			target = NULL;
		}

		(void)memset(canvas->mask_temp->pixels, 0, canvas->pixel_buf_size);
		(void)memset(canvas->temp_layer->pixels, 0, canvas->pixel_buf_size);

		if(bucket->select_mode != BUCKET_SELECT_MODE_ALPHA)
		{
			DetectSameColorArea(
				target, buffer, &canvas->temp_layer->pixels[canvas->width*canvas->height*2],
				(int32)x, (int32)y, &target->pixels[y*target->stride + x*target->channel],
				channel, (int16)bucket->threshold, &min_x, &min_y, &max_x, &max_y,
				bucket->select_direction
			);
		}
		else
		{
			LAYER *local_target = CreateLayer(0, 0, target->width, target->height,
												1, TYPE_NORMAL_LAYER, NULL, NULL, NULL, canvas);
			for(i = 0; i < target->width * target->height; i++)
			{
				local_target->pixels[i] = target->pixels[i*4+3];
			}

			DetectSameColorArea(
				local_target,
				buffer,
				&canvas->temp_layer->pixels[canvas->width*canvas->height*2],
				x, y, &target->pixels[y*target->stride + x*target->channel + 3],
				channel, bucket->threshold, &min_x, &min_y, &max_x, &max_y,
				bucket->select_direction
			);

			DeleteLayer(&local_target);
		}

		if((bucket->core.flags & BRUSH_FLAG_ANTI_ALIAS) != 0)
		{
			(void)memcpy(&canvas->temp_layer->pixels[canvas->width*canvas->height*2],
							buffer, canvas->active_layer->width*canvas->active_layer->height);
			if((canvas->flags & DRAW_WINDOW_HAS_SELECTION_AREA) == 0
				&& (canvas->active_layer->flags & LAYER_LOCK_OPACITY) == 0)
			{
				OldAntiAlias(&canvas->temp_layer->pixels[canvas->width*canvas->height*2],
								buffer,
								canvas->active_layer->width, canvas->active_layer->height,
								canvas->active_layer->stride, canvas->app);
			}
			else
			{
				OldAntiAliasWithSelectionOrAlphaLock(
					(canvas->flags & DRAW_WINDOW_HAS_SELECTION_AREA) == 0 ? NULL : canvas->selection,
					(canvas->active_layer->flags & LAYER_LOCK_OPACITY) == 0 ? NULL : canvas->active_layer,
					&canvas->temp_layer->pixels[canvas->width*canvas->height*2],
					buffer, 0, 0, canvas->active_layer->width, canvas->active_layer->height,
					canvas->active_layer->stride, canvas->app
				);
			}
		}

		core->min_x = min_x - 1, core->min_y = min_y - 1;
		core->max_x = max_x + 1, core->max_y = max_y + 1;

		for(i=0; i<canvas->mask_temp->width*canvas->mask_temp->height; i++)
		{
			canvas->mask_temp->pixels[i*4+0] = buffer[i];
			canvas->mask_temp->pixels[i*4+1] = buffer[i];
			canvas->mask_temp->pixels[i*4+2] = buffer[i];
			canvas->mask_temp->pixels[i*4+3] = buffer[i];
		}

		if(bucket->extend > 0)
		{
			(void)memcpy(canvas->mask_temp->pixels, buffer,
							canvas->mask_temp->width * canvas->mask_temp->height);
			for(i=0; i<bucket->extend; i++)
			{
				ExtendSelectionAreaOneStep(canvas->mask_temp, canvas->temp_layer);
				(void)memcpy(canvas->mask_temp->pixels, canvas->temp_layer->pixels,
								canvas->mask_temp->width * canvas->mask_temp->height);
				core->min_x -= 1, core->min_y -= 1;
				core->max_x += 1, core->max_y += 1;
			}

			for(i=0; i<canvas->mask_temp->width*canvas->mask_temp->height; i++)
			{
				canvas->mask_temp->pixels[i*4+0] = canvas->temp_layer->pixels[i];
				canvas->mask_temp->pixels[i*4+1] = canvas->temp_layer->pixels[i];
				canvas->mask_temp->pixels[i*4+2] = canvas->temp_layer->pixels[i];
				canvas->mask_temp->pixels[i*4+3] = canvas->temp_layer->pixels[i];
			}
		}
		else
		{
			int end = abs(bucket->extend);

			(void)memcpy(canvas->mask_temp->pixels, buffer,
							canvas->mask_temp->width * canvas->mask_temp->height);
			for(i=0; i<end; i++)
			{
				ReductSelectionAreaOneStep(canvas->mask_temp, canvas->temp_layer);
				(void)memcpy(canvas->mask_temp->pixels, canvas->temp_layer->pixels,
								canvas->mask_temp->width * canvas->mask_temp->height);
			}

			for(i=0; i<canvas->width * canvas->height; i++)
			{
				canvas->mask_temp->pixels[i*4+0] = buffer[i];
				canvas->mask_temp->pixels[i*4+1] = buffer[i];
				canvas->mask_temp->pixels[i*4+2] = buffer[i];
				canvas->mask_temp->pixels[i*4+3] = buffer[i];
			}
		}

		GraphicsSetOperator(&canvas->work_layer->context.base, GRAPHICS_OPERATOR_OVER);
		if(canvas->app->textures.active_texture == 0)
		{
			if((canvas->flags & DRAW_WINDOW_HAS_SELECTION_AREA) == 0)
			{
				GraphicsSetSourceRGBA(&canvas->work_layer->context.base, (*core->color)[0] * DIV_PIXEL,
										(*core->color)[1] * DIV_PIXEL, (*core->color)[2] * DIV_PIXEL, core->opacity);
				GraphicsRectangle(&canvas->work_layer->context.base, 0, 0, canvas->work_layer->width, canvas->work_layer->height);
				GraphicsMaskSurface(&canvas->work_layer->context.base,
									&canvas->mask_temp->surface.base, 0, 0);
			}
			else
			{
				GRAPHICS_SURFACE_PATTERN pattern = {0};
				GraphicsSetSourceRGBA(&canvas->work_layer->context.base, (*core->color)[0] * DIV_PIXEL,
					(*core->color)[1] * DIV_PIXEL, (*core->color)[2] * DIV_PIXEL, core->opacity);
				(void)memset(canvas->temp_layer->pixels, 0, canvas->pixel_buf_size);
				GraphicsSetOperator(&canvas->temp_layer->context.base, GRAPHICS_OPERATOR_OVER);
				GraphicsRectangle(&canvas->temp_layer->context.base, 0, 0, canvas->width, canvas->height);
				GraphicsMaskSurface(&canvas->temp_layer->context.base, &canvas->mask_temp->surface.base, 0, 0);
				GraphicsSetSourceSurface(&canvas->work_layer->context.base,
											&canvas->selection->surface.base, 0, 0, &pattern);
			}
		}
		else
		{
			(void)memset(canvas->mask->pixels, 0, canvas->pixel_buf_size);
			if((canvas->flags & DRAW_WINDOW_HAS_SELECTION_AREA) == 0)
			{
				GraphicsSetSourceRGBA(&canvas->work_layer->context.base, (*core->color)[0] * DIV_PIXEL,
					(*core->color)[1] * DIV_PIXEL, (*core->color)[2] * DIV_PIXEL, core->opacity);
				GraphicsRectangle(&canvas->mask->context.base, 0, 0, canvas->mask->width, canvas->mask->height);
				GraphicsMaskSurface(&canvas->mask->context.base, &canvas->mask_temp->surface.base, 0, 0);
				GraphicsMaskSurface(&canvas->work_layer->context.base, &canvas->texture->surface.base, 0, 0);
			}
			else
			{
				GRAPHICS_SURFACE_PATTERN pattern = {0};
				GraphicsSetSourceRGBA(&canvas->work_layer->context.base, (*core->color)[0] * DIV_PIXEL,
					(*core->color)[1] * DIV_PIXEL, (*core->color)[2] * DIV_PIXEL, core->opacity);
				(void)memset(canvas->temp_layer->pixels, 0, canvas->pixel_buf_size);
				GraphicsSetOperator(&canvas->temp_layer->context.base, GRAPHICS_OPERATOR_OVER);
				GraphicsRectangle(&canvas->temp_layer->context.base, 0, 0, canvas->width, canvas->height);
				GraphicsMaskSurface(&canvas->temp_layer->context.base, &canvas->mask_temp->surface.base, 0, 0);
				GraphicsSetSourceSurface(&canvas->work_layer->context.base, &canvas->mask->surface.base, 0, 0, &pattern);
				GraphicsMaskSurface(&canvas->work_layer->context.base, &canvas->temp_layer->surface.base, 0, 0);
			}
		}

		AddBrushHistory(core, canvas->active_layer);

		canvas->layer_blend_functions[canvas->work_layer->layer_mode](canvas->work_layer, canvas->active_layer);

		(void)memset(canvas->work_layer->pixels, 0, canvas->work_layer->stride * canvas->work_layer->height);

		if(bucket->target == BUCKET_TARGET_CANVAS)
		{
			DeleteLayer(&target);
		}

		canvas->work_layer->layer_mode = LAYER_BLEND_NORMAL;

		canvas->update.x = min_x;
		canvas->update.y = min_y;
		canvas->update.width = max_x - min_x;
		canvas->update.height = max_y - min_y;
		canvas->flags |= DRAW_WINDOW_UPDATE_PART;
	}
}

void BucketEditSelectionPressCallBack(
	DRAW_WINDOW* canvas,
	BRUSH_CORE* core,
	EVENT_STATE* state
)
{
	int x, y;

	x = (int)state->cursor_x, y = (int)state->cursor_y;

	if((state->mouse_key_flag & MOUSE_KEY_FLAG_LEFT))
	{
		BUCKET *bucket = (BUCKET*)core;
		LAYER *target = canvas->mask;
		uint8 *buffer = &canvas->temp_layer->pixels[canvas->width * canvas->height];
		int32 min_x, min_y, max_x, max_y;
		int32 before_stride = target->stride;
		uint8 before_channel = target->channel;
		uint8 channel = 1;
		int i;

		canvas->selection->layer_mode = SELECTION_BLEND_NORMAL;

		(void)memcpy(canvas->mask->pixels, canvas->selection->pixels, canvas->width * canvas->height);
		(void)memset(canvas->mask_temp->pixels, 0, canvas->pixel_buf_size);
		(void)memset(canvas->temp_layer->pixels, 0, canvas->pixel_buf_size);

		target->channel = 1;
		target->stride = target->width;
		DetectSameColorArea(
			target,
			buffer,
			&canvas->temp_layer->pixels[canvas->width * canvas->height * 2],
			(int32)x, (int32)y,
			&target->pixels[(int32)y * target->stride + (int32)x * target->channel],
			channel, bucket->threshold, &min_x, &min_y, &max_x, &max_y,
			(eSELECT_FUZZY_DIRECTION)bucket->select_direction
		);
		target->stride = before_stride;
		target->channel = before_channel;

		if((core->flags & BRUSH_FLAG_ANTI_ALIAS) != 0)
		{
			(void)memcpy(&canvas->temp_layer->pixels[canvas->width * canvas->height * 2],
				buffer, canvas->active_layer->width * canvas->active_layer->height);
			OldAntiAliasEditSelection(&canvas->temp_layer->pixels[canvas->width * canvas->height * 2],
				buffer, canvas->active_layer->width, canvas->active_layer->height, canvas->active_layer->width, canvas->app);

		}

		core->min_x = min_x - 1, core->min_y = min_y - 1;
		core->max_x = max_x + 1, core->max_y = max_y + 1;

		for(i = 0; i < canvas->mask_temp->width * canvas->mask->height; i++)
		{
			canvas->mask_temp->pixels[i*4+0] = buffer[i];
			canvas->mask_temp->pixels[i*4+1] = buffer[i];
			canvas->mask_temp->pixels[i*4+2] = buffer[i];
			canvas->mask_temp->pixels[i*4+3] = buffer[i];
		}

		if(bucket->extend > 0)
		{
			(void)memcpy(canvas->mask_temp->pixels, buffer,
				canvas->mask_temp->height * canvas->mask_temp->width);
			for(i = 0; i < bucket->extend; i++)
			{
				ExtendSelectionAreaOneStep(canvas->mask_temp, canvas->temp_layer);
				(void)memcpy(canvas->mask_temp->pixels, canvas->temp_layer->pixels,
					canvas->mask_temp->width * canvas->mask_temp->height);
				core->min_x -= 1, core->min_y -= 1;
				core->max_x += 1, core->max_y += 1;
			}

			for(i = 0; i < canvas->mask_temp->width * canvas->mask->height; i++)
			{
				canvas->mask_temp->pixels[i*4+0] = buffer[i];
				canvas->mask_temp->pixels[i*4+1] = buffer[i];
				canvas->mask_temp->pixels[i*4+2] = buffer[i];
				canvas->mask_temp->pixels[i*4+3] = buffer[i];
			}
		}
		else
		{
			int end = abs(bucket->extend);

			(void)memcpy(canvas->mask_temp->pixels, buffer,
				canvas->mask_temp->height * canvas->mask_temp->width);
			for(i = 0; i < end; i++)
			{
				ReductSelectionAreaOneStep(canvas->mask_temp, canvas->temp_layer);
				(void)memcpy(canvas->mask_temp->pixels, canvas->temp_layer->pixels,
					canvas->mask_temp->width * canvas->mask_temp->height);
			}

			for(i = 0; i < canvas->mask_temp->width * canvas->mask->height; i++)
			{
				canvas->mask_temp->pixels[i*4+0] = buffer[i];
				canvas->mask_temp->pixels[i*4+1] = buffer[i];
				canvas->mask_temp->pixels[i*4+2] = buffer[i];
				canvas->mask_temp->pixels[i*4+3] = buffer[i];
			}
		}

		GraphicsSetOperator(&canvas->work_layer->context.base, GRAPHICS_OPERATOR_OVER);
		GraphicsSetSourceRGB(&canvas->work_layer->context.base, 0, 0, 0);
		GraphicsRectangle(&canvas->work_layer->context.base, 0, 0,
							canvas->work_layer->width, canvas->work_layer->height);
		GraphicsMaskSurface(&canvas->work_layer->context.base, &canvas->mask_temp->surface.base, 0, 0);

		AddSelectionEditHistory(core, canvas->selection);

		canvas->blend_selection_functions[SELECTION_BLEND_NORMAL](canvas->work_layer, canvas->selection);

		(void)memset(canvas->work_layer->pixels, 0, canvas->work_layer->stride * canvas->work_layer->height);

		canvas->work_layer->layer_mode = LAYER_BLEND_NORMAL;
	}
}

/*
* LoadBucketData関数
* バケツツールの設定データを読み取る
* 引数
* file			: 初期化ファイル読み取りデータ
* section_name	: 初期化ファイルにバケツが記録されてるセクション名
* app			: ブラシ初期化にはアプリケーション管理用データが必要
* 返り値
*	バケツデータ 使用終了後にMEM_FREE_FUNC必要
*/
BRUSH_CORE* LoadBucketData(INI_FILE_PTR file, const char* section_name, APPLICATION* app)
{
	BUCKET *bucket = (BUCKET*)MEM_CALLOC_FUNC(1, sizeof(*bucket));

	bucket->core.name = IniFileStrdup(file, section_name, "NAME");
	bucket->core.image_file_path = IniFileStrdup(file, section_name, "IMAGE");

	bucket->core.color = &app->tool_box.color_chooser.rgb;
	bucket->core.back_color = &app->tool_box.color_chooser.back_rgb;
	bucket->core.app = app;

	if(bucket->core.opacity > DEFAULT_BRUSH_FLOW_MAXIMUM)
	{
		bucket->core.opacity = DEFAULT_BRUSH_FLOW_MAXIMUM;
	}
	else if(bucket->core.opacity < DEFAULT_BRUSH_FLOW_MINIMUM)
	{
		bucket->core.opacity = DEFAULT_BRUSH_FLOW_MINIMUM;
	}
	bucket->core.brush_type = BRUSH_TYPE_BUCKET;
	bucket->threshold = IniFileGetInteger(file, section_name, "THRESHOLD");
	if(bucket->threshold <= 0)
	{
		bucket->threshold = 1;
	}
	bucket->select_mode = (eBUCKET_SELECT_MODE)IniFileGetInteger(file, section_name, "MODE");
	bucket->target = (eBUCKET_TARGET)IniFileGetInteger(file, section_name, "TARGET");
	bucket->extend = IniFileGetInteger(file, section_name, "EXTEND");
	bucket->core.press_function = BucketPressCallBack;
	bucket->core.motion_function = BucketMotionCallBack;
	bucket->core.release_function = BucketReleaseCallBack;
	bucket->core.draw_cursor = BucketDrawCursor;
	bucket->core.brush_data = BucketGUI_New(app, &bucket->core);
	bucket->core.create_detail_ui = CreateBucketDetailUI;
	bucket->core.button_update = BucketButtonUpdate;
	bucket->core.motion_update = BucketMotionUpdate;
	bucket->core.change_zoom = BucketChangeZoom;

	return &bucket->core;
}

/*
* WriteBucketData関数
* バケツツールの設定データを記録する
* 引数
* file			: 書き込みファイル操作用データ
* core			: 記録するツールデータ
* section_name	: 記録するセクション名
*/
void WriteBucketData(INI_FILE_PTR file, BRUSH_CORE* core, const char* section_name)
{
	BUCKET *bucket = (BUCKET*)core;

	(void)IniFileAddString(file, section_name, "TYPE", "BUCKET");
	(void)IniFileAddInteger(file, section_name, "THRESHOLD", bucket->extend, 10);
	(void)IniFileAddInteger(file, section_name, "MODE", bucket->select_mode, 10);
	(void)IniFileAddInteger(file, section_name, "TARGET", bucket->target, 10);
	(void)IniFileAddInteger(file, section_name, "EXTEND", bucket->extend, 10);
}

#ifdef __cplusplus
}
#endif
