
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "application.h"
#include "text_layer.h"
#include "text_render.h"
#include "utils.h"
#include "bezier.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_ARROW_WIDTH (M_PI/12)

/*
* CreateTextLayer関数
* テキストレイヤーのデータメモリ確保と初期化
* 引数
* window			: キャンバスの情報
* x				 : レイヤーのX座標
* y				 : レイヤーのY座標
* width			 : レイヤーの幅
* height			: レイヤーの高さ
* base_size		 : 文字サイズの倍率
* font_size		 : 文字サイズ
* font_file_path	: TrueTypeフォントファイルパス
* color			 : 文字色
* balloon_type	  : 吹き出しのタイプ
* back_color		: 吹き出しの背景色
* line_color		: 吹き出しの線の色
* line_width		: 吹き出しの線の太さ
* balloon_data  	: 吹き出し形状の詳細データ
* flags			 : テキスト表示のフラグ
* 返り値
*	初期化された構造体のアドレス
*/
TEXT_LAYER* CreateTextLayer(
	void* window,
	FLOAT_T x,
	FLOAT_T y,
	FLOAT_T width,
	FLOAT_T height,
	int base_size,
	FLOAT_T font_size,
	const char* font_file_path,
	const uint8 color[3],
	uint16 balloon_type,
	const uint8 back_color[4],
	const uint8 line_color[4],
	FLOAT_T line_width,
	TEXT_LAYER_BALLOON_DATA* balloon_data,
	uint32 flags
)
{
	TEXT_LAYER *ret = (TEXT_LAYER*)MEM_ALLOC_FUNC(sizeof(*ret));
	DRAW_WINDOW *canvas = (DRAW_WINDOW*)window;
	FLOAT_T center_x, center_y;
	FLOAT_T angle;
	FLOAT_T edge_point[2];

	(void)memset(ret, 0, sizeof(*ret));

	ret->x = x;
	ret->y = y;
	ret->width = width;
	ret->height = height;
	ret->base_size = base_size;
	ret->font_size = font_size;
	ret->font_file_path = font_file_path;
	if(canvas != NULL)
	{
		ret->edge_position[0][0] = canvas->width * 0.5;
		ret->edge_position[0][1] = canvas->height * 0.5;
	}
	(void)memcpy(ret->color, color, 3);
	(void)memcpy(ret->back_color, back_color, 4);
	(void)memcpy(ret->line_color, line_color, 4);
	ret->balloon_type = balloon_type;
	if(balloon_data != NULL)
	{
		ret->balloon_data = *balloon_data;
	}
	ret->line_width = line_width;
	ret->flags = flags;

	center_x = x + width * 0.5;
	center_y = y + height * 0.5;
	angle = atan2(center_y - ret->edge_position[0][1], center_x - ret->edge_position[0][0]) + M_PI;

	ret->arc_start = angle + DEFAULT_ARROW_WIDTH;
	ret->arc_end = angle - DEFAULT_ARROW_WIDTH + 2 * M_PI;

	TextLayerGetBalloonArcPoint(ret, ret->arc_start, edge_point);
	ret->edge_position[1][0] = (edge_point[0] + ret->edge_position[0][0]) * 0.5;
	ret->edge_position[1][1] = (edge_point[1] + ret->edge_position[0][1]) * 0.5;

	TextLayerGetBalloonArcPoint(ret, ret->arc_end, edge_point);
	ret->edge_position[2][0] = (edge_point[0] + ret->edge_position[0][0]) * 0.5;
	ret->edge_position[2][1] = (edge_point[1] + ret->edge_position[0][1]) * 0.5;

	return ret;
}

/*
* DeleteTextLayer関数
* テキストレイヤーのデータを削除する	
* 引数
* layer	: テキストレイヤーのデータポインタのアドレス
*/
void DeleteTextLayer(TEXT_LAYER** layer)
{
	MEM_FREE_FUNC((*layer)->drag_points);
	MEM_FREE_FUNC((*layer)->text);
	MEM_FREE_FUNC(*layer);

	*layer = NULL;
}

void TextLayerButtonPressCallBack(
	DRAW_WINDOW* canvas,
	const EVENT_STATE* event_info
)
{
	if(event_info->mouse_key_flag & MOUSE_KEY_FLAG_LEFT)
	{
		TEXT_LAYER *layer = canvas->active_layer->layer_data.text_layer;
		FLOAT_T points[4][2];
		FLOAT_T compare = TEXT_LAYER_SELECT_POINT_DISTANCE * canvas->rev_zoom;
		int i;

		points[0][0] = layer->x;
		points[0][1] = layer->y;
		points[1][0] = layer->x;
		points[1][1] = layer->y + layer->height;
		points[2][0] = layer->x + layer->width;
		points[2][1] = layer->y + layer->height;
		points[3][0] = layer->x + layer->width;
		points[3][1] = layer->y;

		for(i=0; i<4; i++)
		{
			if(fabs(points[i][0] - event_info->cursor_x) <= compare
				&& fabs(points[i][1] - event_info->cursor_y) <= compare)
			{
				layer->drag_points = (TEXT_LAYER_POINTS*)MEM_REALLOC_FUNC(
					layer->drag_points, sizeof(*layer->drag_points));
				layer->drag_points->point_index = i;
				(void)memcpy(layer->drag_points->points, points, sizeof(points));
				return;
			}
		}

		if(layer->back_color[3] != 0 ||
			(layer->flags & TEXT_LAYER_BALLOON_HAS_EDGE) != 0)
		{
			FLOAT_T diff_x, diff_y;
			FLOAT_T center_x, center_y;
			FLOAT_T edge_position[2];

			diff_x = layer->edge_position[0][0] - event_info->cursor_x;
			diff_y = layer->edge_position[0][1] - event_info->cursor_y;

			center_x = layer->x + layer->width * 0.5;
			center_y = layer->y + layer->height * 0.5;

			if(diff_x*diff_x + diff_y*diff_y < TEXT_LAYER_SELECT_POINT_DISTANCE * TEXT_LAYER_SELECT_POINT_DISTANCE)
			{
				layer->drag_points = (TEXT_LAYER_POINTS*)MEM_REALLOC_FUNC(
					layer->drag_points, sizeof(*layer->drag_points));
				layer->drag_points->point_index = 4;
				(void)memcpy(layer->drag_points->points, points, sizeof(points));
				return;
			}

			TextLayerGetBalloonArcPoint(layer, layer->arc_start, edge_position);
			diff_x = edge_position[0] - event_info->cursor_x;
			diff_y = edge_position[1] - event_info->cursor_y;
			if(diff_x*diff_x + diff_y*diff_y < TEXT_LAYER_SELECT_POINT_DISTANCE * TEXT_LAYER_SELECT_POINT_DISTANCE)
			{
				layer->drag_points = (TEXT_LAYER_POINTS*)MEM_REALLOC_FUNC(
					layer->drag_points, sizeof(*layer->drag_points));
				layer->drag_points->point_index = 5;
				(void)memcpy(layer->drag_points->points, points, sizeof(points));
				return;
			}

			TextLayerGetBalloonArcPoint(layer, layer->arc_end, edge_position);
			diff_x = edge_position[0] - event_info->cursor_x;
			diff_y = edge_position[1] - event_info->cursor_y;
			if(diff_x*diff_x + diff_y*diff_y < TEXT_LAYER_SELECT_POINT_DISTANCE * TEXT_LAYER_SELECT_POINT_DISTANCE)
			{
				layer->drag_points = (TEXT_LAYER_POINTS*)MEM_REALLOC_FUNC(
					layer->drag_points, sizeof(*layer->drag_points));
				layer->drag_points->point_index = 6;
				(void)memcpy(layer->drag_points->points, points, sizeof(points));
				return;
			}

			diff_x = layer->edge_position[1][0] - event_info->cursor_x;
			diff_y = layer->edge_position[1][1] - event_info->cursor_y;
			if(diff_x*diff_x + diff_y*diff_y < TEXT_LAYER_SELECT_POINT_DISTANCE * TEXT_LAYER_SELECT_POINT_DISTANCE)
			{
				layer->drag_points = (TEXT_LAYER_POINTS*)MEM_REALLOC_FUNC(
					layer->drag_points, sizeof(*layer->drag_points));
				layer->drag_points->point_index = 7;
				(void)memcpy(layer->drag_points->points, points, sizeof(points));
				return;
			}

			diff_x = layer->edge_position[2][0] - event_info->cursor_x;
			diff_y = layer->edge_position[2][1] - event_info->cursor_y;
			if(diff_x*diff_x + diff_y*diff_y < TEXT_LAYER_SELECT_POINT_DISTANCE * TEXT_LAYER_SELECT_POINT_DISTANCE)
			{
				layer->drag_points = (TEXT_LAYER_POINTS*)MEM_REALLOC_FUNC(
					layer->drag_points, sizeof(*layer->drag_points));
				layer->drag_points->point_index = 8;
				(void)memcpy(layer->drag_points->points, points, sizeof(points));
				return;
			}
		}
	}
}

void RenderTextLayer(DRAW_WINDOW* window, LAYER* target, TEXT_LAYER* layer)
{
#define RIGHT_MOVE_AMOUNT 0.3f
#define UPPER_MOVE_AMOUNT 0.3f
	FLOAT_T draw_y;
	char *show_text, *str;
	size_t length, i;
	int draw_width = 0, draw_height = 0;
	FLOAT_T max_size = 0;
	
	GRAPHICS_SURFACE_PATTERN pattern = {0};

	(void)memset(window->mask_temp->pixels, 0, window->pixel_buf_size);
	(void)memset(window->temp_layer->pixels, 0, window->pixel_buf_size);
	(void)memset(target->pixels, 0, target->stride*target->height);

	if(layer->back_color[3] != 0
		|| (layer->flags & TEXT_LAYER_BALLOON_HAS_EDGE) != 0)
	{
		window->app->draw_balloon_functions[layer->balloon_type](layer, target, window);
	}

	if(layer->text == NULL)
	{
		return;
	}

	length = strlen(layer->text);

	GraphicsSave(&window->mask_temp->context.base);

	if((layer->flags & TEXT_LAYER_VERTICAL) == 0)
	{
		DrawText(layer->text, layer->font_file_path, window->mask_temp->pixels, layer->x, layer->y,
					window->mask_temp->width, window->mask_temp->height, window->mask_temp->stride,
					layer->color, layer->font_size, 1, layer->flags & TEXT_LAYER_BOLD, layer->flags & TEXT_LAYER_ITALIC,
					&draw_width, &draw_height);
	}
	else
	{
		/*
		gdouble draw_x = layer->x + layer->width - layer->font_size;
		gchar character_buffer[8];
		guint character_size, j;
		char* next_char;
		uint32 utf8_code;
		gdouble right_move, upper_move, rotate;
		gdouble y_move = 0;

		show_text = str = layer->text;
		draw_y = layer->y;
		draw_width = layer->font_size;

		for(i=0; i<length; i++)
		{
			right_move = upper_move = rotate = 0;

			if(show_text[i] == '\n')
			{
				draw_y = layer->y;
				draw_x -= layer->font_size;
				draw_width += layer->font_size;

				if(max_size < draw_height)
				{
					max_size = draw_height;
				}
				draw_height = 0;
			}
			else if(show_text[i] >= 0x20 && show_text[i] <= 0x7E)
			{
				character_buffer[0] = show_text[i];
				character_buffer[1] = '\0';

				cairo_move_to(window->mask_temp->cairo_p,
					draw_x, draw_y);
				cairo_rotate(window->mask_temp->cairo_p, (FLOAT_T)G_PI*0.5);
				cairo_show_text(window->mask_temp->cairo_p, character_buffer);
				cairo_rotate(window->mask_temp->cairo_p, - (FLOAT_T)G_PI*0.5);
				draw_y += layer->font_size * 0.7f;
			}
			else
			{
				str = (char*)(&utf8_code);
				next_char = g_utf8_next_char(&show_text[i]);
				character_size = (guint)(next_char - &show_text[i]);
				for(j=0; j<sizeof(character_buffer)/sizeof(character_buffer[0]); j++)
				{
					character_buffer[j] = 0;
				}
				for(j=0; j<character_size; j++)
				{
					character_buffer[j] = show_text[i+j];
				}
				character_buffer[character_size] = '\0';
				i += character_size - 1;

				utf8_code = 0;
#ifndef __BIG_ENDIAN
				for(j=0; j<character_size; j++)
				{
					str[character_size-j-1] = character_buffer[j];
				}
#else
				for(j=0; j<character_size; j++)
				{
					str[sizeof(guint)-character_size+j] = character_buffer[j];
				}
#endif
				if(
					// 「ぁ」「ぃ」「ぅ」「ぇ」「ぉ」
						// 「っ」「ゃ」「ゅ」「ょ」「ゎ」
					utf8_code == 0xE38181
					|| utf8_code == 0xE38183
					|| utf8_code == 0xE38185
					|| utf8_code == 0xE38187
					|| utf8_code == 0xE38189
					|| utf8_code == 0xE381A3
					|| utf8_code == 0xE38283
					|| utf8_code == 0xE38285
					|| utf8_code == 0xE38287
					|| utf8_code == 0xE3828E
					// 「ァ」「ィ」「ゥ」「ェ」「ォ」
						// 「ッ」「ャ」「ュ」「ョ」「ヮ」
					|| utf8_code == 0xE382A1
					|| utf8_code == 0xE382A3
					|| utf8_code == 0xE382A5
					|| utf8_code == 0xE382A7
					|| utf8_code == 0xE382A9
					|| utf8_code == 0xE38383
					|| utf8_code == 0xE383A3
					|| utf8_code == 0xE383A5
					|| utf8_code == 0xE383A7
					|| utf8_code == 0xE383AE
					// 「ヵ」「ヶ」
					|| utf8_code == 0xE383B5
					|| utf8_code == 0xE383B6
				)
				{
					right_move = layer->font_size * RIGHT_MOVE_AMOUNT;
					upper_move = layer->font_size * UPPER_MOVE_AMOUNT;
					draw_x += right_move;
					draw_y -= upper_move;
					draw_height -= upper_move;
					upper_move *= 0.5;
					y_move = layer->font_size * 1.2f;
				}
				else if(
					// 「、」「。」
					utf8_code == 0xE38081 || utf8_code == 0xE38082
				)
				{
					right_move = layer->font_size * RIGHT_MOVE_AMOUNT * 2;
					upper_move = layer->font_size * UPPER_MOVE_AMOUNT * 2;
					draw_x += right_move;
					draw_y -= upper_move;
					draw_height -= upper_move;
					upper_move *= 0.25;
					y_move = layer->font_size * 1.2f;
				}
				else if(
					// 「：」
					utf8_code == 0xEFBC9A
					// 「‥」
					|| utf8_code == 0xE280A5
					// 「…」
					|| utf8_code == 0xE280A6
					// 「ー」
					|| utf8_code == 0xE383BC
					// 「―」
					|| utf8_code == 0xE28095
					// 「～」
					|| utf8_code == 0xE3809C
					// 「（）」、「〔〕」、「［］」、「〈〉」、「《》」、
						// 「「」」、「『』」、「【】」
					|| utf8_code == 0xEFBC88
					|| utf8_code == 0xEFBC89
					|| utf8_code == 0xE38094
					|| utf8_code == 0xE38095
					|| utf8_code == 0xEFBCBB
					|| utf8_code == 0xEFBCBD
					|| utf8_code == 0xEFBD9B
					|| utf8_code == 0xEFBD9D
					|| utf8_code == 0xE38088
					|| utf8_code == 0xE38089
					|| utf8_code == 0xE3808A
					|| utf8_code == 0xE3808B
					|| utf8_code == 0xE3808C
					|| utf8_code == 0xE3808D
					|| utf8_code == 0xE3808E
					|| utf8_code == 0xE3808F
					|| utf8_code == 0xE38090
					|| utf8_code == 0xE38091
				)
				{
					right_move = layer->font_size * RIGHT_MOVE_AMOUNT * 0.5;
					upper_move = layer->font_size * UPPER_MOVE_AMOUNT * 3;
					rotate = (FLOAT_T)M_PI * 0.5;
					draw_x += right_move;
					draw_y -= upper_move;
					draw_height -= upper_move;
					upper_move *= - 0.75;
					y_move = layer->font_size * 1.2f;
				}
				else
				{
					// 文字サイズを取得してX座標を修正
					cairo_text_extents_t character_info;
					cairo_text_extents(window->mask_temp->cairo_p, character_buffer, &character_info);
					if(layer->font_size > character_info.width + character_info.x_bearing)
					{
						right_move = (layer->font_size - character_info.width) * 0.5;
					}
					else
					{
						right_move = ((character_info.width + character_info.x_bearing) - layer->font_size) * 0.5;
					}

					right_move -= character_info.x_bearing;
					draw_x += right_move;
					y_move = character_info.height * 1.2;
				}

				cairo_move_to(window->mask_temp->cairo_p,
					draw_x - layer->font_size * 0.2f, draw_y + layer->font_size);
				cairo_rotate(window->mask_temp->cairo_p, rotate);
				cairo_show_text(window->mask_temp->cairo_p, character_buffer);
				cairo_rotate(window->mask_temp->cairo_p, - rotate);

				draw_x -= right_move;

				draw_y += y_move - upper_move;
				draw_height += layer->font_size * 1.2f - upper_move;
			}	// for(i=0; i<length; i++)
		}

		if(max_size < draw_height)
		{
			max_size = draw_height;
		}

		draw_height = max_size;
		*/
	}

	if((layer->flags & TEXT_LAYER_ADJUST_RANGE_TO_TEXT) != 0)
	{
		layer->width = draw_width;
		layer->height = draw_height;
	}

	GraphicsSetSourceRGB(&window->temp_layer->context.base, layer->color[0]*DIV_PIXEL,
		layer->color[1]*DIV_PIXEL, layer->color[2]*DIV_PIXEL);
	GraphicsRectangle(&window->temp_layer->context.base, layer->x, layer->y, layer->width, layer->height);
	GraphicsFill(&window->temp_layer->context.base);
	if((layer->flags & (TEXT_LAYER_CENTERING_HORIZONTALLY | TEXT_LAYER_CENTERING_VERTICALLY)) == 0)
	{
		InitializeGraphicsPatternForSurface(&pattern, &window->mask_temp->surface.base);
		GraphicsMaskSurface(&target->context.base, &window->temp_layer->surface.base, 0, 0);
	}
	else
	{
		GRAPHICS_MATRIX trans_matrix;
		FLOAT_T draw_x, draw_y;

		if((layer->flags & TEXT_LAYER_VERTICAL) == 0)
		{
			if((layer->flags & TEXT_LAYER_CENTERING_HORIZONTALLY) != 0)
			{
				draw_x = - layer->width * 0.5 + draw_width * 0.5;
			}
			else
			{
				draw_x = 0;
			}
			if((layer->flags & TEXT_LAYER_CENTERING_VERTICALLY) != 0)
			{
				draw_y = - layer->height * 0.5 + draw_height * 0.5;
			}
			else
			{
				draw_y = 0;
			}
		}
		else
		{
			if((layer->flags & TEXT_LAYER_CENTERING_HORIZONTALLY) != 0)
			{
				draw_x = (layer->width - draw_width - layer->font_size * 0.5) * 0.5;
			}
			else
			{
				draw_x = 0;
			}
			if((layer->flags & TEXT_LAYER_CENTERING_VERTICALLY) != 0)
			{
				draw_y = - layer->height * 0.5 + draw_height * 0.5;
			}
			else
			{
				draw_y = 0;
			}
		}

		InitializeGraphicsMatrixTranslate(&trans_matrix, draw_x, draw_y);
		InitializeGraphicsPatternForSurface(&pattern, &window->mask_temp->surface.base);
		GraphicsPatternSetMatrix(&pattern.base, &trans_matrix);

		GraphicsSetSource(&target->context.base, &pattern.base);
		GraphicsMaskSurface(&target->context.base, &window->temp_layer->surface.base, 0, 0);

		DestroyGraphicsPattern(&pattern.base);
	}

	GraphicsRestore(&window->mask_temp->context.base);
}

void TextLayerMotionCallBack(
	DRAW_WINDOW* canvas,
	EVENT_STATE* event_state
)
{
	TEXT_LAYER* layer = canvas->active_layer->layer_data.text_layer;

	if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_LEFT) != 0 && layer->drag_points != NULL)
	{
		FLOAT_T x_min, y_min, x_max, y_max;
		FLOAT_T edge_position[2];
		int i;

		layer->drag_points->points[layer->drag_points->point_index][0] = event_state->cursor_x;
		layer->drag_points->points[layer->drag_points->point_index][1] = event_state->cursor_y;

		switch(layer->drag_points->point_index)
		{
		case 0:
			layer->drag_points->points[1][0] = event_state->cursor_x;
			layer->drag_points->points[3][1] = event_state->cursor_y;
			break;
		case 1:
			layer->drag_points->points[0][0] = event_state->cursor_x;
			layer->drag_points->points[2][1] = event_state->cursor_y;
			break;
		case 2:
			layer->drag_points->points[3][0] = event_state->cursor_x;
			layer->drag_points->points[1][1] = event_state->cursor_y;
			break;
		case 3:
			layer->drag_points->points[2][0] = event_state->cursor_x;
			layer->drag_points->points[0][1] = event_state->cursor_y;
			break;
		}

		x_min = x_max = layer->drag_points->points[0][0];
		y_min = y_max = layer->drag_points->points[0][1];
		for(i=1; i<4; i++)
		{
			if(x_min > layer->drag_points->points[i][0])
			{
				x_min = layer->drag_points->points[i][0];
			}
			if(x_max < layer->drag_points->points[i][0])
			{
				x_max = layer->drag_points->points[i][0];
			}
			if(y_min > layer->drag_points->points[i][1])
			{
				y_min = layer->drag_points->points[i][1];
			}
			if(y_max < layer->drag_points->points[i][1])
			{
				y_max = layer->drag_points->points[i][1];
			}
		}

		if(x_min < 0)
		{
			x_min = 0;
		}
		else if(x_min > canvas->width)
		{
			x_min = canvas->width;
		}
		if(x_max < 0)
		{
			x_max = 0;
		}
		else if(x_max > canvas->width)
		{
			x_max = canvas->width;
		}
		if(y_min < 0)
		{
			y_min = 0;
		}
		else if(y_min > canvas->height)
		{
			y_min = canvas->height;
		}
		if(y_max < 0)
		{
			y_max = 0;
		}
		else if(y_max > canvas->height)
		{
			y_max = canvas->height;
		}

		switch(layer->drag_points->point_index)
		{
		case 4:
			{
				FLOAT_T center_x,	center_y;
				FLOAT_T angle;

				layer->edge_position[0][0] = event_state->cursor_x;
				layer->edge_position[0][1] = event_state->cursor_y;

				center_x = layer->x + layer->width * 0.5;
				center_y = layer->y + layer->height * 0.5;
				angle = atan2(center_y - layer->edge_position[0][1], center_x - layer->edge_position[0][0]) + M_PI;

				layer->arc_start = angle + DEFAULT_ARROW_WIDTH;
				layer->arc_end = angle - DEFAULT_ARROW_WIDTH + 2 * M_PI;

				TextLayerGetBalloonArcPoint(layer, layer->arc_start, edge_position);
				layer->edge_position[1][0] = (edge_position[0] + layer->edge_position[0][0]) * 0.5;
				layer->edge_position[1][1] = (edge_position[1] + layer->edge_position[0][1]) * 0.5;

				TextLayerGetBalloonArcPoint(layer, layer->arc_end, edge_position);
				layer->edge_position[2][0] = (edge_position[0] + layer->edge_position[0][0]) * 0.5;
				layer->edge_position[2][1] = (edge_position[1] + layer->edge_position[0][1]) * 0.5;
			}
			break;
		case 5:
			{
				FLOAT_T center_x,	center_y;
				FLOAT_T angle;

				center_x = layer->x + layer->width * 0.5;
				center_y = layer->y + layer->height * 0.5;
				angle = atan2(center_y - event_state->cursor_y, center_x - event_state->cursor_x) + M_PI;
				layer->arc_start = angle;

				TextLayerGetBalloonArcPoint(layer, layer->arc_start, edge_position);
				layer->edge_position[1][0] = (edge_position[0] + layer->edge_position[0][0]) * 0.5;
				layer->edge_position[1][1] = (edge_position[1] + layer->edge_position[0][1]) * 0.5;

				TextLayerGetBalloonArcPoint(layer, layer->arc_end, edge_position);
				layer->edge_position[2][0] = (edge_position[0] + layer->edge_position[0][0]) * 0.5;
				layer->edge_position[2][1] = (edge_position[1] + layer->edge_position[0][1]) * 0.5;
			}
			break;
		case 6:
			{
				FLOAT_T center_x,	center_y;
				FLOAT_T angle;

				center_x = layer->x + layer->width * 0.5;
				center_y = layer->y + layer->height * 0.5;
				angle = atan2(center_y - event_state->cursor_y, center_x - event_state->cursor_x) + M_PI;
				layer->arc_end = angle;

				TextLayerGetBalloonArcPoint(layer, layer->arc_start, edge_position);
				layer->edge_position[1][0] = (edge_position[0] + layer->edge_position[0][0]) * 0.5;
				layer->edge_position[1][1] = (edge_position[1] + layer->edge_position[0][1]) * 0.5;

				TextLayerGetBalloonArcPoint(layer, layer->arc_end, edge_position);
				layer->edge_position[2][0] = (edge_position[0] + layer->edge_position[0][0]) * 0.5;
				layer->edge_position[2][1] = (edge_position[1] + layer->edge_position[0][1]) * 0.5;
			}
			break;
		case 7:
			layer->edge_position[1][0] = event_state->cursor_x,	layer->edge_position[1][1] = event_state->cursor_y;
			break;
		case 8:
			layer->edge_position[2][0] = event_state->cursor_x,	layer->edge_position[2][1] = event_state->cursor_y;
			break;
		}

		layer->x = x_min;
		layer->y = y_min;
		layer->width = x_max - x_min;
		layer->height = y_max - y_min;

		canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_OVER;
	}
}

void TextLayerButtonReleaseCallBack(
	DRAW_WINDOW* canvas,
	EVENT_STATE* event_state
)
{
	TEXT_LAYER* layer = canvas->active_layer->layer_data.text_layer;

	if(event_state->mouse_key_flag & MOUSE_KEY_FLAG_LEFT)
	{
		MEM_FREE_FUNC(layer->drag_points);
		layer->drag_points = NULL;
	}
}

void DisplayTextLayerRange(DRAW_WINDOW* window, TEXT_LAYER* layer)
{
	FLOAT_T points[4][2];
	const FLOAT_T select_range = TEXT_LAYER_SELECT_POINT_DISTANCE;
	int i;

	GraphicsSetLineWidth(&window->disp_temp->context.base,  1);
	GraphicsSetSourceRGB(&window->disp_temp->context.base, 1, 1, 1);
	GraphicsRectangle(&window->disp_temp->context.base, layer->x * window->zoom_rate,
		layer->y * window->zoom_rate, layer->width * window->zoom_rate,
		layer->height * window->zoom_rate);
	GraphicsStroke(&window->disp_temp->context.base);
	
	points[0][0] = layer->x * window->zoom_rate;
	points[0][1] = layer->y * window->zoom_rate;
	points[1][0] = layer->x * window->zoom_rate;
	points[1][1] = (layer->y + layer->height) * window->zoom_rate;
	points[2][0] = (layer->x + layer->width) * window->zoom_rate;
	points[2][1] = (layer->y + layer->height) * window->zoom_rate;
	points[3][0] = (layer->x + layer->width) * window->zoom_rate;
	points[3][1] = layer->y * window->zoom_rate;

	for(i=0; i<4; i++)
	{
		GraphicsRectangle(&window->disp_temp->context.base, points[i][0] - select_range,
			points[i][1] - select_range, select_range * 2, select_range * 2);
		GraphicsStroke(&window->disp_temp->context.base);
	}

	if((layer->flags & TEXT_LAYER_BALLOON_HAS_EDGE) != 0)
	{
		FLOAT_T center_x,	center_y;
		FLOAT_T edge_start_point[2];

		GraphicsArc(&window->disp_temp->context.base, layer->edge_position[0][0] * window->zoom_rate,
			layer->edge_position[0][1] * window->zoom_rate, select_range, 0, 2 * M_PI);
		GraphicsStroke(&window->disp_temp->context.base);

		GraphicsArc(&window->disp_temp->context.base, layer->edge_position[1][0] * window->zoom_rate,
			layer->edge_position[1][1] * window->zoom_rate, select_range, 0, 2 * M_PI);
		GraphicsStroke(&window->disp_temp->context.base);

		GraphicsArc(&window->disp_temp->context.base, layer->edge_position[2][0] * window->zoom_rate,
			layer->edge_position[2][1] * window->zoom_rate, select_range, 0, 2 * M_PI);
		GraphicsStroke(&window->disp_temp->context.base);

		center_x = layer->x + layer->width * 0.5;
		center_y = layer->y + layer->height * 0.5;

		TextLayerGetBalloonArcPoint(layer, layer->arc_start, edge_start_point);
		GraphicsArc(&window->disp_temp->context.base, edge_start_point[0] * window->zoom_rate,
			edge_start_point[1] * window->zoom_rate, select_range, 0, 2 * M_PI);
		GraphicsStroke(&window->disp_temp->context.base);

		TextLayerGetBalloonArcPoint(layer, layer->arc_end, edge_start_point);
		GraphicsArc(&window->disp_temp->context.base, edge_start_point[0] * window->zoom_rate,
			edge_start_point[1] * window->zoom_rate, select_range, 0, 2 * M_PI);
		GraphicsStroke(&window->disp_temp->context.base);
	}
}

/*
* TextLayerGetBalloonArcPoint関数
* 吹き出しの尖り開始座標を取得する
* layer	: テキストレイヤーの情報
* angle	: 吹き出しの中心に対する尖り開始位置の角度
* point	: 座標を取得する配列
*/
void TextLayerGetBalloonArcPoint(
	TEXT_LAYER* layer,
	FLOAT_T angle,
	FLOAT_T point[2]
)
{
	FLOAT_T center_x,	center_y;
	FLOAT_T sin_value,	cos_value;
	FLOAT_T tan_value;

	center_x = layer->x + layer->width * 0.5;
	center_y = layer->y + layer->height * 0.5;

	sin_value = sin(angle);
	cos_value = cos(angle);
	tan_value = tan(angle);

	switch(layer->balloon_type)
	{
	case TEXT_LAYER_BALLOON_TYPE_SQUARE:
		if(fabs(layer->width * 0.5 * tan_value) <= layer->height * 0.5)
		{
			point[0] = (cos_value <= 0) ? layer->x : layer->x + layer->width;
			point[1] = center_y + layer->width * 0.5 * tan_value;
		}
		else
		{
			point[0] = center_x + layer->height * 0.5 * (1 / tan_value);;
			point[1] = (sin_value <= 0) ? layer->y : layer->y + layer->height;
		}
		break;
	default:
		point[0] = center_x + layer->width * 0.5 * cos_value;
		point[1] = center_y + layer->height * 0.5 * sin_value;
	}
}

static void DrawBalloonEdge(TEXT_LAYER* layer, GRAPHICS_CONTEXT* context)
{
	BEZIER_POINT calc[3], inter[2];
	FLOAT_T edge_position[2];

	TextLayerGetBalloonArcPoint(layer, layer->arc_end, edge_position);
	calc[0].x = edge_position[0],			calc[0].y = edge_position[1];
	calc[1].x = layer->edge_position[2][0],	calc[1].y = layer->edge_position[2][1];
	calc[2].x = layer->edge_position[0][0],	calc[2].y = layer->edge_position[0][1];
	MakeBezier3EdgeControlPoint(calc, inter);
	GraphicsLineTo(context, calc[0].x, calc[0].y);
	GraphicsCurveTo(context, calc[0].x, calc[0].y, inter[0].x, inter[0].y,
		layer->edge_position[2][0], layer->edge_position[2][1]);
	GraphicsCurveTo(context, layer->edge_position[2][0], layer->edge_position[2][1],
		inter[1].x, inter[1].y, layer->edge_position[0][0], layer->edge_position[0][1]);

	TextLayerGetBalloonArcPoint(layer, layer->arc_start, edge_position);
	calc[0].x = layer->edge_position[0][0],	calc[0].y = layer->edge_position[0][1];
	calc[1].x = layer->edge_position[1][0],	calc[1].y = layer->edge_position[1][1];
	calc[2].x = edge_position[0],			calc[2].y = edge_position[1];
	MakeBezier3EdgeControlPoint(calc, inter);
	GraphicsCurveTo(context, calc[0].x, calc[0].y, inter[0].x, inter[0].y,
		layer->edge_position[1][0], layer->edge_position[1][1]);
	GraphicsCurveTo(context, layer->edge_position[1][0], layer->edge_position[1][1],
		inter[1].x, inter[1].y, edge_position[0], edge_position[1]);
}

static void DrawEclipseBalloonEdge(TEXT_LAYER* layer, GRAPHICS_CONTEXT* context)
{
	BEZIER_POINT calc[3], inter[2];
	FLOAT_T edge_position[2];
	FLOAT_T r;
	FLOAT_T x_scale, y_scale;

	if(layer->width > layer->height)
	{
		r = layer->height * 0.5;
		x_scale = (FLOAT_T)layer->width / layer->height;
		y_scale = 1;
	}
	else
	{
		r = layer->width * 0.5;
		x_scale = 1;
		y_scale = (FLOAT_T)layer->height / layer->width;
	}
	x_scale = 1 / x_scale;
	y_scale = 1 / y_scale;

	TextLayerGetBalloonArcPoint(layer, layer->arc_end, edge_position);
	calc[0].x = edge_position[0],			calc[0].y = edge_position[1];
	calc[1].x = layer->edge_position[2][0],	calc[1].y = layer->edge_position[2][1];
	calc[2].x = layer->edge_position[0][0],	calc[2].y = layer->edge_position[0][1];
	MakeBezier3EdgeControlPoint(calc, inter);
	GraphicsCurveTo(context, calc[0].x * x_scale, calc[0].y * y_scale, inter[0].x * x_scale, inter[0].y * y_scale,
		layer->edge_position[2][0] * x_scale, layer->edge_position[2][1] * y_scale);
	GraphicsCurveTo(context, layer->edge_position[2][0] * x_scale, layer->edge_position[2][1] * y_scale,
		inter[1].x * x_scale, inter[1].y * y_scale, layer->edge_position[0][0] * x_scale, layer->edge_position[0][1] * y_scale);

	TextLayerGetBalloonArcPoint(layer, layer->arc_start, edge_position);
	calc[0].x = layer->edge_position[0][0],	calc[0].y = layer->edge_position[0][1];
	calc[1].x = layer->edge_position[1][0],	calc[1].y = layer->edge_position[1][1];
	calc[2].x = edge_position[0],			calc[2].y = edge_position[1];
	MakeBezier3EdgeControlPoint(calc, inter);
	GraphicsCurveTo(context, calc[0].x * x_scale, calc[0].y * y_scale, inter[0].x * x_scale, inter[0].y * y_scale,
		layer->edge_position[1][0] * x_scale, layer->edge_position[1][1] * y_scale);
	GraphicsCurveTo(context, layer->edge_position[1][0] * x_scale, layer->edge_position[1][1] * y_scale,
		inter[1].x * x_scale, inter[1].y * y_scale, edge_position[0] * x_scale, edge_position[1] * y_scale);
}

/*
* DrawSquareBalloon関数
* 長方形の吹き出しを描画する
* 引数
* layer		: テキストレイヤーの情報
* target	: 描画を実施するレイヤー
* canvas	: キャンバスの情報
*/
static void DrawSquareBalloon(
	TEXT_LAYER* layer,
	LAYER* target,
	DRAW_WINDOW* canvas
)
{
#define FUZZY_ZERO_VALUE 0.3
	if((layer->flags & TEXT_LAYER_BALLOON_HAS_EDGE) != 0)
	{
		FLOAT_T center_x,	center_y;
		FLOAT_T edge_start_point[2];
		FLOAT_T edge_end_point[2];

		center_x = layer->x + layer->width * 0.5;
		center_y = layer->y + layer->height * 0.5;

		TextLayerGetBalloonArcPoint(layer, layer->arc_start, edge_start_point);
		TextLayerGetBalloonArcPoint(layer, layer->arc_end, edge_end_point);

		if(fabs(edge_start_point[0] - layer->x) <= FUZZY_ZERO_VALUE
			|| fabs(edge_start_point[0] - (layer->x + layer->width)) <= FUZZY_ZERO_VALUE)
		{
			if(fabs(edge_start_point[0] - edge_end_point[0]) <= FUZZY_ZERO_VALUE)
			{
				if(fabs(edge_start_point[0] - layer->x) <= FUZZY_ZERO_VALUE)
				{
					if(edge_start_point[1] < edge_end_point[1])
					{
						GraphicsMoveTo(&target->context.base, edge_start_point[0], edge_start_point[1]);
						GraphicsLineTo(&target->context.base, layer->x, layer->y);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, layer->x, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, edge_end_point[0], edge_end_point[1]);
						DrawBalloonEdge(layer, &target->context.base);
					}
					else
					{
						GraphicsMoveTo(&target->context.base, edge_end_point[0], edge_end_point[1]);
						GraphicsLineTo(&target->context.base, layer->x, layer->y);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, layer->x, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, edge_start_point[0], edge_start_point[1]);
						DrawBalloonEdge(layer, &target->context.base);
					}
				}
				else
				{
					if(edge_start_point[1] < edge_end_point[1])
					{
						GraphicsMoveTo(&target->context.base, edge_start_point[0], edge_start_point[1]);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y);
						GraphicsLineTo(&target->context.base, layer->x, layer->y);
						GraphicsLineTo(&target->context.base, layer->x, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, edge_end_point[0], edge_end_point[1]);
						DrawBalloonEdge(layer, &target->context.base);
					}
					else
					{
						GraphicsMoveTo(&target->context.base, edge_end_point[0], edge_end_point[1]);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y);
						GraphicsLineTo(&target->context.base, layer->x, layer->y);
						GraphicsLineTo(&target->context.base, layer->x, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, edge_start_point[0], edge_start_point[1]);
						DrawBalloonEdge(layer, &target->context.base);
					}
				}
			}
			else
			{
				if(fabs(edge_start_point[0] - layer->x) <= FUZZY_ZERO_VALUE)
				{
					if(edge_start_point[1] < edge_end_point[1])
					{
						GraphicsMoveTo(&target->context.base, edge_start_point[0], edge_start_point[1]);
						GraphicsLineTo(&target->context.base, layer->x, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y);
						GraphicsLineTo(&target->context.base, edge_end_point[0], edge_end_point[1]);
						DrawBalloonEdge(layer, &target->context.base);
					}
					else
					{
						GraphicsMoveTo(&target->context.base, edge_start_point[0], edge_start_point[1]);
						GraphicsLineTo(&target->context.base, layer->x, layer->y);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y + layer->height);\
						GraphicsLineTo(&target->context.base, edge_end_point[0], edge_end_point[1]);
						DrawBalloonEdge(layer, &target->context.base);
					}
				}
				else
				{
					if(edge_start_point[1] < edge_end_point[1])
					{
						GraphicsMoveTo(&target->context.base, edge_start_point[0], edge_start_point[1]);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, layer->x, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y);
						GraphicsLineTo(&target->context.base, edge_end_point[0], edge_end_point[1]);
						DrawBalloonEdge(layer, &target->context.base);
					}
					else
					{
						GraphicsMoveTo(&target->context.base, edge_start_point[0], edge_start_point[1]);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y);
						GraphicsLineTo(&target->context.base, layer->x, layer->y);
						GraphicsLineTo(&target->context.base, layer->x, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, edge_end_point[0], edge_end_point[1]);
						DrawBalloonEdge(layer, &target->context.base);
					}
				}
			}
		}
		else
		{
			if(fabs(edge_start_point[1] - edge_end_point[1]) <= FUZZY_ZERO_VALUE)
			{
				if(fabs(edge_start_point[1] - layer->y) <= FUZZY_ZERO_VALUE)
				{
					if(edge_start_point[0] < edge_end_point[0])
					{
						GraphicsMoveTo(&target->context.base, edge_start_point[0], edge_start_point[1]);
						GraphicsLineTo(&target->context.base, layer->x, layer->y);
						GraphicsLineTo(&target->context.base, layer->x, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y);
						GraphicsLineTo(&target->context.base, edge_end_point[0], edge_end_point[1]);
						DrawBalloonEdge(layer, &target->context.base);
					}
					else
					{
						GraphicsMoveTo(&target->context.base, edge_end_point[0], edge_end_point[1]);
						GraphicsLineTo(&target->context.base, layer->x, layer->y);
						GraphicsLineTo(&target->context.base, layer->x, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y);
						GraphicsLineTo(&target->context.base, edge_start_point[0], edge_start_point[1]);
						DrawBalloonEdge(layer, &target->context.base);
					}
				}
				else
				{
					if(edge_start_point[0] < edge_end_point[0])
					{
						GraphicsMoveTo(&target->context.base, edge_start_point[0], edge_start_point[1]);
						GraphicsLineTo(&target->context.base, layer->x, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, layer->x, layer->y);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, edge_end_point[0], edge_end_point[1]);
						DrawBalloonEdge(layer, &target->context.base);
					}
					else
					{
						GraphicsMoveTo(&target->context.base, edge_end_point[0], edge_end_point[1]);
						GraphicsLineTo(&target->context.base, layer->x, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, layer->x, layer->y);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, edge_start_point[0], edge_start_point[1]);
						DrawBalloonEdge(layer, &target->context.base);
					}
				}
			}
			else
			{
				if(fabs(edge_start_point[1] - layer->y) <= FUZZY_ZERO_VALUE)
				{
					if(edge_start_point[0] < edge_end_point[0])
					{
						GraphicsMoveTo(&target->context.base, edge_start_point[0], edge_start_point[1]);
						GraphicsLineTo(&target->context.base, layer->x, layer->y);
						GraphicsLineTo(&target->context.base, layer->x, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, edge_end_point[0], edge_end_point[1]);
						DrawBalloonEdge(layer, &target->context.base);
					}
					else
					{
						GraphicsMoveTo(&target->context.base, edge_start_point[0], edge_start_point[1]);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, layer->x, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, edge_end_point[0], edge_end_point[1]);
						DrawBalloonEdge(layer, &target->context.base);
					}
				}
				else
				{
					if(edge_start_point[0] < edge_end_point[0])
					{
						GraphicsMoveTo(&target->context.base, edge_start_point[0], edge_start_point[1]);
						GraphicsLineTo(&target->context.base, layer->x, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, layer->x, layer->y);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y);
						GraphicsLineTo(&target->context.base, edge_end_point[0], edge_end_point[1]);
						DrawBalloonEdge(layer, &target->context.base);
					}
					else
					{
						GraphicsMoveTo(&target->context.base, edge_start_point[0], edge_start_point[1]);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y + layer->height);
						GraphicsLineTo(&target->context.base, layer->x + layer->width, layer->y);
						GraphicsLineTo(&target->context.base, layer->x, layer->y);
						GraphicsLineTo(&target->context.base, edge_end_point[0], edge_end_point[1]);
						DrawBalloonEdge(layer, &target->context.base);
					}
				}
			}
		}

		GraphicsClosePath(&target->context.base);
		GraphicsSetSourceRGBA(&target->context.base, layer->back_color[0] * DIV_PIXEL,
			layer->back_color[1] * DIV_PIXEL, layer->back_color[2] * DIV_PIXEL, layer->back_color[3] * DIV_PIXEL);
		GraphicsFillPreserve(&target->context.base);
		GraphicsSetSourceRGBA(&target->context.base, layer->line_color[0] * DIV_PIXEL,
			layer->line_color[1] * DIV_PIXEL, layer->line_color[2] * DIV_PIXEL, layer->line_color[3] * DIV_PIXEL);
		GraphicsSetLineWidth(&target->context.base, layer->line_width);
		GraphicsStroke(&target->context.base);
	}
	else
	{
		GraphicsRectangle(&target->context.base, layer->x, layer->y, layer->width, layer->height);
		GraphicsSetSourceRGBA(&target->context.base, layer->back_color[0] * DIV_PIXEL,
			layer->back_color[1] * DIV_PIXEL, layer->back_color[2] * DIV_PIXEL, layer->back_color[3] * DIV_PIXEL);
		GraphicsFillPreserve(&target->context.base);
		GraphicsSetSourceRGBA(&target->context.base, layer->line_color[0] * DIV_PIXEL,
			layer->line_color[1] * DIV_PIXEL, layer->line_color[2] * DIV_PIXEL, layer->line_color[3] * DIV_PIXEL);
		GraphicsSetLineWidth(&target->context.base, layer->line_width);
		GraphicsStroke(&target->context.base);
	}
#undef FUZZY_ZERO_VALUE
}

/*
* DrawEclipseBalloon関数
* 楕円形の吹き出しを描画する
* 引数*
* layer		: テキストレイヤーの情報
* target	: 描画を実施するレイヤー
* canvas	: キャンバスの情報
*/
static void DrawEclipseBalloon(
	TEXT_LAYER* layer,
	LAYER* target,
	DRAW_WINDOW* canvas
)
{
	GRAPHICS_DEFAULT_CONTEXT context = {0};
	FLOAT_T r;
	FLOAT_T x_scale, y_scale;

	InitializeGraphicsDefaultContext(&context, &target->surface, &canvas->app->graphics);
	
	if(layer->width > layer->height)
	{
		r = layer->height * 0.5;
		x_scale = (FLOAT_T)layer->width / layer->height;
		y_scale = 1;
	}
	else
	{
		r = layer->width * 0.5;
		x_scale = 1;
		y_scale = (FLOAT_T)layer->height / layer->width;
	}

	GraphicsSetLineWidth(&context.base, layer->line_width);
	GraphicsScale(&context.base, x_scale, y_scale);

	if((layer->flags & TEXT_LAYER_BALLOON_HAS_EDGE) == 0)
	{
		GraphicsArc(&context.base, (layer->x + layer->width * 0.5) / x_scale,
			(layer->y + layer->height * 0.5) / y_scale, r, 0, 2 * M_PI);
	}
	else
	{
		GraphicsArc(&target->context.base, (layer->x + layer->width * 0.5) / x_scale,
			(layer->y + layer->height * 0.5) / y_scale, r, layer->arc_start, layer->arc_end);
		DrawEclipseBalloonEdge(layer, &context.base);
	}
	GraphicsClosePath(&context.base);

	GraphicsSetSourceRGBA(&context.base, layer->back_color[0] * DIV_PIXEL,
		layer->back_color[1] * DIV_PIXEL, layer->back_color[2] * DIV_PIXEL, layer->back_color[3] * DIV_PIXEL);
	GraphicsFillPreserve(&context.base);

	GraphicsSetSourceRGBA(&context.base, layer->line_color[0] * DIV_PIXEL,
		layer->line_color[1] * DIV_PIXEL, layer->line_color[2] * DIV_PIXEL, layer->line_color[3] * DIV_PIXEL);
	GraphicsStroke(&context.base);

	DestroyGraphicsContext(&context.base);
}

/*
* DrawEclipse関数
* 楕円を描画する
* 引数
* layer		: テキストレイヤーの情報
* cairp_p	: 描画を行うCairoコンテキスト
* x			: 描画する範囲の左上のX座標
* y			: 描画する範囲の左上のY座標
* width		: 描画する範囲の幅
* height	: 描画する範囲の高さ
*/
static void DrawEclipse(
	TEXT_LAYER* layer,
	GRAPHICS_CONTEXT* context,
	FLOAT_T x,
	FLOAT_T y,
	FLOAT_T width,
	FLOAT_T height
)
{
	FLOAT_T r;
	FLOAT_T x_scale, y_scale;

	if(width > height)
	{
		r = height * 0.5;
		x_scale = width / height;
		y_scale = 1;
	}
	else
	{
		r = width * 0.5;
		x_scale = 1;
		y_scale = height / width;
	}

	GraphicsSave(context);

	GraphicsSetLineWidth(context, layer->line_width);
	GraphicsScale(context, x_scale, y_scale);

	GraphicsArc(context, (x + width * 0.5) / x_scale,
		(y + height * 0.5) / y_scale, r, 0, 2 * M_PI);
	GraphicsClosePath(context);

	GraphicsSetSourceRGBA(context, layer->back_color[0] * DIV_PIXEL,
		layer->back_color[1] * DIV_PIXEL, layer->back_color[2] * DIV_PIXEL, layer->back_color[3] * DIV_PIXEL);
	GraphicsFillPreserve(context);

	GraphicsSetSourceRGBA(context, layer->line_color[0] * DIV_PIXEL,
		layer->line_color[1] * DIV_PIXEL, layer->line_color[2] * DIV_PIXEL, layer->line_color[3] * DIV_PIXEL);
	GraphicsStroke(context);

	GraphicsRestore(context);
}

/*
* DrawEclipseThinkingBalloon関数
* 楕円形の心情描写の吹き出しを描画する
* 引数
* layer		: テキストレイヤーの情報
* target	: 描画を実施するレイヤー
* canvas	: キャンバスの情報
*/
static void DrawEclipseThinkingBalloon(
	TEXT_LAYER* layer,
	LAYER* target,
	DRAW_WINDOW* canvas
)
{
	BEZIER_POINT calc[4], inter[2], draw;
	FLOAT_T mid_position[2][2];
	FLOAT_T scale;
	GRAPHICS_DEFAULT_CONTEXT context = {0};
	FLOAT_T t;
	FLOAT_T dt = 1.0 / (layer->balloon_data.num_children + 1);
	int i;

	InitializeGraphicsDefaultContext(&context, &target->surface, &canvas->app->graphics);

	if((layer->flags & TEXT_LAYER_BALLOON_HAS_EDGE) != 0)
	{
		{
			FLOAT_T edge_position[2][2];
			TextLayerGetBalloonArcPoint(layer, layer->arc_start, edge_position[0]);
			TextLayerGetBalloonArcPoint(layer, layer->arc_end, edge_position[1]);
			mid_position[0][0] = (edge_position[0][0] + edge_position[1][0]) * 0.5;
			mid_position[0][1] = (edge_position[0][1] + edge_position[1][1]) * 0.5;

			mid_position[1][0] = (layer->edge_position[1][0] + layer->edge_position[2][0]) * 0.5;
			mid_position[1][1] = (layer->edge_position[1][1] + layer->edge_position[2][1]) * 0.5;
		}

		calc[0].x = layer->edge_position[0][0],	calc[0].y = layer->edge_position[0][1];
		calc[1].x = mid_position[1][0],			calc[1].y = mid_position[1][1];
		calc[2].x = mid_position[0][0],			calc[2].y = mid_position[0][1];
		MakeBezier3EdgeControlPoint(calc, inter);
		for(i=0, t=dt; i<layer->balloon_data.num_children; i++, t+=dt)
		{
			if(t < 0.5)
			{
				calc[0].x = layer->edge_position[0][0],	calc[0].y = layer->edge_position[0][1];
				calc[1].x = layer->edge_position[0][0],	calc[1].y = layer->edge_position[0][1];
				calc[2] = inter[0];
				calc[3].x = mid_position[1][0],			calc[3].y = mid_position[1][1];
				CalcBezier3(calc, t * 2, &draw);
			}
			else
			{
				calc[0].x = mid_position[1][0],	calc[0].y = mid_position[1][1];
				//calc[1].x = mid_position[1][0],	calc[1].y = mid_position[1][1];
				calc[1] = inter[1];
				calc[2].x = mid_position[0][0],	calc[2].y = mid_position[0][1];
				calc[3].x = mid_position[0][0],	calc[3].y = mid_position[0][1];
				CalcBezier3(calc, t * 2 - 1, &draw);
			}

			scale = layer->balloon_data.start_child_size * t + layer->balloon_data.end_child_size * (1 - t);

			DrawEclipse(layer, &context.base, draw.x - layer->width * 0.5 * scale, draw.y - layer->height * 0.5 * scale,
				layer->width * scale, layer->height * scale);
		}
	}

	DrawEclipse(layer, &context.base, layer->x, layer->y, layer->width, layer->height);

	DestroyGraphicsContext(&context.base);
}

/*
* DrawCrashBalloon関数
* 爆発形(直線)の吹き出しを描画する
* 引数
* layer		: テキストレイヤーの情報
* target	: 描画を実施するレイヤー
* canvas	: キャンバスの情報
*/
static void DrawCrashBalloon(
	TEXT_LAYER* layer,
	LAYER* target,
	DRAW_WINDOW* canvas
)
{
	GRAPHICS_DEFAULT_CONTEXT context;
	FLOAT_T arc_start,	arc_end;
	FLOAT_T x,	y;
	FLOAT_T angle_step;
	FLOAT_T angle;
	const FLOAT_T half_width = layer->width * 0.5;
	const FLOAT_T half_height = layer->height * 0.5;
	const FLOAT_T center_x = layer->x + half_width;
	const FLOAT_T center_y = layer->y + half_height;
	FLOAT_T random_angle;
	FLOAT_T random_size;
	const int num_points = (layer->balloon_data.num_edge + 1) * 2 + 1;
	int i;

	InitializeGraphicsDefaultContext(&context, &target->surface, &canvas->app->graphics);
	
	srand(layer->balloon_data.random_seed);

	if((layer->flags & TEXT_LAYER_BALLOON_HAS_EDGE) != 0)
	{
		if(layer->arc_start < layer->arc_end)
		{
			arc_start = layer->arc_start,	arc_end = layer->arc_end;
		}
		else
		{
			arc_start = layer->arc_start,		arc_end = layer->arc_end + 2 * M_PI;
		}
	}
	else
	{
		arc_start = 0,	arc_end = 2 * M_PI;
	}

	angle = arc_start;
	angle_step = (arc_end - arc_start) / (num_points + 1);
	if((layer->flags & TEXT_LAYER_BALLOON_HAS_EDGE) != 0)
	{
		x = center_x + half_width * cos(angle);
		y = center_y + half_height * sin(angle);
	}
	else
	{
		random_angle = angle + angle_step
			* ((rand() / (FLOAT_T)RAND_MAX) * layer->balloon_data.edge_random_distance
				- layer->balloon_data.edge_random_distance * 0.5);
		random_size = 1 - ((rand() / (FLOAT_T)RAND_MAX) * layer->balloon_data.edge_random_size);
		random_size *= (1 - layer->balloon_data.edge_size);
		x = center_x + half_width * random_size * cos(random_angle);
		y = center_y + half_height * random_size * sin(random_angle);
	}

	GraphicsMoveTo(&context.base, x, y);
	angle += angle_step;
	for(i=0; i<num_points; i++, angle += angle_step)
	{
		random_angle = angle + angle_step
			* ((rand() / (FLOAT_T)RAND_MAX) * layer->balloon_data.edge_random_distance
				- layer->balloon_data.edge_random_distance * 0.5);
		if((i & 0x01) != 0)
		{
			random_size = 1 - ((rand() / (FLOAT_T)RAND_MAX) * layer->balloon_data.edge_random_size);
			random_size *= (1 - layer->balloon_data.edge_size);
		}
		else
		{
			random_size = 1 + ((rand() / (FLOAT_T)RAND_MAX) * layer->balloon_data.edge_random_size);
			random_size *= (1 + layer->balloon_data.edge_size);
		}

		x = center_x + half_width * random_size * cos(random_angle);
		y = center_y + half_height * random_size * sin(random_angle);

		GraphicsLineTo(&context.base, x, y);
	}

	if((layer->flags & TEXT_LAYER_BALLOON_HAS_EDGE) != 0)
	{
		x = center_x + half_width * cos(layer->arc_end);
		y = center_y + half_height * sin(layer->arc_end);
		GraphicsLineTo(&context.base, x, y);
		DrawBalloonEdge(layer, &context.base);
	}
	GraphicsClosePath(&context.base);

	GraphicsSetLineWidth(&context.base, layer->line_width);

	GraphicsSetSourceRGBA(&context.base, layer->back_color[0] * DIV_PIXEL,
		layer->back_color[1] * DIV_PIXEL, layer->back_color[2] * DIV_PIXEL, layer->back_color[3] * DIV_PIXEL);
	GraphicsFillPreserve(&context.base);

	GraphicsSetSourceRGBA(&context.base, layer->line_color[0] * DIV_PIXEL,
		layer->line_color[1] * DIV_PIXEL, layer->line_color[2] * DIV_PIXEL, layer->line_color[3] * DIV_PIXEL);
	GraphicsStroke(&context.base);

	DestroyGraphicsContext(&context.base);
}

/*
* DrawSmokeBalloon関数
* モヤモヤ形の吹き出しを描画する
* 引数
* layer		: テキストレイヤーの情報
* target	: 描画を実施するレイヤー
* canvas	: キャンバスの情報
*/
static void DrawSmokeBalloon(
	TEXT_LAYER* layer,
	LAYER* target,
	DRAW_WINDOW* canvas
)
{
	GRAPHICS_DEFAULT_CONTEXT context;
	const FLOAT_T half_width = layer->width * 0.5;
	const FLOAT_T half_height = layer->height * 0.5;
	const FLOAT_T center_x = layer->x + half_width;
	const FLOAT_T center_y = layer->y + half_height;
	FLOAT_T arc_start,	arc_end;
	FLOAT_T angle;
	FLOAT_T angle_step;
	FLOAT_T random_angle;
	FLOAT_T random_size;
	FLOAT_T before_angle;
	FLOAT_T mid_angle;
	FLOAT_T x, y;
	FLOAT_T mid_x,	mid_y;
	FLOAT_T before_x, before_y;
	const int num_points = layer->balloon_data.num_edge + 1;
	int i;

	InitializeGraphicsDefaultContext(&context, &target->surface, &canvas->app->graphics);
	
	srand(layer->balloon_data.random_seed);

	if((layer->flags & TEXT_LAYER_BALLOON_HAS_EDGE) != 0)
	{
		if(layer->arc_start < layer->arc_end)
		{
			arc_start = layer->arc_start,	arc_end = layer->arc_end;
		}
		else
		{
			arc_start = layer->arc_start,		arc_end = layer->arc_end + 2 * M_PI;
		}
	}
	else
	{
		arc_start = 0,	arc_end = 2 * M_PI;
	}

	angle = arc_start;
	angle_step = (arc_end - arc_start) / num_points;
	if((layer->flags & TEXT_LAYER_BALLOON_HAS_EDGE) != 0)
	{
		x = center_x + half_width * cos(angle);
		y = center_y + half_height * sin(angle);
		before_angle = angle + angle_step
			* ((rand() / (FLOAT_T)RAND_MAX) * layer->balloon_data.edge_random_distance
				- layer->balloon_data.edge_random_distance * 0.5);
		angle += angle_step;
	}
	else
	{
		random_angle = angle_step
			* ((rand() / (FLOAT_T)RAND_MAX) * layer->balloon_data.edge_random_distance
				- layer->balloon_data.edge_random_distance * 0.5);
		x = center_x + half_width * cos(random_angle);
		y = center_y + half_height * sin(random_angle);

		angle = angle_step;
		arc_end += before_angle = random_angle;
	}

	GraphicsMoveTo(&context.base, x, y);
	before_x = x,	before_y = y;
	for(i=0; i<num_points-1; i++, angle += angle_step)
	{
		random_angle = angle + angle_step
			* ((rand() / (FLOAT_T)RAND_MAX) * layer->balloon_data.edge_random_distance
				- layer->balloon_data.edge_random_distance * 0.5);
		random_size = 1 + ((rand() / (FLOAT_T)RAND_MAX) * layer->balloon_data.edge_random_size);
		random_size *= (1 + layer->balloon_data.edge_size);

		x = center_x + half_width * cos(random_angle);
		y = center_y + half_height * sin(random_angle);

		mid_angle = (random_angle + before_angle) * 0.5;
		mid_x = center_x + half_width * random_size * cos(mid_angle);
		mid_y = center_y + half_height * random_size * sin(mid_angle);

		GraphicsCurveTo(&context.base, (before_x + mid_x) * 0.5, (before_y + mid_y) * 0.5,
			(x + mid_x) * 0.5, (y + mid_y) * 0.5, x, y);

		before_angle = random_angle;
		before_x = x,	before_y = y;
	}
	random_angle = arc_end;
	random_size = 1 + ((rand() / (FLOAT_T)RAND_MAX) * layer->balloon_data.edge_random_size);
	random_size *= (1 + layer->balloon_data.edge_size);

	x = center_x + half_width * cos(random_angle);
	y = center_y + half_height * sin(random_angle);

	mid_angle = (random_angle + before_angle) * 0.5;
	mid_x = center_x + half_width * random_size * cos(mid_angle);
	mid_y = center_y + half_height * random_size * sin(mid_angle);

	GraphicsCurveTo(&context.base, (before_x + mid_x) * 0.5, (before_y + mid_y) * 0.5,
		(x + mid_x) * 0.5, (y + mid_y) * 0.5, x, y);

	if((layer->flags & TEXT_LAYER_BALLOON_HAS_EDGE) != 0)
	{
		DrawBalloonEdge(layer, &context.base);
	}
	GraphicsClosePath(&context.base);

	GraphicsSetLineWidth(&context.base, layer->line_width);

	GraphicsSetSourceRGBA(&context.base, layer->back_color[0] * DIV_PIXEL,
		layer->back_color[1] * DIV_PIXEL, layer->back_color[2] * DIV_PIXEL, layer->back_color[3] * DIV_PIXEL);
	GraphicsFillPreserve(&context.base);

	GraphicsSetSourceRGBA(&context.base, layer->line_color[0] * DIV_PIXEL,
		layer->line_color[1] * DIV_PIXEL, layer->line_color[2] * DIV_PIXEL, layer->line_color[3] * DIV_PIXEL);
	GraphicsStroke(&context.base);

	DestroyGraphicsContext(&context.base);
}

/*
* DrawSmoke関数
* モヤモヤを描画する
* 引数
* layer		: テキストレイヤーの情報
* cairp_p	: 描画を行うCairoコンテキスト
* x			: 描画する範囲の左上のX座標
* y			: 描画する範囲の左上のY座標
* width		: 描画する範囲の幅
* height	: 描画する範囲の高さ
*/
static void DrawSmoke(
	GRAPHICS_CONTEXT* context,
	FLOAT_T x,
	FLOAT_T y,
	FLOAT_T width,
	FLOAT_T height,
	TEXT_LAYER* layer
)
{
	const FLOAT_T half_width = width * 0.5;
	const FLOAT_T half_height = height * 0.5;
	const FLOAT_T center_x = x + half_width;
	const FLOAT_T center_y = y + half_height;
	FLOAT_T arc_start,	arc_end;
	FLOAT_T angle;
	FLOAT_T angle_step;
	FLOAT_T random_angle;
	FLOAT_T random_size;
	FLOAT_T before_angle;
	FLOAT_T mid_angle;
	FLOAT_T mid_x,	mid_y;
	FLOAT_T before_x, before_y;
	const int num_points = layer->balloon_data.num_edge + 1;
	int i;

	arc_start = 0,	arc_end = 2 * M_PI;

	angle = arc_start;
	angle_step = (arc_end - arc_start) / num_points;

	random_angle = angle_step
		* ((rand() / (FLOAT_T)RAND_MAX) * layer->balloon_data.edge_random_distance
			- layer->balloon_data.edge_random_distance * 0.5);
	x = center_x + half_width * cos(random_angle);
	y = center_y + half_height * sin(random_angle);

	angle = angle_step;
	arc_end += before_angle = random_angle;

	GraphicsMoveTo(context, x, y);
	before_x = x,	before_y = y;
	for(i=0; i<num_points-1; i++, angle += angle_step)
	{
		random_angle = angle + angle_step
			* ((rand() / (FLOAT_T)RAND_MAX) * layer->balloon_data.edge_random_distance
				- layer->balloon_data.edge_random_distance * 0.5);
		random_size = 1 + ((rand() / (FLOAT_T)RAND_MAX) * layer->balloon_data.edge_random_size);
		random_size *= (1 + layer->balloon_data.edge_size);

		x = center_x + half_width * cos(random_angle);
		y = center_y + half_height * sin(random_angle);

		mid_angle = (random_angle + before_angle) * 0.5;
		mid_x = center_x + half_width * random_size * cos(mid_angle);
		mid_y = center_y + half_height * random_size * sin(mid_angle);

		GraphicsCurveTo(context, (before_x + mid_x) * 0.5, (before_y + mid_y) * 0.5,
			(x + mid_x) * 0.5, (y + mid_y) * 0.5, x, y);

		before_angle = random_angle;
		before_x = x,	before_y = y;
	}
	random_angle = arc_end;
	random_size = 1 + ((rand() / (FLOAT_T)RAND_MAX) * layer->balloon_data.edge_random_size);
	random_size *= (1 + layer->balloon_data.edge_size);

	x = center_x + half_width * cos(random_angle);
	y = center_y + half_height * sin(random_angle);

	mid_angle = (random_angle + before_angle) * 0.5;
	mid_x = center_x + half_width * random_size * cos(mid_angle);
	mid_y = center_y + half_height * random_size * sin(mid_angle);

	GraphicsCurveTo(context, (before_x + mid_x) * 0.5, (before_y + mid_y) * 0.5,
		(x + mid_x) * 0.5, (y + mid_y) * 0.5, x, y);

	GraphicsClosePath(context);

	GraphicsSetLineWidth(context, layer->line_width);

	GraphicsSetSourceRGBA(context, layer->back_color[0] * DIV_PIXEL,
		layer->back_color[1] * DIV_PIXEL, layer->back_color[2] * DIV_PIXEL, layer->back_color[3] * DIV_PIXEL);
	GraphicsFillPreserve(context);

	GraphicsSetSourceRGBA(context, layer->line_color[0] * DIV_PIXEL,
		layer->line_color[1] * DIV_PIXEL, layer->line_color[2] * DIV_PIXEL, layer->line_color[3] * DIV_PIXEL);
	GraphicsStroke(context);
}

/*
* DrawSmokeThinkingBalloon1関数
* 煙形の心情描写の吹き出しを描画するその1
* 引数
* layer		: テキストレイヤーの情報
* target	: 描画を実施するレイヤー
* canvas	: キャンバスの情報
*/
static void DrawSmokeThinkingBalloon1(
	TEXT_LAYER* layer,
	LAYER* target,
	DRAW_WINDOW* canvas
)
{
	BEZIER_POINT calc[4], inter[2], draw;
	FLOAT_T mid_position[2][2];
	FLOAT_T scale;
	GRAPHICS_DEFAULT_CONTEXT context;
	FLOAT_T t;
	FLOAT_T dt = 1.0 / (layer->balloon_data.num_children + 1);
	int i;

	srand(layer->balloon_data.random_seed);

	InitializeGraphicsDefaultContext(&context, &target->surface, &canvas->app->graphics);

	if((layer->flags & TEXT_LAYER_BALLOON_HAS_EDGE) != 0)
	{
		{
			FLOAT_T edge_position[2][2];
			TextLayerGetBalloonArcPoint(layer, layer->arc_start, edge_position[0]);
			TextLayerGetBalloonArcPoint(layer, layer->arc_end, edge_position[1]);
			mid_position[0][0] = (edge_position[0][0] + edge_position[1][0]) * 0.5;
			mid_position[0][1] = (edge_position[0][1] + edge_position[1][1]) * 0.5;

			mid_position[1][0] = (layer->edge_position[1][0] + layer->edge_position[2][0]) * 0.5;
			mid_position[1][1] = (layer->edge_position[1][1] + layer->edge_position[2][1]) * 0.5;
		}

		calc[0].x = layer->edge_position[0][0],	calc[0].y = layer->edge_position[0][1];
		calc[1].x = mid_position[1][0],			calc[1].y = mid_position[1][1];
		calc[2].x = mid_position[0][0],			calc[2].y = mid_position[0][1];
		MakeBezier3EdgeControlPoint(calc, inter);
		for(i=0, t=dt; i<layer->balloon_data.num_children; i++, t+=dt)
		{
			if(t < 0.5)
			{
				calc[0].x = layer->edge_position[0][0],	calc[0].y = layer->edge_position[0][1];
				calc[1].x = layer->edge_position[0][0],	calc[1].y = layer->edge_position[0][1];
				calc[2] = inter[0];
				calc[3].x = mid_position[1][0],			calc[3].y = mid_position[1][1];
				CalcBezier3(calc, t * 2, &draw);
			}
			else
			{
				calc[0].x = mid_position[1][0],	calc[0].y = mid_position[1][1];
				calc[1] = inter[1];
				calc[2].x = mid_position[0][0],	calc[2].y = mid_position[0][1];
				calc[3].x = mid_position[0][0],	calc[3].y = mid_position[0][1];
				CalcBezier3(calc, t * 2 - 1, &draw);
			}

			scale = layer->balloon_data.start_child_size * t + layer->balloon_data.end_child_size * (1 - t);

			DrawEclipse(layer, &context.base, draw.x - layer->width * 0.5 * scale, draw.y - layer->height * 0.5 * scale,
				layer->width * scale, layer->height * scale);
		}
	}

	DrawSmoke(&context.base, layer->x, layer->y, layer->width, layer->height, layer);

	DestroyGraphicsContext(&context.base);
}

/*
* DrawSmokeThinkingBalloon2関数
* 煙形の心情描写の吹き出しを描画するその2
* 引数
* layer		: テキストレイヤーの情報
* target	: 描画を実施するレイヤー
* canvas	: キャンバスの情報
*/
static void DrawSmokeThinkingBalloon2(
	TEXT_LAYER* layer,
	LAYER* target,
	DRAW_WINDOW* canvas
)
{
	BEZIER_POINT calc[4], inter[2], draw;
	FLOAT_T mid_position[2][2];
	FLOAT_T scale;
	GRAPHICS_DEFAULT_CONTEXT context;
	FLOAT_T t;
	FLOAT_T dt = 1.0 / (layer->balloon_data.num_children + 1);
	int i;

	srand(layer->balloon_data.random_seed);

	InitializeGraphicsDefaultContext(&context, &target->surface, &canvas->app->graphics);

	if((layer->flags & TEXT_LAYER_BALLOON_HAS_EDGE) != 0)
	{
		{
			FLOAT_T edge_position[2][2];
			TextLayerGetBalloonArcPoint(layer, layer->arc_start, edge_position[0]);
			TextLayerGetBalloonArcPoint(layer, layer->arc_end, edge_position[1]);
			mid_position[0][0] = (edge_position[0][0] + edge_position[1][0]) * 0.5;
			mid_position[0][1] = (edge_position[0][1] + edge_position[1][1]) * 0.5;

			mid_position[1][0] = (layer->edge_position[1][0] + layer->edge_position[2][0]) * 0.5;
			mid_position[1][1] = (layer->edge_position[1][1] + layer->edge_position[2][1]) * 0.5;
		}

		calc[0].x = layer->edge_position[0][0],	calc[0].y = layer->edge_position[0][1];
		calc[1].x = mid_position[1][0],			calc[1].y = mid_position[1][1];
		calc[2].x = mid_position[0][0],			calc[2].y = mid_position[0][1];
		MakeBezier3EdgeControlPoint(calc, inter);
		for(i=0, t=dt; i<layer->balloon_data.num_children; i++, t+=dt)
		{
			if(t < 0.5)
			{
				calc[0].x = layer->edge_position[0][0],	calc[0].y = layer->edge_position[0][1];
				calc[1].x = layer->edge_position[0][0],	calc[1].y = layer->edge_position[0][1];
				calc[2] = inter[0];
				calc[3].x = mid_position[1][0],			calc[3].y = mid_position[1][1];
				CalcBezier3(calc, t * 2, &draw);
			}
			else
			{
				calc[0].x = mid_position[1][0],	calc[0].y = mid_position[1][1];
				calc[1] = inter[1];
				calc[2].x = mid_position[0][0],	calc[2].y = mid_position[0][1];
				calc[3].x = mid_position[0][0],	calc[3].y = mid_position[0][1];
				CalcBezier3(calc, t * 2 - 1, &draw);
			}

			scale = layer->balloon_data.start_child_size * t + layer->balloon_data.end_child_size * (1 - t);

			DrawSmoke(&context.base, draw.x - layer->width * 0.5 * scale, draw.y - layer->height * 0.5 * scale,
				layer->width * scale, layer->height * scale, layer);
		}
	}

	DrawSmoke(&context.base, layer->x, layer->y, layer->width, layer->height, layer);

	DestroyGraphicsContext(&context.base);
}

/*
* DrawBurstBalloon関数
* 爆発形(曲線)の吹き出しを描画する
* 引数
* layer		: テキストレイヤーの情報
* target	: 描画を実施するレイヤー
* canvas	: キャンバスの情報
*/
static void DrawBurstBalloon(
	TEXT_LAYER* layer,
	LAYER* target,
	DRAW_WINDOW* canvas
)
{
	GRAPHICS_DEFAULT_CONTEXT context;
	const FLOAT_T half_width = layer->width * 0.5;
	const FLOAT_T half_height = layer->height * 0.5;
	const FLOAT_T center_x = layer->x + half_width;
	const FLOAT_T center_y = layer->y + half_height;
	FLOAT_T arc_start,	arc_end;
	FLOAT_T angle;
	FLOAT_T angle_step;
	FLOAT_T random_angle;
	FLOAT_T random_size;
	FLOAT_T before_angle;
	FLOAT_T mid_angle;
	FLOAT_T x, y;
	FLOAT_T mid_x,	mid_y;
	FLOAT_T before_x, before_y;
	const int num_points = layer->balloon_data.num_edge + 1;
	int i;
	
	InitializeGraphicsDefaultContext(&context, &target->surface, &canvas->app->graphics);

	srand(layer->balloon_data.random_seed);

	if((layer->flags & TEXT_LAYER_BALLOON_HAS_EDGE) != 0)
	{
		if(layer->arc_start < layer->arc_end)
		{
			arc_start = layer->arc_start,	arc_end = layer->arc_end;
		}
		else
		{
			arc_start = layer->arc_start,		arc_end = layer->arc_end + 2 * M_PI;
		}
	}
	else
	{
		arc_start = 0,	arc_end = 2 * M_PI;
	}

	angle = arc_start;
	angle_step = (arc_end - arc_start) / num_points;
	if((layer->flags & TEXT_LAYER_BALLOON_HAS_EDGE) != 0)
	{
		x = center_x + half_width * cos(angle);
		y = center_y + half_height * sin(angle);
		before_angle = angle + angle_step
			* ((rand() / (FLOAT_T)RAND_MAX) * layer->balloon_data.edge_random_distance
				- layer->balloon_data.edge_random_distance * 0.5);
		angle += angle_step;
	}
	else
	{
		random_angle = angle_step
			* ((rand() / (FLOAT_T)RAND_MAX) * layer->balloon_data.edge_random_distance
				- layer->balloon_data.edge_random_distance * 0.5);
		x = center_x + half_width * cos(random_angle);
		y = center_y + half_height * sin(random_angle);

		angle = angle_step;
		arc_end += before_angle = random_angle;
	}

	GraphicsMoveTo(&context.base, x, y);
	before_x = x,	before_y = y;
	for(i=0; i<num_points-1; i++, angle += angle_step)
	{
		random_angle = angle + angle_step
			* ((rand() / (FLOAT_T)RAND_MAX) * layer->balloon_data.edge_random_distance
				- layer->balloon_data.edge_random_distance * 0.5);
		random_size = 1 + ((rand() / (FLOAT_T)RAND_MAX) * layer->balloon_data.edge_random_size);
		random_size *= (1 - layer->balloon_data.edge_size);

		x = center_x + half_width * cos(random_angle);
		y = center_y + half_height * sin(random_angle);

		mid_angle = (random_angle + before_angle) * 0.5;
		mid_x = center_x + half_width * random_size * cos(mid_angle);
		mid_y = center_y + half_height * random_size * sin(mid_angle);

		GraphicsCurveTo(&context.base, (before_x + mid_x) * 0.5, (before_y + mid_y) * 0.5,
			(x + mid_x) * 0.5, (y + mid_y) * 0.5, x, y);

		before_angle = random_angle;
		before_x = x,	before_y = y;
	}
	random_angle = arc_end;
	random_size = 1 + ((rand() / (FLOAT_T)RAND_MAX) * layer->balloon_data.edge_random_size);
	random_size *= (1 - layer->balloon_data.edge_size);

	x = center_x + half_width * cos(random_angle);
	y = center_y + half_height * sin(random_angle);

	mid_angle = (random_angle + before_angle) * 0.5;
	mid_x = center_x + half_width * random_size * cos(mid_angle);
	mid_y = center_y + half_height * random_size * sin(mid_angle);

	GraphicsCurveTo(&context.base, (before_x + mid_x) * 0.5, (before_y + mid_y) * 0.5,
		(x + mid_x) * 0.5, (y + mid_y) * 0.5, x, y);

	if((layer->flags & TEXT_LAYER_BALLOON_HAS_EDGE) != 0)
	{
		DrawBalloonEdge(layer, &context.base);
	}
	GraphicsClosePath(&context.base);

	GraphicsSetLineWidth(&context.base, layer->line_width);

	GraphicsSetSourceRGBA(&context.base, layer->back_color[0] * DIV_PIXEL,
		layer->back_color[1] * DIV_PIXEL, layer->back_color[2] * DIV_PIXEL, layer->back_color[3] * DIV_PIXEL);
	GraphicsFillPreserve(&context.base);

	GraphicsSetSourceRGBA(&context.base, layer->line_color[0] * DIV_PIXEL,
		layer->line_color[1] * DIV_PIXEL, layer->line_color[2] * DIV_PIXEL, layer->line_color[3] * DIV_PIXEL);
	GraphicsStroke(&context.base);

	DestroyGraphicsContext(&context.base);
}

/***********************************************************************
* SetTextLayerDrawBalloonFunctions関数								 *
* 吹き出しを描画する関数の関数ポインタ配列の中身を設定				 *
* 引数																 *
* draw_balloon_functions	: 吹き出しを描画する関数の関数ポインタ配列 *
***********************************************************************/
void SetTextLayerDrawBalloonFunctions(void (*draw_balloon_functions[])(TEXT_LAYER*, LAYER*, DRAW_WINDOW*)
)
{
	draw_balloon_functions[TEXT_LAYER_BALLOON_TYPE_SQUARE] = DrawSquareBalloon;
	draw_balloon_functions[TEXT_LAYER_BALLOON_TYPE_ECLIPSE] = DrawEclipseBalloon;
	draw_balloon_functions[TEXT_LAYER_BALLOON_TYPE_ECLIPSE_THINKING] = DrawEclipseThinkingBalloon;
	draw_balloon_functions[TEXT_LAYER_BALLOON_TYPE_CRASH] = DrawCrashBalloon;
	draw_balloon_functions[TEXT_LAYER_BALLOON_TYPE_SMOKE] = DrawSmokeBalloon;
	draw_balloon_functions[TEXT_LAYER_BALLOON_TYPE_SMOKE_THINKING1] = DrawSmokeThinkingBalloon1;
	draw_balloon_functions[TEXT_LAYER_BALLOON_TYPE_SMOKE_THINKING2] = DrawSmokeThinkingBalloon2;
	draw_balloon_functions[TEXT_LAYER_BALLOON_TYPE_BURST] = DrawBurstBalloon;
}
