#include <math.h>
#include "smoother.h"
#include "draw_window.h"
#include "application.h"
#include "brush_core.h"
#include "tool_box.h"

#ifndef FALSE
# define FALSE (0)
#endif
#ifndef TRUE
# define TRUE (!FALSE)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
* Smooth関数
* 手ブレ補正を行う
* 引数
* smoother	: 手ブレ補正のデータ
* x			: 現在のx座標。補正後の値が入る
* y			: 現在のy座標。補正後の値が入る
* this_time	: 手ブレ補正処理開始時間
* zoom_rate	: 描画領域の拡大縮小率
*/
void Smooth(
	SMOOTHER* smoother,
	FLOAT_T *x,
	FLOAT_T *y,
	unsigned int this_time,
	FLOAT_T zoom_rate
)
{
// 履歴データを追加する閾値
#define IGNORE_THRESHOULD 0.0001f
// sqrt(2*PI)
#define SQRT_2_PI 1.7724538509055160272981674833411f

	// 履歴データに座標データを追加
	smoother->buffer[smoother->index].x = *x;
	smoother->buffer[smoother->index].y = *y;

	// データ点があれば補正を行う
	if(smoother->num_data > 0)
	{
		// 計算結果
		FLOAT_T result_x = 0.0, result_y = 0.0;
		// 前回の処理からの距離
		FLOAT_T distance =
			((*x - smoother->before_point.x)*(*x - smoother->before_point.x)
			+ (*y - smoother->before_point.y)*(*y - smoother->before_point.y)) * zoom_rate;
		// マウスカーソルの移動速度
		FLOAT_T velocity;
		// ガウス関数を適用する割合
		FLOAT_T weight = 0.0, weight2 = smoother->rate * smoother->rate;
		// 速度の合計値
		FLOAT_T velocity_sum = 0.0;
		FLOAT_T scale_sum = 0.0;
		FLOAT_T rate;
		// wieightが0でないことのフラグ
		int none_0_wieight = 0;
		// 配列のインデックス
		int index;
		// for文用のカウンタ
		int i;

		if(weight2 != 0.0)
		{
			weight = 1.0 / (SQRT_2_PI * smoother->rate);
			none_0_wieight++;
		}
		
		distance = sqrt(distance);
		// 前回からの距離が閾値以上ならば履歴データ追加
		if(distance >= IGNORE_THRESHOULD)
		{
			velocity = distance * 10.0;
			smoother->velocity[smoother->index] = velocity;
			smoother->before_point = smoother->buffer[smoother->index];

			// インデックス更新。範囲をオーバーしたらはじめに戻す
			smoother->index++;
			if(smoother->index >= smoother->num_use)
			{
				smoother->index = 0;
			}

			if(smoother->num_data < smoother->num_use)
			{
				smoother->num_data++;
			}
		}

		// 速度ベースのガウシアン重み付けで手ブレ補正実行
		index = smoother->index-1;
		for(i=0; i<smoother->num_data; i++, index--)
		{
			rate = 0.0;
			if(index < 0)
			{
				index = smoother->num_use-1;
			}

			if(none_0_wieight != 0)
			{
				velocity_sum += smoother->velocity[index];
				rate = weight * exp(-velocity_sum*velocity_sum / (2.0*weight2));

				if(i == 0 && rate == 0.0)
				{
					rate = 1.0;
				}
			}
			else
			{
				rate = (i == 0) ? 1.0 : 0.0;
			}

			scale_sum += rate;
			result_x += rate * smoother->buffer[index].x;
			result_y += rate * smoother->buffer[index].y;
		}

		// 閾値以上なら補正実行(3/1
		if(scale_sum > 0.01)
		{
			result_x /= scale_sum;
			result_y /= scale_sum;
			*x = result_x, *y = result_y;
		}
	}
	else
	{
		smoother->before_point = smoother->buffer[smoother->index];
		smoother->velocity[smoother->index] = 0.0;
		smoother->index++;
		smoother->num_data = 1;
	}

	// 今回の時間を記憶
	smoother->last_time = this_time;
}

/*
* AddAverageSmoothPoint関数
* 座標平均化による手ブレ補正にデータ点を追加
* 引数
* smoother	: 手ブレ補正のデータ
* state	 : 座標、筆圧等のデータ
* zoom_rate	: 描画領域の拡大縮小率
* 返り値
*	ブラシの描画を行う:TRUE	行わない:FALSE
*/
int AddAverageSmoothPoint(
	SMOOTHER* smoother,
	EVENT_STATE* state,
	FLOAT_T zoom_rate
)
{
	int ref_index = smoother->index % smoother->num_use;

	if(smoother->index == 0)
	{
		smoother->buffer[0].x = state->cursor_x;
		smoother->buffer[0].y = state->cursor_y;
		smoother->velocity[0] = state->pressure;
		smoother->before_point.x = state->cursor_x;
		smoother->before_point.y = state->cursor_y;
		smoother->sum.x = state->cursor_x;
		smoother->sum.y = state->cursor_y;
		smoother->index++;
		smoother->last_time = 0;
	}
	else
	{
		if(fabs(smoother->before_point.x - state->cursor_x) +
			 fabs(smoother->before_point.y - state->cursor_y) < 0.8 * zoom_rate)
		{
			int before_index = ref_index - 1;
			if(before_index < 0)
			{
				before_index = smoother->num_use - 1;
				if(before_index < 0)
				{
					return FALSE;
				}
			}

			if(smoother->velocity[before_index] < state->pressure)
			{
				smoother->velocity[before_index] = state->pressure;
			}
			
			return FALSE;
		}

		if(smoother->index >= smoother->num_use)
		{
			FLOAT_T add_x = state->cursor_x, add_y = state->cursor_y, add_pressure = state->pressure;
			state->cursor_x = smoother->sum.x / smoother->num_use;
			state->cursor_y = smoother->sum.y / smoother->num_use;
			state->pressure = smoother->velocity[ref_index];
			smoother->sum.x -= smoother->buffer[ref_index].x;
			smoother->sum.y -= smoother->buffer[ref_index].y;

			smoother->sum.x += add_x;
			smoother->sum.y += add_y;
			smoother->before_point.x = smoother->buffer[ref_index].x = add_x;
			smoother->before_point.y = smoother->buffer[ref_index].y = add_y;
			smoother->velocity[ref_index] = add_pressure;

			smoother->index++;

			return TRUE;
		}
		else
		{
			smoother->sum.x += state->cursor_x;
			smoother->sum.y += state->cursor_y;
			smoother->before_point.x = smoother->buffer[ref_index].x = state->cursor_x;
			smoother->before_point.y = smoother->buffer[ref_index].y = state->cursor_y;
			smoother->velocity[ref_index] = state->pressure;

			smoother->index++;
		}
	}

	return FALSE;
}

/*
* AverageSmoothFlush関数
* 平均化による手ブレ補正の残りバッファを1つ取り出す
* 引数
* smoother	: 手ブレ補正の情報
* state	 : 座標, 筆圧等の情報
* 返り値
*	バッファの残り無し:TRUE	残り有り:FALSE
*/
int AverageSmoothFlush(
	SMOOTHER* smoother,
	EVENT_STATE* state
)
{
	int ref_index;

	if(smoother->last_time == 0)
	{
		smoother->last_time++;
		if(smoother->index >= smoother->num_use)
		{
			smoother->num_data = smoother->num_use;
		}
		else
		{
			smoother->num_data = smoother->index;
			smoother->index = 0;
		}
	}

	ref_index = smoother->index % smoother->num_use;
	state->cursor_x = smoother->sum.x / smoother->num_data;
	state->cursor_y = smoother->sum.y / smoother->num_data;
	state->pressure = smoother->velocity[ref_index];
	smoother->sum.x -= smoother->buffer[ref_index].x;
	smoother->sum.y -= smoother->buffer[ref_index].y;
	smoother->index++;
	smoother->num_data--;

	if(smoother->num_data <= 0)
	{
		state->cursor_x = smoother->buffer[ref_index].x;
		state->cursor_y = smoother->buffer[ref_index].y;

		return TRUE;
	}

	return FALSE;
}

/*
* MotionQueueAppendItem関数
* マウスカーソルの座標を追加する
* 引数
* queue	 : 座標管理のデータ
* state	 : 座標, 筆圧などのデータ
*/
void MotionQueueAppendItem(
	MOTION_QUEUE* queue,
	EVENT_STATE* state
)
{
	int index;

	queue->last_queued_x = state->cursor_x;
	queue->last_queued_y = state->cursor_y;

	if(queue->num_items >= queue->max_items)
	{
		index = (queue->start_index + queue->max_items - 1) % queue->max_items;
	}
	else
	{
		index = (queue->start_index + queue->num_items) % queue->max_items;
		queue->num_items++;
	}

	queue->queue[index].state = *state;
}

/*
* ClearMotionQueue関数
* 待ち行列に溜まったデータを全て処理する
* 引数
* canvas	: 対応する描画領域
*/
void ClearMotionQueue(DRAW_WINDOW* canvas)
{
	APPLICATION *app = canvas->app;
	MOTION_QUEUE *motion_queue = &app->tool_box.motion_queue;
	brush_update_function update_func = DefaultToolUpdate;
	void *update_data = NULL;
	FLOAT_T x, y;
	FLOAT_T x0, y0;
	int index;
	int i;

	// 画面更新用のデータを取得
	if(canvas->transform == NULL)
	{
		if((app->tool_box.flags & TOOL_USING_BRUSH) != 0)
		{
			if(canvas->active_layer->layer_type == TYPE_NORMAL_LAYER)
			{
				update_func = app->tool_box.active_brush[app->input]->motion_update;
				update_data = app->tool_box.active_brush[app->input];
			}
			else if(canvas->active_layer->layer_type == TYPE_VECTOR_LAYER)
			{
				update_func = app->tool_box.active_vector_brush[app->input]->motion_update;
				update_data = app->tool_box.active_vector_brush[app->input];
			}
		}
		else
		{
			update_func = app->tool_box.active_common_tool->motion_update;
			update_data = app->tool_box.active_common_tool;
		}
	}

	if((app->tool_box.flags & TOOL_USING_BRUSH) == 0)
	{
		EVENT_STATE state = {0};
		
		for(i=0; i<motion_queue->num_items; i++)
		{
			index = (motion_queue->start_index + i) % motion_queue->max_items;
			x = motion_queue->queue[index].state.cursor_x,  y = motion_queue->queue[index].state.cursor_y;
			app->tool_box.active_common_tool->motion_function(canvas, app->tool_box.active_common_tool, &motion_queue->queue[index].state);

			x0 = ((x-canvas->width/2)*canvas->cos_value + (y-canvas->height/2)*canvas->sin_value) * canvas->zoom_rate
				+ canvas->rev_add_cursor_x;
			y0 = (- (x-canvas->width/2)*canvas->sin_value + (y-canvas->height/2)*canvas->cos_value) * canvas->zoom_rate
				+ canvas->rev_add_cursor_y;

			canvas->before_cursor_x = x0;
			canvas->before_cursor_y = y0;

			update_func(canvas, x, y, update_data);
		}
	}
	else
	{
		if(canvas->active_layer->layer_type == TYPE_NORMAL_LAYER
			|| (canvas->flags & DRAW_WINDOW_EDIT_SELECTION) != 0)
		{
			for(i=0; i<motion_queue->num_items; i++)
			{
				index = (motion_queue->start_index + i) % motion_queue->max_items;
		
				app->tool_box.active_brush[app->input]->motion_function(canvas, app->tool_box.active_brush[app->input], &motion_queue->queue[index]);

				x = motion_queue->queue[index].state.cursor_x, y = motion_queue->queue[index].state.cursor_y;
				x0 = ((x-canvas->width/2)*canvas->cos_value + (y-canvas->height/2)*canvas->sin_value) * canvas->zoom_rate
					+ canvas->rev_add_cursor_x;
				y0 = (- (x-canvas->width/2)*canvas->sin_value + (y-canvas->height/2)*canvas->cos_value) * canvas->zoom_rate
					+ canvas->rev_add_cursor_y;

				canvas->before_cursor_x = x0;
				canvas->before_cursor_y = y0;

				update_func(canvas, x, y, update_data);
			}
		}
		else if(canvas->active_layer->layer_type == TYPE_VECTOR_LAYER)
		{
			for(i=0; i<motion_queue->num_items; i++)
			{
				index = (motion_queue->start_index + i) % motion_queue->max_items;
				x = motion_queue->queue[index].state.cursor_x,  y = motion_queue->queue[index].state.cursor_y;

				if((motion_queue->queue[index].state.mouse_key_flag & MOUSE_KEY_FLAG_SHIFT) != 0)
				{
					app->tool_box.vector_control.mode = CONTROL_POINT_DELETE;
					app->tool_box.vector_control_core.motion_func(
						canvas, &app->tool_box.vector_control_core, &motion_queue->queue[index].state
					);
				}
				else if((motion_queue->queue[index].state.mouse_key_flag & MOUSE_KEY_FLAG_CONTROL) != 0 ||
					(app->tool_box.vector_control.flags & CONTROL_POINT_TOOL_HAS_POINT) != 0)
				{
					app->tool_box.vector_control.mode = CONTROL_POINT_MOVE;
					app->tool_box.vector_control_core.motion_func(
						canvas, &app->tool_box.vector_control_core, &motion_queue->queue[index].state
					);
				}
				else
				{
					app->tool_box.active_vector_brush[app->input]->motion_func(
						canvas,	app->tool_box.active_vector_brush[app->input], &motion_queue->queue[index].state
					);
				}

				x0 = ((x-canvas->width/2)*canvas->cos_value + (y-canvas->height/2)*canvas->sin_value) * canvas->zoom_rate
					+ canvas->rev_add_cursor_x;
				y0 = (- (x-canvas->width/2)*canvas->sin_value + (y-canvas->height/2)*canvas->cos_value) * canvas->zoom_rate
					+ canvas->rev_add_cursor_y;

				canvas->before_cursor_x = x0;
				canvas->before_cursor_y = y0;

				update_func(canvas, x, y, update_data);
			}
		}
		else
		{
			for(i=0; i<motion_queue->num_items; i++)
			{
				index = (motion_queue->start_index + i) % motion_queue->max_items;
				x = motion_queue->queue[index].state.cursor_x, y = motion_queue->queue[index].state.cursor_y;

				TextLayerMotionCallBack(canvas, &motion_queue->queue[index].state);

				x0 = ((x-canvas->width/2)*canvas->cos_value + (y-canvas->height/2)*canvas->sin_value) * canvas->zoom_rate
					+ canvas->rev_add_cursor_x;
				y0 = (- (x-canvas->width/2)*canvas->sin_value + (y-canvas->height/2)*canvas->cos_value) * canvas->zoom_rate
					+ canvas->rev_add_cursor_y;

				canvas->before_cursor_x = x0;
				canvas->before_cursor_y = y0;

				update_func(canvas, x, y, update_data);
			}
		}
	}

	motion_queue->start_index = 0;
	motion_queue->num_items = 0;
}

#ifdef __cplusplus
}
#endif
