#include <string.h>
#include "../draw_window.h"
#include "../application.h"
#include "../brushes.h"
#include "../anti_alias.h"
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
* EraserPressCallBack関数
* 消しゴムツール使用時のマウスクリックに対するコールバック関数
* 引数
* canvas	: 描画領域の情報
* state		: マウスの状態
*/
void EraserPressCallBack(
	DRAW_WINDOW* canvas,
	BRUSH_CORE* core,
	EVENT_STATE* state
)
{
	FLOAT_T x = state->cursor_x, y = state->cursor_y;
	FLOAT_T pressure = state->pressure;

	// 左クリックならば
	if((state->mouse_key_flag & MOUSE_KEY_FLAG_LEFT) != 0)
	{
		// ブラシの詳細情報にキャスト
		ERASER* eraser = (PENCIL*)core;
		// ブラシ効果を適用するレイヤー
		LAYER* brush_target;
		// 描画範囲のイメージ情報
		GRAPHICS_DEFAULT_CONTEXT update = { 0 };
		GRAPHICS_IMAGE_SURFACE update_surface = { 0 };
		// ブラシの半径と不透明度
		FLOAT_T r, alpha;
		// 座標の最大・最小値
		FLOAT_T min_x, min_y, max_x, max_y;
		// ブラシの座標指定用
		GRAPHICS_MATRIX matrix;
		// 筆圧によるブラシ縮小用
		FLOAT_T zoom;
		// ピクセルデータをリセットする座標
		int start_x, start_y;
		// ピクセルデータリセット時の高さと1行分のバイト数
		int height, stride;
		// 不透明部保護時のマスク用
		uint8* mask;
		int i;	// for文用のカウンタ

		// ツールボックスのデータへのアクセス用
		APPLICATION* app = canvas->app;

		// 作業レイヤーの合成方法を設定
		canvas->work_layer->layer_mode = LAYER_BLEND_ALPHA_MINUS;

		if((core->flags & BRUSH_FLAG_USE_OLD_ANTI_ALIAS) == 0 && (eraser->core.flags & BRUSH_FLAG_ANTI_ALIAS) != 0)
		{
			brush_target = canvas->anti_alias;
		}
		else
		{
			brush_target = canvas->work_layer;
		}

		// 最低筆圧のチェック
		if(pressure < eraser->core.minimum_pressure)
		{
			pressure = eraser->core.minimum_pressure;
		}

		// 筆圧でサイズ変更するかフラグを見てからブラシの半径決定
		if((eraser->core.flags & BRUSH_FLAG_SIZE) == 0)
		{
			r = eraser->core.radius;
			zoom = 1;
		}
		else
		{
			r = eraser->core.radius * pressure;
			zoom = 1 / pressure;
		}
		// 筆圧で不透明度変更するかフラグを見てから不透明度決定
		alpha = ((eraser->core.flags & BRUSH_FLAG_FLOW) == 0) ? 1 : pressure;

		// マウスの座標とブラシの半径から
			// 描画する座標の最大・最小値を決定
		min_x = x - r, min_y = y - r;
		max_x = x + r + 1, max_y = y + r + 1;

		// 更新範囲のサーフェース作成
		InitializeGraphicsImageSurfaceForRectangle(
			&update_surface,
			&brush_target->surface,
			min_x, min_y,
			r * 2 + 1, r * 2 + 1
		);
		InitializeGraphicsDefaultContext(&update, &update_surface, &app->graphics);

		// 今回の座標を記憶
		eraser->core.before_x = x, eraser->core.before_y = y;

		// 描画領域からはみ出たら修正
		if(min_x < 0.0)
		{
			min_x = 0.0;
		}
		else if(min_x >= canvas->width)
		{
			return;
		}
		if(min_y < 0.0)
		{
			min_y = 0.0;
		}
		else if(min_y >= canvas->height)
		{
			return;
		}
		if(max_x > canvas->work_layer->width)
		{
			max_x = canvas->work_layer->width;
		}
		else if(max_x <= 0)
		{
			GraphicsSurfaceFinish(&update_surface.base);
			DestroyGraphicsContext(&update.base);
			return;
		}
		if(max_y > canvas->work_layer->height)
		{
			max_y = canvas->work_layer->height;
		}
		else if(max_y <= 0)
		{
			GraphicsSurfaceFinish(&update_surface.base);
			DestroyGraphicsContext(&update.base);
			return;
		}

		// 再描画の範囲を指定
		canvas->update.x = start_x = (int)min_x;
		canvas->update.y = start_y = (int)min_y;
		canvas->update.width = (int)max_x - canvas->update.x;
		stride = (int)canvas->update.width * 4;
		canvas->update.height = (int)max_y - canvas->update.y;
		height = (int)canvas->update.height;

		// 現在の座標の最大・最小値を記憶しておく
		core->min_x = min_x, core->min_y = min_y;
		core->max_x = max_x, core->max_y = max_y;

		// 作業レイヤーの描画内容の合成方法を通常にしてから
		GraphicsSetOperator(&canvas->work_layer->context.base, GRAPHICS_OPERATOR_OVER);

		mask = canvas->mask_temp->pixels;
		if(app->textures.active_texture == 0)
		{
			if((canvas->flags & DRAW_WINDOW_HAS_SELECTION_AREA) == 0)
			{
				if((canvas->active_layer->flags & LAYER_LOCK_OPACITY) == 0)
				{
					InitializeGraphicsMatrixScale(&matrix, zoom, zoom);
					GraphicsPatternSetMatrix(&core->brush_pattern.base, &matrix);
					GraphicsSetSource(&update.base, &core->brush_pattern.base);
					GraphicsPaintWithAlpha(&update.base, alpha);
				}
				else
				{
					InitializeGraphicsMatrixScale(&matrix, zoom, zoom);
					GraphicsPatternSetMatrix(&core->brush_pattern.base, &matrix);
					GraphicsSetSource(&core->temp_context.base, &core->brush_pattern.base);
					GraphicsPaintWithAlpha(&core->temp_context.base, alpha);
					InitializeGraphicsMatrixIdentity(&matrix);
					GraphicsPatternSetMatrix(&core->temp_pattern.base, &matrix);
					GraphicsSetSource(&update.base, &core->temp_pattern.base);
					GraphicsMaskSurface(&update.base, &canvas->active_layer->surface.base,
						-x + r, -y + r);
				}
			}
			else
			{
				if((canvas->active_layer->flags & LAYER_LOCK_OPACITY) == 0)
				{
					InitializeGraphicsMatrixScale(&matrix, zoom, zoom);
					GraphicsPatternSetMatrix(&core->brush_pattern.base, &matrix);
					GraphicsSetSource(&core->temp_context.base, &core->brush_pattern.base);
					GraphicsPaintWithAlpha(&core->temp_context.base, alpha);
					InitializeGraphicsMatrixIdentity(&matrix);
					GraphicsPatternSetMatrix(&core->temp_pattern.base, &matrix);
					GraphicsSetSource(&update.base, &core->temp_pattern.base);
					GraphicsMaskSurface(&canvas->mask_temp->context.base,
						&canvas->selection->surface.base, -x + r, -y + r);
				}
				else
				{
					GRAPHICS_IMAGE_SURFACE temp_surface = { 0 };
					GRAPHICS_DEFAULT_CONTEXT update_temp = { 0 };
					GRAPHICS_SURFACE_PATTERN surface_pattern = { 0 };

					InitializeGraphicsImageSurfaceForRectangle(&temp_surface, &canvas->temp_layer->surface,
						start_x, start_y, r * 2 + 1, r * 2 + 1);
					InitializeGraphicsDefaultContext(&update_temp, &temp_surface, &app->graphics);

					InitializeGraphicsMatrixScale(&matrix, zoom, zoom);
					GraphicsPatternSetMatrix(&core->brush_pattern.base, &matrix);
					GraphicsSetSource(&core->temp_context.base, &core->brush_pattern.base);
					GraphicsPaintWithAlpha(&core->temp_context.base, alpha);
					InitializeGraphicsMatrixIdentity(&matrix);
					GraphicsPatternSetMatrix(&core->temp_pattern.base, &matrix);
					GraphicsSetSource(&update.base, &core->temp_pattern.base);

					for(i = 0; i < height; i++)
					{
						(void)memset(&canvas->temp_layer->pixels[
							(i + start_y) * canvas->temp_layer->stride + start_x * 4],
							0, stride
						);
					}

					GraphicsMaskSurface(&update.base, &canvas->selection->surface.base,
						-x + r, -y + r);
					GraphicsSetSourceSurface(
						&update_temp.base, &update_surface.base, 0, 0, &surface_pattern);
					GraphicsMaskSurface(&update_temp.base, &canvas->active_layer->surface.base,
						-x + r, -y + r);

					mask = canvas->temp_layer->pixels;

					GraphicsSurfaceFinish(&temp_surface.base);
					DestroyGraphicsContext(&update_temp.base);
				}
			}
		}
		else
		{
			GRAPHICS_IMAGE_SURFACE temp_surface = { 0 };
			GRAPHICS_DEFAULT_CONTEXT update_temp = { 0 };
			GRAPHICS_SURFACE_PATTERN surface_pattern = { 0 };

			InitializeGraphicsImageSurfaceForRectangle(&temp_surface, &canvas->temp_layer->surface,
				min_x, min_y, r * 2 + 1, r * 2 + 1);
			InitializeGraphicsDefaultContext(&update_temp, &temp_surface.base, &app->graphics);

			for(i = 0; i < height; i++)
			{
				(void)memset(&canvas->temp_layer->pixels[
					(i + start_y) * canvas->temp_layer->stride + start_x * 4],
					0, stride
				);
			}

			if((canvas->flags & DRAW_WINDOW_HAS_SELECTION_AREA) == 0)
			{
				if((canvas->active_layer->flags & LAYER_LOCK_OPACITY) == 0)
				{
					InitializeGraphicsMatrixScale(&matrix, zoom, zoom);
					GraphicsPatternSetMatrix(&core->brush_pattern.base, &matrix);
					GraphicsSetSource(&update_temp.base, &core->brush_pattern.base);
					GraphicsPaintWithAlpha(&update_temp.base, alpha);
				}
				else
				{
					InitializeGraphicsMatrixScale(&matrix, zoom, zoom);
					GraphicsPatternSetMatrix(&core->brush_pattern.base, &matrix);
					GraphicsSetSource(&core->temp_context.base, &core->brush_pattern.base);
					GraphicsPaintWithAlpha(&core->temp_context.base, alpha);
					InitializeGraphicsMatrixIdentity(&matrix);
					GraphicsPatternSetMatrix(&core->temp_pattern.base, &matrix);
					GraphicsSetSource(&update_temp.base, &core->temp_pattern.base);
					GraphicsMaskSurface(&update_temp.base, &canvas->active_layer->surface.base,
						-x + r, -y + r);
				}

				GraphicsSetSourceSurface(&update.base, &temp_surface.base, 0, 0, &surface_pattern);
				GraphicsMaskSurface(&update.base, &canvas->texture->surface.base,
					-x + r, -y + r);

				mask = canvas->temp_layer->pixels;
			}
			else
			{
				if((canvas->active_layer->flags & LAYER_LOCK_OPACITY) == 0)
				{
					InitializeGraphicsMatrixScale(&matrix, zoom, zoom);
					GraphicsPatternSetMatrix(&core->brush_pattern.base, &matrix);
					GraphicsSetSource(&core->temp_context.base, &core->brush_pattern.base);
					GraphicsPaintWithAlpha(&core->temp_context.base, alpha);
					InitializeGraphicsMatrixIdentity(&matrix);
					GraphicsPatternSetMatrix(&core->temp_pattern.base, &matrix);
					GraphicsSetSource(&update.base, &core->temp_pattern.base);
					GraphicsMaskSurface(&update.base, &canvas->selection->surface.base,
						-x + r, -y + r);
					GraphicsMaskSurface(&update.base, &canvas->selection->surface.base, -x + r, -y + r);

					for(i = 0; i < height; i++)
					{
						(void)memset(&canvas->temp_layer->pixels[
							(i + start_y) * canvas->temp_layer->stride + start_x * 4],
							0, stride
						);
					}

					GraphicsSetSourceSurface(&update_temp.base, &update_surface.base, 0, 0, &surface_pattern);
					GraphicsMaskSurface(&update_temp.base, &canvas->texture->surface.base, -x + r, -y + r);
				}
				else
				{
					InitializeGraphicsMatrixScale(&matrix, zoom, zoom);
					GraphicsPatternSetMatrix(&core->brush_pattern.base, &matrix);
					GraphicsSetSource(&core->temp_context.base, &core->brush_pattern.base);
					GraphicsPaintWithAlpha(&core->temp_context.base, alpha);
					InitializeGraphicsMatrixIdentity(&matrix);
					GraphicsPatternSetMatrix(&core->temp_pattern.base, &matrix);
					GraphicsSetSource(&update.base, &core->temp_pattern.base);

					for(i = 0; i < height; i++)
					{
						(void)memset(&canvas->temp_layer->pixels[
							(i + start_y) * canvas->temp_layer->stride + start_x * 4],
							0, stride
						);
					}

					GraphicsMaskSurface(&update.base, &canvas->selection->surface.base,
						-x + r, -y + r);
					GraphicsSetSourceSurface(&update_temp.base, &update_surface.base, 0, 0, &surface_pattern);
					GraphicsMaskSurface(&update_temp.base, &canvas->active_layer->surface.base,
						-x + r, -y + r);

					for(i = 0; i < height; i++)
					{
						(void)memset(&canvas->mask_temp->pixels[
							(i + start_y) * canvas->mask_temp->stride + start_x * 4],
							0, stride
						);
					}

					GraphicsSetSourceSurface(&update.base, &temp_surface.base, 0, 0, &surface_pattern);
					GraphicsMaskSurface(&update.base, &canvas->texture->surface.base,
						-x + r, -y + r);
					GraphicsMaskSurface(&update.base, &canvas->texture->surface.base,
						-x + r, -y + r);
				}
			}

			GraphicsSurfaceFinish(&temp_surface.base);
			DestroyGraphicsContext(&update_temp.base);
		}

		eraser->core.before_x = x, eraser->core.before_y = y;
		GraphicsSurfaceFinish(&update_surface.base);
		DestroyGraphicsContext(&update.base);

		if((core->flags & BRUSH_FLAG_USE_OLD_ANTI_ALIAS) == 0 && (eraser->core.flags & BRUSH_FLAG_ANTI_ALIAS) != 0)
		{
			if((canvas->flags & DRAW_WINDOW_DISPLAY_HORIZON_REVERSE) == 0)
			{
#ifdef _OPENMP
#pragma omp parallel for firstprivate(height, start_y, start_x, stride, window)
#endif
				for(i = 0; i < height; i++)
				{
					(void)memcpy(&canvas->work_layer->pixels[(start_y + i) * canvas->work_layer->stride + start_x * 4],
						&canvas->anti_alias->pixels[(start_y + i) * canvas->work_layer->stride + start_x * 4], stride);
				}
			}
			else
			{
				int width = stride / 4;
#ifdef _OPENMP
#pragma omp parallel for firstprivate(width, height, start_y, start_x, window)
#endif

				for(i = 0; i < height; i++)
				{
					uint8* ref, * src;
					int x;

					src = &canvas->work_layer->pixels[(start_y + i) * canvas->work_layer->stride + start_x * 4];
					ref = &canvas->anti_alias->pixels[(start_y + i) * canvas->work_layer->stride + (width - start_x) * 4];
					for(x = 0; x < width; x++, ref++, src++)
					{
						ref[0] = src[0], ref[1] = src[1], ref[2] = src[2], ref[3] = src[3];
					}
				}
			}
		}

		canvas->flags |= DRAW_WINDOW_UPDATE_PART;
	}
}

void EraserMotionCallBack(
	DRAW_WINDOW* canvas,
	BRUSH_CORE* core,
	EVENT_STATE* state
)
{
	FLOAT_T x = state->cursor_x, y = state->cursor_y;
	FLOAT_T pressure = state->pressure;

	if(state->mouse_key_flag & MOUSE_KEY_FLAG_LEFT)
	{
		ERASER* eraser = (ERASER*)core;
		// ブラシ効果を適用するレイヤー
		LAYER* brush_target;
		GRAPHICS_MATRIX matrix;
		// 描画範囲のイメージ情報
		GRAPHICS_DEFAULT_CONTEXT update = { 0 };
		GRAPHICS_IMAGE_SURFACE update_surface = { 0 };
		FLOAT_T r, step, alpha, d;
		FLOAT_T min_x, min_y, max_x, max_y;
		FLOAT_T draw_x = eraser->core.before_x, draw_y = eraser->core.before_y;
		FLOAT_T dx, dy, diff_x, diff_y;
		FLOAT_T zoom;
		int start_x, width, start_y, height;
		int stride = 0;
		uint8* work_pixel;
		uint8* mask;
		int i;

		if((core->flags & BRUSH_FLAG_USE_OLD_ANTI_ALIAS) == 0 && (eraser->core.flags & BRUSH_FLAG_ANTI_ALIAS) != 0)
		{
			brush_target = canvas->mask_temp;
			work_pixel = canvas->anti_alias->pixels;
		}
		else
		{
			brush_target = canvas->temp_layer;
			work_pixel = canvas->work_layer->pixels;
		}

		// 最低筆圧のチェック
		if(pressure < eraser->core.minimum_pressure)
		{
			pressure = eraser->core.minimum_pressure;
		}

		if((eraser->core.flags & BRUSH_FLAG_SIZE) == 0)
		{
			r = eraser->core.radius;
			zoom = 1;
		}
		else
		{
			r = eraser->core.radius * pressure;
			zoom = 1 / pressure;
		}
		step = r * BRUSH_STEP;
		if(step < MIN_BRUSH_STEP)
		{
			dx = 0;
			goto skip_draw;
		}
		alpha = ((eraser->core.flags & BRUSH_FLAG_FLOW) == 0) ?
			1 : pressure;
		dx = x - eraser->core.before_x, dy = y - eraser->core.before_y;
		d = sqrt(dx * dx + dy * dy);
		if(step < MINIMUM_BRUSH_DISTANCE)
		{
			step = MINIMUM_BRUSH_DISTANCE;
		}
		diff_x = step * dx / d, diff_y = step * dy / d;

		min_x = x - r * 2, min_y = y - r * 2;
		max_x = x + r * 2, max_y = y + r * 2;

		if((eraser->core.flags & BRUSH_FLAG_ANTI_ALIAS) != 0)
		{
			max_x++;
			max_y++;
		}

		if(min_x < 0.0)
		{
			min_x = 0.0;
		}
		if(min_y < 0.0)
		{
			min_y = 0.0;
		}
		if(max_x > canvas->work_layer->width)
		{
			max_x = canvas->work_layer->width;
		}
		if(max_y > canvas->work_layer->height)
		{
			max_y = canvas->work_layer->height;
		}

		if(canvas->update.x > min_x)
		{
			canvas->update.width += canvas->update.x - (int)min_x;
			canvas->update.x = (int)min_x;
		}
		if(canvas->update.width + canvas->update.x < max_x)
		{
			canvas->update.width += (int)max_x - canvas->update.width + canvas->update.x;
		}
		if(canvas->update.y > min_y)
		{
			canvas->update.height += canvas->update.y - (int)min_y;
			canvas->update.y = (int)min_y;
		}
		if(canvas->update.height + canvas->update.y < max_y)
		{
			canvas->update.height += (int)max_y - canvas->update.height + canvas->update.y;
		}

		if(core->min_x > min_x)
		{
			core->min_x = min_x;
		}
		if(core->min_y > min_y)
		{
			core->min_y = min_y;
		}
		if(core->max_x < max_x)
		{
			core->max_x = max_x;
		}
		if(core->max_y < max_y)
		{
			core->max_y = max_y;
		}

		dx = d;
		do
		{
			start_x = (int)(draw_x - r);
			start_y = (int)(draw_y - r);
			width = (int)(draw_x + r + 1);
			height = (int)(draw_y + r + 1);

			if(start_x < 0)
			{
				start_x = 0;
			}
			else if(start_x > canvas->work_layer->width)
			{
				goto skip_draw;
			}
			if(start_y < 0)
			{
				start_y = 0;
			}
			else if(start_y > canvas->work_layer->height)
			{
				goto skip_draw;
			}
			if(width > canvas->work_layer->width)
			{
				width = canvas->work_layer->width - start_x;
			}
			else
			{
				width = width - start_x;
			}
			if(height > canvas->work_layer->height)
			{
				height = canvas->work_layer->height - start_y;
			}
			else
			{
				height = height - start_y;
			}
			stride = width * 4;

			if(width <= 0 || height <= 0)
			{
				goto skip_draw;
			}

			InitializeGraphicsImageSurfaceForRectangle(&update_surface, &brush_target->surface,
				draw_x - r, draw_y - r, r * 2 + 2, r * 2 + 2);
			InitializeGraphicsDefaultContext(&update, &update_surface.base, &core->app->graphics);

			canvas->flags |= DRAW_WINDOW_UPDATE_PART;

			mask = brush_target->pixels;
			for(i = 0; i < height; i++)
			{
				(void)memset(&mask[(i + start_y) * canvas->mask_temp->stride + start_x * 4],
					0, stride);
			}

			GraphicsSetOperator(&canvas->mask_temp->context.base, GRAPHICS_OPERATOR_OVER);

			if(canvas->app->textures.active_texture == 0)
			{
				if((canvas->flags & DRAW_WINDOW_HAS_SELECTION_AREA) == 0)
				{
					if((canvas->active_layer->flags & LAYER_LOCK_OPACITY) == 0)
					{
						InitializeGraphicsMatrixScale(&matrix, zoom, zoom);
						GraphicsPatternSetMatrix(&core->brush_pattern.base, &matrix);
						GraphicsSetSource(&update.base, &core->brush_pattern.base);
						GraphicsSetOperator(&update.base, GRAPHICS_OPERATOR_SOURCE);
						GraphicsPaintWithAlpha(&update.base, alpha);
					}
					else
					{
						InitializeGraphicsMatrixScale(&matrix, zoom, zoom);
						GraphicsPatternSetMatrix(&core->brush_pattern.base, &matrix);
						GraphicsSetSource(&core->temp_context.base, &core->brush_pattern.base);
						GraphicsPaintWithAlpha(&core->temp_context.base, alpha);
						InitializeGraphicsMatrixIdentity(&matrix);
						GraphicsPatternSetMatrix(&core->temp_pattern.base, &matrix);
						GraphicsSetSource(&update.base, &core->temp_pattern.base);
						GraphicsMaskSurface(&update.base, &canvas->active_layer->surface.base,
							-draw_x + r, -draw_y + r);
					}
				}
				else
				{
					if((canvas->active_layer->flags & LAYER_LOCK_OPACITY) == 0)
					{
						InitializeGraphicsMatrixScale(&matrix, zoom, zoom);
						GraphicsPatternSetMatrix(&core->brush_pattern.base, &matrix);
						GraphicsSetSource(&core->temp_context.base, &core->brush_pattern.base);
						GraphicsPaintWithAlpha(&core->temp_context.base, alpha);
						InitializeGraphicsMatrixIdentity(&matrix);
						GraphicsPatternSetMatrix(&core->temp_pattern.base, &matrix);
						GraphicsSetSource(&update.base, &core->temp_pattern.base);
						GraphicsMaskSurface(&update.base, &canvas->selection->surface.base,
							-draw_x + r, -draw_y + r);
					}
					else
					{
						GRAPHICS_IMAGE_SURFACE temp_surface = { 0 };
						GRAPHICS_DEFAULT_CONTEXT update_temp = { 0 };
						GRAPHICS_SURFACE_PATTERN temp_pattern = { 0 };

						InitializeGraphicsImageSurfaceForRectangle(&temp_surface, &canvas->temp_layer->surface,
							start_x, start_y, r * 2 + 1, r * 2 + 1);
						InitializeGraphicsDefaultContext(&update_temp, &temp_surface, &core->app->graphics);

						InitializeGraphicsMatrixScale(&matrix, zoom, zoom);
						GraphicsPatternSetMatrix(&core->brush_pattern.base, &matrix);
						GraphicsSetSource(&core->temp_context.base, &core->brush_pattern.base);
						GraphicsPaintWithAlpha(&core->temp_context.base, alpha);
						InitializeGraphicsMatrixIdentity(&matrix);
						GraphicsPatternSetMatrix(&core->temp_pattern.base, &matrix);
						GraphicsSetSource(&update.base, &core->temp_pattern.base);

						for(i = 0; i < height; i++)
						{
							(void)memset(&canvas->temp_layer->pixels[
								(i + start_y) * canvas->temp_layer->stride + start_x * 4],
								0, stride
							);
						}

						GraphicsMaskSurface(&update.base, &canvas->selection->surface.base,
							-draw_x + r, -draw_y + r);
						GraphicsSetSourceSurface(&update_temp.base, &update_surface.base,
							0, 0, &temp_pattern);
						GraphicsMaskSurface(&update_temp.base, &canvas->active_layer->surface.base,
							-draw_x + r, -draw_y + r);

						mask = canvas->temp_layer->pixels;

						GraphicsSurfaceFinish(&temp_surface.base);
						DestroyGraphicsContext(&update_temp.base);
					}
				}
			}
			else
			{
				GRAPHICS_IMAGE_SURFACE temp_surface = { 0 };
				GRAPHICS_DEFAULT_CONTEXT update_temp = { 0 };
				GRAPHICS_SURFACE_PATTERN surface_pattern = { 0 };

				InitializeGraphicsImageSurfaceForRectangle(&temp_surface, &canvas->temp_layer->surface,
					start_x, start_y, r * 2 + 1, r * 2 + 1);
				InitializeGraphicsDefaultContext(&update_temp, &temp_surface, &core->app->graphics);

				if((canvas->flags & DRAW_WINDOW_HAS_SELECTION_AREA) == 0)
				{
					if((canvas->active_layer->flags & LAYER_LOCK_OPACITY) == 0)
					{
						InitializeGraphicsMatrixScale(&matrix, zoom, zoom);
						GraphicsPatternSetMatrix(&core->brush_pattern.base, &matrix);
						GraphicsSetSource(&update.base, &core->brush_pattern.base);
						GraphicsSetOperator(&update.base, GRAPHICS_OPERATOR_SOURCE);
						GraphicsPaintWithAlpha(&update.base, alpha);
					}
					else
					{
						InitializeGraphicsMatrixScale(&matrix, zoom, zoom);
						GraphicsPatternSetMatrix(&core->brush_pattern.base, &matrix);
						GraphicsSetSource(&core->temp_context.base, &core->brush_pattern.base);
						GraphicsPaintWithAlpha(&core->temp_context.base, alpha);
						InitializeGraphicsMatrixIdentity(&matrix);
						GraphicsPatternSetMatrix(&core->temp_pattern.base, &matrix);
						GraphicsSetSource(&update.base, &core->temp_pattern.base);
						GraphicsMaskSurface(&update.base, &canvas->active_layer->surface.base,
							-draw_x + r, -draw_y + r);
					}

					for(i = 0; i < height; i++)
					{
						(void)memset(&canvas->temp_layer->pixels[
							(i + start_y) * canvas->temp_layer->stride + start_x * 4],
							0, stride
						);
					}

					GraphicsSetSourceSurface(&update_temp.base, &update_surface.base,
						0, 0, &surface_pattern);
					GraphicsMaskSurface(&update_temp.base, &canvas->texture->surface.base,
						-draw_x + r, -draw_y + r);

					mask = canvas->temp_layer->pixels;
				}
				else
				{
					if((canvas->active_layer->flags & LAYER_LOCK_OPACITY) == 0)
					{
						GRAPHICS_SURFACE_PATTERN surface_pattern = { 0 };

						InitializeGraphicsMatrixScale(&matrix, zoom, zoom);
						GraphicsPatternSetMatrix(&core->brush_pattern.base, &matrix);
						GraphicsSetSource(&core->temp_context.base, &core->brush_pattern.base);
						GraphicsPaintWithAlpha(&core->temp_context.base, alpha);
						InitializeGraphicsMatrixIdentity(&matrix);
						GraphicsPatternSetMatrix(&core->temp_pattern.base, &matrix);
						GraphicsSetSource(&update.base, &core->temp_pattern.base);
						GraphicsMaskSurface(&update.base, &canvas->selection->surface.base,
							-draw_x + r, -draw_y + r);

						for(i = 0; i < height; i++)
						{
							(void)memset(&canvas->temp_layer->pixels[
								(i + start_y) * canvas->temp_layer->stride + start_x * 4],
								0, stride
							);
						}

						GraphicsSetSourceSurface(&update_temp.base, &update_surface.base,
							0, 0, &surface_pattern);
						GraphicsMaskSurface(&update_temp.base, &canvas->texture->surface.base,
							-draw_x + r, -draw_y + r);

						mask = canvas->temp_layer->pixels;
					}
					else
					{
						GRAPHICS_SURFACE_PATTERN surface_pattern = { 0 };

						InitializeGraphicsMatrixScale(&matrix, zoom, zoom);
						GraphicsPatternSetMatrix(&core->brush_pattern.base, &matrix);
						GraphicsSetSource(&core->temp_context.base, &core->brush_pattern.base);
						GraphicsPaintWithAlpha(&core->temp_context.base, alpha);
						InitializeGraphicsMatrixIdentity(&matrix);
						GraphicsPatternSetMatrix(&core->temp_pattern.base, &matrix);
						GraphicsSetSource(&update.base, &core->temp_pattern.base);

						for(i = 0; i < height; i++)
						{
							(void)memset(&canvas->temp_layer->pixels[
								(i + start_y) * canvas->temp_layer->stride + start_x * 4],
								0, stride
							);
						}

						GraphicsSetSourceSurface(&update_temp.base, &update_surface.base,
							0, 0, &surface_pattern);
						GraphicsMaskSurface(&update.base, &canvas->selection->surface.base,
							-draw_x + r, -draw_y + r);

						for(i = 0; i < height; i++)
						{
							(void)memset(&canvas->mask_temp->pixels[
								(i + start_y) * canvas->mask_temp->stride + start_x * 4],
								0, stride
							);
						}

						GraphicsSetSourceSurface(&update.base, &temp_surface.base,
							0, 0, &surface_pattern);
						GraphicsMaskSurface(&update.base, &canvas->texture->surface.base,
							-draw_x + r, -draw_y + r);
					}
				}

				GraphicsSurfaceFinish(&temp_surface.base);
				DestroyGraphicsContext(&update_temp.base);
			}

			GraphicsSurfaceFinish(&update_surface.base);
			DestroyGraphicsContext(&update.base);

#ifdef _OPENMP
			if(height <= MINIMUM_PARALLEL_SIZE)
			{
				omp_set_dynamic(FALSE);
				omp_set_num_threads(1);
			}
#pragma omp parallel for firstprivate(width, work_pixel, start_x, start_y)
#endif
			for(i = 0; i < height; i++)
			{
				uint8* ref_pix = &work_pixel[
					(start_y + i) * canvas->work_layer->stride + start_x * 4];
				uint8* mask_pix = &mask[(start_y + i) * canvas->work_layer->stride
					+ start_x * 4];
				int j;

				for(j = 0; j < width; j++, ref_pix += 4, mask_pix += 4)
				{
					if(ref_pix[3] < mask_pix[3])
					{
						ref_pix[0] = (uint8)((uint32)(((int)mask_pix[0] - (int)ref_pix[0])
							* mask_pix[3] >> 8) + ref_pix[0]);
						ref_pix[1] = (uint8)((uint32)(((int)mask_pix[1] - (int)ref_pix[1])
							* mask_pix[3] >> 8) + ref_pix[1]);
						ref_pix[2] = (uint8)((uint32)(((int)mask_pix[2] - (int)ref_pix[2])
							* mask_pix[3] >> 8) + ref_pix[2]);
						ref_pix[3] = (uint8)((((uint32)((int)mask_pix[3] - (int)ref_pix[3])
							* mask_pix[3]) >> 8) + ref_pix[3]);
					}
				}
			}

#ifdef _OPENMP
			omp_set_dynamic(TRUE);
			omp_set_num_threads(core->app->max_threads);
#endif
			// アンチエイリアスを適用
			if((eraser->core.flags & BRUSH_FLAG_ANTI_ALIAS) != 0 && (core->flags & BRUSH_FLAG_USE_OLD_ANTI_ALIAS) == 0)
			{
				ANTI_ALIAS_RECTANGLE range = { start_x - 1, start_y - 1,
				width + 3, height + 3 };
				if((canvas->flags & DRAW_WINDOW_HAS_SELECTION_AREA) == 0
					&& (canvas->active_layer->flags & LAYER_LOCK_OPACITY) == 0)
				{
					OldAntiAliasLayer(canvas->work_layer, canvas->temp_layer, &range, (void*)canvas->app);
				}
				else
				{
					OldAntiAliasLayerWithSelectionOrAlphaLock(canvas->work_layer, canvas->temp_layer,
						(canvas->flags & DRAW_WINDOW_HAS_SELECTION_AREA) == 0 ? NULL : canvas->selection,
						canvas->active_layer, &range, (void*)canvas->app);
				}
			}
		skip_draw:
			dx -= step;
			if(dx < 1)
			{
				break;
			}
			else if(dx >= step)
			{
				draw_x += diff_x, draw_y += diff_y;
			}
			else
			{
				draw_x = x;
				draw_y = y;
			}
		} while(1);

		if((core->flags & BRUSH_FLAG_USE_OLD_ANTI_ALIAS) == 0 && (eraser->core.flags & BRUSH_FLAG_ANTI_ALIAS) != 0)
		{
			//ANTI_ALIAS_RECTANGLE range = {(int)core->min_x - 1, (int)core->min_y - 1,
			//	(int)(core->max_x - core->min_x) + 3, (int)(core->max_y - core->min_y) + 3};
			ANTI_ALIAS_RECTANGLE range;
			if(eraser->core.before_x < x)
			{
				range.x = (int)(eraser->core.before_x - r * 2 - 4);
				range.width = (int)(x - eraser->core.before_x + r * 4 + 4);
			}
			else
			{
				range.x = (int)(x - r * 2 - 4);
				range.width = (int)(eraser->core.before_x - x + r * 4 + 4);
			}
			if(eraser->core.before_y < y)
			{
				range.y = (int)(eraser->core.before_y - r * 2 - 4);
				range.height = (int)(y - eraser->core.before_y + r * 4 + 4);
			}
			else
			{
				range.y = (int)(y - r * 2 - 4);
				range.height = (int)(eraser->core.before_y - y + r * 4 + 4);
			}

			if((canvas->flags & DRAW_WINDOW_HAS_SELECTION_AREA) == 0
				&& (canvas->active_layer->flags & LAYER_LOCK_OPACITY) == 0)
			{
				AntiAliasLayer(canvas->anti_alias, canvas->work_layer, &range, (void*)canvas->app);
			}
			else
			{
				AntiAliasLayerWithSelectionOrAlphaLock(canvas->anti_alias, canvas->work_layer,
					(canvas->flags & DRAW_WINDOW_HAS_SELECTION_AREA) == 0 ? NULL : canvas->selection,
					canvas->active_layer, &range, (void*)canvas->app);
			}

			{
				int width = stride / 4;
#ifdef _OPENMP
#pragma omp parallel for firstprivate(width, height, start_y, start_x, window)
#endif

				for(i = 0; i < height; i++)
				{
					uint8* ref, * src;
					int x;

					src = &canvas->work_layer->pixels[(start_y + i) * canvas->work_layer->stride + start_x * 4];
					ref = &canvas->anti_alias->pixels[(start_y + i) * canvas->work_layer->stride + (width - start_x) * 4];
					for(x = 0; x < width; x++, ref++, src++)
					{
						ref[0] = src[0], ref[1] = src[1], ref[2] = src[2], ref[3] = src[3];
					}
				}
			}
		}

		eraser->core.before_x = x, eraser->core.before_y = y;
		canvas->flags |= DRAW_WINDOW_UPDATE_PART;
	}
}

void EraserReleaseCallBack(
	DRAW_WINDOW* canvas,
	BRUSH_CORE* core,
	EVENT_STATE* state
)
{
	if(state->mouse_key_flag & MOUSE_KEY_FLAG_LEFT)
	{
		ERASER* eraser = (ERASER*)core;
		UPDATE_RECTANGLE update = { 0 };

		if((eraser->core.flags & BRUSH_FLAG_ANTI_ALIAS) != 0)
		{
			if((core->flags & BRUSH_FLAG_USE_OLD_ANTI_ALIAS) == 0)
			{
				ANTI_ALIAS_RECTANGLE range = { (int)core->min_x - 1, (int)core->min_y - 1,
					(int)(core->max_x - core->min_x) + 3, (int)(core->max_y - core->min_y) + 3 };
				AntiAliasLayer(canvas->anti_alias, canvas->work_layer, &range, (void*)canvas->app);
			}
		}

		AddBrushHistory(core, canvas->active_layer);

		InitializeGraphicsImageSurfaceForRectangle(&canvas->update.surface, &canvas->active_layer->surface,
			canvas->update.x, canvas->update.y, canvas->update.width, canvas->update.height);
		InitializeGraphicsDefaultContext(&canvas->update.context.base, &canvas->update.surface.base, &core->app->graphics);

		update.x = canvas->update.x, update.y = canvas->update.y;
		update.width = canvas->update.width, update.height = canvas->update.height;
		InitializeGraphicsImageSurfaceForRectangle(&update.surface, &canvas->active_layer->surface, update.x, update.y, update.width, update.height);
		InitializeGraphicsDefaultContext(&update.context, &update.surface, &canvas->app->graphics);
		canvas->part_layer_blend_functions[canvas->work_layer->layer_mode](
			canvas->work_layer, canvas->active_layer, &update);

		WriteSurfacePngFile("active_layer.png", &canvas->active_layer->surface);

		(void)memset(canvas->work_layer->pixels, 0, canvas->work_layer->stride * canvas->work_layer->height);

		GraphicsSurfaceFinish(&canvas->update.surface.base);
		GraphicsDefaultContextFinish(&canvas->update.context);

		canvas->work_layer->layer_mode = LAYER_BLEND_NORMAL;
	}
}

/*
* LoadEraserData関数
* 消しゴムツールの設定データを読み取る
* 引数
* file			: 初期化ファイル読み取りデータ
* section_name	: 初期化ファイルに鉛筆が記録されてるセクション名
* app			: ブラシ初期化にはアプリケーション管理用データが必要
* 返り値
*	消しゴムデータ 使用終了後にMEM_FREE_FUNC必要
*/
BRUSH_CORE* LoadEraserData(INI_FILE_PTR file, const char* section_name, APPLICATION* app)
{
	ERASER *eraser = (ERASER*)MEM_CALLOC_FUNC(1, sizeof(*eraser));

	InitializeBrushCore(&eraser->core, app);
	LoadDefaultBrushData(&eraser->core, file, section_name, app);

	eraser->core.color = &app->tool_box.color_chooser.rgb;
	eraser->core.back_color = &app->tool_box.color_chooser.back_rgb;

	eraser->core.brush_type = BRUSH_TYPE_ERASER;
	eraser->core.press_function = EraserPressCallBack;
	eraser->core.motion_function = EraserMotionCallBack;
	eraser->core.release_function = EraserReleaseCallBack;
	eraser->core.draw_cursor = EraserDrawCursor;
	eraser->core.brush_data = EraserGUI_New(app, &eraser->core);
	eraser->core.create_detail_ui = CreateEraserDetailUI;
	eraser->core.button_update = EraserButtonUpdate;
	eraser->core.motion_update = EraserMotionUpdate;
	eraser->core.change_zoom = EraserChangeZoom;

	return &eraser->core;
}

/*
* WriteEraserData関数
* 消しゴムツールの設定データを記録する
* 引数
* file			: 書き込みファイル操作用データ
* core			: 記録するツールデータ
* section_name	: 記録するセクション名
*/
void WriteEraserData(INI_FILE_PTR file, BRUSH_CORE* core, const char* section_name)
{
	WriteDefaultBrushData(core, file, section_name, "ERASER");
}

#ifdef __cplusplus
}
#endif
