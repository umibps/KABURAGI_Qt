#include <math.h>
#include <string.h>
#include "perspective_ruler.h"
#include "draw_window.h"
#include "application.h"
#include "text_render.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

/*
* SetPerspectiveRulerClickPoint関数
* 左クリック時にパース定規の設定を行う
* 引数
* ruler	: パース定規設定
* x		: 左クリックしたキャンバスのX座標
* y		: 左クリックしたキャンバスのY座標
*/
void SetPerspectiveRulerClickPoint(PERSPECTIVE_RULER* ruler, FLOAT_T x, FLOAT_T y)
{
	ruler->click_point[0] = x,	ruler->click_point[1] = y;
	switch(ruler->type)
	{
	case PERSPECTIVE_RULER_TYPE_CIRCLE:
		ruler->angle = atan2(y - ruler->start_point[ruler->active_point][0][1],
			x - ruler->start_point[ruler->active_point][0][0]);
		ruler->sin_value = sin(ruler->angle);
		ruler->cos_value = cos(ruler->angle);
		break;
	case PERSPECTIVE_RULER_TYPE_PARALLEL_LINES:
		ruler->angle = atan2(ruler->start_point[ruler->active_point][1][1] - ruler->start_point[ruler->active_point][0][1],
			ruler->start_point[ruler->active_point][1][0] - ruler->start_point[ruler->active_point][0][0]);
		ruler->sin_value = sin(ruler->angle);
		ruler->cos_value = cos(ruler->angle);
		break;
	}
}

/*
* GetPerspenctiveRulerPoint関数
* パース定規の補正後座標を取得する
* 引数
* ruler	: パース定規設定
* in_x	: 補正前X座標
* in_y	: 補正前Y座標
* out_x	: 補正後X座標を入れるアドレス
* out_y	: 補正後Y座標を入れるアドレス
*/
void GetPerspectiveRulerPoint(PERSPECTIVE_RULER* ruler, FLOAT_T in_x, FLOAT_T in_y, FLOAT_T* out_x, FLOAT_T* out_y)
{
	FLOAT_T distance;
	FLOAT_T diff_x, diff_y, diff_x2, diff_y2;
	FLOAT_T angle;

	diff_x = in_x - ruler->click_point[0];
	diff_y = in_y - ruler->click_point[1];

	switch(ruler->type)
	{
	case PERSPECTIVE_RULER_TYPE_CIRCLE:
		diff_x2 = ruler->click_point[0] - ruler->start_point[ruler->active_point][0][0];
		diff_y2 = ruler->click_point[1] - ruler->start_point[ruler->active_point][0][1];
		if(fabs(diff_x2) > fabs(diff_y2))
		{
			if(diff_x2 < 0)
			{
				*out_x = in_x;
				*out_y = ruler->click_point[1] - diff_x * ruler->sin_value;
			}
			else
			{
				*out_x = in_x;
				*out_y = ruler->click_point[1] + diff_x * ruler->sin_value;
			}
		}
		else
		{
			if(diff_y2 < 0)
			{
				*out_x = ruler->click_point[0] - diff_y * ruler->cos_value;
				*out_y = in_y;
			}
			else
			{
				*out_x = ruler->click_point[0] + diff_y * ruler->cos_value;
				*out_y = in_y;
			}
		}
		break;
	case PERSPECTIVE_RULER_TYPE_PARALLEL_LINES:
		angle = atan2(diff_y, diff_x);
		distance = sqrt(diff_x * diff_x + diff_y * diff_y);
		if(fabs(ruler->angle - angle) > M_PI * 0.5)
		{
			*out_x = ruler->click_point[0] - distance * ruler->cos_value;
			*out_y = ruler->click_point[1] - distance * ruler->sin_value;
		}
		else
		{
			*out_x = ruler->click_point[0] + distance * ruler->cos_value;
			*out_y = ruler->click_point[1] + distance * ruler->sin_value;
		}
		break;
	}
}

#define DRAW_LINE_LENGTH 12
#define DRAW_TEXT_SIZE 12

/*
* DisplayPerspectiveRulerMarker関数
* パース定規のマーカーを表示する
* 引数
* canvas	: マーカーを表示するキャンバス
* target	: 表示結果を描画するレイヤー
*/
void DisplayPerspectiveRulerMarker(DRAW_WINDOW* canvas, LAYER* target)
{
	UPDATE_RECTANGLE area;
	FLOAT_T zoom = canvas->zoom * 0.01;
	uint8 text_color[4] = {0, 0, 0, 0xFF};
	char show_text[2] = {0};
	int clear_x, clear_y;
	int clear_width, clear_height;
	int i, j;

	GraphicsSave(&target->context.base);

	// cairo_set_font_size(target->cairo_p, DRAW_TEXT_SIZE);

	switch(canvas->perspective_ruler.type)
	{
	case PERSPECTIVE_RULER_TYPE_CIRCLE:
		for(i=0; i<NUM_PERSPECTIVE_RULER_POINTS; i++)
		{
			// アクティブな点なら赤、そうでなければ青
			if(i == canvas->perspective_ruler.active_point)
			{
#if defined(USE_BGR_COLOR_SPACE) && USE_BGR_COLOR_SPACE != 0
				GraphicsSetSourceRGB(&target->context.base, 0, 1, 1);
				text_color[1] = text_color[2] = 0xFF;
				text_color[0] = 0;
#else
				GraphicsSetSourceRGB(&target->context.base, 1, 1, 0);
				text_color[0] = text_color[1] = 0xFF;
				text_color[2] = 0;
#endif
			}
			else
			{
#if defined(USE_BGR_COLOR_SPACE) && USE_BGR_COLOR_SPACE != 0
				GraphicsSetSourceRGB(&target->context.base, 1, 1, 0);
				text_color[0] = text_color[1] = 0xFF;
				text_color[2] = 0;
#else
				GraphicsSetSourceRGB(&target->context.base, 0, 1, 1);
				text_color[1] = text_color[2] = 0xFF;
				text_color[0] = 0;
#endif
			}

			// 描画エリアのピクセルデータ消去
			clear_x = (int)canvas->perspective_ruler.start_point[i][0][0] - DRAW_LINE_LENGTH;
			clear_width = clear_height = DRAW_LINE_LENGTH * 2;
			if(clear_x < 0)
			{
				clear_width += clear_x;
				clear_x = 0;
			}
			clear_y = (int)canvas->perspective_ruler.start_point[i][0][1] - DRAW_LINE_LENGTH;
			if(clear_y < 0)
			{
				clear_height += clear_y;
				clear_y = 0;
			}

			if(clear_x > canvas->width || clear_y > canvas->height || clear_width <= 0 || clear_height <= 0)
			{
				continue;
			}

			for(j=0; j<clear_height; j++)
			{
				(void)memset(&target->pixels[(int)(clear_y * zoom + j) * canvas->disp_temp->stride
					+ (int)(clear_x * zoom) * 4], 0, clear_width * 4);
			}

			// 十字を引く
			GraphicsMoveTo(&target->context.base, canvas->perspective_ruler.start_point[i][0][0] - DRAW_LINE_LENGTH, canvas->perspective_ruler.start_point[i][0][1]);
			GraphicsLineTo(&target->context.base, canvas->perspective_ruler.start_point[i][0][0] + DRAW_LINE_LENGTH, canvas->perspective_ruler.start_point[i][0][1]);
			GraphicsStroke(&target->context.base);

			GraphicsMoveTo(&target->context.base, canvas->perspective_ruler.start_point[i][0][0], canvas->perspective_ruler.start_point[i][0][1] - DRAW_LINE_LENGTH);
			GraphicsLineTo(&target->context.base, canvas->perspective_ruler.start_point[i][0][0], canvas->perspective_ruler.start_point[i][0][1] + DRAW_LINE_LENGTH);
			GraphicsStroke(&target->context.base);

			area.x = canvas->perspective_ruler.start_point[i][0][0] - DRAW_LINE_LENGTH;
			area.y = canvas->perspective_ruler.start_point[i][0][1] - DRAW_LINE_LENGTH;
			area.width = DRAW_LINE_LENGTH * 2 + DRAW_TEXT_SIZE;
			area.height = DRAW_LINE_LENGTH * 2;
			InitializeGraphicsImageSurfaceForRectangle(&area.surface, &target->surface, area.x, area.y,
									area.width, area.height);
			InitializeGraphicsDefaultContext(&area.context, &area.surface.base, &canvas->app->graphics);

			// 番号を描画
			show_text[0] = '1' + i;
			DrawText(show_text, "DMSans-Black.ttf", target->pixels, canvas->perspective_ruler.start_point[i][0][0] + DRAW_LINE_LENGTH, canvas->perspective_ruler.start_point[i][0][1] - DRAW_LINE_LENGTH,
				target->width, target->height, target->stride, text_color, DRAW_TEXT_SIZE, 1, FALSE, FALSE, NULL, NULL);

			canvas->part_layer_blend_functions[LAYER_BLEND_NORMAL](canvas->disp_temp, canvas->disp_layer, &area);
			GraphicsSurfaceFinish(&area.surface.base);
			GraphicsDefaultContextFinish(&area.context);
		}
		break;
	case PERSPECTIVE_RULER_TYPE_PARALLEL_LINES:
		for(i=0; i<NUM_PERSPECTIVE_RULER_POINTS; i++)
		{
			// アクティブな点なら赤、そうでなければ青
			if(i == canvas->perspective_ruler.active_point)
			{
#if defined(USE_BGR_COLOR_SPACE) && USE_BGR_COLOR_SPACE != 0
				GraphicsSetSourceRGB(&target->context.base, 0, 1, 1);
				text_color[1] = text_color[2] = 0xFF;
				text_color[0] = 0;
#else
				GraphicsSetSourceRGB(&target->context.base, 1, 1, 0);
				text_color[0] = text_color[1] = 0xFF;
				text_color[2] = 0;
#endif
			}
			else
			{
#if defined(USE_BGR_COLOR_SPACE) && USE_BGR_COLOR_SPACE != 0
				GraphicsSetSourceRGB(&target->context.base, 1, 1, 0);
				text_color[0] = text_color[1] = 0xFF;
				text_color[2] = 0;
#else
				GraphicsSetSourceRGB(&target->context.base, 0, 1, 1);
				text_color[1] = text_color[2] = 0xFF;
				text_color[0] = 0;
#endif
			}

			// 描画エリアのピクセルデータ消去
			clear_x = (int)canvas->perspective_ruler.start_point[i][0][0] - DRAW_LINE_LENGTH;
			clear_width = clear_height = DRAW_LINE_LENGTH * 2;
			if(clear_x < 0)
			{
				clear_width += clear_x;
				clear_x = 0;
			}
			clear_y = (int)canvas->perspective_ruler.start_point[i][0][1] - DRAW_LINE_LENGTH;
			if(clear_y < 0)
			{
				clear_height += clear_y;
				clear_y = 0;
			}

			if(clear_x > canvas->width || clear_y > canvas->height || clear_width <= 0 || clear_height <= 0)
			{
				continue;
			}

			for(j=0; j<clear_height; j++)
			{
				(void)memset(&target->pixels[(int)(clear_y * zoom + j) * canvas->disp_temp->stride
					+ (int)(clear_x * zoom) * 4], 0, clear_width * 4);
			}

			clear_x = (int)canvas->perspective_ruler.start_point[i][1][0] - DRAW_LINE_LENGTH;
			clear_width = clear_height = DRAW_LINE_LENGTH * 2;
			if(clear_x < 0)
			{
				clear_width += clear_x;
				clear_x = 0;
			}
			clear_y = (int)canvas->perspective_ruler.start_point[i][1][1] - DRAW_LINE_LENGTH;
			if(clear_y < 0)
			{
				clear_height += clear_y;
				clear_y = 0;
			}

			if(clear_x > canvas->width || clear_y > canvas->height || clear_width <= 0 || clear_height <= 0)
			{
				continue;
			}

			for(j=0; j<clear_height; j++)
			{
				(void)memset(&target->pixels[(int)(clear_y * zoom + j) * canvas->disp_temp->stride
					+ (int)(clear_x * zoom) * 4], 0, clear_width * 4);
			}

			// 十字を引く
			GraphicsMoveTo(&target->context.base, canvas->perspective_ruler.start_point[i][0][0] - DRAW_LINE_LENGTH, canvas->perspective_ruler.start_point[i][0][1]);
			GraphicsLineTo(&target->context.base, canvas->perspective_ruler.start_point[i][0][0] + DRAW_LINE_LENGTH, canvas->perspective_ruler.start_point[i][0][1]);
			GraphicsStroke(&target->context.base);

			GraphicsMoveTo(&target->context.base, canvas->perspective_ruler.start_point[i][0][0], canvas->perspective_ruler.start_point[i][0][1] - DRAW_LINE_LENGTH);
			GraphicsLineTo(&target->context.base, canvas->perspective_ruler.start_point[i][0][0], canvas->perspective_ruler.start_point[i][0][1] + DRAW_LINE_LENGTH);
			GraphicsStroke(&target->context.base);

			GraphicsMoveTo(&target->context.base, canvas->perspective_ruler.start_point[i][1][0] - DRAW_LINE_LENGTH, canvas->perspective_ruler.start_point[i][1][1]);
			GraphicsLineTo(&target->context.base, canvas->perspective_ruler.start_point[i][1][0] + DRAW_LINE_LENGTH, canvas->perspective_ruler.start_point[i][1][1]);
			GraphicsStroke(&target->context.base);

			GraphicsMoveTo(&target->context.base, canvas->perspective_ruler.start_point[i][1][0], canvas->perspective_ruler.start_point[i][1][1] - DRAW_LINE_LENGTH);
			GraphicsLineTo(&target->context.base, canvas->perspective_ruler.start_point[i][1][0], canvas->perspective_ruler.start_point[i][1][1] + DRAW_LINE_LENGTH);
			GraphicsStroke(&target->context.base);

			// 番号を描画
			show_text[0] = '1' + i;
			DrawText(show_text, "DMSans-Black.ttf", target->pixels, canvas->perspective_ruler.start_point[i][0][0] + DRAW_LINE_LENGTH, canvas->perspective_ruler.start_point[i][0][1] - DRAW_LINE_LENGTH,
				target->width, target->height, target->stride, text_color, DRAW_TEXT_SIZE, 1, FALSE, FALSE, NULL, NULL);

			DrawText(show_text, "DMSans-Black.ttf", target->pixels, canvas->perspective_ruler.start_point[i][1][0] + DRAW_LINE_LENGTH, canvas->perspective_ruler.start_point[i][1][1] - DRAW_LINE_LENGTH,
				target->width, target->height, target->stride, text_color, DRAW_TEXT_SIZE, 1, FALSE, FALSE, NULL, NULL);

			// 間の線を描画
			GraphicsMoveTo(&target->context.base, canvas->perspective_ruler.start_point[i][0][0], canvas->perspective_ruler.start_point[i][0][1]);
			GraphicsLineTo(&target->context.base, canvas->perspective_ruler.start_point[i][1][0], canvas->perspective_ruler.start_point[i][1][1]);
			GraphicsStroke(&target->context.base);

			if(canvas->perspective_ruler.start_point[i][0][0] < canvas->perspective_ruler.start_point[i][1][0])
			{
				area.x = canvas->perspective_ruler.start_point[i][0][0] - DRAW_LINE_LENGTH;
				area.width= canvas->perspective_ruler.start_point[i][1][0] - canvas->perspective_ruler.start_point[i][0][0] + DRAW_LINE_LENGTH * 2 + DRAW_TEXT_SIZE;
			}
			else
			{
				area.x = canvas->perspective_ruler.start_point[i][1][0] - DRAW_LINE_LENGTH;
				area.width = canvas->perspective_ruler.start_point[i][0][0] - canvas->perspective_ruler.start_point[i][1][0] + DRAW_LINE_LENGTH * 2 + DRAW_TEXT_SIZE;
			}
			if(canvas->perspective_ruler.start_point[i][0][1] < canvas->perspective_ruler.start_point[i][1][1])
			{
				area.y = canvas->perspective_ruler.start_point[i][0][1] - DRAW_LINE_LENGTH;
				area.height = canvas->perspective_ruler.start_point[i][1][1] - canvas->perspective_ruler.start_point[i][0][1] + DRAW_LINE_LENGTH * 2;
			}
			else
			{
				area.y = canvas->perspective_ruler.start_point[i][1][1] - DRAW_LINE_LENGTH;
				area.height = canvas->perspective_ruler.start_point[i][0][1] - canvas->perspective_ruler.start_point[i][1][1] + DRAW_LINE_LENGTH * 2;
			}
			InitializeGraphicsImageSurfaceForRectangle(&area.surface, &target->surface, area.x, area.y,
								area.width, area.height);
			InitializeGraphicsDefaultContext(&area.context, &area.surface.base, &canvas->app->graphics);

			canvas->part_layer_blend_functions[LAYER_BLEND_NORMAL](canvas->disp_temp, canvas->disp_layer, &area);
			GraphicsSurfaceFinish(&area.surface.base);
			GraphicsDefaultContextFinish(&area.context);
		}
		break;
	}

	GraphicsRestore(&target->context.base);
}

/***********************************************
* DisplayPerspectiveRulerMarkerOnWidget関数	*
* パース定規のマーカーをウィジェットに描画する *
* 引数										 *
* canvas	: マーカーを描画するキャンバス	 *
* context	: 描画用情報					   *
***********************************************/
void DisplayPerspectiveRulerMarkerOnWidget(DRAW_WINDOW* canvas, void* context)
{
/*
	FLOAT_T draw_x[2], draw_y[2];
	FLOAT_T translate_x, translate_y;
	char show_text[2] = {0};
	int i;

	GraphicsSave(context);

	switch(canvas->perspective_ruler.type)
	{
	case PERSPECTIVE_RULER_TYPE_CIRCLE:
		for(i=0; i<NUM_PERSPECTIVE_RULER_POINTS; i++)
		{	// アクティブな点なら赤、そうでなければ青
			if(i == canvas->perspective_ruler.active_point)
			{
				cairo_set_source_rgb((cairo_t*)context, 1, 0, 0);
			}
			else
			{
				cairo_set_source_rgb((cairo_t*)context, 0, 0, 1);
			}

			translate_x = - canvas->width * canvas->zoom_rate * 0.5 + canvas->perspective_ruler.start_point[i][0][0] * canvas->zoom_rate;
			translate_y = - canvas->height * canvas->zoom_rate * 0.5 + canvas->perspective_ruler.start_point[i][0][1] * canvas->zoom_rate;
			draw_x[0] = translate_x * canvas->cos_value + translate_y * canvas->sin_value;
			draw_y[0] = translate_y * canvas->cos_value - translate_x * canvas->sin_value;
			draw_x[0] = draw_x[0] + canvas->disp_size * 0.5;
			draw_y[0] = draw_y[0] + canvas->disp_size * 0.5;

			// 十字を引く
			cairo_move_to((cairo_t*)context, draw_x[0] - DRAW_LINE_LENGTH, draw_y[0]);
			cairo_line_to((cairo_t*)context, draw_x[0] + DRAW_LINE_LENGTH, draw_y[0]);
			cairo_stroke((cairo_t*)context);

			cairo_move_to((cairo_t*)context, draw_x[0], draw_y[0] - DRAW_LINE_LENGTH);
			cairo_line_to((cairo_t*)context, draw_x[0], draw_y[0] + DRAW_LINE_LENGTH);
			cairo_stroke((cairo_t*)context);

			// 番号を描画
			show_text[0] = '1' + i;
			cairo_move_to((cairo_t*)context, draw_x[0] + DRAW_LINE_LENGTH, draw_y[0] - DRAW_LINE_LENGTH);
			cairo_show_text((cairo_t*)context, show_text);
		}
		break;
	case PERSPECTIVE_RULER_TYPE_PARALLEL_LINES:
		for(i=0; i<NUM_PERSPECTIVE_RULER_POINTS; i++)
		{	// アクティブな点なら赤、そうでなければ青
			if(i == canvas->perspective_ruler.active_point)
			{
				cairo_set_source_rgb((cairo_t*)context, 1, 0, 0);
			}
			else
			{
				cairo_set_source_rgb((cairo_t*)context, 0, 0, 1);
			}

			translate_x = - canvas->width * canvas->zoom_rate * 0.5 + canvas->perspective_ruler.start_point[i][0][0] * canvas->zoom_rate;
			translate_y = - canvas->height * canvas->zoom_rate * 0.5 + canvas->perspective_ruler.start_point[i][0][1] * canvas->zoom_rate;
			draw_x[0] = translate_x * canvas->cos_value + translate_y * canvas->sin_value;
			draw_y[0] = translate_y * canvas->cos_value - translate_x * canvas->sin_value;
			draw_x[0] = draw_x[0] + canvas->disp_size * 0.5;
			draw_y[0] = draw_y[0] + canvas->disp_size * 0.5;

			// 十字を引く
			cairo_move_to((cairo_t*)context, draw_x[0] - DRAW_LINE_LENGTH, draw_y[0]);
			cairo_line_to((cairo_t*)context, draw_x[0] + DRAW_LINE_LENGTH, draw_y[0]);
			cairo_stroke((cairo_t*)context);

			cairo_move_to((cairo_t*)context, draw_x[0], draw_y[0] - DRAW_LINE_LENGTH);
			cairo_line_to((cairo_t*)context, draw_x[0], draw_y[0] + DRAW_LINE_LENGTH);
			cairo_stroke((cairo_t*)context);

			// 番号を描画
			show_text[0] = '1' + i;
			cairo_move_to((cairo_t*)context, draw_x[0] + DRAW_LINE_LENGTH, draw_y[0] - DRAW_LINE_LENGTH);
			cairo_show_text((cairo_t*)context, show_text);

			translate_x = - canvas->width * canvas->zoom_rate * 0.5 + canvas->perspective_ruler.start_point[i][1][0] * canvas->zoom_rate;
			translate_y = - canvas->height * canvas->zoom_rate * 0.5 + canvas->perspective_ruler.start_point[i][1][1] * canvas->zoom_rate;
			draw_x[1] = translate_x * canvas->cos_value + translate_y * canvas->sin_value;
			draw_y[1] = translate_y * canvas->cos_value - translate_x * canvas->sin_value;
			draw_x[1] = draw_x[1] + canvas->disp_size * 0.5;
			draw_y[1] = draw_y[1] + canvas->disp_size * 0.5;

			// 十字を引く
			cairo_move_to((cairo_t*)context, draw_x[1] - DRAW_LINE_LENGTH, draw_y[1]);
			cairo_line_to((cairo_t*)context, draw_x[1] + DRAW_LINE_LENGTH, draw_y[1]);
			cairo_stroke((cairo_t*)context);

			cairo_move_to((cairo_t*)context, draw_x[1], draw_y[1] - DRAW_LINE_LENGTH);
			cairo_line_to((cairo_t*)context, draw_x[1], draw_y[1] + DRAW_LINE_LENGTH);
			cairo_stroke((cairo_t*)context);

			// 番号を描画
			// show_text[0] = '1' + i;
			cairo_move_to((cairo_t*)context, draw_x[1] + DRAW_LINE_LENGTH, draw_y[1] - DRAW_LINE_LENGTH);
			cairo_show_text((cairo_t*)context, show_text);

			// 間の線を描画
			cairo_move_to((cairo_t*)context, draw_x[0], draw_y[0]);
			cairo_line_to((cairo_t*)context, draw_x[1], draw_y[1]);
			cairo_stroke((cairo_t*)context);
		}
		break;
	}

	cairo_restore((cairo_t*)context);
*/
}

#ifdef __cplusplus
}
#endif
